#ifndef DM_SPINLOCK_H
#define DM_SPINLOCK_H

#include <stdint.h>

namespace dmSpinlock
{
    /// Initial spinlock value
    const int32_t INITIAL_VALUE = 0;
}

#if defined(__linux__)
#include <pthread.h>
namespace dmSpinlock
{
    typedef pthread_spinlock_t lock_t;

    static inline void Lock(lock_t* lock)
    {
        int ret = pthread_spin_lock(lock);
        assert(ret == 0);
    }

    static inline void Unlock(lock_t* lock)
    {
        int ret = pthread_spin_unlock(lock);
        assert(ret == 0);
    }
}
#elif defined(__MACH__)
#include <libkern/OSAtomic.h>
namespace dmSpinlock
{
    typedef OSSpinLock lock_t;

    static inline void Lock(lock_t* lock)
    {
        OSSpinLockLock(lock);
    }

    static inline void Unlock(lock_t* lock)
    {
        OSSpinLockUnlock(lock);
    }
}
#elif defined(_MSC_VER) && defined(_M_IX86)
#include <windows.h>
namespace dmSpinlock
{
    typedef volatile long lock_t;

    static inline void Lock(lock_t* lock)
    {
        // NOTE: No barriers. Only valid on x86
        while (InterlockedCompareExchange(lock, 1, 0) != 0)
        {

        }
    }

    static inline void Unlock(lock_t* lock)
    {
        // NOTE: No barriers. Only valid on x86
        *lock = 0;
    }
}
#else
#error "Unsupported platform"
#endif

#endif
