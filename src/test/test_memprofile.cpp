#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/memprofile.h"

bool g_MemprofileActive = false;

extern void dmMemProfileInternalData();

TEST(dmMemProfile, TestMalloc)
{
    // We assume that the memory (actual size) allocated is 1024 <= x <= 1028
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);
    void* p = malloc(1024);
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1U, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, 1024U);
        ASSERT_LE(stats2.m_TotalActive - stats1.m_TotalActive, 1028U);
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1024U);
        ASSERT_LE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1028U);
    }

    free(p);
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1U, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0U, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1024U);
        ASSERT_LE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1028U);
    }
}

TEST(dmMemProfile, TestNewDelete1)
{
    // We assume that the memory (actual size) allocated is sizeof(int) <= x <= 20
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);
    int* p = new int;
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1U, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, sizeof(int));
        ASSERT_LE(stats2.m_TotalActive - stats1.m_TotalActive, 20U);
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, sizeof(int));
        ASSERT_LE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 20U);
    }

    delete p;
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1U, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0U, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, sizeof(int));
        ASSERT_LE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 20U);
    }
}


TEST(dmMemProfile, TestNewDelete2)
{
    // We assume that the memory (actual size) allocated is sizeof(int) <= x <= 12
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);
    int* p = new int[1];
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1U, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, sizeof(int));
        ASSERT_LE(12U, stats2.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, sizeof(int));
        ASSERT_LE(12U, stats2.m_TotalAllocated - stats1.m_TotalAllocated);
    }

    delete[] p;
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1U, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0U, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, sizeof(int));
        ASSERT_LE(12U, stats3.m_TotalAllocated - stats1.m_TotalAllocated);
    }
}

int main(int argc, char **argv)
{
    // Memprofile is active if a dummy argument is passed
    // We could use dmMemProfile::IsEnabled but we are testing.
    g_MemprofileActive = argc >= 2;

    dmMemProfile::Initialize();

    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmMemProfile::Finalize();
    return ret;
}

