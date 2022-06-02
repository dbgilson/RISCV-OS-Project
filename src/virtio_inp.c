#include <virtio_inp.h>
#include <ringbuff.h>
#include <input-event-codes.h>
#include <virtio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <memhelp.h>
#include <pci.h>
#include <kmalloc.h>
#include <mmu.h>
#include <csr.h>
#include <lock.h>
#include <cpage.h>


static inline uint64_t pack_event(uint64_t evaddr){
    return *((const uint64_t *)evaddr);
}

void virtio_input_init(volatile VirtioIptDevice *ipt){
    ipt->common->device_status = 0; // Device Reset
    ipt->common->device_status = 1; // Acknowledge
    ipt->common->device_status |= 2; // Driver

    //Read Device Features
    uint32_t features = ipt->common->device_feature;
    //printf("IPT: Device Feature Set is: 0x%08lx\n", features);

    //Write Understood Feature Bits to Device
    ipt->common->driver_feature = features;
    //printf("IPT: Driver Feature Set Returned: 0x%08lx\n", features);

    ipt->common->device_status |= 8; // Features OK

    //printf("IPT: After setting device status to Feature OK (8), it returns: 0x%08lx\n", ipt->common->device_status);
    //Setup Queue 0
    ipt->common->queue_select = 0;
    ipt->common->queue_size = 512;
    uint16_t qsize = ipt->common->queue_size;

    //Descriptor Table
    //uint64_t virt = (uint64_t)kzalloc(16 * qsize);
    uint64_t virt = (uint64_t)cpage_znalloc(5);
    ipt->qdesc = (struct virtq_desc *) virt;
    ipt->common->queue_desc = mmu_translate(kernel_mmu_table, virt);
    //printf("IPT: Gave queue_desc address 0x%08lx. Virt Address was0x%08lx\n", ipt->common->queue_desc, virt);

    //Driver Ring
    //virt = (uint64_t)kzalloc(6 + 2 * qsize);
    virt = (uint64_t)cpage_znalloc(1);
    ipt->qdriver = (struct virtq_avail *) virt;
    ipt->common->queue_driver = mmu_translate(kernel_mmu_table, virt);
    //printf("IPT: Gave queue_driver address 0x%08lx. Virt Address was0x%08lx\n", ipt->common->queue_driver, virt);

    //Device ring
    //virt = (uint64_t)kzalloc(6 + 8 * qsize);
    virt = (uint64_t)cpage_znalloc(3);
    ipt->qdevice = (struct virtq_used *) virt;
    ipt->common->queue_device = mmu_translate(kernel_mmu_table, virt);
    //printf("IPT: Gave queue_device address 0x%08lx. Virt Address was0x%08lx\n", ipt->common->queue_device, virt);

    //Enable the queue
    ipt->common->queue_enable = 1;

    //Make the device LIVE
    ipt->common->device_status |= 4; //Driver OK
    //printf("IPT: After setting Device Live Status (4), it returns: 0x%08lx\n", ipt->common->device_status);
    //pci_entropy_device->enabled = true;


    InputEvent *evts = (InputEvent *)cpage_znalloc(ALIGN_UP_POT(ipt->common->queue_size * sizeof(InputEvent), PAGE_SIZE) / PAGE_SIZE);

    for (uint16_t i = 0; i < ipt->common->queue_size; i++) {
        InputEvent *ev = evts + i;
        uint64_t phys = mmu_translate(kernel_mmu_table, (uint64_t)ev);
        ipt->qdesc[i].addr = phys;
        ipt->qdesc[i].len = sizeof(InputEvent);
        ipt->qdesc[i].flags = VIRTQ_DESC_F_WRITE;
        ipt->qdesc[i].next = 0;
        ipt->qdriver->ring[ipt->drvidx % ipt->common->queue_size] = i;
        ipt->drvidx += 1;
    }
    ipt->qdriver->idx += ipt->common->queue_size;

};