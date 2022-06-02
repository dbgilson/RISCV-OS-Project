#include <pci.h>
#include <printf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <kmalloc.h>
#include <memhelp.h>
#include <virtio.h>
#include <virtioblk.h>
#include <virtio_gpu.h>
#include <virtio_inp.h>
#include <ringbuff.h>
#include <csr.h>
#define MMIO_ECAM_BASE 0x30000000

uint64_t bar_address_free = 0x40000000;
uint64_t dev_count = 0;

struct virtio_config_loc *pci_blk_device;
struct virtio_config_loc *minix_blk_device;
struct virtio_config_loc *ext4_blk_device;

Virt_GPU_Device *gdev;

volatile VirtioIptDevice *ipt;
volatile VirtioIptDevice *keyboard;
volatile int ipt_dev_idx;
volatile RingBuffer *ipt_ringbuf;
static inline uint64_t pack_event(uint64_t evaddr){
    return *((const uint64_t *)evaddr);
}

//This struct holds a list of the found drivers
//This is used for pci_find_driver, pci_register_driver, and pci_dispatch_irq (not currently implemented)
typedef struct PciDriverListElem{
    uint16_t vendorid;
    uint16_t deviceid;
    PciDriver drv;
    struct PciDriverListElem *next;
} PciDriverListElem;

//Pointer to driver list head
static PciDriverListElem *d_list_head;

//This function simply probes the ecam list to see if slot is active 
bool PciDriver_probe(uint8_t b, uint8_t s, uint8_t f){
    struct EcamHeader *ec = pci_get_ecam(b, s, f, 0);
    if (ec->vendor_id == 0xffff || ec->header_type != 0){
        return false;
    }
    else{
        return true;
    }
}

//This function will be used to claim an IRQ in correlation with the plic
bool PciDriver_claim_irq(int irq){
    PciDriverListElem *elem;
    int pci_irq = 0;
    for(elem = d_list_head; elem != NULL; elem = elem->next){
        pci_irq = 32 + ((elem->drv.bus + elem->drv.slot) % 4);
        if(pci_irq == irq){
            //Possible match, so ask device
            //Only one device is up right now (entropy), so will make a better
            //system in the future instead of hardcoding (just getting it working right now)
            if(pci_entropy_device->isr_cap->queue_interrupt){ // Entropy Device Check
                pci_entropy_device->isr_cap->queue_interrupt = 0;
                printf("Entropy Device queue interrupt cleared\n");
                return true;
            }
            if (pci_blk_device->isr_cap->queue_interrupt){ // Disk Device Check
                virtio_disk_isr();
                return true;
            }
            if (gdev->isr->queue_interrupt){ // GPU Device Check
                gdev->isr->queue_interrupt = 0;
                while (gdev->devidx != gdev->qdevice->idx) {
                    gdev->devidx += 1;
                }
                //printf("GPU Device queue interrupt cleared\n");
                return true;
            }
            if (ipt->isr->queue_interrupt){ // Input 0 Device Check
                ipt->isr->queue_interrupt = 0;
                while (ipt->devidx != ipt->qdevice->idx) {
                    int s = ipt->devidx % ipt->common->queue_size;
                    struct virtq_used_elem *elem = ipt->qdevice->ring + s;
                    struct virtq_desc *desc = ipt->qdesc + elem->id;
                    ring_push(ipt_ringbuf, (void *)pack_event(desc->addr));
                    ipt->qdriver->idx += 1;
                    ipt->devidx += 1;
                }
                printf("IPT Device queue interrupt cleared\n");
                return true;
            }
            if (keyboard->isr->queue_interrupt){ // Input 0 Device Check
                keyboard->isr->queue_interrupt = 0;
                while (keyboard->devidx != keyboard->qdevice->idx) {
                    int s = keyboard->devidx % keyboard->common->queue_size;
                    struct virtq_used_elem *elem = keyboard->qdevice->ring + s;
                    struct virtq_desc *desc = keyboard->qdesc + elem->id;
                    ring_push(ipt_ringbuf, (void *)pack_event(desc->addr));
                    keyboard->qdriver->idx += 1;
                    keyboard->devidx += 1;
                }
                printf("Keyboard Device queue interrupt cleared\n");
                return true;
            }
        }
    }
}

//This function initializes the drivers (devices) that are found once a bridge has been initialized
void PciDriver_init(void){
    int bus;
    int slot;
    for (bus = 0;bus < 256;bus++) {
        for (slot = 0;slot < 32;slot++) {
            struct EcamHeader *ec = pci_get_ecam(bus, slot, 0, 0);
            if (ec->vendor_id == 0xffff) continue;
            if (ec->header_type != 0) continue;

            //We found a device, update the driver list
            ec->command_reg = COMMAND_REG_MMIO; //Make sure to enable this for bar access
            PciDriver *drvtmp;
            drvtmp = (PciDriver *)kmalloc(sizeof(PciDriver));
            drvtmp->bus = bus;
            drvtmp->slot = slot;
            drvtmp->function = 0;
            drvtmp->vendor_id = ec->vendor_id;
            drvtmp->device_id = ec->device_id;

            //Register the driver (device) to the driver element list
            pci_register_driver(ec->vendor_id, ec->device_id, drvtmp);
        }
    }
}

//This function returns a pointer to the Ecam Header specified by the given bus, device, function, and reg
static volatile struct EcamHeader *pci_get_ecam(uint8_t bus, uint8_t device, uint8_t function, uint16_t reg) {
    uint64_t bus64 = bus & 0xff;
    uint64_t device64 = device & 0x1f;
    uint64_t function64 = function & 0x7;
    uint64_t reg64 = reg & 0x3ff; 
    return (struct EcamHeader *)(MMIO_ECAM_BASE | (bus64 << 20) | (device64 << 15) | (function64 << 12) | (reg64 << 2));
}

//This function prints out connected devices (type 1) to the console
void print_pci_connect(void){
    int bus;
    int slot;
    //We loop through each slot for each bus, checking its ECAM header.
    //If it has an invalid vendor id or is a bridge (type 1), we skip it.
    //Otherwise, we print the device info
    for (bus = 0;bus < 256;bus++) {
        for (slot = 0;slot < 32;slot++) {
            struct EcamHeader *ec = pci_get_ecam(bus, slot, 0, 0);
            if (ec->vendor_id == 0xffff) continue;
            if (ec->header_type != 0) continue;
 
            printf("Device at bus %d, slot %d (MMIO @ 0x%08lx), class: 0x%04x\n", bus, slot, ec, ec->class_code);
            printf("   Device ID    : 0x%04x, Vendor ID    : 0x%04x\n", ec->device_id, ec->vendor_id);
        }
    }
}

//This function registers the driver (device) to the driver element list
//It also allocates the bars using the pci_allocate_bar function
void pci_register_driver(uint16_t vendor, uint16_t device, const PciDriver *drv){
    if (pci_find_driver(vendor, device) != NULL) {
        //printf("[pcie_register_driver]: Already registered VID: 0x%04x, DID: 0x%04x\n", vendor, device);
        return;
    }

    //Allocate memory for the element and append it to the list
    PciDriverListElem *elem;
    elem = (PciDriverListElem *)kmalloc(sizeof(PciDriverListElem));
    memcpy(&elem->drv, drv, sizeof(PciDriver));
    elem->vendorid = vendor;
    elem->deviceid = device;
    elem->next = d_list_head;
    d_list_head = elem;
    printf("Registered   Device ID    : 0x%04x, Vendor ID    : 0x%04x\n", elem->deviceid, elem->vendorid);

    //Allocate the bars for the device (skip certain bar numbers if they read back as a 64 bit bar)
    for(int i = 0; i < 6; i++){
        if(pci_allocate_bar(drv, i) == 2){
            i++;
        }
    }
    dev_count++;
    bar_address_free = 0x40000000 + (0x100000 * dev_count);
    printf("\n");
}

//Configure type 0 header (pci device)
void pci_config_t0(uint8_t bus, uint8_t slot, uint8_t function){
    //Don't want to config root pci node
    if(bus == 0 && slot == 0){
        printf("\nRoot PCI NODE Found\n\n");
        return;
    }
    struct EcamHeader *ec = pci_get_ecam(bus, slot, function, 0);
    PciDriver *drv = pci_find_driver(ec->vendor_id, ec->device_id);

    //Try to find the driver using probe function.  If found, initialize the driver
    if(drv == NULL || !PciDriver_probe(bus, slot, function)){
        printf("Could not initialize PCI 0x%04x:0x%04x, couldn't find driver\n", ec->vendor_id, ec->device_id);
    }
    else if(bus != 0 && slot != 0){
        //drv->init();
        //I've pretty much accomplished drv->init() with PciDriver_init, but
        //I may reorganize this structure in the future
    }
}

//Configure type 1 header (pci bridge)
void pci_config_t1(uint8_t bus, uint8_t slot){
    //Make subordinate static as each additional bridge will will have the previous
    //one as a subordinate bridge.
    static uint8_t subordinate = 1;
    struct EcamHeader *ec = pci_get_ecam(bus, slot, 0, 0);
    ec->command_reg = COMMAND_REG_MMIO | (1 << 2);
    //These count as the upper 4 bytes of the address
    ec->type1.memory_base = 0x4000;
    ec->type1.memory_limit = 0x4fff;
    ec->type1.prefetch_memory_base = 0x4000;
    ec->type1.prefetch_memory_limit = 0x4fff;
    ec->type1.primary_bus_no = bus;
    ec->type1.secondary_bus_no = subordinate++;
    //printf("Configured PCI bus %d, slot %d for bridge with sub = %d\n\n", bus, slot, subordinate);

    //Allocate the bars for the bridge (skip certain bar numbers if they read back as a 64 bit bar)
    PciDriver *drvtmp;
    drvtmp = (PciDriver *)kmalloc(sizeof(PciDriver));
    drvtmp->bus = bus;
    drvtmp->slot = slot;
    drvtmp->function = 0;
    for(int i = 0; i < 2; i++){
        if(pci_allocate_bar(drvtmp, i) == 2){
            i++;
        }
    }
    dev_count++;
    bar_address_free = 0x40000000 + (0x100000 * dev_count);
    printf("\n");
}

//This function finds a driver in the driver element list and returns it, returns null otherwise
PciDriver *pci_find_driver(uint16_t vendor, uint16_t device){
    PciDriverListElem *elem;
    for(elem = d_list_head; elem != NULL; elem = elem->next){
        if((elem->vendorid == vendor) && (elem->deviceid == device)){
            return &elem->drv;
        }
    }
    return NULL;
}

//This function is a placeholder for dispatching irqs through the pci.
bool pci_dispatch_irq(int irq){
    PciDriverListElem *elem;
    for(elem = d_list_head; elem != NULL; elem = elem->next){
        if(elem->drv.claim_irq && elem->drv.claim_irq(irq)){
            return true;
        }
    }
    return false;
}

//This function enumerates through the ECAM structure and configures the type 0 (devices)
//and type 1 (bridges) units it finds.
void pci_init(void){
    uint16_t bus;
    uint16_t slot;
    struct EcamHeader *ec;
    //Go through all the slots and check if there is a valid vender.
    //If so, delegate to either the type 1 or type 0 config function
    for(bus = 0; bus < 256; bus += 1){
        for(slot = 0; slot < 32; slot += 1){
            ec = pci_get_ecam(bus, slot, 0, 0);
            if(ec->vendor_id != 0xffff && ec->vendor_id != 0x0000){
                //We have a valid slot, now we need to delegate the type
                if(ec->header_type == 0){
                    pci_config_t0(bus, slot, 0);
                }
                else{
                    pci_config_t1(bus, slot);
                    PciDriver_init();
                }
            }
        }
    }
    
    //Testing pci_entropy_device setup/allocation
    pci_entropy_device = (struct virtio_config_loc *)kzalloc(sizeof(struct virtio_config_loc));
    pci_entropy_device->at_idx = 0;
    pci_entropy_device->enabled = false;
    pci_cape_ptr_check(1,2); //Entropy Device

    //Testing pci_blk_device setup/allocation
    pci_blk_device = (struct virtio_config_loc *)kzalloc(sizeof(struct virtio_config_loc));
    pci_blk_device->at_idx = 0;
    pci_blk_device->enabled = false;
    pci_cape_ptr_check(2,1); //Blk Device

    minix_blk_device = (struct virtio_config_loc *)kzalloc(sizeof(struct virtio_config_loc));
    minix_blk_device->at_idx = 0;
    minix_blk_device->enabled = false;
    PciDriver *drvtmp;
    drvtmp = (PciDriver *)kmalloc(sizeof(PciDriver));
    drvtmp->bus = 2;
    drvtmp->slot = 2;
    drvtmp->function = 0;
    for(int i = 0; i < 6; i++){
        if(pci_allocate_bar(drvtmp, i) == 2){
            i++;
        }
    }
    dev_count++;
    bar_address_free = 0x40000000 + (0x100000 * dev_count);
    pci_cape_ptr_check(2,2); //MINIX Device


    ext4_blk_device = (struct virtio_config_loc *)kzalloc(sizeof(struct virtio_config_loc));
    ext4_blk_device->at_idx = 0;
    ext4_blk_device->enabled = false;
    PciDriver *drvtmp2;
    drvtmp2 = (PciDriver *)kmalloc(sizeof(PciDriver));
    drvtmp2->bus = 2;
    drvtmp2->slot = 3;
    drvtmp2->function = 0;
    for(int i = 0; i < 6; i++){
        if(pci_allocate_bar(drvtmp2, i) == 2){
            i++;
        }
    }
    dev_count++;
    bar_address_free = 0x40000000 + (0x100000 * dev_count);
    pci_cape_ptr_check(2,3); //Ext4 Device

    //Testing gpu_device setup/allocation
    gdev = (struct Virt_GPU_Device *)kzalloc(sizeof(struct Virt_GPU_Device));
    pci_cape_ptr_check(2,0);


    //Testing input device setup/allocation
    ipt = (VirtioIptDevice *)kzalloc(sizeof(VirtioIptDevice) * 2);
    pci_cape_ptr_check(1,0);

    keyboard = (VirtioIptDevice *)kzalloc(sizeof(VirtioIptDevice) * 2);
    PciDriver *drvtmp3;
    drvtmp3 = (PciDriver *)kmalloc(sizeof(PciDriver));
    drvtmp3->bus = 1;
    drvtmp3->slot = 1;
    drvtmp3->function = 0;
    for(int i = 0; i < 6; i++){
        if(pci_allocate_bar(drvtmp3, i) == 2){
            i++;
        }
    }
    dev_count++;
    bar_address_free = 0x40000000 + (0x100000 * dev_count);
    pci_cape_ptr_check(1,1);


    virtio_rng_device_init();
    virtio_blk_device_init();
    minix_blk_device_init();
    ext4_blk_device_init();
    virtio_gpu_init();

    //Allocate Ringbuffer
    ipt_ringbuf = ring_new(256, RB_DISCARD);
    virtio_input_init(ipt);
    virtio_input_init(keyboard);

}

//This function will go through the capability list first pointed to by the ecam header
//related to the bus and slot given.  It will then delegate the capabilities to the specific 
//cape ids found.
void pci_cape_ptr_check(uint16_t bus, uint16_t slot){
    struct Capability{
        uint8_t id;
        uint8_t next;
    };
    volatile struct EcamHeader *ecam = pci_get_ecam(bus, slot, 0, 0);
    //printf("Device ID is: 0x%08lx\n", ecam->device_id);
    // Make sure there are capabilities (bit 4 of the status register).
    if (0 != (ecam->status_reg & (1 << 4))){
        unsigned char capes_next = ecam->common.capes_pointer;
        while (capes_next != 0){
            unsigned long cap_addr = (unsigned long)pci_get_ecam(bus, slot, 0, 0) + capes_next;
            struct Capability *cap = (struct Capability *)cap_addr;
            //printf("CAP ID is: 0x%08lx\n", cap->id);;
            switch (cap->id){
            case 0x09: //Vendor Specific
            {
                //Change cap pointer to virtio cap pointer
                struct virtio_pci_cap *v_pci_cap = (struct virtio_pci_cap *)cap;
               
				/*
                printf("Capability @ 0x%02x\n", capes_next);
                printf("    u8 cap_vndr = 0x%02x\n", v_pci_cap->cap_vndr);
                printf("    u8 cap_next = 0x%02x\n", v_pci_cap->cap_next);
                printf("    u8 cap_len = 0x%02x\n", v_pci_cap->cap_len);
                printf("    u8 cap_type = 0x%02x\n", v_pci_cap->cfg_type);
                printf("    u8 bar = 0x%02x\n", v_pci_cap->bar);
                printf("    u32 offset = 0x%08lx\n", (uint32_t)v_pci_cap->offset);
                printf("    u32 length = 0x%08lx\n", v_pci_cap->length);
                */
               //Delegate virtio capability type
                switch (v_pci_cap->cfg_type) {
                    case VIRTIO_PCI_CAP_COMMON_CFG: // 1
                        if(ecam->device_id == 0x1052){ //Input Device
                            if(slot == 0){
                                ipt->common = (struct virtio_pci_common_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to IPT_common_cfg is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)ipt->common,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else{
                                keyboard->common = (struct virtio_pci_common_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to Keyboard_common_cfg is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)keyboard->common,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                        }
                        if(ecam->device_id == 0x1050){ //GPU
                            gdev->common = (struct virtio_pci_common_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                            printf("Address given to GPU_common_cfg is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)gdev->common,
                            v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                        }
                        if(ecam->device_id == 0x1044){ //Entropy Device
                            pci_entropy_device->baraddr = ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0);
                            pci_entropy_device->common_cfg = (struct virtio_pci_common_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                            printf("Address given to rng_common_cfg is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)pci_entropy_device->common_cfg,
                            v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                        }
                        if(ecam->device_id == 0x1042){ //Block Device
                            if(slot == 1){
                                pci_blk_device->baraddr = ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0);
                                pci_blk_device->common_cfg = (struct virtio_pci_common_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to blk_common_cfg is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)pci_blk_device->common_cfg,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else if (slot == 2){
                                minix_blk_device->baraddr = ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0);
                                minix_blk_device->common_cfg = (struct virtio_pci_common_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to minix_blk_device_common_cfg is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)minix_blk_device->common_cfg,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else{
                                ext4_blk_device->baraddr = ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0);
                                ext4_blk_device->common_cfg = (struct virtio_pci_common_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to ext4_blk_device_common_cfg is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)ext4_blk_device->common_cfg,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                        }
                    break;
                    case VIRTIO_PCI_CAP_NOTIFY_CFG: // 2
                        if(ecam->device_id == 0x1052){ //Input Device
                            if(slot == 0){
                                ipt->notify = (struct virtio_pci_notify_cap *)v_pci_cap;
                                ipt->qnotmult = ipt->notify->notify_off_multiplier;
                                ipt->qnotaddr = (uint64_t)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to IPT_notify_cap is 0x%08lx\n", (unsigned long)&ipt->notify);
                            }
                            else{
                                keyboard->notify = (struct virtio_pci_notify_cap *)v_pci_cap;
                                keyboard->qnotmult = keyboard->notify->notify_off_multiplier;
                                keyboard->qnotaddr = (uint64_t)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to Keyboard_notify_cap is 0x%08lx\n", (unsigned long)&keyboard->notify);
                            }
                        }
                        if(ecam->device_id == 0x1050){ //GPU Device
                            gdev->notify = (struct virtio_pci_notify_cap *)v_pci_cap;
                            gdev->qnotmult = gdev->notify->notify_off_multiplier;
                            gdev->qnotaddr = (uint64_t)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                            printf("Address given to GPU_notify_cap is 0x%08lx\n", (unsigned long)&gdev->notify);
                        }
                        if(ecam->device_id == 0x1044){ //Entropy Device
                            pci_entropy_device->notify_cap = (struct virtio_pci_notify_cap *)v_pci_cap;
                            printf("Address given to rng_notify_cap is 0x%08lx\n", (unsigned long)&pci_entropy_device->notify_cap);
                        }
                        if(ecam->device_id == 0x1042){ //Block Device
                            if(slot == 1){
                                pci_blk_device->notify_cap = (struct virtio_pci_notify_cap *)v_pci_cap;
                                printf("Address given to blk_notify_cap is 0x%08lx\n", (unsigned long)&pci_blk_device->notify_cap);
                            }
                            else if(slot == 2){
                                minix_blk_device->notify_cap = (struct virtio_pci_notify_cap *)v_pci_cap;
                                printf("Address given to minix_blk_device_notify_cap is 0x%08lx\n", (unsigned long)&minix_blk_device->notify_cap);
                            }
                            else{
                                ext4_blk_device->notify_cap = (struct virtio_pci_notify_cap *)v_pci_cap;
                                printf("Address given to ext4_blk_device_notify_cap is 0x%08lx\n", (unsigned long)&ext4_blk_device->notify_cap);
                            }
                        }
                    break;
                    case VIRTIO_PCI_CAP_ISR_CFG:    // 3
                        if(ecam->device_id == 0x1052){ //Input Device
                            if(slot == 0){
                                ipt->isr = (struct virtio_pci_isr_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to IPT_isr_cap is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)ipt->isr,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else{
                                keyboard->isr = (struct virtio_pci_isr_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to Keyboard_isr_cap is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)keyboard->isr,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                        }
                        if(ecam->device_id == 0x1050){ //GPU Device
                            gdev->isr = (struct virtio_pci_isr_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                            printf("Address given to GPU_isr_cap is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)gdev->isr,
                            v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                        }
                        if(ecam->device_id == 0x1044){ //Entropy Device
                            pci_entropy_device->isr_cap = (struct virtio_pci_isr_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                            printf("Address given to isr_cap is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)pci_entropy_device->isr_cap,
                            v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                        }
                         if(ecam->device_id == 0x1042){ //BLK Device
                            if(slot == 1){
                                pci_blk_device->isr_cap = (struct virtio_pci_isr_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to blk_isr_cap is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)pci_blk_device->isr_cap,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else if(slot == 2){
                                minix_blk_device->isr_cap = (struct virtio_pci_isr_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to minix_blk_device_isr_cap is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)minix_blk_device->isr_cap,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else{
                                ext4_blk_device->isr_cap = (struct virtio_pci_isr_cfg *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to ext4_blk_device_isr_cap is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n", (unsigned long)ext4_blk_device->isr_cap,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                        }
                    break;
                    case VIRTIO_PCI_CAP_DEVICE_CFG: // 4
                        if(ecam->device_id == 0x1052){ //Input Device
                            if(slot == 0){
                                ipt->t4 = (struct VirtioIptConfig *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to IPT_type_4 struct is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n",(unsigned long)ipt->t4,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else{
                                keyboard->t4 = (struct VirtioIptConfig *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to Keyboard_type_4 struct is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n",(unsigned long)keyboard->t4,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                        }
                        if(ecam->device_id == 0x1050){ //GPU Device
                            gdev->t4 = (struct VirtioGpuConfig *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                            printf("Address given to GPU_type_4 struct is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n",(unsigned long)gdev->t4,
                            v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                        }
                        if(ecam->device_id == 0x1042){ //BLK Device
                            if(slot == 1){
                                pci_blk_device->t4 = (struct virtio_blk_config *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to blk_type_4 struct is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n",(unsigned long)pci_blk_device->t4,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else if(slot == 2){
                                minix_blk_device->t4 = (struct virtio_blk_config *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to minix_blk_device_type_4 struct is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n",(unsigned long)minix_blk_device->t4,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                            else{
                                ext4_blk_device->t4 = (struct virtio_blk_config *)(((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0) + (uint32_t)v_pci_cap->offset);
                                printf("Address given to ext4_blk_device_type_4 struct is 0x%08lx at BAR: %d, Bar address 0x%08lx + Cap Offset 0x%08lx\n",(unsigned long)ext4_blk_device->t4,
                                v_pci_cap->bar, ((uint32_t)ecam->type0.bar[v_pci_cap->bar] & 0xFFFFFFF0), (uint32_t)v_pci_cap->offset);
                            }
                        }
                    break;
                    case VIRTIO_PCI_CAP_PCI_CFG:    //5
                     break;
                    default:
                        //printf("Unknown virtio capability %d\n", v_pci_cap->cfg_type);
                    break;
                }

            }
            break;
            case 0x10: //PCI Express
            {

            }
            break;
            case 0x11: //Blk
            {

            }
            break;
            default:
                //printf("Unknown capability ID 0x%02x (next: 0x%02x)\n", cap->id, cap->next);
                return;
                break;
            }
            capes_next = cap->next;
        }
    }
}

//This function allocates the bar for the given driver (device)
int pci_allocate_bar(struct PciDriver * drv, int which){
    uint64_t bar;

    //Check size of bar
    pci_set_bar(drv->bus, drv->slot, drv->function, which, -1UL);

    bar = pci_get_bar(drv->bus, drv->slot, drv->function, which) & ~0xfUL; //Make sure to mask last 4 bits

    if (bar == 0x00000000){
            //printf("Don't need to allocate Bar %d since bar returned 0x00000000\n", which);
            return 1;
    }
    //Got bar size, give the next free address
    //printf("Bar Address Free Given to Set Bar is 0x%08lx\n", bar_address_free);
    pci_set_bar(drv->bus, drv->slot, drv->function, which, (bar_address_free & bar));

    printf("Gave PCI 0x%04x:0x%04x Bar %d the free bar address of 0x%08lx, bar was 0x%08lx\n", drv->vendor_id, drv->device_id, which, (bar_address_free & bar), bar);
    //Add bar size to the bar address free (take 2's complement)
    bar_address_free += (-bar & 0x00000000ffffffff) + 0x10000;
    
    if (bar > 0xffffffff){
        return 2;
    }
    else{
        return 1;
    }
}

//This function returns the value read from the BAR (used for after we set the bar with -1)
uint64_t pci_get_bar(uint16_t bus, uint16_t slot, uint16_t function, uint16_t bar) {
    volatile struct EcamHeader *ecam = pci_get_ecam(bus, slot, function, 0);
    uint32_t bartype = (ecam->type0.bar[bar] >> 1) & 0b11;
    if (NULL == ecam || bar > 5) {
        return 0;
    }
    else if (0b10 == bartype) {
        return ((uint64_t)(ecam->type0.bar[bar + 1]) << 32) | ecam->type0.bar[bar];
    }
    else if (0b00 == bartype) {
        return ecam->type0.bar[bar];
    }
    else {
        //Invalid bar
        return -1UL;
    }
}

//This function sets a bar with the given value, essentially giving it the desired address we want.
void pci_set_bar(uint16_t bus, uint16_t slot, uint16_t function, uint16_t bar, uint64_t value) {
    volatile struct EcamHeader *ecam = pci_get_ecam(bus, slot, function, 0);
    uint32_t bartype = (ecam->type0.bar[bar] >> 1) & 0b11;
    if (NULL == ecam) {
        return;
    }
    else if (bartype == 0b10) {
        ecam->type0.bar[bar] = value;
        ecam->type0.bar[bar + 1] = value >> 32;
        //printf("Set type 0 64 Bit Bar %d to 0x%08lx\n", bar, value);
    }
    else if (bartype == 0b00) {
        ecam->type0.bar[bar] = value;
        //printf("Set type 0 32 Bit Bar %d to 0x%08lx\n", bar, value);
    }
    else {
        //Invalid bar
        return -1UL;
    }
}
