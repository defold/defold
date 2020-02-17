#ifndef DM_SPINLOCKTYPES_H
#define DM_SPINLOCKTYPES_H

// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/namespacenn_1_1os.html#ad902011901e60f940cb26ffb62989688
#include <stdint.h>

#include <dlib/atomic.h>

namespace dmSpinlock
{
    typedef int32_atomic_t lock_t;

    static inline void Init(lock_t* lock)
    {
        dmAtomicStore32(lock, 0);
    }

    static inline void Lock(lock_t* lock)
    {
        while (1) {
            if (__sync_bool_compare_and_swap(lock, 0, 1)) {
                return;
            }
        }
    }

    static inline void Unlock(lock_t* lock)
    {
        dmAtomicStore32(lock, 0);
    }
}

#endif // DM_SPINLOCKTYPES_H
