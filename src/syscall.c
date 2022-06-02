#include <csr.h>
#include <errno.h>
#include <kmalloc.h>
#include <lock.h>
#include <mmu.h>
#include <printf.h>
#include <process.h>
#include <sbi.h>
#include <sched.h>
#include <stddef.h>
#include <uaccess.h>
#include <memhelp.h>

#define XREG(x)             (scratch[XREG_##x])
#define SYSCALL_RETURN_TYPE void
#define SYSCALL_PARAM_LIST  int hart, uint64_t epc, int64_t *scratch
#define SYSCALL(t)          static SYSCALL_RETURN_TYPE syscall_##t(SYSCALL_PARAM_LIST)
#define SYSCALL_PTR(t)      syscall_##t
#define SYSCALL_EXEC(x)     SYSCALLS[(x)](hart, epc, scratch)
#define SYSCALL_ENTER() \
    (void)hart;         \
    (void)epc;          \
    (void)scratch

SYSCALL(exit){
    SYSCALL_ENTER();
    Process *p;
    if ((p = p_get_on_hart(hart)) != NULL) {
        p->state = P_DEAD;
    }
    // Now that we removed this process, find another to schedule.
    schedule_hart_spawn(hart);
}

SYSCALL(putchar){
    SYSCALL_ENTER();
    sbi_putchar(XREG(A0));
}

SYSCALL(getchar){
    SYSCALL_ENTER();
    XREG(A0) = sbi_getchar();
}

SYSCALL(yield){
    SYSCALL_ENTER();
    schedule_hart_spawn(hart);
}

SYSCALL(sleep){
    SYSCALL_ENTER();
    Process *current = p_get_on_hart(hart);
    if (current == NULL) {
        printf("Error!\n");
    }
    else {
        uint64_t tm          = sbi_get_time();
        current->sleep_until = XREG(A0) * VIRT_TIMER_FREQ + tm;
        current->state       = P_SLEEPING;
    }
    schedule_hart_spawn(hart);
}

static SYSCALL_RETURN_TYPE (*const SYSCALLS[])(SYSCALL_PARAM_LIST) = {
    SYSCALL_PTR(exit),       /* 0 */
    SYSCALL_PTR(putchar),    /* 1 */
    SYSCALL_PTR(getchar),    /* 2 */
    SYSCALL_PTR(yield),      /* 3 */
    SYSCALL_PTR(sleep),      /* 4 */
};

static const int NUM_SYSCALLS = sizeof(SYSCALLS) / sizeof(SYSCALLS[0]);

void syscall_handle(int hart, uint64_t epc, int64_t *scratch){
    // Sched invoke will save sepc, so we want it to resume
    // 4 bytes ahead, which will be the next instruction.
    CSR_WRITE("sepc", epc + 4);

    if (XREG(A7) >= NUM_SYSCALLS) {
        // Invalid syscall
        XREG(A0) = -EINVAL;
    }
    else {
        SYSCALL_EXEC(XREG(A7));
    }
}