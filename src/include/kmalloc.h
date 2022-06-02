#pragma once
#include <stddef.h>

#define KERNEL_HEAP_START_VADDR 0x120000000
#define KERNEL_HEAP_PAGES 0

void init_kernel_heap(void);
void *kmalloc(size_t bytes);
void *kzalloc(size_t bytes);
void kfree(void *mem);
void coalesce_free_list(void);
