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

#ifndef DMSDK_THREAD_H
#define DMSDK_THREAD_H

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

/*# SDK Thread API documentation
 * [file:<dmsdk/dlib/thread.h>]
 *
 * Thread functions.
 *
 * @document
 * @name Thread
 * @namespace dmThread
 */

namespace dmThread
{
    typedef void (*ThreadStart)(void*);

    /*# create a new thread
     * Create a new named thread
     * @note thread name currently not supported on win32
     * @name dmThread::New
     * @param thread_start Thread entry function
     * @param stack_size Stack size
     * @param arg Thread argument
     * @param name Thread name
     * @return Thread handle
     * @examples
     *
     * Create a thread
     *
     * ```cpp
     * #include <dmsdh/sdk.h>
     *
     * struct Context
     * {
     *     bool m_DoWork;
     *     int  m_Work;
     * };
     *
     * static void Worker(void* _ctx)
     * {
     *     Context* ctx = (Context*)_ctx;
     *     while (ctx->m_DoWork)
     *     {
     *         ctx->m_Work++; // do work
     *         dmTime::Sleep(10*1000); // yield
     *     }
     * }
     *
     * int StartThread()
     * {
     *     Context ctx;
     *     ctx.m_DoWork = true;
     *     ctx.m_Work = 0;
     *     dmThread::HThread thread = dmThread::New(dmLogThread, 0x80000, (void*)&ctx, "my_thread");
     *
     *     // do other work...
     *     // ..eventually stop the thread:
     *     ctx.m_DoWork = false;
     *
     *     // wait for thread
     *     dmThread::Join(thread);
     *
     *     printf("work done: %d\n", ctx.m_Work);
     * }
     * ```
     */
    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg, const char* name);

    /*# join thread
     *
     * Join thread
     * @name dmThread::Join
     * @param thread Thread to join
     */
    void Join(Thread thread);

    /*# allocate thread local storage key
     * Allocate thread local storage key
     * @name dmThread::AllocTls
     * @return Key
     */
    TlsKey AllocTls();

    /*# free thread local storage key
     * Free thread local storage key
     * @name dmThread::FreeTls
     * @param key Key
     */
    void FreeTls(TlsKey key);

    /*# set thread specific data
     * Set thread specific data
     * @name dmThread::SetTlsValue
     * @param key Key
     * @param value Value
     */
    void SetTlsValue(TlsKey key, void* value);

    /*# get thread specific data
     * Get thread specific data
     * @name dmThread::GetTlsVAlue
     * @param key Key
     */
    void* GetTlsValue(TlsKey key);

    /*# gets the current thread
     * Gets the current thread
     * @name dmThread::GetCurrentThread
     * @return the current thread
     */
    Thread GetCurrentThread();

    /*# sets the current thread name
     * Sets the current thread name
     * @name dmThread::SetThreadName
     * @param thread the thread
     * @param name the thread name
     * @note The thread argument is unused on Darwin (uses current thread)
     */
    void SetThreadName(Thread thread, const char* name);
}

#endif // DMSDK_THREAD_H
