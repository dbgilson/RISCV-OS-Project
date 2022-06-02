#pragma once

//Our base page size is 4K, so 4096 Bytes
#define PAGE_SIZE 4096

void ncpage_init(void);
void *ncpage_zalloc(void);
void ncpage_free(void *page);