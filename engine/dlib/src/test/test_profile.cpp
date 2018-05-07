#include <stdint.h>
#include <vector>
#include <map>
#include <string>
#include <gtest/gtest.h>
#include "dlib/hash.h"
#include "dlib/profile.h"
#include "dlib/time.h"
#include "dlib/thread.h"

#if !defined(_WIN32)

void ProfileSampleCallback(void* context, const dmProfile::Sample* sample)
{
    std::vector<dmProfile::Sample>* samples = (std::vector<dmProfile::Sample>*) context;
    samples->push_back(*sample);
}

void ProfileScopeCallback(void* context, const dmProfile::ScopeData* scope_data)
{
    std::map<std::string, const dmProfile::ScopeData*>* scopes = (std::map<std::string, const dmProfile::ScopeData*>*) context;
    (*scopes)[std::string(scope_data->m_Scope->m_Name)] = scope_data;
}

void ProfileCounterCallback(void* context, const dmProfile::CounterData* counter)
{
    std::map<std::string, const dmProfile::CounterData*>* counters = (std::map<std::string, const dmProfile::CounterData*>*) context;
    (*counters)[std::string(counter->m_Counter->m_Name)] = counter;
}

// TODO
// 100 msec, which is in fact much higher than the expected time of the profiler
// On OSX, the time is usually a few microseconds, but once in a while the time spikes to ~0.5 ms
// On Linux CI, the time can be as high as 16 msec
// The timings (dmTime::BusyWait) is based around dmTime::GetTime, this issue is a revisit to improve the expected granularity: DEF-2013
#define TOL 0.1

TEST(dmProfile, Profile)
{
    dmProfile::Initialize(128, 1024, 0);

    for (int i = 0; i < 2; ++i)
    {
        {
            dmProfile::HProfile profile = dmProfile::Begin();
            dmProfile::Release(profile);
            {
                DM_PROFILE(A, "a")
                dmTime::BusyWait(100000);
                {
                    {
                        DM_PROFILE(B, "a_b1")
                        dmTime::BusyWait(50000);
                        {
                            DM_PROFILE(C, "a_b1_c")
                            dmTime::BusyWait(40000);
                        }
                    }
                    {
                        DM_PROFILE(B, "b2")
                        dmTime::BusyWait(50000);
                        {
                            DM_PROFILE(C, "a_b2_c1")
                            dmTime::BusyWait(40000);
                        }
                        {
                            DM_PROFILE(C, "a_b2_c2")
                            dmTime::BusyWait(60000);
                        }
                    }
                }
            }
            {
                DM_PROFILE(D, "a_d")
                dmTime::BusyWait(80000);
            }
        }

        dmProfile::HProfile profile = dmProfile::Begin();

        std::vector<dmProfile::Sample> samples;
        std::map<std::string, const dmProfile::ScopeData*> scopes;

        dmProfile::IterateSamples(profile, &samples, &ProfileSampleCallback);
        dmProfile::IterateScopeData(profile, &scopes, &ProfileScopeCallback);
        dmProfile::Release(profile);

        ASSERT_EQ(7U, samples.size());

        double ticks_per_sec = dmProfile::GetTicksPerSecond();

        ASSERT_STREQ("a", samples[0].m_Name);
        ASSERT_STREQ("a_b1", samples[1].m_Name);
        ASSERT_STREQ("a_b1_c", samples[2].m_Name);
        ASSERT_STREQ("b2", samples[3].m_Name);
        ASSERT_STREQ("a_b2_c1", samples[4].m_Name);
        ASSERT_STREQ("a_b2_c2", samples[5].m_Name);
        ASSERT_STREQ("a_d", samples[6].m_Name);

        ASSERT_NEAR((100000 + 50000 + 40000 + 50000 + 40000 + 60000) / 1000000.0, samples[0].m_Elapsed / ticks_per_sec, TOL);
        ASSERT_NEAR((50000 + 40000) / 1000000.0, samples[1].m_Elapsed / ticks_per_sec, TOL);
        ASSERT_NEAR((40000) / 1000000.0, samples[2].m_Elapsed / ticks_per_sec, TOL);
        ASSERT_NEAR((50000 + 40000 + 60000) / 1000000.0, samples[3].m_Elapsed / ticks_per_sec, TOL);
        ASSERT_NEAR((40000) / 1000000.0, samples[4].m_Elapsed / ticks_per_sec, TOL);
        ASSERT_NEAR((60000) / 1000000.0, samples[5].m_Elapsed / ticks_per_sec, TOL);
        ASSERT_NEAR((80000) / 1000000.0, samples[6].m_Elapsed / ticks_per_sec, TOL);

        ASSERT_TRUE(scopes.end() != scopes.find("A"));
        ASSERT_TRUE(scopes.end() != scopes.find("B"));
        ASSERT_TRUE(scopes.end() != scopes.find("C"));
        ASSERT_TRUE(scopes.end() != scopes.find("D"));

        ASSERT_NEAR((100000 + 50000 + 40000 + 50000 + 40000 + 60000) / 1000000.0,
                    scopes["A"]->m_Elapsed / ticks_per_sec, TOL);

        ASSERT_NEAR((50000 + 40000 + 50000 + 40000 + 60000) / 1000000.0,
                    scopes["B"]->m_Elapsed / ticks_per_sec, TOL);

        ASSERT_NEAR((40000 + 40000 + 60000) / 1000000.0,
                    scopes["C"]->m_Elapsed / ticks_per_sec, TOL);

        ASSERT_NEAR((80000) / 1000000.0,
                    scopes["D"]->m_Elapsed / ticks_per_sec, TOL);

    }
    dmProfile::Finalize();
}

TEST(dmProfile, Nested)
{
    dmProfile::Initialize(128, 1024, 0);

    for (int i = 0; i < 2; ++i)
    {
        {
            dmProfile::HProfile profile = dmProfile::Begin();
            dmProfile::Release(profile);
            {
                DM_PROFILE(A, "a")
                dmTime::BusyWait(50000);
                {
                    DM_PROFILE(A, "a_nest")
                    dmTime::BusyWait(50000);
                }
            }
        }

        dmProfile::HProfile profile = dmProfile::Begin();

        std::vector<dmProfile::Sample> samples;
        std::map<std::string, const dmProfile::ScopeData*> scopes;

        dmProfile::IterateSamples(profile, &samples, &ProfileSampleCallback);
        dmProfile::IterateScopeData(profile, &scopes, &ProfileScopeCallback);
        dmProfile::Release(profile);

        ASSERT_EQ(2U, samples.size());

        double ticks_per_sec = dmProfile::GetTicksPerSecond();

        ASSERT_STREQ("a", samples[0].m_Name);
        ASSERT_STREQ("a_nest", samples[1].m_Name);

        ASSERT_NEAR((50000 + 50000) / 1000000.0, samples[0].m_Elapsed / ticks_per_sec, TOL);
        ASSERT_NEAR((50000) / 1000000.0, samples[1].m_Elapsed / ticks_per_sec, TOL);

        ASSERT_TRUE(scopes.end() != scopes.find("A"));

        ASSERT_NEAR((100000) / 1000000.0,
                    scopes["A"]->m_Elapsed / ticks_per_sec, TOL);

    }
    dmProfile::Finalize();
}

TEST(dmProfile, ProfileOverflow1)
{
    dmProfile::Initialize(128, 2, 0);
    {
        dmProfile::HProfile profile = dmProfile::Begin();
        dmProfile::Release(profile);
        {
            { DM_PROFILE(X, "a") }
            { DM_PROFILE(X, "b") }
            { DM_PROFILE(X, "c") }
            { DM_PROFILE(X, "d") }
        }
    }
    dmProfile::HProfile profile = dmProfile::Begin();

    std::vector<dmProfile::Sample> samples;
    dmProfile::IterateSamples(profile, &samples, &ProfileSampleCallback);
    dmProfile::Release(profile);

    ASSERT_EQ(2U, samples.size());

    dmProfile::Finalize();
}

TEST(dmProfile, ProfileOverflow2)
{
    dmProfile::Initialize(128, 0, 0);
    {
        dmProfile::HProfile profile = dmProfile::Begin();
        dmProfile::Release(profile);
        {
            { DM_PROFILE(X, "a") }
            { DM_PROFILE(X, "b") }
            { DM_PROFILE(X, "c") }
            { DM_PROFILE(X, "d") }
        }
    }

    dmProfile::Finalize();
}

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

uint32_t g_c1hash = dmHashString32("c1");
void CounterThread(void* arg)
{
    for (int i = 0; i < 2000; ++i)
    {
        DM_COUNTER_HASH("c1", g_c1hash, 1);
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
        DM_PROFILE(X, "a")
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
    dmProfile::IterateSamples(profile, &samples, &ProfileSampleCallback);
    dmProfile::IterateScopeData(profile, &scopes, &ProfileScopeCallback);
    dmProfile::Release(profile);

    ASSERT_EQ(20000U * 2U, samples.size());
    ASSERT_EQ(20000 * 2, scopes["X"]->m_Count);

    dmProfile::Finalize();
}

#else
#endif

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
