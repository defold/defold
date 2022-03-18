// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_THREAD_H
#define DMSDK_THREAD_H

#include <stdint.h>

// These headers define the dmThread::Thread and dmThread::TlsKey with the native types

#if defined(_WIN32)
    #include <dmsdk/dlib/thread_native_win32.h>
#elif defined(__NX__)
    #include <dmsdk/dlib/thread_native_nx64.h>
#else
    #include <dmsdk/dlib/thread_native_posix.h>
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
     * #include <dmsdk/sdk.h>
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
     *     dmThread::Thread thread = dmThread::New(Worker, 0x80000, (void*)&ctx, "my_thread");
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
     * Join thread. Waits for the thread specified by thread to terminate.  If
     * that thread has already terminated, then Join() returns immediately.  The
     * thread specified by thread must be joinable (see Detach()).
     * @name dmThread::Join
     * @param thread Thread to join
     */
    void Join(Thread thread);

    /*# detach thread
     *
     * Detach thread. When a detached thread terminates, its resources are
     * automatically released back to the system without the need for another
     * thread to join with the terminated thread.
     * @name dmThread::Detach
     * @param thread Thread to detach
     */
    void Detach(Thread thread);

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
     * @name dmThread::GetTlsValue
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
