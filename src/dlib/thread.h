#ifndef DM_THREAD_H
#define DM_THREAD_H

#include <stdint.h>

#if defined(__linux__) || defined(__MACH__)
#include <pthread.h>
namespace dmThread
{
    typedef pthread_t Thread;
}

#elif defined(_WIN32)
#include <windows.h>
namespace dmThread
{
    typedef HANDLE Thread;
}

#else
#error "Unsupported platform"
#endif

namespace dmThread
{
    typedef void (*ThreadStart)(void*);

    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg);
    void   Join(Thread thread);
}

#endif // DM_THREAD_H
