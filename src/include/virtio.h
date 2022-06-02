#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#pragma once

struct virtio_pci_cap {
   uint8_t cap_vndr;   /* Generic PCI field: PCI_CAP_ID_VNDR */
   uint8_t cap_next;   /* Generic PCI field: next ptr. */
   uint8_t cap_len;    /* Generic PCI field: capability length */
   uint8_t cfg_type;   /* Identifies the structure. */
   uint8_t bar;        /* Which BAR to find it. */
   uint8_t padding[3]; /* Pad to full dword. */
   uint32_t offset;    /* Offset within bar. */
   uint32_t length;    /* Length of the structure, in bytes. */
};

/* Common configuration */
#define VIRTIO_PCI_CAP_COMMON_CFG 1
/* Notifications */
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
/* ISR Status */
#define VIRTIO_PCI_CAP_ISR_CFG 3
/* Device specific configuration */
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
/* PCI configuration access */
#define VIRTIO_PCI_CAP_PCI_CFG 5

struct virtio_pci_common_cfg {
   uint32_t device_feature_select; /* read-write */
   uint32_t device_feature; /* read-only for driver */
   uint32_t driver_feature_select; /* read-write */
   uint32_t driver_feature; /* read-write */
   uint16_t msix_config; /* read-write */
   uint16_t num_queues; /* read-only for driver */
   uint8_t device_status; /* read-write */
   uint8_t config_generation; /* read-only for driver */
   /* About a specific virtqueue. */
   uint16_t queue_select; /* read-write */
   uint16_t queue_size; /* read-write */
   uint16_t queue_msix_vector; /* read-write */
   uint16_t queue_enable; /* read-write */
   uint16_t queue_notify_off; /* read-only for driver */
   uint64_t queue_desc; /* read-write */
   uint64_t queue_driver; /* read-write */
   uint64_t queue_device; /* read-write */
};

struct virtio_pci_notify_cap {
   struct virtio_pci_cap cap;
   uint32_t notify_off_multiplier; /* Multiplier for queue_notify_off. */
};
#define BAR_NOTIFY_CAP(offset, queue_notify_off, notify_off_multiplier)   ((offset) + (queue_notify_off) * (notify_off_multiplier))

struct virtio_pci_isr_cap {
    union {
       struct {
          unsigned queue_interrupt: 1;
          unsigned device_cfg_interrupt: 1;
          unsigned reserved: 30;
       };
       unsigned int isr_cap;
    };
};

extern struct virtio_config_loc *pci_entropy_device;
extern struct virtio_config_loc *pci_blk_device;
extern struct virtio_config_loc *minix_blk_device;
extern struct virtio_config_loc *ext4_blk_device;

struct virtio_config_loc {
   struct virtio_pci_common_cfg *common_cfg;
   struct virtio_pci_notify_cap *notify_cap;
   struct virtio_pci_isr_cap    *isr_cap;
   void                         *t4;
   struct virtq_desc            *desc;
   struct virtq_avail           *driver;
   struct virtq_used            *device;
   uint16_t                      at_idx;
   bool                          enabled;
   uint32_t                      baraddr;
};

struct virtq_desc {
   /* Address (guest-physical). */
   uint64_t addr;
   /* Length. */
   uint32_t len;
   /* This marks a buffer as continuing via the next field. */
   #define VIRTQ_DESC_F_NEXT   1
   /* This marks a buffer as device write-only (otherwise device read-only). */
   #define VIRTQ_DESC_F_WRITE     2
   /* This means the buffer contains a list of buffer descriptors. */
   #define VIRTQ_DESC_F_INDIRECT   4
   /* The flags as indicated above. */
   uint16_t flags;
   /* Next field if flags & NEXT */
   uint16_t next;
};

struct virtq_avail {
   #define VIRTQ_AVAIL_F_NO_INTERRUPT      1
   uint16_t flags;
   uint16_t idx;
   uint16_t ring[ /* Queue Size */ ];
   //uint16_t used_event; /* Only if VIRTIO_F_EVENT_IDX */
};

/* le32 is used here for ids for padding reasons. */
struct virtq_used_elem {
   /* Index of start of used descriptor chain. */
   uint32_t id;
   /* Total length of the descriptor chain which was used (written to) */
   uint32_t len;
};

struct virtq_used {
   #define VIRTQ_USED_F_NO_NOTIFY  1
   uint16_t flags;
   uint16_t idx;
   struct virtq_used_elem ring[ /* Queue Size */];
   //uint16_t avail_event; /* Only if VIRTIO_F_EVENT_IDX */
};


void virtio_rng_device_init(void);
bool rng_fill(void *buffer, uint16_t size);
