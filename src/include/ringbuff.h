#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <lock.h>
#include <virtio.h>

//This ringbuffer code was given by Stephen Marz on his virtio input page.

// RingBufferBehavior describes what should be done if the
// ring buffer is full.
typedef enum RingBufferBehavior {
    RB_OVERWRITE,  // Overwrite old buffer data
    RB_DISCARD,    // Silently discard new buffer data
    RB_ERROR       // Don't do anything and return an error.
} RingBufferBehavior;

typedef struct RingBuffer {
    uint32_t at;            // The starting index of the data
    uint32_t num_elements;  // The number of *valid* elements in the ring
    uint32_t capacity;      // The size of the ring buffer
    uint32_t behavior;      // What should be done if the ring buffer is full?
    int64_t *buffer;        // The actual ring buffer in memory
} RingBuffer;

extern volatile RingBuffer *ipt_ringbuf;

RingBuffer *ring_init(RingBuffer *buf, uint32_t capacity, RingBufferBehavior rbb);
RingBuffer *ring_new(uint32_t capacity, RingBufferBehavior rbb);
bool ring_push(RingBuffer *buf, int64_t elem);
bool ring_pop(RingBuffer *buf, int64_t *elem);
