#include "os.h"

void yield(void) {
    asm volatile("mv a7, %0\necall" : : "r"(3) : "a7");
}

void sleep(int tm) {
    asm volatile("mv a7, %0\nmv a0, %1\necall" : : "r"(4), "r"(tm) : "a0", "a7");
}