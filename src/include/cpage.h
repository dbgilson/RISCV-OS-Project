#pragma once

#define PAGE_SIZE 4096

//Start with one byte per page (1:4096)
//heap size / 4096 gives # of pages
//use sym_start() and sym_end() with heap
//sym_start(heap) + (# bytes that bookkeeping bytes take)
//Still a doubly nested for loop
//cpage_znalloc(# of pages)
//cpage_free(page pointer)

void cpage_init(void);
void *cpage_znalloc(int num_pages);
void cpage_free(void *page);