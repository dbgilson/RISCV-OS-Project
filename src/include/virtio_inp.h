#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <lock.h>
#include <virtio.h>
#pragma once

#define ALIGN_UP_POT(x, y)   (((x) + (y) - 1) & -(y))
#define ALIGN_DOWN_POT(x, y)   ((x) & -(y))


enum virtio_input_config_select {
    VIRTIO_INPUT_CFG_UNSET     = 0x00,
    VIRTIO_INPUT_CFG_ID_NAME   = 0x01,
    VIRTIO_INPUT_CFG_ID_SERIAL = 0x02,
    VIRTIO_INPUT_CFG_ID_DEVIDS = 0x03,
    VIRTIO_INPUT_CFG_PROP_BITS = 0x10,
    VIRTIO_INPUT_CFG_EV_BITS   = 0x11,
    VIRTIO_INPUT_CFG_ABS_INFO  = 0x12,
};

struct virtio_input_absinfo {
    uint32_t min;
    uint32_t max;
    uint32_t fuzz;
    uint32_t flat;
    uint32_t res;
};

struct virtio_input_devids {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

typedef struct virtio_input_config {
    uint8_t select;
    uint8_t subsel;
    uint8_t size;
    uint8_t reserved[5];
    union {
        char string[128];
        uint8_t bitmap[128];
        struct virtio_input_absinfo abs;
        struct virtio_input_devids ids;
    };
} VirtioIptConfig;

typedef struct VirtioIptDevice {
    struct virtio_pci_common_cfg *common;
    struct virtio_pci_isr_cap *isr;
    struct virtio_pci_notify_cap *notify;
    struct VirtioIptConfig *t4;
    struct virtq_desc *qdesc;
    struct virtq_avail *qdriver;
    struct virtq_used *qdevice;
    uint64_t qnotmult;
    uint64_t qnotaddr;
    uint16_t drvidx;
    uint16_t devidx;
    uint16_t irq;
} VirtioIptDevice;

typedef struct virtio_input_event {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} InputEvent;

extern volatile VirtioIptDevice *ipt;
extern volatile VirtioIptDevice *keyboard;
extern volatile int ipt_dev_idx;

void virtio_input_init(volatile VirtioIptDevice *ipt);