#pragma once
#include "../../sbi/src/include/sbicalls.h"

#define TMR_INFINITE 0x7FFFFFFFFFFFFFFFUL

typedef enum HartStatus {
    HS_INVALID, // A HART that doesn't exist
    HS_STOPPED,
    HS_STARTING,
    HS_STARTED,
    HS_STOPPING
} HartStatus;

//These are our current system calls
void sbi_putchar(char c);
char sbi_getchar(void);
int sbi_hart_get_status(unsigned int hart);
int sbi_hart_start(unsigned int hart, unsigned long target, unsigned long scratch);
int sbi_hart_stop(void);
unsigned long sbi_get_time(void);
void sbi_set_timecmp(unsigned int hart, unsigned long val);
void sbi_add_timercmp(unsigned int hart, unsigned long val);
unsigned long sbi_rtc_get_time(void);
int sbi_whoami(void);
void sbi_ack_timer(void);