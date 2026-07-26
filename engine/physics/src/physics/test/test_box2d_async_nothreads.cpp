// Copyright 2020-2026 The Defold Foundation
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

// Exercises the no-threads fallback of the async worker substrate. The target compiles
// box2d_async_thread.cpp with DM_BOX2D_ASYNC_NO_THREADS forced on, so Submit runs the job inline on
// the calling thread and Wait is a no-op. This is the path a web build without pthreads takes.

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../box2d/box2d_async_thread.h"

using namespace dmPhysics;

static void SetJob(void* p)
{
    *(int*)p = 42;
}

// Submit runs the job inline (before returning); the worker is always idle and Wait is a no-op.
TEST(AsyncWorkerNoThreads, SubmitRunsInline)
{
    AsyncWorker* w = NewAsyncWorker("nothreads", false);
    ASSERT_TRUE(AsyncWorkerIsIdle(w));

    int value = 0;
    AsyncWorkerSubmit(w, SetJob, &value);
    ASSERT_EQ(42, value);            // ran inside Submit, no thread
    ASSERT_TRUE(AsyncWorkerIsIdle(w));

    AsyncWorkerWait(w);              // no-op
    ASSERT_TRUE(AsyncWorkerIsIdle(w));

    DeleteAsyncWorker(w);
}

// Many inline submit/wait cycles accumulate correctly, and teardown is clean.
TEST(AsyncWorkerNoThreads, ManyCycles)
{
    AsyncWorker* w = NewAsyncWorker("nothreads", false);
    int value = 0;
    for (int i = 0; i < 64; ++i)
    {
        int add = 1;
        AsyncWorkerSubmit(w, SetJob, &add); // writes 42 into add (inline)
        AsyncWorkerWait(w);
        value += add;
    }
    ASSERT_EQ(64 * 42, value);
    DeleteAsyncWorker(w);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
