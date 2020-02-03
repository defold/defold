#ifndef DM_SPINLOCK_H
#define DM_SPINLOCK_H

#include <stdint.h>
#include <dlib/spinlocktypes.h>

namespace dmSpinlock
{
    struct ScopedLock
    {
        dmSpinlock::lock_t& m_Spinlock;
        ScopedLock(dmSpinlock::lock_t& spinlock)
            : m_Spinlock(spinlock)
        {
            dmSpinlock::Lock(&m_Spinlock);
        }

        ~ScopedLock()
        {
            dmSpinlock::Unlock(&m_Spinlock);
        }
    };
}

#define SCOPED_SPINLOCK_PASTE(x, y) x ## y
#define SCOPED_SPINLOCK_PASTE2(x, y) SCOPED_SPINLOCK_PASTE(x, y)
#define DM_SPINLOCK_SCOPED_LOCK(spinlock) dmSpinlock::ScopedLock SCOPED_SPINLOCK_PASTE2(lock, __LINE__)(spinlock);

#endif
