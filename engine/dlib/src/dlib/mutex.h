#ifndef DM_MUTEX_H
#define DM_MUTEX_H

#include <dmsdk/dlib/mutex.h>

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
#include <pthread.h>
#elif defined(_WIN32)
#include "safe_windows.h"
#else
#error "Unsupported platform"
#endif

namespace dmMutex
{
    struct Mutex
    {
#if defined(_WIN32)
        CRITICAL_SECTION m_NativeHandle;
#else
        pthread_mutex_t  m_NativeHandle;
#endif
    };

}

#endif // DM_MUTEX_H
