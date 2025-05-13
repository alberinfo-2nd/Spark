#ifndef SPINLOCK_H
#define SPINLOCK_H

#define LOCK_FREE   0
#define LOCK_INUSE  1

#include <types.h>

//In the future, add code to handle task switching so that a task is put to sleep when it tries to lock on a resource

struct spinlock_t {
    u8 state;
} __attribute__((aligned(4))); //Align to dword, read instructions are guaranteed to be atomic

extern void SPINLOCK_acquire_lock(void*);
extern void SPINLOCK_release_lock(void*);

#endif