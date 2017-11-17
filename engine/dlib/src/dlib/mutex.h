#ifndef DM_MUTEX_H
#define DM_MUTEX_H

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <pthread.h>
namespace dmMutex
{
    typedef pthread_mutex_t* Mutex;
}

#elif defined(_WIN32)
#include "safe_windows.h"
namespace dmMutex
{
    typedef CRITICAL_SECTION* Mutex;
}

#else
#error "Unsupported platform"
#endif

namespace dmMutex
{
    /**
     * Create a new Mutex
     * @return New mutex.
     */
    Mutex New();

    /**
     * Delete mutex
     * @param mutex
     */
    void Delete(Mutex mutex);

    /**
     * Lock mutex
     * @param mutex Mutex
     */
    void Lock(Mutex mutex);

    /**
     * Try lock mutex
     * @param mutex Mutex
     * @return true if the mutex was successfully locked.
     */
    bool TryLock(Mutex mutex);

    /**
     * Unlock mutex
     * @param mutex Mutex
     */
    void Unlock(Mutex mutex);

    struct ScopedLock
    {
        Mutex m_Mutex;
        ScopedLock(Mutex mutex)
        {
            m_Mutex = mutex;
            Lock(m_Mutex);
        }

        ~ScopedLock()
        {
            Unlock(m_Mutex);
        }
    };

    #define SCOPED_LOCK_PASTE(x, y) x ## y
    #define SCOPED_LOCK_PASTE2(x, y) SCOPED_LOCK_PASTE(x, y)
    #define DM_MUTEX_SCOPED_LOCK(mutex) dmMutex::ScopedLock SCOPED_LOCK_PASTE2(lock, __LINE__)(mutex);

}  // namespace dmMutex


#endif // DM_MUTEX_H
