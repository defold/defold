#ifndef DM_SPINLOCKTYPES_MACH_H
#define DM_SPINLOCKTYPES_MACH_H

#include <libkern/OSAtomic.h>
namespace dmSpinlock
{
    typedef OSSpinLock Spinlock;

    static inline void Init(Spinlock* lock)
    {
        *lock = 0;
    }

    static inline void Lock(Spinlock* lock)
    {
        OSSpinLockLock(lock);
    }

    static inline void Unlock(Spinlock* lock)
    {
        OSSpinLockUnlock(lock);
    }
}

#endif
