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

#include <stdint.h>
#include <stdlib.h>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/spinlock.h"
#include "../dlib/thread.h"

// NOTE: volatile. Otherwise gcc will consider the addition loop invariant
volatile int32_t g_Value = 0;
dmSpinlock::Spinlock g_Lock;

const int32_t ITER = 400000;

void Thread(void* arg)
{
    for (int i = 0; i < ITER; ++i)
    {
        dmSpinlock::Lock(&g_Lock);
        ++g_Value;
        dmSpinlock::Unlock(&g_Lock);
    }
}

TEST(dmSpinlock, Test)
{
    dmSpinlock::Create(&g_Lock);
    dmThread::Thread t1 = dmThread::New(Thread, 0xf000, 0, "t1");
    dmThread::Thread t2 = dmThread::New(Thread, 0xf000, 0, "t2");
    dmThread::Join(t1);
    dmThread::Join(t2);
    ASSERT_EQ(ITER * 2, g_Value);
    dmSpinlock::Destroy(&g_Lock);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
