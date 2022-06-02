#include <virtio_gpu.h>
#include <virtio_inp.h>
#include <input-event-codes.h>
#include <virtio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <memhelp.h>
#include <pci.h>
#include <kmalloc.h>
#include <cpage.h>
#include <mmu.h>
#include <csr.h>
#include <lock.h>

Framebuffer screen_buff;
void virtio_gpu_init(void){
    gdev->common->device_status = 0; // Device Reset
    gdev->common->device_status = 1; // Acknowledge
    gdev->common->device_status |= 2; // Driver

    //Read Device Features
    uint32_t features = gdev->common->device_feature;
    //printf("GPU: Device Feature Set is: 0x%08lx\n", features);

    //Write Understood Feature Bits to Device
    gdev->common->driver_feature = features;
    //printf("GPU: Driver Feature Set Returned: 0x%08lx\n", features);

    gdev->common->device_status |= 8; // Features OK

    //printf("GPU: After setting device status to Feature OK (8), it returns: 0x%08lx\n", gdev->common->device_status);
    //Setup Queue 0
    gdev->common->queue_select = 0;
    uint16_t qsize = gdev->common->queue_size;

    //Descriptor Table
    uint64_t virt = (uint64_t)kzalloc(16 * qsize);
    gdev->qdesc = (struct virtq_desc *) virt;
    gdev->common->queue_desc = mmu_translate(kernel_mmu_table, virt);
    //printf("GPU: Gave queue_desc address 0x%08lx. Virt Address was0x%08lx\n", gdev->common->queue_desc, virt);

    //Driver Ring
    virt = (uint64_t)kzalloc(6 + 2 * qsize);
    gdev->qdriver = (struct virtq_avail *) virt;
    gdev->common->queue_driver = mmu_translate(kernel_mmu_table, virt);
    //printf("GPU: Gave queue_driver address 0x%08lx. Virt Address was0x%08lx\n", gdev->common->queue_driver, virt);

    //Device ring
    virt = (uint64_t)kzalloc(6 + 8 * qsize);
    gdev->qdevice = (struct virtq_used *) virt;
    gdev->common->queue_device = mmu_translate(kernel_mmu_table, virt);
    //printf("GPU: Gave queue_device address 0x%08lx. Virt Address was0x%08lx\n", gdev->common->queue_device, virt);

    //Enable the queue
    gdev->common->queue_enable = 1;

    //Make the device LIVE
    gdev->common->device_status |= 4; //Driver OK
    //printf("GPU: After setting Device Live Status (4), it returns: 0x%08lx\n", gdev->common->device_status);
    //pci_entropy_device->enabled = true;
};

//This function below was developed by Stephen Marz to notify the gpu device in order 
//to complete a GPU request.  
bool virtio_gpu_wait(int which, void *rq, int rq_size, void *data, int data_size, void *resp, int resp_size, bool write){
    uint16_t desc = gdev->drvidx % gdev->common->queue_size;
    uint16_t head = desc;

    gdev->qdesc[desc].addr = mmu_translate(kernel_mmu_table, (uint64_t)rq);
    gdev->qdesc[desc].len = rq_size;
    gdev->qdesc[desc].flags = 0;

    gdev->drvidx += 1;
    if (data != NULL) {
        uint16_t next = (desc + 1) % gdev->common->queue_size;
        gdev->qdesc[desc].flags |= VIRTQ_DESC_F_NEXT;
        gdev->qdesc[desc].next = next;
        gdev->qdesc[next].addr = mmu_translate(kernel_mmu_table, (uint64_t)data);
        gdev->qdesc[next].len = data_size;
        gdev->qdesc[next].flags = write ? 0 : VIRTQ_DESC_F_WRITE;
        desc = next;
        gdev->drvidx += 1;
    }
 
    if (resp != NULL) {
        uint16_t next = (desc + 1) % gdev->common->queue_size;
        gdev->qdesc[desc].flags |= VIRTQ_DESC_F_NEXT;
        gdev->qdesc[desc].next = next;
        gdev->qdesc[next].addr = mmu_translate(kernel_mmu_table, (uint64_t)resp);
        gdev->qdesc[next].len = resp_size;
        gdev->qdesc[next].flags = VIRTQ_DESC_F_WRITE;
        gdev->qdesc[next].next  = 0;
        desc = next;
        gdev->drvidx += 1;
    }


    gdev->qdriver->ring[gdev->qdriver->idx % gdev->common->queue_size] = head;
    gdev->qdriver->idx += 1;
    //printf("Calculated PCI Notification Location to Notify GPU Device is: 0x%08lx\n\n", gdev->qnotaddr + gdev->common->queue_notify_off * gdev->qnotmult);
    *((uint32_t *)(gdev->qnotaddr + gdev->common->queue_notify_off * gdev->qnotmult)) = 0;

    //printf("P5\n");
    desc = gdev->devidx;
    //printf("desc is %d\n", desc);
    //printf("gdev qdevice index is %d\n", gdev->qdevice->idx);
    bool found = false;
    //printf("P6\n");
    do {
        while (desc != gdev->qdevice->idx) {
            //printf("P5\n");
            if (gdev->qdevice->ring[desc % gdev->common->queue_size].id == head) {
                found = true;
                break;
            }
            desc += 1;
        }
    } while (!found);
    return true;
}

bool gpu_init(Virt_GPU_Device *gdev){
    struct DisplayInfoResponse disp;
    struct ControlHeader ctrl;
    Create2D res2d;
    AttachBacking attach;
    ScanoutRequest scan;
    MemEntry mem;
    ResourceFlush flush;
    TransferToHost2d tx;

    //Get Display Dimensions
    ctrl.control_type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    ctrl.context_id = 0;
    ctrl.fence_id = 0;
    ctrl.flags = 0;

    virtio_gpu_wait(0, &ctrl, sizeof(ctrl), &disp, sizeof(disp), NULL, 0, false); 
    if(disp.hdr.control_type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO){
        return false;
    }

    //Start allocating framebuffer
    screen_buff.gpu = 0;
    screen_buff.height = disp.displays[0].rect.height;
    screen_buff.width = disp.displays[0].rect.width;

    printf("Height is %d, width is %d, Frame buff size: %d\n", screen_buff.height, screen_buff.width, screen_buff.height * screen_buff.width * sizeof(struct Pixel));
    //**************************
    //Currently, this is throwing off the GPU init by having a syncronous interrupt 15
    //being thrown (Store/AMO Fault), so it is unable to get past this point as of now.
    //screen_buff.pixels = (struct Pixel *)kzalloc(screen_buff.height * screen_buff.width * sizeof(struct Pixel));
    //screen_buff.pixels = kzalloc(12207);
    screen_buff.pixels = (struct Pixel *)cpage_znalloc(350);
    //**************************

    //Start creating 2D resource
    res2d.hdr.control_type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    res2d.resource_id = 1;
    res2d.format = R8G8B8A8_UNORM;
    res2d.width = screen_buff.width;
    res2d.height = screen_buff.height;
    
    ctrl.control_type = 0;

    virtio_gpu_wait(0, &res2d, sizeof(res2d), NULL, 0, &ctrl, sizeof(ctrl), true);
    if(ctrl.control_type != VIRTIO_GPU_RESP_OK_NODATA){
        return false;
    }
 
    //Attach the 2D resource 
    attach.hdr.control_type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach.num_entries = 1;
    attach.resource_id = 1;
    ctrl.control_type = 0;

    mem.addr = mmu_translate(kernel_mmu_table, (uint64_t)screen_buff.pixels);
    mem.length = screen_buff.width * screen_buff.height * sizeof(struct Pixel);

    virtio_gpu_wait(0, &attach, sizeof(attach), &mem, sizeof(mem), &ctrl, sizeof(ctrl), true);
    if(ctrl.control_type != VIRTIO_GPU_RESP_OK_NODATA){
        return false;
    }

    //Set Scanout
    scan.hdr.control_type = VIRTIO_GPU_CMD_SET_SCANOUT;
    scan.rect.x = 0;
    scan.rect.y = 0;
    scan.rect.width = screen_buff.width;
    scan.rect.height = screen_buff.height;
    scan.resource_id = 1;
    scan.scanout_id = 0;

    ctrl.control_type = 0;
    virtio_gpu_wait(0, &scan, sizeof(scan), NULL, 0, &ctrl, sizeof(ctrl), true);
    if(ctrl.control_type != VIRTIO_GPU_RESP_OK_NODATA){
        return false;
    }

    int i;
    //Create just a simple color background as an initial test for the framebuffer
    for (i = 0; i < (screen_buff.width * screen_buff.height); i++) {
        screen_buff.pixels[i].r = 255;
        screen_buff.pixels[i].g = 255;
        screen_buff.pixels[i].b = 255;
        screen_buff.pixels[i].a = 255;
    }

    //Transfer Framebuffer to the host 2d
    tx.hdr.control_type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    tx.resource_id = 1;
    tx.offset = 0;
    tx.rect = scan.rect;
    ctrl.control_type = 0;

    virtio_gpu_wait(0, &tx, sizeof(tx), NULL, 0, &ctrl, sizeof(ctrl), true);
    if(ctrl.control_type != VIRTIO_GPU_RESP_OK_NODATA){
        return false;
    }

    //Flush resource to draw to screen
    flush.hdr.control_type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    flush.resource_id = 1;
    flush.rect = scan.rect;
    ctrl.control_type = 0;

    virtio_gpu_wait(0, &flush, sizeof(flush), NULL, 0, &ctrl, sizeof(ctrl), true);
    if(ctrl.control_type != VIRTIO_GPU_RESP_OK_NODATA){
        return false;
    }

    paint();
    //If we made it here, we should be good
    return true;
}

bool gpu_redraw(struct Rectangle *rect){
    struct ControlHeader ctrl;
    ResourceFlush flush;
    TransferToHost2d tx;

    ctrl.control_type = 0;
    tx.hdr.control_type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    tx.resource_id = 1;

    tx.offset = rect->x * sizeof(struct Pixel) + rect->y * screen_buff.width * sizeof(struct Pixel);
    tx.rect = *rect;
    virtio_gpu_wait(0, &tx, sizeof(tx), NULL, 0, &ctrl, sizeof(ctrl), true);
    if(ctrl.control_type != VIRTIO_GPU_RESP_OK_NODATA) {
        return false;
    }

    ctrl.control_type = 0;
    flush.hdr.control_type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    flush.resource_id = 1;
    flush.rect = *rect;
    virtio_gpu_wait(0, &flush, sizeof(flush), NULL, 0, &ctrl, sizeof(ctrl), true);

    virtio_gpu_wait(0, &tx, sizeof(tx), NULL, 0, &ctrl, sizeof(ctrl), true);
    if(ctrl.control_type != VIRTIO_GPU_RESP_OK_NODATA) {
        return false;
    }

    return true;

}

void fill_rect(uint32_t screen_width, uint32_t screen_height, struct Pixel *buffer, struct Rectangle *rect, struct Pixel *fill_color){
   uint32_t top = rect->y;
   uint32_t bottom = rect->y + rect->height;
   uint32_t left = rect->x;
   uint32_t right = rect->x + rect->width;
   uint32_t row;
   uint32_t col;
   if (bottom > screen_height) bottom = screen_height;
   if (right > screen_width) right = screen_width;
   for (row = top;row < bottom;row++) {
      for (col = left;col < right;col++) {
          uint32_t offset = row * screen_width + col;
          buffer[offset] = *fill_color;
      }
   }
}
void stroke_rect(uint32_t screen_width, uint32_t screen_height, struct Pixel *buffer, struct Rectangle *rect, struct Pixel *line_color, uint32_t line_size){
    // Top
   struct Rectangle top_rect = {rect->x, 
                         rect->y,
                         rect->width,
                         line_size};
   fill_rect(screen_width, screen_height, buffer, &top_rect, line_color);
   // Bottom
   struct Rectangle bot_rect = {rect->x,
                         rect->height + rect->y,
                         rect->width,
                         line_size};
   fill_rect(screen_width, screen_height, buffer, &bot_rect, line_color);
   // Left
   struct Rectangle left_rect = {rect->x,
                          rect->y,
                          line_size,
                          rect->height};
   fill_rect(screen_width, screen_height, buffer, &left_rect, line_color);
   // Right
   struct Rectangle right_rect = {rect->x + rect->width,
                           rect->y,
                           line_size,
                           rect->height + line_size};
   fill_rect(screen_width, screen_height, buffer, &right_rect, line_color);
}
