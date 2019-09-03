#include <assert.h>
#include "mutex.h"

namespace dmMutex
{
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
    HMutex New()
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

        Mutex* mutex = new Mutex();

        ret = pthread_mutex_init(&mutex->m_NativeHandle, &attr);
        assert(ret == 0);
        ret = pthread_mutexattr_destroy(&attr);
        assert(ret == 0);

        return mutex;
    }

    void Delete(HMutex mutex)
    {
        assert(mutex);
        int ret = pthread_mutex_destroy(&mutex->m_NativeHandle);
        assert(ret == 0);
        delete mutex;
    }

    void Lock(HMutex mutex)
    {
        assert(mutex);
        int ret = pthread_mutex_lock(&mutex->m_NativeHandle);
        assert(ret == 0);
    }

    bool TryLock(HMutex mutex)
    {
        assert(mutex);
        return (pthread_mutex_trylock(&mutex->m_NativeHandle) == 0) ? true : false;
    }

    void Unlock(HMutex mutex)
    {
        assert(mutex);
        int ret = pthread_mutex_unlock(&mutex->m_NativeHandle);
        assert(ret == 0);
    }

#elif defined(_WIN32)
    HMutex New()
    {
        Mutex* mutex = new Mutex();
        InitializeCriticalSection(&mutex->m_NativeHandle);
        return mutex;
    }

    void Delete(HMutex mutex)
    {
        assert(mutex);
        DeleteCriticalSection(&mutex->m_NativeHandle);
        delete mutex;
    }

    void Lock(HMutex mutex)
    {
        assert(mutex);
        EnterCriticalSection(&mutex->m_NativeHandle);
    }

    bool TryLock(HMutex mutex)
    {
        assert(mutex);
        return (TryEnterCriticalSection(&mutex->m_NativeHandle)) == 0 ? false : true;
    }

    void Unlock(HMutex mutex)
    {
        assert(mutex);
        LeaveCriticalSection(&mutex->m_NativeHandle);
    }

#else
#error "Unsupported platform"
#endif

}

