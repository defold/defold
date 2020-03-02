#if !defined(__NX__)
#error "Unsupported platform"
#endif

#include <assert.h>
#include <stdlib.h> // malloc
#include "mutex.h"

#include <nn/os/os_MutexTypes.h>

namespace dmMutex
{
    HMutex New()
    {
        // Kept this comment from the pthread implementation since it might still be relevant
        // // NOTE: We should perhaps consider non-recursive mutex:
        // // from http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutexattr_settype.html
        // // It is advised that an application should not use a PTHREAD_MUTEX_RECURSIVE mutex
        // // with condition variables because the implicit unlock performed for a pthread_cond_wait()
        // // or pthread_cond_timedwait() may not actually release the mutex (if it had been locked
        // // multiple times). If this happens, no other thread can satisfy the condition of the predicate.

        Mutex* mutex = (Mutex*)malloc(sizeof(Mutex));
        nn::os::InitializeMutex(&mutex->m_NativeHandle, true, 0);
        return mutex;
    }

    void Delete(HMutex mutex)
    {
        assert(mutex);
        nn::os::FinalizeMutex(&mutex->m_NativeHandle);
        free(mutex);
    }

    void Lock(HMutex mutex)
    {
        assert(mutex);
        nn::os::LockMutex(&mutex->m_NativeHandle);
    }

    bool TryLock(HMutex mutex)
    {
        assert(mutex);
        return nn::os::TryLockMutex(&mutex->m_NativeHandle);
    }

    void Unlock(HMutex mutex)
    {
        assert(mutex);
        nn::os::UnlockMutex(&mutex->m_NativeHandle);
    }
}

