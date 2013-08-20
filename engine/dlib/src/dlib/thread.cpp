#include <assert.h>
#include "thread.h"

namespace dmThread
{
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg)
    {
        pthread_attr_t attr;
        int ret = pthread_attr_init(&attr);
        assert(ret == 0);

        // NOTE: At least on OSX the stack-size must be a multiple of page size
        stack_size /= 4096;
        stack_size += 1;
        stack_size *= 4096;

        ret = pthread_attr_setstacksize(&attr, stack_size);
        assert(ret == 0);

        pthread_t thread;

        ret = pthread_create(&thread, &attr, (void* (*)(void*)) thread_start, arg);
        assert(ret == 0);
        ret = pthread_attr_destroy(&attr);
        assert(ret == 0);

        return thread;
    }

    void Join(Thread thread)
    {
        int ret = pthread_join(thread, 0);
        assert(ret == 0);
    }

    TlsKey AllocTls()
    {
        pthread_key_t key;
        int ret = pthread_key_create(&key, 0);
        assert(ret == 0);
        return key;
    }

    void FreeTls(TlsKey key)
    {
        int ret = pthread_key_delete(key);
        assert(ret == 0);
    }

    void SetTlsValue(TlsKey key, void* value)
    {
        int ret = pthread_setspecific(key, value);
        assert(ret == 0);
    }

    void* GetTlsValue(TlsKey key)
    {
        return pthread_getspecific(key);
    }


#elif defined(_WIN32)
    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg)
    {
        DWORD thread_id;
        HANDLE thread = CreateThread(NULL, stack_size,
                                     (LPTHREAD_START_ROUTINE) thread_start,
                                     arg, 0, &thread_id);
        assert(thread);

        return thread;
    }

    void Join(Thread thread)
    {
        uint32_t ret = WaitForSingleObject(thread, INFINITE);
        assert(ret == WAIT_OBJECT_0);
    }

    TlsKey AllocTls()
    {
        return TlsAlloc();
    }

    void FreeTls(TlsKey key)
    {
        BOOL ret = TlsFree(key);
        assert(ret);
    }

    void SetTlsValue(TlsKey key, void* value)
    {
        BOOL ret = TlsSetValue(key, value);
        assert(ret);
    }

    void* GetTlsValue(TlsKey key)
    {
        return TlsGetValue(key);
    }

#else
#error "Unsupported platform"
#endif

}



