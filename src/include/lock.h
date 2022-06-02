#pragma once
#define MUTEX_LOCKED   1
#define MUTEX_UNLOCKED 0

typedef int Mutex;

int mutex_trylock(Mutex *mutex);
void mutex_spinlock(Mutex *mutex);
void mutex_unlock(Mutex *mutex);

struct Semaphore{
    int value;
};

int semaphore_trydown(struct Semaphore *s);
void semaphore_up(struct Semaphore *s);

struct Barrier_List{
    struct Process *proc; //Not actual processes at the moment, just using a dummy value to show implementation
    struct Barrier_List *next;
};

struct Barrier{
    struct Barrier_List *head;
    int value;
};

void barrier_init(struct Barrier *barrier);
void barrier_add_process(struct Barrier *barrier, struct Process *proc);
void barrier_wall(struct Barrier *barrier);