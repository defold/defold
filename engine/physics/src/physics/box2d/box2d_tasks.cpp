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

#include <dlib/thread.h>

#include "box2d_physics.h"

namespace dmPhysics
{
#if defined(DM_HAS_THREADS)
    static void* CreateThread(void (*function)(void*), void* context, const char* name)
    {
        dmThread::Thread* thread = new dmThread::Thread;
        *thread = dmThread::New(function, 0x80000, context, name);
        return thread;
    }

    static void JoinThread(void* handle)
    {
        dmThread::Thread* thread = (dmThread::Thread*)handle;
        dmThread::Join(*thread);
#if defined(_WIN32)
        // Win32 Join waits for termination; Detach closes the thread handle.
        dmThread::Detach(*thread);
#endif
        delete thread;
    }
#endif

    b2Scheduler* NewBox2DScheduler(uint32_t worker_count)
    {
#if defined(DM_HAS_THREADS)
        // Initialize Box2D's lazy timer before any worker reads it.
        b2GetMilliseconds(b2GetTicks());
        const b2SchedulerThreadInterface thread_interface = { CreateThread, JoinThread };
        return b2CreateScheduler(worker_count + 1, &thread_interface);
#else
        (void)worker_count;
        return 0;
#endif
    }
}
