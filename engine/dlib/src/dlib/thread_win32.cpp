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
#include <dlib/profile/profile.h>
#include <dmsdk/dlib/thread.h>

#include <stdlib.h>
#include <wchar.h>

namespace dmThread
{
    static void* GetFunctionPtr(const char* dllname, const char* fnname)
    {
        return (void*)GetProcAddress(GetModuleHandleA(dllname), fnname);
    }

    typedef HRESULT (*PfnSetThreadDescription)(HANDLE,PCWSTR);

    void SetThreadName(Thread thread, const char* name)
    {
        (void)thread;
        (void)name;
    // Currently, this crashed mysteriously on Win32, so we'll keep it only for Win64 until we've figured it out
    #if defined(_WIN64)
        static PfnSetThreadDescription pfn = (PfnSetThreadDescription)GetFunctionPtr("kernel32.dll", "SetThreadDescription");
        if (pfn) {
            size_t wn = mbsrtowcs(NULL, &name, 0, NULL);
            wchar_t* buf = (wchar_t*)malloc((wn + 1) * sizeof(wchar_t));
            wn = mbsrtowcs(buf, &name, wn + 1, NULL);

            pfn(thread, buf);

            free(buf);
        }
    #endif
    }

    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg, const char* name)
    {
        DWORD thread_id;
        HANDLE thread = CreateThread(NULL, stack_size,
                                     (LPTHREAD_START_ROUTINE) thread_start,
                                     arg, 0, &thread_id);
        assert(thread);

        SetThreadName((Thread)thread, name);

        return thread;
    }

    void Join(Thread thread)
    {
        uint32_t ret = WaitForSingleObject(thread, INFINITE);
        assert(ret == WAIT_OBJECT_0);
    }

    void Detach(Thread thread)
    {
        CloseHandle(thread);
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

    Thread GetCurrentThread()
    {
        return ::GetCurrentThread();
    }
}
