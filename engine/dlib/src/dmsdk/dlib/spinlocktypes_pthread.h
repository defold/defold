#ifndef DM_SPINLOCKTYPES_PTHREAD_H
#define DM_SPINLOCKTYPES_PTHREAD_H

#include <stdint.h>
#include <pthread.h>
namespace dmSpinlock
{
    typedef pthread_spinlock_t Spinlock;

    static inline void Init(Spinlock* lock)
    {
        int ret = pthread_spin_init(lock, 0);
        assert(ret == 0);
    }

    static inline void Lock(Spinlock* lock)
    {
        int ret = pthread_spin_lock(lock);
        assert(ret == 0);
    }

    static inline void Unlock(Spinlock* lock)
    {
        int ret = pthread_spin_unlock(lock);
        assert(ret == 0);
    }
}

#endif
