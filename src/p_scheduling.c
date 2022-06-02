#include <rbtree.h>
#include <process.h>
#include <stddef.h>
#include <sbi.h>
#include <mmu.h>
#include <csr.h>
#include <lock.h>
#include <p_scheduling.h>
#include <printf.h>

//This array of processes will hold idle processes for each hart
//so that we can put an idle process on a hart if needed
extern Process *main_idle_processes[];

//We'll use a red black tree to implement the Completely Fair Scheduling algorithm
RBTree *p_scheduling_tree;

//For MLF, we'll use a 3 process queues for different priorities (Not Implemented)
Process mlf_p_queue[3][5];

//We'll need a mutex since different harts will be accessing the 
//scheduling map.
Mutex scheduling_mutex = MUTEX_UNLOCKED;

//Have a global variable that holds which mode of scheduling (MLF = 0, CFS = 1)
static int SCHED_MODE;

void scheduling_init(int mode){
    mutex_spinlock(&scheduling_mutex);
    if(mode == 0){
        SCHED_MODE = 0; //MLF (Not Implemented)
    }
    else{
        SCHED_MODE = 1; //CFS
        p_scheduling_tree = rb_new();
    }
    mutex_unlock(&scheduling_mutex);
}

//This function invokes the schedule find next function to get the next
//available process and spawn it on a hart (otherwise spawn an idle process)
void schedule_hart_spawn(int hart){
    schedule_hart_check(hart);
    Process *n = schedule_find_next(hart);
    if (n == NULL) {
        n = main_idle_processes[hart];
    }
    p_spawn_on_hart(n, hart);
}

//This function gets the current process on the hart and checks if it is an idle process.
//If it's not, it performs the vruntime calculation and adds it back to the tree for scheduling.
void schedule_hart_check(int hart){
   Process *p = p_get_on_hart(hart);
    if (p == NULL) {
        return;
    }

    if(p != main_idle_processes[hart]){
        p->v_runtime += sbi_get_time() - p->start_time;
        schedule_add(p);
    }
    CSR_READ(p->frame.sepc, "sepc");
    p->hart_num = -1;
}

//This process searches for the leftmost node on the process tree (CFS) to
//find the next process to run (it will have the lowest virtual runtime guaranteed)
struct Process *schedule_find_next(int hart){
    mutex_spinlock(&scheduling_mutex);
    //Get Left Most Node from tree
    RBNode *temp = p_scheduling_tree->root;

    //*****Having allocation issues here whenever reading from temp, even though I
    //*****thought that I had allocated the memory correctly in rb_new and create_node in rbtree.c
    while (temp && temp != NULL && temp->left != NULL){
        temp = temp->left;
    }
    Process *p = temp->value;

    //Check if process is runnable
    if(p->state == P_RUNNING){
        int hart = sbi_whoami();
        //Setup process and return it
        p->start_time = sbi_get_time();
        p->hart_num = hart;
        rb_delete(&p_scheduling_tree, p->v_runtime);
        p_current[hart] = p;
        mutex_unlock(&scheduling_mutex);
        return p;
    }
    mutex_unlock(&scheduling_mutex);
    return NULL; //Couldn't Find a Runnable Process, return NULL
}

//This function adds a process to the process tree using the virtual runtime as the
//rb tree key.
void schedule_add(struct Process *p){
    mutex_spinlock(&scheduling_mutex);
    //Ensure that the state of the process is "Running"
    p->state = P_RUNNING;
    rb_insert(&p_scheduling_tree, p->v_runtime, p);
    mutex_unlock(&scheduling_mutex);
}

//This function deletes the process from the process tree so that it can't
//be scheduled again
void schedule_remove(struct Process *p){
    mutex_spinlock(&scheduling_mutex);
    rb_delete(&p_scheduling_tree, p->v_runtime);
    mutex_unlock(&scheduling_mutex);
}

//Attempt to possibly make a continuous scheduler, but I think it is handled with the mtimecmp interrupt
void scheduler(void){
    Process *p;
    int hart = sbi_whoami();
    for(;;){
        schedule_hart_spawn(hart);
    }
}