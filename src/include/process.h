#include <stdbool.h>
#include <stdint.h>
#pragma once

typedef enum{
    P_DEAD,
    P_UNINT,
    P_RUNNING,
    P_SLEEPING,
    P_WAITING
}P_STATE;

typedef struct {
    uint64_t gpregs[32];
    double fpregs[32];
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t sie;
    uint64_t satp;
    uint64_t sscratch;
    uint64_t stvec;
    uint64_t trap_satp;
    uint64_t trap_stack;
} ProcFrame;

struct PageTable;

typedef struct Process{
    ProcFrame frame;
    P_STATE state;
    uint64_t priority;
    uint16_t quantum;
    uint64_t q_mult;
    uint64_t v_runtime;
    uint64_t start_time;
    uint64_t sleep_until;
    uint16_t pid;
    uint64_t image_pg_num;
    uint64_t stack_pg_num;
    uint64_t heap_pg_num;
    struct PageTable *ptable;
    void *image;
    void *stack;
    void *heap;
    int hart_num;
    bool kmode;
} Process;

#define P_DEFAULT_CTXTMR 10000
#define P_DEFAULT_QUANTUM 100
#define P_DEFAULT_VADDR 0x400000
#define P_DEFAULT_STACK_POINTER 0x1cafe0000UL

extern Process *p_current[];
extern uint64_t p_trap_stacks[];

Process *p_init(Process *p);
Process *p_new(bool user);
void p_free(Process *p);
bool p_spawn(Process *p);
bool p_spawn_on_hart(Process *p, int hart);

#define p_get_on_hart(which)      (p_current[which])
#define p_set_on_hart(which, val) (p_current[which] = val)