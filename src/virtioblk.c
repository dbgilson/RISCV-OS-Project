#include <virtio.h>
#include <virtioblk.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <pci.h>
#include <kmalloc.h>
#include <cpage.h>
#include <mmu.h>
#include <csr.h>
#include <lock.h>
#include <printf.h>

#define NUM 8

#define R(addr) ((volatile uint32_t *)(VIRTIO_MMIO_BASE + (addr)))
#define BSIZE 512 // block size
#define PGSHIFT 12
#define PGSIZE 4096

typedef struct virtio_blk_hdr
{
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
} VirtioBlkHeader;

typedef struct virtio_blk_rq {
    VirtioBlkHeader hdr;
    char *data;
    int8_t status;
    uint16_t head;
} BlockRequest;

void virtio_blk_device_init(){
    //mutex_spinlock(&disk.vdisk_lock);
    struct virtio_blk_config *blkt4 = (struct virtio_blk_config *)(pci_blk_device->t4);
    pci_blk_device->common_cfg->device_status = 0; // Device Reset
    pci_blk_device->common_cfg->device_status = 1; // Acknowledge
    pci_blk_device->common_cfg->device_status |= 2; // Driver
    //Read Device Features
    uint32_t features = pci_blk_device->common_cfg->device_feature;
    printf("BLK: Device Feature Set is: 0x%08lx\n", features);

    //Write Understood Feature Bits to Device
    pci_blk_device->common_cfg->driver_feature = features;
    printf("BLK: Driver Feature Set Returned: 0x%08lx\n", features);

    pci_blk_device->common_cfg->device_status |= 8; // Features OK

    //Setup Queue 0
    pci_blk_device->common_cfg->queue_select = 0;
    uint16_t qsize = pci_blk_device->common_cfg->queue_size;

    //Descriptor Table
    //uint64_t virt = (uint64_t)kzalloc(16 * qsize);
    uint64_t virt = (uint64_t)cpage_znalloc(1);
    pci_blk_device->desc = (struct virtq_desc *) virt;
    pci_blk_device->common_cfg->queue_desc = mmu_translate(kernel_mmu_table, virt);
    printf("BLK: Gave queue_desc address 0x%08lx. Virt Address was0x%08lx\n", pci_blk_device->common_cfg->queue_desc, virt);

    //Driver Ring
    //virt = (uint64_t)kzalloc(6 + 2 * qsize);
    virt = (uint64_t)cpage_znalloc(1);
    pci_blk_device->driver = (struct virtq_avail *) virt;
    pci_blk_device->common_cfg->queue_driver = mmu_translate(kernel_mmu_table, virt);
    printf("BLK: Gave queue_driver address 0x%08lx. Virt Address was0x%08lx\n", pci_blk_device->common_cfg->queue_driver, virt);

    //Device ring
    //virt = (uint64_t)kzalloc(6 + 8 * qsize);
    virt = (uint64_t)cpage_znalloc(1);
    pci_blk_device->device = (struct virtq_used *) virt;
    pci_blk_device->common_cfg->queue_device = mmu_translate(kernel_mmu_table, virt);
    printf("BLK: Gave queue_device address 0x%08lx. Virt Address was0x%08lx\n", pci_blk_device->common_cfg->queue_device, virt);

    //Enable the queue
    pci_blk_device->common_cfg->queue_enable = 1;

    //Make the device LIVE
    pci_blk_device->common_cfg->device_status |= 4; //Driver OK
    printf("BLK: After setting Device Live Status (4), it returns: 0x%08lx\n", pci_blk_device->common_cfg->device_status);
    pci_blk_device->enabled = true;
}

bool virtio_disk_rw(char *buff, uint64_t byte_start, uint64_t byte_end, int rw){
  //printf("Got into virtio disk rw\n");
  BlockRequest *rq;

  //printf("Size of buff is %d\n", sizeof(buff));
  //Using 3 descriptors for header, data, and status
  uint16_t hdr_desc = (pci_blk_device->at_idx + 0) % (pci_blk_device->common_cfg->queue_size - 1);
  uint16_t data_desc = (pci_blk_device->at_idx + 1) % (pci_blk_device->common_cfg->queue_size - 1);
  uint16_t status_desc = (pci_blk_device->at_idx + 2) % (pci_blk_device->common_cfg->queue_size - 1);

  pci_blk_device->at_idx += 3;

  //Allocate Block Request
  rq = (BlockRequest *)kzalloc(sizeof(BlockRequest));

  struct virtio_blk_config *blk_config;
  blk_config = (struct virtio_blk_config *)(pci_blk_device->t4);

  //printf("byte_start / blk_config->blk_size is %d\n", byte_start / blk_config->blk_size);
  //Fill in header data
  rq->hdr.sector = byte_start / blk_config->blk_size;
  if(rw == 0){ //Reading
    rq->hdr.type = VIRTIO_BLK_T_IN;
    rq->data = (char *)buff;
    rq->head = hdr_desc;
  }
  else{        //Writing
    rq->hdr.type = VIRTIO_BLK_T_OUT;
    rq->data = (char *)buff;
  }
  rq->status = 99;

  //Put header data into 1st descriptor
  pci_blk_device->desc[hdr_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)rq);
  pci_blk_device->desc[hdr_desc].len = sizeof(VirtioBlkHeader);
  pci_blk_device->desc[hdr_desc].flags = VIRTQ_DESC_F_NEXT;
  pci_blk_device->desc[hdr_desc].next = data_desc;  

  //Put data location into 2nd descriptor
  pci_blk_device->desc[data_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)rq->data);
  pci_blk_device->desc[data_desc].len = byte_end - byte_start;
  if(rw == 0){ //Reading
    pci_blk_device->desc[data_desc].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
  }
  else{        //Writing
    pci_blk_device->desc[data_desc].flags = VIRTQ_DESC_F_NEXT;
  }
  pci_blk_device->desc[data_desc].next = status_desc;

  //Put status data into 3rd descriptor
  pci_blk_device->desc[status_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)&rq->status);
  pci_blk_device->desc[status_desc].len = sizeof(rq->status);
  pci_blk_device->desc[status_desc].flags = VIRTQ_DESC_F_WRITE;
  pci_blk_device->desc[status_desc].next = 0;  //Don't need next since it is the last descriptor

  //Update the actual driver ring index by putting the descriptor head on the ring
  pci_blk_device->driver->ring[pci_blk_device->driver->idx % pci_blk_device->common_cfg->queue_size] = hdr_desc;
  pci_blk_device->driver->idx += 1;

  // The device starts immediately when we write 0 to queue_notify.
  *((uint32_t *)(pci_blk_device->baraddr + pci_blk_device->notify_cap->cap.offset
                   + pci_blk_device->common_cfg->queue_notify_off * pci_blk_device->notify_cap->notify_off_multiplier)) = 0;

  //Adding reading poll functionality
  if(rw == 0){
    if(hdr_desc >= 0){
      while (pci_blk_device->device->idx == pci_blk_device->at_idx)
              ;
    }
  }
  return true;
}

void virtio_disk_isr(){
  //printf("BLK Device queue interrupt cleared\n");
  pci_blk_device->isr_cap->queue_interrupt = 0;
  while (pci_blk_device->at_idx != pci_blk_device->device->idx){
    //For future use
    int id = pci_blk_device->device->ring[pci_blk_device->at_idx % pci_blk_device->common_cfg->queue_size].id;

    //Increment at_idx
    pci_blk_device->at_idx += 1;
  }
}

//**************************************************************************************************
//Minix Block Device Operations Below

void minix_blk_device_init(){
    //mutex_spinlock(&disk.vdisk_lock);
    struct virtio_blk_config *blkt4 = (struct virtio_blk_config *)(minix_blk_device->t4);
    minix_blk_device->common_cfg->device_status = 0; // Device Reset
    minix_blk_device->common_cfg->device_status = 1; // Acknowledge
    minix_blk_device->common_cfg->device_status |= 2; // Driver
    //Read Device Features
    uint32_t features = minix_blk_device->common_cfg->device_feature;
    printf("MINIX: Device Feature Set is: 0x%08lx\n", features);

    //Write Understood Feature Bits to Device
    minix_blk_device->common_cfg->driver_feature = features;
    printf("MINIX: Driver Feature Set Returned: 0x%08lx\n", features);

    minix_blk_device->common_cfg->device_status |= 8; // Features OK

    //Setup Queue 0
    minix_blk_device->common_cfg->queue_select = 0;
    uint16_t qsize = minix_blk_device->common_cfg->queue_size;

    //Descriptor Table
    //uint64_t virt = (uint64_t)kzalloc(16 * qsize);
    uint64_t virt = (uint64_t)cpage_znalloc(1);
    minix_blk_device->desc = (struct virtq_desc *) virt;
    minix_blk_device->common_cfg->queue_desc = mmu_translate(kernel_mmu_table, virt);
    printf("MINIX: Gave queue_desc address 0x%08lx. Virt Address was0x%08lx\n", minix_blk_device->common_cfg->queue_desc, virt);

    //Driver Ring
    //virt = (uint64_t)kzalloc(6 + 2 * qsize);
    virt = (uint64_t)cpage_znalloc(1);
    minix_blk_device->driver = (struct virtq_avail *) virt;
    minix_blk_device->common_cfg->queue_driver = mmu_translate(kernel_mmu_table, virt);
    printf("MINIX: Gave queue_driver address 0x%08lx. Virt Address was0x%08lx\n", minix_blk_device->common_cfg->queue_driver, virt);

    //Device ring
    //virt = (uint64_t)kzalloc(6 + 8 * qsize);
    virt = (uint64_t)cpage_znalloc(1);
    minix_blk_device->device = (struct virtq_used *) virt;
    minix_blk_device->common_cfg->queue_device = mmu_translate(kernel_mmu_table, virt);
    printf("MINIX: Gave queue_device address 0x%08lx. Virt Address was0x%08lx\n", minix_blk_device->common_cfg->queue_device, virt);

    //Enable the queue
    minix_blk_device->common_cfg->queue_enable = 1;

    //Make the device LIVE
    minix_blk_device->common_cfg->device_status |= 4; //Driver OK
    printf("MINIX: After setting Device Live Status (4), it returns: 0x%08lx\n", minix_blk_device->common_cfg->device_status);
    minix_blk_device->enabled = true;
}

bool minix_disk_rw(char *buff, uint64_t byte_start, uint64_t byte_end, int rw){
  //printf("Got into virtio disk rw\n");
  BlockRequest *rq;

  //printf("Size of buff is %d\n", sizeof(buff));
  //Using 3 descriptors for header, data, and status
  uint16_t hdr_desc = (minix_blk_device->at_idx + 0) % (minix_blk_device->common_cfg->queue_size - 1);
  uint16_t data_desc = (minix_blk_device->at_idx + 1) % (minix_blk_device->common_cfg->queue_size - 1);
  uint16_t status_desc = (minix_blk_device->at_idx + 2) % (minix_blk_device->common_cfg->queue_size - 1);

  minix_blk_device->at_idx += 3;

  //Allocate Block Request
  //rq = (BlockRequest *)kzalloc(sizeof(BlockRequest));
  rq = (BlockRequest *)cpage_znalloc(1);

  struct virtio_blk_config *blk_config;
  blk_config = (struct virtio_blk_config *)(minix_blk_device->t4);

  //printf("byte_start / blk_config->blk_size is %d\n", byte_start / blk_config->blk_size);
  //Fill in header data
  rq->hdr.sector = byte_start / blk_config->blk_size;
  if(rw == 0){ //Reading
    rq->hdr.type = VIRTIO_BLK_T_IN;
    rq->data = (char *)buff;
    rq->head = hdr_desc;
  }
  else{        //Writing
    rq->hdr.type = VIRTIO_BLK_T_OUT;
    rq->data = (char *)buff;
  }
  rq->status = 99;

  //Put header data into 1st descriptor
  minix_blk_device->desc[hdr_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)rq);
  minix_blk_device->desc[hdr_desc].len = sizeof(VirtioBlkHeader);
  minix_blk_device->desc[hdr_desc].flags = VIRTQ_DESC_F_NEXT;
  minix_blk_device->desc[hdr_desc].next = data_desc;  

  //Put data location into 2nd descriptor
  minix_blk_device->desc[data_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)rq->data);
  minix_blk_device->desc[data_desc].len = byte_end - byte_start;
  if(rw == 0){ //Reading
    minix_blk_device->desc[data_desc].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
  }
  else{        //Writing
    minix_blk_device->desc[data_desc].flags = VIRTQ_DESC_F_NEXT;
  }
  minix_blk_device->desc[data_desc].next = status_desc;

  //Put status data into 3rd descriptor
  minix_blk_device->desc[status_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)&rq->status);
  minix_blk_device->desc[status_desc].len = sizeof(rq->status);
  minix_blk_device->desc[status_desc].flags = VIRTQ_DESC_F_WRITE;
  minix_blk_device->desc[status_desc].next = 0;  //Don't need next since it is the last descriptor

  //Update the actual driver ring index by putting the descriptor head on the ring
  minix_blk_device->driver->ring[minix_blk_device->driver->idx % minix_blk_device->common_cfg->queue_size] = hdr_desc;
  minix_blk_device->driver->idx += 1;

  // The device starts immediately when we write 0 to queue_notify.
  *((uint32_t *)(minix_blk_device->baraddr + minix_blk_device->notify_cap->cap.offset
                   + minix_blk_device->common_cfg->queue_notify_off * minix_blk_device->notify_cap->notify_off_multiplier)) = 0;

  //Adding reading poll functionality
  if(rw == 0){
    if(hdr_desc >= 0){
      while (minix_blk_device->device->idx == minix_blk_device->at_idx)
              ;
    }
  }
  return true;
}

void minix_disk_isr(){
  //printf("BLK Device queue interrupt cleared\n");
  minix_blk_device->isr_cap->queue_interrupt = 0;
  while (minix_blk_device->at_idx != minix_blk_device->device->idx){
    //For future use
    int id = minix_blk_device->device->ring[minix_blk_device->at_idx % minix_blk_device->common_cfg->queue_size].id;

    //Increment at_idx
    minix_blk_device->at_idx += 1;
  }
}


//***********************************************************************************************
//Ext4 block device operation below
void ext4_blk_device_init(){
    //mutex_spinlock(&disk.vdisk_lock);
    struct virtio_blk_config *blkt4 = (struct virtio_blk_config *)(ext4_blk_device->t4);
    ext4_blk_device->common_cfg->device_status = 0; // Device Reset
    ext4_blk_device->common_cfg->device_status = 1; // Acknowledge
    ext4_blk_device->common_cfg->device_status |= 2; // Driver
    //Read Device Features
    uint32_t features = ext4_blk_device->common_cfg->device_feature;
    printf("EXT4: Device Feature Set is: 0x%08lx\n", features);

    //Write Understood Feature Bits to Device
    ext4_blk_device->common_cfg->driver_feature = features;
    printf("EXT4: Driver Feature Set Returned: 0x%08lx\n", features);

    ext4_blk_device->common_cfg->device_status |= 8; // Features OK

    //Setup Queue 0
    ext4_blk_device->common_cfg->queue_select = 0;
    uint16_t qsize = ext4_blk_device->common_cfg->queue_size;

    //Descriptor Table
    //uint64_t virt = (uint64_t)kzalloc(16 * qsize);
    uint64_t virt = (uint64_t)cpage_znalloc(1);
    ext4_blk_device->desc = (struct virtq_desc *) virt;
    ext4_blk_device->common_cfg->queue_desc = mmu_translate(kernel_mmu_table, virt);
    printf("EXT4: Gave queue_desc address 0x%08lx. Virt Address was0x%08lx\n", ext4_blk_device->common_cfg->queue_desc, virt);

    //Driver Ring
    //virt = (uint64_t)kzalloc(6 + 2 * qsize);
    virt = (uint64_t)cpage_znalloc(1);
    ext4_blk_device->driver = (struct virtq_avail *) virt;
    ext4_blk_device->common_cfg->queue_driver = mmu_translate(kernel_mmu_table, virt);
    printf("EXT4: Gave queue_driver address 0x%08lx. Virt Address was0x%08lx\n", ext4_blk_device->common_cfg->queue_driver, virt);

    //Device ring
    //virt = (uint64_t)kzalloc(6 + 8 * qsize);
    virt = (uint64_t)cpage_znalloc(1);
    ext4_blk_device->device = (struct virtq_used *) virt;
    ext4_blk_device->common_cfg->queue_device = mmu_translate(kernel_mmu_table, virt);
    printf("EXT4: Gave queue_device address 0x%08lx. Virt Address was0x%08lx\n", ext4_blk_device->common_cfg->queue_device, virt);

    //Enable the queue
    ext4_blk_device->common_cfg->queue_enable = 1;

    //Make the device LIVE
    ext4_blk_device->common_cfg->device_status |= 4; //Driver OK
    printf("EXT4: After setting Device Live Status (4), it returns: 0x%08lx\n", ext4_blk_device->common_cfg->device_status);
    ext4_blk_device->enabled = true;
}

bool ext4_disk_rw(char *buff, uint64_t byte_start, uint64_t byte_end, int rw){
  //printf("Got into virtio disk rw\n");
  BlockRequest *rq;

  //printf("Size of buff is %d\n", sizeof(buff));
  //Using 3 descriptors for header, data, and status
  uint16_t hdr_desc = (ext4_blk_device->at_idx + 0) % (ext4_blk_device->common_cfg->queue_size - 1);
  uint16_t data_desc = (ext4_blk_device->at_idx + 1) % (ext4_blk_device->common_cfg->queue_size - 1);
  uint16_t status_desc = (ext4_blk_device->at_idx + 2) % (ext4_blk_device->common_cfg->queue_size - 1);

  ext4_blk_device->at_idx += 3;

  //Allocate Block Request
  //rq = (BlockRequest *)kzalloc(sizeof(BlockRequest));
  rq = (BlockRequest *)cpage_znalloc(1);

  struct virtio_blk_config *blk_config;
  blk_config = (struct virtio_blk_config *)(ext4_blk_device->t4);
  //printf("byte_start / blk_config->blk_size is %d\n", byte_start / blk_config->blk_size);
  //Fill in header data
  rq->hdr.sector = byte_start / blk_config->blk_size;
  if(rw == 0){ //Reading
    rq->hdr.type = VIRTIO_BLK_T_IN;
    rq->data = (char *)buff;
    rq->head = hdr_desc;
  }
  else{        //Writing
    rq->hdr.type = VIRTIO_BLK_T_OUT;
    rq->data = (char *)buff;
  }
  rq->status = 99;
  //Put header data into 1st descriptor
  ext4_blk_device->desc[hdr_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)rq);
  ext4_blk_device->desc[hdr_desc].len = sizeof(VirtioBlkHeader);
  ext4_blk_device->desc[hdr_desc].flags = VIRTQ_DESC_F_NEXT;
  ext4_blk_device->desc[hdr_desc].next = data_desc;  

  //Put data location into 2nd descriptor
  ext4_blk_device->desc[data_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)rq->data);
  ext4_blk_device->desc[data_desc].len = byte_end - byte_start;
  if(rw == 0){ //Reading
    ext4_blk_device->desc[data_desc].flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE;
  }
  else{        //Writing
    ext4_blk_device->desc[data_desc].flags = VIRTQ_DESC_F_NEXT;
  }
  ext4_blk_device->desc[data_desc].next = status_desc;

  //Put status data into 3rd descriptor
  ext4_blk_device->desc[status_desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)&rq->status);
  ext4_blk_device->desc[status_desc].len = sizeof(rq->status);
  ext4_blk_device->desc[status_desc].flags = VIRTQ_DESC_F_WRITE;
  ext4_blk_device->desc[status_desc].next = 0;  //Don't need next since it is the last descriptor

  //Update the actual driver ring index by putting the descriptor head on the ring
  ext4_blk_device->driver->ring[ext4_blk_device->driver->idx % ext4_blk_device->common_cfg->queue_size] = hdr_desc;
  ext4_blk_device->driver->idx += 1;

  // The device starts immediately when we write 0 to queue_notify.
  *((uint32_t *)(ext4_blk_device->baraddr + ext4_blk_device->notify_cap->cap.offset
                   + ext4_blk_device->common_cfg->queue_notify_off * ext4_blk_device->notify_cap->notify_off_multiplier)) = 0;

  //Adding reading poll functionality
  if(rw == 0){
    if(hdr_desc >= 0){
      while (ext4_blk_device->device->idx == ext4_blk_device->at_idx)
              ;
    }
  }
  return true;
}

void ext4_disk_isr(){
  //printf("BLK Device queue interrupt cleared\n");
  ext4_blk_device->isr_cap->queue_interrupt = 0;
  while (ext4_blk_device->at_idx != ext4_blk_device->device->idx){
    //For future use
    int id = ext4_blk_device->device->ring[ext4_blk_device->at_idx % ext4_blk_device->common_cfg->queue_size].id;

    //Increment at_idx
    ext4_blk_device->at_idx += 1;
  }
}