#include <lock.h>
#include <process.h>
#include <stddef.h>
#include <kmalloc.h>

//Use AMO instruction set to atomically lock mutex
int mutex_trylock(Mutex *mutex) {
    int old;
    asm volatile("amoswap.w.aq %0, %1, (%2)" 
                  : "=r"(old) 
                  : "r"(MUTEX_LOCKED), "r"(mutex)
                );
    return old != MUTEX_LOCKED;
}

//Simple Spinlock to try to aquire and hold lock
void mutex_spinlock(Mutex *mutex) {
    while (!mutex_trylock(mutex));
}

//Use AMO instruction set to atomically unlock mutex
void mutex_unlock(Mutex *mutex) {
    asm volatile("amoswap.w.rl zero, zero, (%0)" 
                  : : "r"(mutex)
                 );
}

//Use AMO instruction to try and decrement the semaphore
int semaphore_trydown(struct Semaphore *s){
    int old;
    asm volatile ("amoadd.w %0, %1, (%2)" 
                    : "=r"(old) 
                    : "r"(-1), "r"(&s->value)
                 );
    if (old <= 0) {
        semaphore_up(s); //If we were at 0, we need to add 1 back so we don't get negative values
    }
    return old > 0;
}

//Use AMO instruction to simply add 1 to the semaphore value.
void semaphore_up(struct Semaphore *s){
    asm volatile ("amoadd.w zero, %0, (%1)" 
                    : 
                    : "r"(1), "r"(&s->value)
                 );
}

//This function initializes the barrier list and value of a given barrier struct
void barrier_init(struct Barrier *barrier){
    barrier->head = NULL;
    barrier->value = 0;
}

//This function adds a process (could be a pointer to anything really, but I'm doing the process struct pointer
//implementation since processes will likely utilize barriers and was the example provided in the notes) to the barrier list pointed to by the given barrier.
//I borrowed Dr. Marz kmalloc to help implement this, but I'll be updating kmalloc to be my own (will transfer what I did in CS 360)
void barrier_add_process(struct Barrier *barrier, struct Process *proc){
    struct Barrier_List *bl;
    bl = kmalloc(sizeof(*bl));
    bl->proc = proc;
    bl->next = barrier->head;
    barrier->head = bl;
    barrier->value += 1;
}

//You would place this function at the actual "barrier" spot in your code that you want a process
//to stop at.  This function decrements the barrier value once a process reaches this function, 
//and once the final process reaches this function, the barrier "opens" and it goes through the barrier list, setting
//the processes to start running or try to start running again.
void barrier_wall(struct Barrier *barrier){
    int old;
    asm volatile("amoadd.w %0, %1, (%2)" 
                   : "=r"(old) 
                   : "r"(-1), "r"(&barrier->value)
                );
    if (old <= 1) {
        struct Barrier_List *bl, *next;
        //Go through each node in the barrier list, set its "process" to run (assuming it is in the WAITING state), then free that node
        for (bl = barrier->head;NULL != bl;bl = next) {
            next = bl->next;
            bl->proc->state = P_RUNNING;
            kfree(bl);
        }
        barrier->head = NULL;
    }
}
