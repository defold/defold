#ifndef DM_MUTEX_H
#define DM_MUTEX_H

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
#include <pthread.h>
namespace dmMutex
{
    typedef pthread_mutex_t* Mutex;
}

#elif defined(_WIN32)
#include "safe_windows.h"
namespace dmMutex
{
    typedef HANDLE Mutex;
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


}  // namespace dmMutex


#endif // DM_MUTEX_H
