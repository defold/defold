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

#include <stdint.h>
#include <vector>
#include <map>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "dlib/dstrings.h"
#include "dlib/hash.h"
#include "dlib/time.h"
#include "dlib/mutex.h"
#include "dlib/thread.h"
#include "dlib/profile/profile.h"

#if !defined(_WIN32)

struct TestSample
{
    char m_Name[32];
    uint64_t m_Elapsed;
    uint32_t m_Count;

    TestSample() {
        memset(this, 0, sizeof(*this));
    }

    TestSample(const TestSample& rhs) {
        memcpy(this, &rhs, sizeof(*this));
    }
};

struct TestCtx
{
    dmMutex::HMutex m_Mutex;
    std::vector<TestSample> samples;
};

static void ProcessSample(TestCtx* ctx, dmProfile::HSample sample)
{
    TestSample out;

    const char* name = dmProfile::SampleGetName(sample); // Do not store this pointer!

    dmStrlCpy(out.m_Name, name, sizeof(out.m_Name));
    out.m_Name[sizeof(out.m_Name)-1] = 0;
    out.m_Elapsed = dmProfile::SampleGetTime(sample);
    out.m_Count = dmProfile::SampleGetCallCount(sample);

    ctx->samples.push_back(out);

    printf("%s %u  time: %llu\n", name, out.m_Count, out.m_Elapsed);
}

static void TraverseSampleTree(TestCtx* ctx, int indent, dmProfile::HSample sample)
{
    ProcessSample(ctx, sample);

    dmProfile::SampleIterator iter;
    dmProfile::SampleIterateChildren(sample, &iter);
    while (dmProfile::SampleIterateNext(&iter))
    {
        TraverseSampleTree(ctx, indent + 1, iter.m_Sample);
    }
}

static void SampleTreeCallback(void* _ctx, const char* thread_name, dmProfile::HSample root)
{
    // Since the profiling internals may be threaded, we want to gather the statistics into a ready-to-use place for the
    // our internal profile renderer
    //dmProfile::IterateSamples(root, void* context, void (*callback)(void* context, const Sample* sample));

    TestCtx* ctx = (TestCtx*)_ctx;
    if (strcmp(thread_name, "Remotery") == 0)
        return;

    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
    ctx->samples.clear();

    printf("Thread: %s\n", thread_name);
    TraverseSampleTree(ctx, 1, root);
}

// TODO
// 100 msec, which is in fact much higher than the expected time of the profiler
// On OSX, the time is usually a few microseconds, but once in a while the time spikes to ~0.5 ms
// On Linux CI, the time can be as high as 16 msec
// The timings (dmTime::BusyWait) is based around dmTime::GetTime, this issue is a revisit to improve the expected granularity: DEF-2013
#define TOL 0.1

TEST(dmProfile, Profile)
{
    TestCtx ctx;
    ctx.m_Mutex = dmMutex::New();

    dmProfile::Initialize(0);

    dmProfile::SetSampleTreeCallback(&ctx, SampleTreeCallback);

    for (int i = 0; i < 2; ++i)
    {
        {
            dmProfile::HProfile profile = dmProfile::BeginFrame();
            {
                DM_PROFILE("a")
                dmTime::BusyWait(100000);
                {
                    {
                        DM_PROFILE("a_b1")
                        dmTime::BusyWait(50000);
                        {
                            DM_PROFILE("a_b1_c")
                            dmTime::BusyWait(40000);
                        }
                    }
                    {
                        DM_PROFILE("b2")
                        dmTime::BusyWait(50000);
                        {
                            DM_PROFILE("a_b2_c1")
                            dmTime::BusyWait(40000);
                        }
                        {
                            DM_PROFILE("a_b2_c2")
                            dmTime::BusyWait(60000);
                        }
                    }
                }
            }
            {
                DM_PROFILE("a_d")
                dmTime::BusyWait(80000);
            }
            dmProfile::EndFrame(profile);
            dmTime::BusyWait(80000);
        }

        {
            DM_MUTEX_SCOPED_LOCK(ctx.m_Mutex);

            double ticks_per_sec = (double)dmProfile::GetTicksPerSecond();

            ASSERT_EQ(8U, (uint32_t)ctx.samples.size());

            int index = 0;
            ASSERT_STREQ("FrameBegin", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_b1", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_b1_c", ctx.samples[index++].m_Name);
            ASSERT_STREQ("b2", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_b2_c1", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_b2_c2", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_d", ctx.samples[index++].m_Name);

            index = 1;
            ASSERT_NEAR((100000 + 50000 + 40000 + 50000 + 40000 + 60000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((50000 + 40000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((40000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((50000 + 40000 + 60000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((40000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((60000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((80000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
        }

        // ASSERT_TRUE(scopes.end() != scopes.find("A"));
        // ASSERT_TRUE(scopes.end() != scopes.find("B"));
        // ASSERT_TRUE(scopes.end() != scopes.find("C"));
        // ASSERT_TRUE(scopes.end() != scopes.find("D"));

        // ASSERT_NEAR((100000 + 50000 + 40000 + 50000 + 40000 + 60000) / 1000000.0,
        //             scopes["A"]->m_Elapsed / ticks_per_sec, TOL);

        // ASSERT_NEAR((50000 + 40000 + 50000 + 40000 + 60000) / 1000000.0,
        //             scopes["B"]->m_Elapsed / ticks_per_sec, TOL);

        // ASSERT_NEAR((40000 + 40000 + 60000) / 1000000.0,
        //             scopes["C"]->m_Elapsed / ticks_per_sec, TOL);

        // ASSERT_NEAR((80000) / 1000000.0,
        //             scopes["D"]->m_Elapsed / ticks_per_sec, TOL);

    }
    dmProfile::Finalize();

    dmMutex::Delete(ctx.m_Mutex);
}

#endif

/*

TEST(dmProfile, Counter1)
{
    dmProfile::Initialize(128, 0, 16);
    {
        for (int i = 0; i < 2; ++i)
        {
            dmProfile::HProfile profile = dmProfile::Begin();
            dmProfile::Release(profile);
            { DM_COUNTER("c1", 1); }
            { DM_COUNTER("c1", 2); }
            { DM_COUNTER("c1", 4); }
            { DM_COUNTER("c2", 123); }

            profile = dmProfile::Begin();
            std::map<std::string, dmProfile::CounterData*> counters;
            dmProfile::IterateCounterData(profile, &counters, ProfileCounterCallback);
            dmProfile::Release(profile);

            ASSERT_EQ(7, counters["c1"]->m_Value);
            ASSERT_EQ(123, counters["c2"]->m_Value);
            ASSERT_EQ(2U, counters.size());
        }
    }

    dmProfile::Finalize();
}

void CounterThread(void* arg)
{
    for (int i = 0; i < 2000; ++i)
    {
        DM_COUNTER("c1", 1);
    }
}

TEST(dmProfile, Counter2)
{
    dmProfile::Initialize(128, 0, 16);

    dmProfile::HProfile profile = dmProfile::Begin();
    dmProfile::Release(profile);
    dmThread::Thread t1 = dmThread::New(CounterThread, 0xf0000, 0, "c1");
    dmThread::Thread t2 = dmThread::New(CounterThread, 0xf0000, 0, "c2");

    dmThread::Join(t1);
    dmThread::Join(t2);

    std::map<std::string, dmProfile::CounterData*> counters;
    profile = dmProfile::Begin();
    dmProfile::IterateCounterData(profile, &counters, ProfileCounterCallback);
    dmProfile::Release(profile);

    ASSERT_EQ(2000 * 2, counters["c1"]->m_Value);
    ASSERT_EQ(1U, counters.size());

    dmProfile::Finalize();
}

void ProfileThread(void* arg)
{
    for (int i = 0; i < 20000; ++i)
    {
        DM_PROFILE("a")
    }
}

TEST(dmProfile, ThreadProfile)
{
    dmProfile::Initialize(128, 1024 * 1024, 16);

    dmProfile::HProfile profile = dmProfile::Begin();
    dmProfile::Release(profile);
    uint64_t start = dmTime::GetTime();
    dmThread::Thread t1 = dmThread::New(ProfileThread, 0xf0000, 0, "p1");
    dmThread::Thread t2 = dmThread::New(ProfileThread, 0xf0000, 0, "p2");
    dmThread::Join(t1);
    dmThread::Join(t2);
    uint64_t end = dmTime::GetTime();

    printf("Elapsed: %f ms\n", (end-start) / 1000.0f);

    std::vector<dmProfile::Sample> samples;
    std::map<std::string, const dmProfile::ScopeData*> scopes;

    profile = dmProfile::Begin();
    dmProfile::IterateSamples(profile, &samples, false, &ProfileSampleCallback);
    dmProfile::IterateScopeData(profile, &scopes, false, &ProfileScopeCallback);
    dmProfile::Release(profile);

    ASSERT_EQ(20000U * 2U, samples.size());
    ASSERT_EQ(20000 * 2U, scopes["X"]->m_Count);

    dmProfile::Finalize();
}

TEST(dmProfile, DynamicScope)
{
    const char* FUNCTION_NAMES[] = {
        "FirstFunction",
        "SecondFunction",
        "ThirdFunction"
    };

    const char* SCOPE_NAMES[] = {
        "Scope1",
        "Scope2"
    };

    dmProfile::Initialize(128, 1024 * 1024, 16);

    dmProfile::HProfile profile = dmProfile::Begin();
    dmProfile::Release(profile);

    char names[3][128];
    dmSnPrintf(names[0], sizeof(names[0]), "%s@%s", "test.script", FUNCTION_NAMES[0]);
    dmSnPrintf(names[1], sizeof(names[1]), "%s@%s", "test.script", FUNCTION_NAMES[1]);
    dmSnPrintf(names[2], sizeof(names[2]), "%s@%s", "test.script", FUNCTION_NAMES[2]);
    uint32_t names_hash[3] = {
        dmProfile::GetNameHash(names[0], strlen(names[0])),
        dmProfile::GetNameHash(names[1], strlen(names[1])),
        dmProfile::GetNameHash(names[2], strlen(names[2]))
    };

    for (uint32_t i = 0; i < 10 ; ++i)
    {
        {
            DM_PROFILE_DYN(Scope1, names[0]);
            DM_PROFILE_DYN(Scope2, names[1]);
        }
        {
            DM_PROFILE_DYN(Scope2, names[2]);
        }
        DM_PROFILE_DYN(Scope1, names[0]);
    }

    std::vector<dmProfile::Sample> samples;
    std::map<std::string, const dmProfile::ScopeData*> scopes;

    profile = dmProfile::Begin();
    dmProfile::IterateSamples(profile, &samples, false ,&ProfileSampleCallback);
    dmProfile::IterateScopeData(profile, &scopes, false ,&ProfileScopeCallback);

    ASSERT_EQ(10U * 4U, samples.size());
    ASSERT_EQ(10U * 2, scopes[SCOPE_NAMES[0]]->m_Count);
    ASSERT_EQ(10U * 2, scopes[SCOPE_NAMES[1]]->m_Count);

    char name0[128];
    dmSnPrintf(name0, sizeof(name0), "%s@%s", "test.script", FUNCTION_NAMES[0]);

    char name1[128];
    dmSnPrintf(name1, sizeof(name1), "%s@%s", "test.script", FUNCTION_NAMES[1]);

    char name2[128];
    dmSnPrintf(name2, sizeof(name2), "%s@%s", "test.script", FUNCTION_NAMES[2]);

    for (size_t i = 0; i < samples.size(); i++)
    {
        dmProfile::Sample* sample = &samples[i];
        if (sample->m_Scope->m_NameHash == dmProfile::GetNameHash("Scope1", (uint32_t)strlen("Scope1")))
        {
            ASSERT_STREQ(sample->m_Name, name0);
        }
        else if (sample->m_Scope->m_NameHash == dmProfile::GetNameHash("Scope2", (uint32_t)strlen("Scope2")))
        {
            if (sample->m_NameHash == dmProfile::GetNameHash(name1, (uint32_t)strlen(name1)))
            {
                ASSERT_STREQ(sample->m_Name, name1);
            }
            else if (sample->m_NameHash == dmProfile::GetNameHash(name2, (uint32_t)strlen(name2)))
            {
                ASSERT_STREQ(sample->m_Name, name2);
            }
            else
            {
                ASSERT_TRUE(false);
            }
        }
        else
        {
            ASSERT_TRUE(false);
        }
    }

    dmProfile::Release(profile);

    dmProfile::Finalize();
}
*/


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
