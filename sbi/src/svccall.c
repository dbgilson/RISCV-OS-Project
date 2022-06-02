#include <svccall.h>
#include <csr.h>
#include <uart.h>
#include <printf.h>
#include <hart.h>
#include <ring_buff.h>
#include <rtc.h>

//This is the OS trap handler which checks the cause of the trap and delegates accordingly
//The IDs of the traps were made in sbicalls.h
void svccall_handle(int hart) {
    long *mscratch;
    CSR_READ(mscratch, "mscratch");
    switch (mscratch[XREG_A7]) {
        case SBI_SVCCALL_HART_STATUS:
            mscratch[XREG_A0] = hart_get_status(mscratch[XREG_A0]);
        break;
        case SBI_SVCCALL_HART_START:
            mscratch[XREG_A0] = hart_start(mscratch[XREG_A0], mscratch[XREG_A1], mscratch[XREG_A2]);
        break;
        case SBI_SVCCALL_HART_STOP:
            mscratch[XREG_A0] = hart_stop(hart);
        break;
        case SBI_SVCCALL_GET_TIME:
            mscratch[XREG_A0] = clint_get_time();
        break;
        case SBI_SVCCALL_SET_TIMECMP:
            clint_set_mtimecmp(mscratch[XREG_A0], mscratch[XREG_A1]);
        break;
        case SBI_SVCCALL_ADD_TIMECMP:
            clint_add_mtimecmp(mscratch[XREG_A0], mscratch[XREG_A1]);
        break;
        case SBI_SVCCALL_ACK_TIMER: {
            unsigned long mip;
            CSR_READ(mip, "mip");
            CSR_WRITE("mip", mip & ~SIP_STIP);
        }
        case SBI_SVCCALL_RTC_GET_TIME:
            mscratch[XREG_A0] = rtc_get_time();
        break;
        case SBI_SVCCALL_PUTCHAR:
            uart_put(mscratch[XREG_A0]);
        break;
        case SBI_SVCCALL_GETCHAR:
            mscratch[XREG_A0] = ring_get();
        break;
        case SBI_SVCCALL_WHOAMI:
            mscratch[XREG_A0] = hart;
        break;
        default:
            printf("Unknown system call %d on hart %d\n", mscratch[XREG_A7], hart);
        break;
    }
}
