#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <lock.h>
#pragma once

#define VIRTIO_GPU_EVENT_DISPLAY   1
struct VirtioGpuConfig {
   uint32_t  events_read;
   uint32_t  events_clear;
   uint32_t  num_scanouts;
   uint32_t  reserved;
};

#define VIRTIO_GPU_FLAG_FENCE  1
struct ControlHeader {
   uint32_t control_type;
   uint32_t flags;
   uint64_t fence_id;
   uint32_t context_id;
   uint32_t padding;
};

enum ControlType {
   /* 2D Commands */
   VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
   VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
   VIRTIO_GPU_CMD_RESOURCE_UNREF,
   VIRTIO_GPU_CMD_SET_SCANOUT,
   VIRTIO_GPU_CMD_RESOURCE_FLUSH,
   VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
   VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
   VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
   VIRTIO_GPU_CMD_GET_CAPSET_INFO,
   VIRTIO_GPU_CMD_GET_CAPSET,
   VIRTIO_GPU_CMD_GET_EDID,
   /* cursor commands */
   VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
   VIRTIO_GPU_CMD_MOVE_CURSOR,
   /* success responses */
   VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
   VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
   VIRTIO_GPU_RESP_OK_CAPSET_INFO,
   VIRTIO_GPU_RESP_OK_CAPSET,
   VIRTIO_GPU_RESP_OK_EDID,
   /* error responses */
   VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
   VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
   VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
   VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
   VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
   VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

#define VIRTIO_GPU_MAX_SCANOUTS 16
struct Rectangle {
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
};
struct DisplayInfoResponse {
   struct ControlHeader hdr;  /* VIRTIO_GPU_RESP_OK_DISPLAY_INFO */
   struct GpuDisplay {
       struct Rectangle rect;
       uint32_t enabled;
       uint32_t flags;
   } displays[VIRTIO_GPU_MAX_SCANOUTS];
};

typedef struct GetEdID{
   struct ControlHeader hdr;
   uint32_t scanout;
   uint32_t padding;
} GetEdID;
typedef struct RespEdID{
   struct ControlHeader hdr;
   uint32_t size;
   uint32_t padding;
   uint8_t  edid[1024];
} RespEdID;

enum GpuFormats {
   B8G8R8A8_UNORM = 1,
   B8G8R8X8_UNORM = 2,
   A8R8G8B8_UNORM = 3,
   X8R8G8B8_UNORM = 4,
   R8G8B8A8_UNORM = 67,
   X8B8G8R8_UNORM = 68,
   A8B8G8R8_UNORM = 121,
   R8G8B8X8_UNORM = 134,
};

typedef struct ResourceCreate2dRequest {
   struct ControlHeader hdr; /* VIRTIO_GPU_CMD_RESOURCE_CREATE_2D */
   uint32_t resource_id;   /* We get to give a unique ID to each resource */
   uint32_t format;        /* From GpuFormats above */
   uint32_t width;
   uint32_t height;
} Create2D;


typedef struct SetScanoutRequest {
    struct ControlHeader hdr; /* VIRTIO_GPU_CMD_SET_SCANOUT */
    struct Rectangle rect;
    uint32_t scanout_id;
    uint32_t resource_id;
} ScanoutRequest;

typedef struct ResourceFlush{
   struct ControlHeader hdr;
   struct Rectangle rect;
   uint32_t resource_id;
   uint32_t padding;
} ResourceFlush;

typedef struct TransferToHost2d{
   struct ControlHeader hdr;
   struct Rectangle rect;
   uint64_t offset;
   uint32_t resource_id;
   uint32_t padding;
} TransferToHost2d;

typedef struct AttachBacking{
   struct ControlHeader hdr;
   uint32_t resource_id;
   uint32_t num_entries;
} AttachBacking;

typedef struct MemEntry{
   uint64_t addr;
   uint32_t length;
   uint32_t padding;
} MemEntry;

struct Pixel {
    /* This pixel structure must match the format! */
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

typedef struct Virt_GPU_Config{
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t reserved;
} Virt_GPU_Config;

typedef struct Virt_GPU_Device{
    struct virtio_pci_common_cfg *common;
    struct virtio_pci_isr_cap *isr;
    struct virtio_pci_notify_cap *notify;
    struct VirtioGpuConfig *t4;
    struct virtq_desc *qdesc;
    struct virtq_avail *qdriver;
    struct virtq_used *qdevice;
    uint64_t qnotmult;
    uint64_t qnotaddr;
    uint16_t drvidx;
    uint16_t devidx;
    uint16_t irq;
    Mutex bufmutex;
} Virt_GPU_Device;

extern Virt_GPU_Device *gdev;

typedef struct Framebuffer {
    int gpu;
    uint32_t width;
    uint32_t height;
    struct Pixel *pixels;
} Framebuffer;

extern Framebuffer screen_buff;

void virtio_gpu_init(void);
bool gpu_init(Virt_GPU_Device *gdev);

//This function below was developed by Stephen Marz to notify the gpu device in order to complete a GPU request. 
bool virtio_gpu_wait(int which, void *rq, int rq_size, void *data, int data_size, void *resp, int resp_size, bool write);

bool gpu_redraw(struct Rectangle *rect);
void fill_rect(uint32_t screen_width, uint32_t screen_height, struct Pixel *buffer, struct Rectangle *rect, struct Pixel *fill_color);
void stroke_rect(uint32_t screen_width, uint32_t screen_height, struct Pixel *buffer, struct Rectangle *rect, struct Pixel *line_color, uint32_t line_size);