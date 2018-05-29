#include "thread.h"

#include <assert.h>

namespace dmThread
{
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
    struct ThreadData
    {
        ThreadStart m_Start;
        const char* m_Name;
        void*       m_Arg;
    };

    static void ThreadStartProxy(void* arg)
    {
        ThreadData* data = (ThreadData*) arg;
#if defined(__MACH__)
        int ret = pthread_setname_np(data->m_Name);
        assert(ret == 0);
#elif defined(__EMSCRIPTEN__)
#else
        int ret = pthread_setname_np(pthread_self(), data->m_Name);
        assert(ret == 0);
#endif
        data->m_Start(data->m_Arg);
        delete data;
    }

    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg, const char* name)
    {
        pthread_attr_t attr;
        long page_size = sysconf(_SC_PAGESIZE);
        int ret = pthread_attr_init(&attr);
        assert(ret == 0);

        if (page_size == -1)
            page_size = 4096;

        if (PTHREAD_STACK_MIN > stack_size)
            stack_size = PTHREAD_STACK_MIN;

        // NOTE: At least on OSX the stack-size must be a multiple of page size
        stack_size /= page_size;
        stack_size += 1;
        stack_size *= page_size;

        ret = pthread_attr_setstacksize(&attr, stack_size);
        assert(ret == 0);

        pthread_t thread;

        ThreadData* thread_data = new ThreadData;
        thread_data->m_Start = thread_start;
        thread_data->m_Name = name;
        thread_data->m_Arg = arg;

        ret = pthread_create(&thread, &attr, (void* (*)(void*)) ThreadStartProxy, thread_data);
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
    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg, const char* name)
    {
        (void*) name;
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



