#ifndef DMSDK_MUTEX_H
#define DMSDK_MUTEX_H

namespace dmMutex
{
    typedef struct OpaqueMutex* Mutex;

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

#endif // #ifndef DMSDK_MUTEX_H
