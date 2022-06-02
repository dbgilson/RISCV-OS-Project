#include <stddef.h>
#include <mmu.h>
#include <kmalloc.h>
#include <sbi.h>
#include <cpage.h>
#include <memhelp.h>
#include <csr.h>
#include <process.h>

//Spawn locations defined in spawn.S
extern uint64_t spawn_thread_start;
extern uint64_t spawn_thread_end;
extern uint64_t spawn_trap_start;
extern uint64_t spawn_trap_end;

//Simple array of the current process running on a given hart by index
Process *p_current[8];

//Simple array that holds trap stack locations of processes on a given hart by index
uint64_t p_trap_stacks[8];

//This function initialize the given process
Process *p_init(Process *p){
    uint64_t phys;
    uint64_t frame;

    //Check for NULL pointers
    if(p == NULL)
        return NULL;

    //Start initializing process variables

    p->state = P_UNINT;             //Start in Uninitialized State
    p->ptable = cpage_znalloc(1);   //Give process page table a page
    p->stack = cpage_znalloc(1);    //Give process stack a page
    p->stack_pg_num = 1;            //Currently only have 1 page for stack
    p->quantum = P_DEFAULT_QUANTUM; //Use Default Quantum (100)
    p->hart_num = -1;               //Currently not on a Hart

    p->priority = 0;
    p->q_mult = 1;
    p->v_runtime = 0;

    //Here is the "trampoline" into the process's address space
    phys = mmu_translate(kernel_mmu_table, spawn_thread_start);
    mmu_map_multiple(p->ptable, spawn_thread_start, spawn_thread_end, phys, PB_EXECUTE);

    phys = mmu_translate(kernel_mmu_table, spawn_trap_start);
    mmu_map_multiple(p->ptable, spawn_trap_start, spawn_trap_end, phys, PB_EXECUTE);

    //Map the trap frame
    frame = (uint64_t)&p->frame;
    phys = mmu_translate(kernel_mmu_table, frame);
    mmu_map_multiple(p->ptable, frame, frame + sizeof(p->frame), phys, PB_READ | PB_WRITE);

    //Setup process's frame data
    p->frame.gpregs[XREG_SP] = P_DEFAULT_STACK_POINTER + p->stack_pg_num * PAGE_SIZE;
    p->frame.sie = SIE_SEIE | SIE_SSIE | SIE_STIE;
    p->frame.sscratch = (uint64_t)&p->frame;
    p->frame.stvec = spawn_trap_start;
    p->frame.trap_satp = SATP(kernel_mmu_table, KERNEL_ASID);

    //Were done, so return process pointer
    return p;
}

//This function returns a pointer to a process, either a user or OS process
Process *p_new(bool user){
    //Allocate a process (could do it before calling p_new,
    //but we'll do it in the function instead)
    Process *p = p_init((Process *)cpage_znalloc(1));
    //Process *p = p_init((Process *)kzalloc(sizeof(Process)));

    //Check for NULL Pointers
    if(p == NULL)
        return NULL;
    
    //Setup stack pointer 
    uint64_t stack = (uint64_t)p->stack;

    //Check whether it is an OS or user process
    if(user){
        p->kmode = false;
        p->frame.sstatus = SSTATUS_FS_INITIAL | SSTATUS_SPIE | SSTATUS_SPP_USER;
        mmu_map_multiple(p->ptable, P_DEFAULT_STACK_POINTER, P_DEFAULT_STACK_POINTER + p->stack_pg_num * PAGE_SIZE,
        stack, PB_USER | PB_READ | PB_WRITE);
    }
    else{
        p->kmode = true;
        p->frame.sstatus = SSTATUS_FS_INITIAL | SSTATUS_SPIE | SSTATUS_SPP_SUPERVISOR;
        mmu_map_multiple(p->ptable, P_DEFAULT_STACK_POINTER, P_DEFAULT_STACK_POINTER + p->stack_pg_num * PAGE_SIZE,
        stack, PB_READ | PB_WRITE);
    }

    return p;
}

//This function frees the memory that a given process holds (stack, image, and page table);
void p_free(Process *p){
    //Check for NULL Pointers
    if(p == NULL)
        return NULL;

    //Make the process state dead and free 
    //the allocated page tables and heap space
    p->state = P_DEAD;
    if(p->stack){
        cpage_free(p->stack);
    }
    if(p->image){
        cpage_free(p->image);
    }
    if(p->ptable){
        mmu_free(p->ptable);
    }
    //kfree(p);
    cpage_free(p);
}

//This function spawns a process on the hart which calls it
bool p_spawn(Process *p){
    return p_spawn_on_hart(p, sbi_whoami());
}

//This function spawns a process on a given hart
bool p_spawn_on_hart(Process *p, int hart){
    //Check if hart is able to spawn processes
    int whoami = sbi_whoami();
    if((hart != whoami) && (sbi_hart_get_status(hart) != 1)){
        return false;
    }

    //Update process's hart status
    p->hart_num = hart;
    p->frame.trap_stack = p_trap_stacks[hart];

    //Update Global Current Process Array (used later for scheduling)
    p_current[hart] = p;

    //Set context switch timer sometime using the default quantum value
    //(Will be used when doing scheduling)
    sbi_add_timercmp(hart, p->quantum * P_DEFAULT_CTXTMR);

    //Debugging process frame data
    printf("Frame data:\n");
    printf("    sepc: 0x%08lx\n", p->frame.sepc);
    printf("    sstatus: 0x%08lx\n", p->frame.sstatus);
    printf("    sie: 0x%08lx\n", p->frame.sie);
    printf("    satp: 0x%08lx\n", p->frame.satp);
    printf("    sscratch: 0x%08lx\n", p->frame.sscratch);
    printf("    stvec: 0x%08lx\n", p->frame.stvec);
    printf("    trap_satp: 0x%08lx\n", p->frame.trap_satp);
    printf("    trap_stack: 0x%08lx\n", p->frame.trap_stack);
    
    //If the current hart is the one which the process will
    //be on, just go right into the process spawning thread
    if(whoami == hart){
        CSR_WRITE("sscratch", p->frame.sscratch);
        ((void (*)(void))spawn_thread_start)();

        return true;
    }

    uint64_t phys = mmu_translate(p->ptable, p->frame.sscratch);

    if(phys == -1UL){
        return false;
    }

    return sbi_hart_start(hart, spawn_thread_start, phys);
}
