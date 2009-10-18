#include <assert.h>
#include "thread.h"

namespace dmThread
{
#if defined(__linux__) || defined(__MACH__)
    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg)
    {
        pthread_attr_t attr;
        assert(pthread_attr_init(&attr) == 0);
        
        assert(pthread_attr_setstacksize(&attr, stack_size));
        
        pthread_t thread;
        
        int ret = pthread_create(&thread, &attr, (void* (*)(void*)) thread_start, arg);
        assert(ret == 0);
        assert(pthread_attr_destroy(&attr) == 0);
        
        return thread;
    }
    
    void Join(Thread thread)
    {
        assert(pthread_join(thread, 0) == 0);
    }

#elif defined(_WIN32)
    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg)
    {
        uint32_t thread_id;
        HANDLE thread = CreateThread(NULL, stack_size, 
                                     (LPTHREAD_START_ROUTINE) thread_start, 
                                     arg, 0, &thread_id);
        assert(thread);

        return thread;
    }

    void Join(Thread thread)
    {
        uint32_t ret = WaitForSingleObject(thread, INFINITE);
        assert(ret);
    }

#else
#error "Unsupported platform"
#endif

}


 
