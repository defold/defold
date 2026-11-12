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

struct SpinlockPublicationContext
{
    dmSpinlock::Spinlock m_Lock;
    uint32_t             m_Value;
    uint32_t             m_Inverse;
    uint32_t             m_Failures;
};

static void UpdateProtectedData(void* data)
{
    SpinlockPublicationContext* context = (SpinlockPublicationContext*)data;
    for (int32_t i = 0; i < ITER; ++i)
    {
        dmSpinlock::Lock(&context->m_Lock);
        if (context->m_Inverse != ~context->m_Value)
        {
            ++context->m_Failures;
        }
        ++context->m_Value;
        context->m_Inverse = ~context->m_Value;
        dmSpinlock::Unlock(&context->m_Lock);
    }
}

// Unlock must publish ordinary protected data to the next thread acquiring the lock.
TEST(dmSpinlock, PublishesProtectedData)
{
    SpinlockPublicationContext context = {};
    context.m_Inverse = ~context.m_Value;
    dmSpinlock::Create(&context.m_Lock);
    dmThread::Thread first = dmThread::New(UpdateProtectedData, 0x20000, &context, "spin-publish-1");
    dmThread::Thread second = dmThread::New(UpdateProtectedData, 0x20000, &context, "spin-publish-2");
    dmThread::Join(first);
    dmThread::Join(second);
    ASSERT_EQ(0u, context.m_Failures);
    ASSERT_EQ((uint32_t)ITER * 2, context.m_Value);
    ASSERT_EQ(~context.m_Value, context.m_Inverse);
    dmSpinlock::Destroy(&context.m_Lock);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
