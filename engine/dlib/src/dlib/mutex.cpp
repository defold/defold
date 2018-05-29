#include "mutex.h"

#include <assert.h>

namespace dmMutex
{
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
    Mutex New()
    {
        pthread_mutexattr_t attr;
        int ret = pthread_mutexattr_init(&attr);

        // NOTE: We should perhaps consider non-recursive mutex:
        // from http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutexattr_settype.html
        // It is advised that an application should not use a PTHREAD_MUTEX_RECURSIVE mutex
        // with condition variables because the implicit unlock performed for a pthread_cond_wait()
        // or pthread_cond_timedwait() may not actually release the mutex (if it had been locked
        // multiple times). If this happens, no other thread can satisfy the condition of the predicate.
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

        assert(ret == 0);

        pthread_mutex_t* mutex = new pthread_mutex_t;

        ret = pthread_mutex_init(mutex, &attr);
        assert(ret == 0);
        ret = pthread_mutexattr_destroy(&attr);
        assert(ret == 0);

        return mutex;
    }

    void Delete(Mutex mutex)
    {
        int ret = pthread_mutex_destroy(mutex);
        assert(ret == 0);
        delete mutex;
    }

    void Lock(Mutex mutex)
    {
        int ret = pthread_mutex_lock(mutex);
        assert(ret == 0);
    }

    bool TryLock(Mutex mutex)
    {
        return (pthread_mutex_trylock(mutex) == 0) ? true : false;
    }

    void Unlock(Mutex mutex)
    {
        int ret = pthread_mutex_unlock(mutex);
        assert(ret == 0);
    }

#elif defined(_WIN32)
    Mutex New()
    {
        CRITICAL_SECTION* mutex = new CRITICAL_SECTION;
        InitializeCriticalSection(mutex);
        return mutex;
    }

    void Delete(Mutex mutex)
    {
        DeleteCriticalSection(mutex);
        delete mutex;
    }

    void Lock(Mutex mutex)
    {
        EnterCriticalSection(mutex);
    }

    bool TryLock(Mutex mutex)
    {
        return (TryEnterCriticalSection(mutex)) == 0 ? false : true;
    }

    void Unlock(Mutex mutex)
    {
        LeaveCriticalSection(mutex);
    }

#else
#error "Unsupported platform"
#endif

}

