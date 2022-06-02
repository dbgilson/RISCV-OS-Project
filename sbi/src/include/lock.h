#pragma once

//I kept simple mutexing in the locking for SBI
//Will add barrier/semaphore later as needed
typedef int Mutex;

int mutex_trylock(Mutex *mutex);
void mutex_spinlock(Mutex *mutex);
void mutex_unlock(Mutex *mutex);

#define MUTEX_LOCKED   1
#define MUTEX_UNLOCKED 0
