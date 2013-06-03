#include <assert.h>
#include "mutex.h"

namespace dmMutex
{
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
    Mutex New()
    {
        pthread_mutexattr_t attr;
        int ret = pthread_mutexattr_init(&attr);
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

    void Unlock(Mutex mutex)
    {
        int ret = pthread_mutex_unlock(mutex);
        assert(ret == 0);
    }

#elif defined(_WIN32)
    Mutex New()
    {
        HANDLE mutex = CreateMutex(NULL, FALSE, NULL);
        assert(mutex);
        return mutex;
    }

    void Delete(Mutex mutex)
    {
        BOOL ret = CloseHandle(mutex);
        assert(ret);
    }

    void Lock(Mutex mutex)
    {
        DWORD ret = WaitForSingleObject(mutex, INFINITE);
        assert(ret == WAIT_OBJECT_0);
    }

    void Unlock(Mutex mutex)
    {
        BOOL ret = ReleaseMutex(mutex);
        assert(ret);
    }

#else
#error "Unsupported platform"
#endif

}

