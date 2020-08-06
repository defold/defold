#ifndef DM_THREADTYPES_H
#define DM_THREADTYPES_H

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)
#include <pthread.h>
#include <limits.h>
#include <unistd.h>
namespace dmThread
{
    typedef pthread_t Thread;
    typedef pthread_key_t TlsKey;
}

#elif defined(_WIN32)
#include "safe_windows.h"
namespace dmThread
{
    typedef HANDLE Thread;
    typedef DWORD TlsKey;
}

#else
#error "Unsupported platform"
#endif

#endif // DM_THREADTYPES_H