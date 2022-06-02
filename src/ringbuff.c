#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <ringbuff.h>
#include <cpage.h>
#include <memhelp.h>
#include <pci.h>
#include <kmalloc.h>
#include <mmu.h>
#include <csr.h>
#include <lock.h>

//This ringbuffer code was given by Stephen Marz on his virtio input page.
RingBuffer *ring_init(RingBuffer *buf, uint32_t capacity, RingBufferBehavior rbb){
    buf->at = 0;
    buf->capacity = capacity;
    buf->num_elements = 0;
    //printf("Num elements in buffer in init is: %d\n", buf->num_elements);
    buf->behavior = rbb;
    //buf->buffer = (int64_t *)kmalloc(sizeof(uint64_t) * capacity);
    buf->buffer = (int64_t *)cpage_znalloc(10);
    return buf;
}

RingBuffer *ring_new(uint32_t capacity, RingBufferBehavior rbb){
    //return ring_init((RingBuffer *)kmalloc(sizeof(RingBuffer)), capacity, rbb);
    return ring_init((RingBuffer *)cpage_znalloc(10), capacity, rbb);

}

bool ring_push(RingBuffer *buf, int64_t elem){
    if (buf->num_elements >= buf->capacity) {
        if (buf->behavior == RB_DISCARD){
            return true;
        }
        else if (buf->behavior == RB_ERROR){
            return false;
        }
        else { // RB_OVERWRITE
            buf->num_elements -= 1;
            buf->at = (buf->at + 1) % buf->capacity;
        }
    }
    // Find out where the next data should go.
    uint32_t b = (buf->at + buf->num_elements) % buf->capacity;
    // Add the data and increment the number of elements.
    buf->buffer[b] = elem;
    buf->num_elements += 1;
    return true;
}

bool ring_pop(RingBuffer *buf, int64_t *elem){
    // Return false if there are no elements on the ring.
    if (buf->num_elements == 0) {
        return false;
    }
    // Pop the ring element, and update the pointers.
    *elem = buf->buffer[buf->at];
    buf->at = (buf->at + 1) % buf->capacity;
    buf->num_elements -= 1;
    return true;
}