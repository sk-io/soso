#include "spinlock.h"

static inline int32_t exchangeAtomic(volatile int32_t* oldValueAddress, int32_t newValue)
{
    //no need to use lock instruction on xchg

    asm volatile ("xchgl %0, %1"
                   : "=r"(newValue)
                   : "m"(*oldValueAddress), "0"(newValue)
                   : "memory");
    return newValue;
}

void Spinlock_Init(Spinlock* spinlock)
{
    *spinlock = 0;
}

void Spinlock_Lock(Spinlock* spinlock)
{
    while (exchangeAtomic((int32_t*)spinlock, 1))
    {
        halt();
    }
}

BOOL Spinlock_TryLock(Spinlock* spinlock)
{
    if (exchangeAtomic((int32_t*)spinlock, 1))
    {
        return FALSE;
    }

    return TRUE;
}

void Spinlock_Unlock(Spinlock* spinlock)
{
    *spinlock = 0;
}
