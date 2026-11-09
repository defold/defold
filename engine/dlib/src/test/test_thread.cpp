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
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/thread.h"
#include "../dlib/atomic.h"

struct ThreadArg
{
    uint32_t* m_P;
    uint32_t  m_Index;
    uint32_t  m_Value;
};

static void ThreadFunction(void* arg)
{
    ThreadArg* a = (ThreadArg*) arg;
    a->m_P[a->m_Index] = a->m_Value;
}

TEST(Thread, Basic1)
{
    uint32_t arr[4] = {0, 0};
    ThreadArg a1, a2;
    a1.m_P = arr;
    a1.m_Index = 0;
    a1.m_Value = 10;
    a2.m_P = arr;
    a2.m_Index = 1;
    a2.m_Value = 20;

    dmThread::Thread t1 = dmThread::New(&ThreadFunction, 0x80000, &a1, "t1");
    dmThread::Thread t2 = dmThread::New(&ThreadFunction, 0x80000, &a2, "t2");

    dmThread::Join(t1);
    dmThread::Join(t2);

    ASSERT_EQ((uint32_t) 10, arr[0]);
    ASSERT_EQ((uint32_t) 20, arr[1]);
}

dmThread::TlsKey g_TlsKey;
int g_TlsData[2] = { 0, 0 };
int32_atomic_t g_NextTlsIndex = 0;

static void TlsThreadFunction(void* arg)
{
    uintptr_t n = (uintptr_t) arg;

    void* data = dmThread::GetTlsValue(g_TlsKey);
    assert(data == 0);
    int32_t i = dmAtomicIncrement32(&g_NextTlsIndex);
    data = &g_TlsData[i];
    dmThread::SetTlsValue(g_TlsKey, data);

    for (uintptr_t i = 0; i < n; ++i)
    {
        int* tls_data = (int*) dmThread::GetTlsValue(g_TlsKey);
        *tls_data = *tls_data + 1;
    }
}

TEST(Thread, Tls)
{
    g_TlsKey = dmThread::AllocTls();

    dmThread::Thread t1 = dmThread::New(&TlsThreadFunction, 0x80000, (void*) 1000, "t1");
    dmThread::Thread t2 = dmThread::New(&TlsThreadFunction, 0x80000, (void*) 2000, "t2");

    dmThread::Join(t1);
    dmThread::Join(t2);

    // Don't rely on thread start order
    if (g_TlsData[0] == 1000)
    {
        ASSERT_EQ(1000, g_TlsData[0]);
        ASSERT_EQ(2000, g_TlsData[1]);
    }
    else
    {
        ASSERT_EQ(1000, g_TlsData[1]);
        ASSERT_EQ(2000, g_TlsData[0]);
    }

    dmThread::FreeTls(g_TlsKey);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}


