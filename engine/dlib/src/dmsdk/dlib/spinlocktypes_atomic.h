#ifndef DM_SPINLOCKTYPES_ATOMIC_H
#define DM_SPINLOCKTYPES_ATOMIC_H

#include <stdint.h>
#include <dlib/atomic.h>

namespace dmSpinlock
{
    typedef int32_atomic_t Spinlock;

    static inline void Init(Spinlock* lock)
    {
        dmAtomicStore32(lock, 0);
    }

    static inline void Lock(Spinlock* lock)
    {
        while (dmAtomicCompareStore32(lock, 1, 0) != 0) {
        }
    }

    static inline void Unlock(Spinlock* lock)
    {
        dmAtomicStore32(lock, 0);
    }
}

#endif
