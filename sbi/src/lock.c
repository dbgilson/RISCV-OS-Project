#include <lock.h>

//I kept simple mutexing in the locking for SBI
//Will add barrier/semaphore later as needed
int mutex_trylock(Mutex *mutex) {
    int old;
    asm volatile("amoswap.w.aq %0, %1, (%2)" 
                  : "=r"(old) 
                  : "r"(MUTEX_LOCKED), "r"(mutex)
                );
    return old != MUTEX_LOCKED;
}
void mutex_spinlock(Mutex *mutex) {
    while (!mutex_trylock(mutex));
}
void mutex_unlock(Mutex *mutex) {
    asm volatile("amoswap.w.rl zero, zero, (%0)" 
                  : : "r"(mutex)
                 );
}
