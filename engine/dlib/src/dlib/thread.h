// Copyright 2020 The Defold Foundation
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

#ifndef DM_THREAD_H
#define DM_THREAD_H

#include <stdint.h>

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

namespace dmThread
{
    typedef void (*ThreadStart)(void*);

    /**
     * Create a new named thread
     * @note thread name currently not supported on win32
     * @param thread_start Thread entry function
     * @param stack_size Stack size
     * @param arg Thread argument
     * @param name Thread name
     * @return Thread handle
     */
    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg, const char* name);

    /**
     * Join thread
     * @param thread Thread to join
     */
    void Join(Thread thread);

    /**
     * Allocate thread local storage key
     * @return Key
     */

    TlsKey AllocTls();

    /**
     * Free thread local storage key
     * @param key Key
     */

    void FreeTls(TlsKey key);

    /**
     * Set thread specific data
     * @param key Key
     * @param value Value
     */
    void SetTlsValue(TlsKey key, void* value);

    /**
     * Get thread specific data
     * @param key Key
     */
    void* GetTlsValue(TlsKey key);

    /** Gets the current thread
     * @return the current thread
     */
    Thread GetCurrentThread();

    /** Sets the current thread name
     * @param thread the thread
     * @param name the thread name
     * @note The thread argument is unused on Darwin (uses current thread)
     */
    void SetThreadName(Thread thread, const char* name);
}

#endif // DM_THREAD_H
