#include <clint.h>

//This function sets the msip for a given hart via the clint.
void clint_set_msip(unsigned int hart) {
    if (hart >= 8) return;

    unsigned int *clint = (unsigned int *)CLINT_BASE_ADDRESS;
    clint[hart] = 1;
}

//This function clears the msip for a given hart via the clint.
void clint_clear_msip(unsigned int hart) {
    if (hart >= 8) return;

    unsigned int *clint = (unsigned int *)CLINT_BASE_ADDRESS;
    clint[hart] = 0;
}

//This function sets the mtimecmp register of a given hart with a specific value (used for timer interrupts)
void clint_set_mtimecmp(unsigned int hart, unsigned long val) {
    if (hart >= 8) return;

    ((unsigned long *)(CLINT_BASE_ADDRESS + CLINT_MTIMECMP_OFFSET))[hart] = val;
}

//This function adds to the current clint timecmp register value
void clint_add_mtimecmp(unsigned int hart, unsigned long val) {
    clint_set_mtimecmp(hart, clint_get_time() + val);
}
//This function reads the time on the clint using the rdtime assembly call.
unsigned long clint_get_time() {
    unsigned long tm;
    asm volatile("rdtime %0" : "=r"(tm));
    return tm;
}
