// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <dlib/profile/profile.h>
#include <dmsdk/dlib/thread.h>

#if defined(_WIN32)
#include <wchar.h>
#endif

namespace dmThread
{
    struct ThreadData
    {
        ThreadStart m_Start;
        char*       m_Name;
        void*       m_Arg;
    };

    static void *ThreadStartProxy(void* arg)
    {
        ThreadData* data = (ThreadData*) arg;
        SetThreadName(GetCurrentThread(), data->m_Name);
        ProfileSetThreadName(data->m_Name);

        data->m_Start(data->m_Arg);
        free(data->m_Name);
        delete data;
        return nullptr;
    }

    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg, const char* name)
    {
        pthread_attr_t attr;
#if defined(DM_DLIB_THREAD_HAS_NO_SYSCONF)
        long page_size = -1;
#else
        long page_size = sysconf(_SC_PAGESIZE);
#endif
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
        thread_data->m_Name = strdup(name);
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

    void Detach(Thread thread)
    {
        int ret = pthread_detach(thread);
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

    Thread GetCurrentThread()
    {
        return pthread_self();
    }

#if !defined(DM_DLIB_THREAD_USE_CUSTOM_SETNAME)
    void SetThreadName(Thread thread, const char* name)
    {
#if defined(__MACH__)
        (void)thread;
        pthread_setname_np(name);
#elif defined(__EMSCRIPTEN__)
#else
        pthread_setname_np(thread, name);
#endif
    }
#endif // DM_DLIB_THREAD_USE_CUSTOM_SETNAME
}
