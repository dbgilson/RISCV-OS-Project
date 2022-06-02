#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ALIGN_UP_POT(x, y)   (((x) + (y) - 1) & -(y))
#define ALIGN_DOWN_POT(x, y)   ((x) & -(y))

void *memset(void *dst, char data, int64_t size);
void *memcpy(void *dst, const void *src, int64_t size);
bool memcmp(const void *haystack, const void *needle, int64_t size);
