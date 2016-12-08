#ifndef DM_SPINLOCK_H
#define DM_SPINLOCK_H

#include <stdint.h>

#if defined(__linux__) && !defined(ANDROID)
#include <pthread.h>
namespace dmSpinlock
{
    typedef pthread_spinlock_t lock_t;

    static inline void Init(lock_t* lock)
    {
        int ret = pthread_spin_init(lock, 0);
        assert(ret == 0);
    }

    static inline void Lock(lock_t* lock)
    {
        int ret = pthread_spin_lock(lock);
        assert(ret == 0);
    }

    static inline void Unlock(lock_t* lock)
    {
        int ret = pthread_spin_unlock(lock);
        assert(ret == 0);
    }
}
#elif defined(__MACH__)
#include <libkern/OSAtomic.h>
namespace dmSpinlock
{
    typedef OSSpinLock lock_t;

    static inline void Init(lock_t* lock)
    {
        *lock = 0;
    }

    static inline void Lock(lock_t* lock)
    {
        OSSpinLockLock(lock);
    }

    static inline void Unlock(lock_t* lock)
    {
        OSSpinLockUnlock(lock);
    }
}
#elif defined(_MSC_VER) && defined(_M_IX86)
#include "safe_windows.h"
namespace dmSpinlock
{
    typedef volatile long lock_t;

    static inline void Init(lock_t* lock)
    {
        *lock = 0;
    }

    static inline void Lock(lock_t* lock)
    {
        // NOTE: No barriers. Only valid on x86
        while (InterlockedCompareExchange(lock, 1, 0) != 0)
        {

        }
    }

    static inline void Unlock(lock_t* lock)
    {
        // NOTE: No barriers. Only valid on x86
        *lock = 0;
    }
}
#elif defined(_MSC_VER) && defined(_M_X64)
#include "safe_windows.h"
namespace dmSpinlock
{
  typedef __declspec(align(8)) volatile LONGLONG  lock_t;

  static inline void Init(lock_t* lock)
  {
    /*
     * "Simple reads and writes to properly aligned 64-bit variables are atomic on 64-bit Windows"
     * https://msdn.microsoft.com/en-us/library/windows/desktop/ms684122(v=vs.85).aspx
     */
    
    *lock = 0LL;
  }

  static inline void Lock(lock_t* lock)
  {
    while (InterlockedCompareExchange64(lock, 1LL, 0LL) != 0LL)
    {
      
    }
  }

  static inline void Unlock(lock_t* lock)
  {
    *lock = 0LL;
  }
}
#elif defined(ANDROID)
namespace dmSpinlock
{
    typedef uint32_t lock_t;

    static inline void Init(lock_t* lock)
    {
        *lock = 0;
    }

    static inline void Lock(lock_t* lock)
    {
        uint32_t tmp;
         __asm__ __volatile__(
 "1:     ldrex   %0, [%1]\n"
 "       teq     %0, #0\n"
// "       wfene\n"  -- WFENE gives SIGILL for still unknown reasons on some 64-bit ARMs Aarch32 mode.
//                      Disassembly of libc.so and pthread_mutex_lock shows no use of the instruction.
 "       strexeq %0, %2, [%1]\n"
 "       teqeq   %0, #0\n"
 "       bne     1b\n"
"        dsb\n"
         : "=&r" (tmp)
         : "r" (lock), "r" (1)
         : "cc");

    }

    static inline void Unlock(lock_t* lock)
    {
        __asm__ __volatile__(
"       dsb\n"
"       str     %1, [%0]\n"
        :
        : "r" (lock), "r" (0)
        : "cc");
    }
}

#elif defined(__EMSCRIPTEN__) || defined(__AVM2__)

namespace dmSpinlock
{
    typedef volatile int lock_t;

    static inline void Init(lock_t* lock)
    {
        *lock = 0;
    }

    static inline void Lock(lock_t* lock)
    {
        while (*lock != 0)
        {

        }
    }

    static inline void Unlock(lock_t* lock)
    {
        *lock = 0;
    }
}

#else
#error "Unsupported platform"
#endif

#endif
