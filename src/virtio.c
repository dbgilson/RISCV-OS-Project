#include <virtio.h>
#include <virtioblk.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <pci.h>
#include <kmalloc.h>
#include <mmu.h>
#include <csr.h>

struct virtio_config_loc *pci_entropy_device;

//This function initializes the rng device through the common_cfg, as well as 
//gives it memory for the rings.
void virtio_rng_device_init(void){
    //printf("got into rng device init\n");
    pci_entropy_device->common_cfg->device_status = 0; // Device Reset
    pci_entropy_device->common_cfg->device_status = 1; // Acknowledge
    pci_entropy_device->common_cfg->device_status |= 2; // Driver

    //Read Device Features
    uint32_t features = pci_entropy_device->common_cfg->device_feature;
    //printf("RNG: Device Feature Set is: 0x%08lx\n", features);

    //Write Understood Feature Bits to Device
    pci_entropy_device->common_cfg->driver_feature = features;
    //printf("RNG: Driver Feature Set Returned: 0x%08lx\n", features);

    pci_entropy_device->common_cfg->device_status |= 8; // Features OK

    //printf("RNG: After setting device status to Feature OK (8), it returns: 0x%08lx\n", pci_entropy_device->common_cfg->device_status);
    //Setup Queue 0
    pci_entropy_device->common_cfg->queue_select = 0;
    uint16_t qsize = pci_entropy_device->common_cfg->queue_size;

    //Descriptor Table
    uint64_t virt = (uint64_t)kzalloc(16 * qsize);
    pci_entropy_device->desc = (struct virtq_desc *) virt;
    pci_entropy_device->common_cfg->queue_desc = mmu_translate(kernel_mmu_table, virt);
    //printf("RNG: Gave queue_desc address 0x%08lx. Virt Address was0x%08lx\n", pci_entropy_device->common_cfg->queue_desc, virt);

    //Driver Ring
    virt = (uint64_t)kzalloc(6 + 2 * qsize);
    pci_entropy_device->driver = (struct virtq_avail *) virt;
    pci_entropy_device->common_cfg->queue_driver = mmu_translate(kernel_mmu_table, virt);
    //printf("RNG: Gave queue_driver address 0x%08lx. Virt Address was0x%08lx\n", pci_entropy_device->common_cfg->queue_driver, virt);

    //Device ring
    virt = (uint64_t)kzalloc(6 + 8 * qsize);
    pci_entropy_device->device = (struct virtq_used *) virt;
    pci_entropy_device->common_cfg->queue_device = mmu_translate(kernel_mmu_table, virt);
    //printf("RNG: Gave queue_device address 0x%08lx. Virt Address was0x%08lx\n", pci_entropy_device->common_cfg->queue_device, virt);

    //Enable the queue
    pci_entropy_device->common_cfg->queue_enable = 1;

    //Make the device LIVE
    pci_entropy_device->common_cfg->device_status |= 4; //Driver OK
    //printf("RNG: After setting Device Live Status (4), it returns: 0x%08lx\n", pci_entropy_device->common_cfg->device_status);
    pci_entropy_device->enabled = true;
}

//This function takes a buffer and its size and tells the virtio entropy device to fill
//that buffer location with random bytes.
bool rng_fill(void *buffer, uint16_t size){
    uint64_t phys;
    uint32_t at_idx;
    uint32_t mod;
    if(pci_entropy_device->enabled == false){
        return false;
    }

    at_idx = pci_entropy_device->at_idx;
    mod = pci_entropy_device->common_cfg->queue_size;

    //VIRTIO Deals with Physical Memory
    phys = mmu_translate(kernel_mmu_table, (uint64_t)buffer);

    //Fill all fields of the descriptor.
    pci_entropy_device->desc[at_idx].addr = phys;
    pci_entropy_device->desc[at_idx].len = size;
    pci_entropy_device->desc[at_idx].flags = VIRTQ_DESC_F_WRITE;
    pci_entropy_device->desc[at_idx].next = 0;

    //Add descriptor to driver ring
    pci_entropy_device->driver->ring[pci_entropy_device->driver->idx % mod] = at_idx;

    //As soon as it increments, it is visibile
    pci_entropy_device->driver->idx += 1;
    pci_entropy_device->at_idx = (pci_entropy_device->at_idx + 1) % mod;
    //Write to notify register to tell it to "do something", and we can just
    //write a 0 to it.

    //printf("Calculated PCI Notification Location to Notify Entropy Device is: 0x%08lx\n\n", pci_entropy_device->baraddr + pci_entropy_device->notify_cap->cap.offset
    //               + pci_entropy_device->common_cfg->queue_notify_off * pci_entropy_device->notify_cap->notify_off_multiplier);

    //printf("Driver ring index is %d, Device ring index is %d\n", pci_entropy_device->driver->idx, pci_entropy_device->device->idx);
    *((uint32_t *)(pci_entropy_device->baraddr + pci_entropy_device->notify_cap->cap.offset
                   + pci_entropy_device->common_cfg->queue_notify_off * pci_entropy_device->notify_cap->notify_off_multiplier)) = 0;

    while (pci_entropy_device->at_idx != pci_entropy_device->device->idx) {
            pci_entropy_device->at_idx += 1;
        }
    return true;
}