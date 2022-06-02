//This file is to help us reference the memory addresses used by
//the riscv.lds.
#pragma once
extern void *_heap_start;
extern void *_heap_end;
extern void *_stack_start;
extern void *_stack_end;
extern void *_bss_start;
extern void *_bss_end;
extern void *_data_start;
extern void *_data_end;
extern void *_text_start;
extern void *_text_end;
extern void *_rodata_start;
extern void *_rodata_end;
extern void *_memory_start;
extern void *_memory_end;

//Just a couple of macros to help create the right address variable
//and make it more readable.
#define sym_start(segment) \
    ((unsigned long)&_##segment##_start)

#define sym_end(segment) \
    ((unsigned long)&_##segment##_end)
