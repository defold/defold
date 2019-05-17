#ifndef DMSDK_MUTEX_H
#define DMSDK_MUTEX_H

namespace dmMutex
{
    /*# SDK Mutex API documentation
     * [file:<dmsdk/dlib/mutex.h>]
     *
     * API for platform independent mutex synchronization primitive.
     *
     * @document
     * @name Mutex
     * @namespace dmMutex
     */

    /*# HMutex type definition
     *
     * ```cpp
     * typedef struct Mutex* HMutex;
     * ```
     *
     * @typedef
     * @name dmMutex::HMutex
     *
     */
    typedef struct Mutex* HMutex;

    /*# create Mutex
     *
     * Creates a new HMutex.
     *
     * @name dmMutex::New
     * @return mutex [type:dmMutex::HMutex] A new Mutex handle.
     */
    HMutex New();

    /*# delete Mutex.
     *
     * Deletes a HMutex.
     *
     * @name dmMutex::Delete
     * @param mutex [type:dmMutex::HMutex] Mutex handle to delete.
     */
    void Delete(HMutex mutex);

    /*# lock Mutex.
     *
     * Lock a HMutex, will block until mutex is unlocked if already locked elsewhere.
     *
     * @name dmMutex::Lock
     * @param mutex [type:dmMutex::HMutex] Mutex handle to lock.
     */
    void Lock(HMutex mutex);

    /*# non-blocking lock of Mutex.
     *
     * Tries to lock a HMutex, if mutex is already locked it will return false and
     * continue without locking the mutex.
     *
     * @name dmMutex::TryLock
     * @param mutex [type:dmMutex::HMutex] Mutex handle to lock.
     * @return result [type:bool] True if mutex was successfully locked, false otherwise.
     */
    bool TryLock(HMutex mutex);

    /*# unlock Mutex.
     *
     * Unlock a HMutex.
     *
     * @name dmMutex::Unlock
     * @param mutex [type:dmMutex::HMutex] Mutex handle to unlock.
     */
    void Unlock(HMutex mutex);

    struct ScopedLock
    {
        HMutex m_Mutex;
        ScopedLock(HMutex mutex)
        {
            m_Mutex = mutex;
            Lock(m_Mutex);
        }

        ~ScopedLock()
        {
            Unlock(m_Mutex);
        }
    };

    /*# macro for scope lifetime Mutex locking
     *
     * Will lock a Mutex and automatically unlock it at the end of the scope.
     *
     * @macro
     * @name DM_MUTEX_SCOPED_LOCK
     * @param mutex [type:dmMutex::HMutex] Mutex handle to lock.
     *
     */
    #define SCOPED_LOCK_PASTE(x, y) x ## y
    #define SCOPED_LOCK_PASTE2(x, y) SCOPED_LOCK_PASTE(x, y)
    #define DM_MUTEX_SCOPED_LOCK(mutex) dmMutex::ScopedLock SCOPED_LOCK_PASTE2(lock, __LINE__)(mutex);

}  // namespace dmMutex

#endif // #ifndef DMSDK_MUTEX_H
