#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#pragma once

struct virtio_blk_config {
   uint64_t capacity;
   uint32_t size_max;
   uint32_t seg_max;
   struct virtio_blk_geometry {
      uint16_t cylinders;
      uint8_t heads;
      uint8_t sectors;
   } geometry;
   uint32_t blk_size; // the size of a sector, usually 512
   struct virtio_blk_topology {
      // # of logical blocks per physical block (log2)
      uint8_t physical_block_exp;
      // offset of first aligned logical block
      uint8_t alignment_offset;
      // suggested minimum I/O size in blocks
      uint16_t min_io_size;
      // optimal (suggested maximum) I/O size in blocks
      uint32_t opt_io_size;
   } topology;
   uint8_t writeback;
   uint8_t unused0[3];
   uint32_t max_discard_sectors;
   uint32_t max_discard_seg;
   uint32_t discard_sector_alignment;
   uint32_t max_write_zeroes_sectors;
   uint32_t max_write_zeroes_seg;
   uint8_t write_zeroes_may_unmap;
   uint8_t unused1[3];
};

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER 2
#define VIRTIO_CONFIG_S_DRIVER_OK 4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

// device feature bits
#define VIRTIO_BLK_F_RO 5          /* Disk is read-only */
#define VIRTIO_BLK_F_SCSI 7        /* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE 11 /* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ 12         /* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT 27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX 29

#define VRING_DESC_F_NEXT 1     // chained with another descriptor
#define VRING_DESC_F_WRITE 2    // device writes (vs read)
#define VRING_DESC_F_INDIRECT 4 // buffer contains a list of buffer descriptors

#define VIRTIO_BLK_T_IN 0  // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk
#define VIRTIO_BLK_T_FLUSH 4
#define VIRTIO_BLK_T_DISCARD 11
#define VIRTIO_BLK_T_WRITE_ZEROES 13

void virtio_blk_device_init(void);
bool virtio_disk_rw(char *buff, uint64_t byte_start, uint64_t byte_end, int rw);

//Functions for minix block device
void minix_blk_device_init(void);
bool minix_disk_rw(char *buff, uint64_t byte_start, uint64_t byte_end, int rw);

//Functions for ext4 block device
void ext4_blk_device_init(void);
bool ext4_disk_rw(char *buff, uint64_t byte_start, uint64_t byte_end, int rw);
