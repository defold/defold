#ifndef DM_SPINLOCKTYPES_H
#define DM_SPINLOCKTYPES_H

// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/namespacenn_1_1os.html#ad902011901e60f940cb26ffb62989688
#include <stdint.h>
#include <nn/os/os_BusyMutexTypes.h>
#include <nn/os/os_BusyMutexApi.h>

namespace dmSpinlock
{
    typedef nn::os::BusyMutexType lock_t;

    static inline void Init(lock_t* lock)
    {
        nn::os::InitializeBusyMutex(lock);
    }

    static inline void Lock(lock_t* lock)
    {
        nn::os::LockBusyMutex(lock);
    }

    static inline void Unlock(lock_t* lock)
    {
        nn::os::UnlockBusyMutex(lock);
    }
}

#endif // DM_SPINLOCKTYPES_H
