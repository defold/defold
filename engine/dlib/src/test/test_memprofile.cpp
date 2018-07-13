#include <stdint.h>
#ifdef __linux__
#include <malloc.h>
#endif
#if defined(__EMSCRIPTEN__)
#include <libc/malloc.h>
#endif
#include <gtest/gtest.h>
#include "../dlib/memprofile.h"
#include "../dlib/profile.h"

bool g_MemprofileActive = false;

extern void dmMemProfileInternalData();

void* g_dont_optimize = 0;

#if !(defined(SANITIZE_ADDRESS) || defined(SANITIZE_MEMORY)) // until we can load the dylibs properly

TEST(dmMemProfile, TestMalloc)
{
    // We assume that the memory (actual size) allocated is 1024 <= x <= 1024 + sizeof(size_t)
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);
    void* p = malloc(1024);
    g_dont_optimize = p;
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, 1024);
        ASSERT_LE(stats2.m_TotalActive - stats1.m_TotalActive, 1024 + sizeof(size_t));
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1024);
        ASSERT_LE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1024 + sizeof(size_t));
    }

    free(p);
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1024);
        ASSERT_LE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1024 + sizeof(size_t));
    }
}

TEST(dmMemProfile, TestCalloc)
{
    // We assume that the memory (actual size) allocated is 1024 <= x <= 1024 + sizeof(size_t)
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);
    void* p = calloc(1024, 1);
    g_dont_optimize = p;
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, 1024);
        ASSERT_LE(stats2.m_TotalActive - stats1.m_TotalActive, 1024 + sizeof(size_t));
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1024);
        ASSERT_LE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1024 + sizeof(size_t));
    }

    free(p);
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1024);
        ASSERT_LE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1024 + sizeof(size_t));
    }
}

TEST(dmMemProfile, TestRealloc)
{
    // We assume that the memory (actual size) allocated is 1024 <= x <= 1024 + sizeof(size_t)
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);
    void* p = malloc(1024);
    memset(p, 0xcc, 1024);
    p = realloc(p, 1024);
    uint8_t *p8 = (uint8_t*) p;
    for (int i = 0; i < 1024; ++i)
        ASSERT_EQ((uint8_t) 0xcc, p8[i]);
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(2, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, 1024);
        ASSERT_LE(stats2.m_TotalActive - stats1.m_TotalActive, 1024 + sizeof(size_t));
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 2*1024);
        ASSERT_LE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 2*(1024 + sizeof(size_t)));
    }

    free(p);
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(2, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 2*1024);
        ASSERT_LE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 2*(1024 + sizeof(size_t)));
    }
}

#ifndef _MSC_VER

#if !defined(__MACH__) and !defined(__AVM2__)
TEST(dmMemProfile, TestMemAlign)
{
    // We assume that the memory (actual size) allocated is 1024 <= x <= 1044
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);

    void* p = memalign(16, 1024);
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, 1024);
        ASSERT_LE(stats2.m_TotalActive - stats1.m_TotalActive, 1044);
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1024);
        ASSERT_LE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1044);
    }

    free(p);
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1024);
        ASSERT_LE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1044);
    }
}
#endif


#if !defined(ANDROID)
TEST(dmMemProfile, TestPosixMemAlign)
{
    // We assume that the memory (actual size) allocated is 1024 <= x <= 1044
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);

    void* p;
    posix_memalign(&p, 16, 1024);
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, 1024);
        ASSERT_LE(stats2.m_TotalActive - stats1.m_TotalActive, 1044);
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1024);
        ASSERT_LE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 1044);
    }

    free(p);
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1024);
        ASSERT_LE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 1044);
    }
}
#endif

#endif

TEST(dmMemProfile, TestNewDelete1)
{
    // We assume that the memory (actual size) allocated is sizeof(int) <= x <= 20
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);
    int* p = new int;
    g_dont_optimize = p;
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, (int) sizeof(int));
        ASSERT_LE(stats2.m_TotalActive - stats1.m_TotalActive, 16 + sizeof(size_t));
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, (int) sizeof(int));
        ASSERT_LE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, 16 + sizeof(size_t));
    }

    delete p;
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, (int) sizeof(int));
        ASSERT_LE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, 16 + sizeof(size_t));
    }
}

TEST(dmMemProfile, TestNewDelete2)
{
    // We assume that the memory (actual size) allocated is sizeof(int) <= x <= 12
    // This is by inspection...

    dmMemProfile::Stats stats1, stats2, stats3;
    dmMemProfile::GetStats(&stats1);
    int* p = new int[1];
    g_dont_optimize = p;
    dmMemProfile::GetStats(&stats2);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats2.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_GE(stats2.m_TotalActive - stats1.m_TotalActive, (int) sizeof(int));
        ASSERT_LE(12, stats2.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats2.m_TotalAllocated - stats1.m_TotalAllocated, (int) sizeof(int));
        ASSERT_LE(12, stats2.m_TotalAllocated - stats1.m_TotalAllocated);
    }

    delete[] p;
    dmMemProfile::GetStats(&stats3);

    if (g_MemprofileActive)
    {
        ASSERT_EQ(1, stats3.m_AllocationCount - stats1.m_AllocationCount);
        ASSERT_EQ(0, stats3.m_TotalActive - stats1.m_TotalActive);
        ASSERT_GE(stats3.m_TotalAllocated - stats1.m_TotalAllocated, (int) sizeof(int));
        ASSERT_LE(12, stats3.m_TotalAllocated - stats1.m_TotalAllocated);
    }
}

#ifndef _MSC_VER

#include <vector>

void func2(std::vector<void*>& allocations)
{
    for (int i = 0; i < 8; ++i)
    {
        void*p = malloc(16);
        g_dont_optimize = p;
        allocations.push_back(p);
    }
}

void func1a(std::vector<void*>& allocations)
{
    for (int i = 0; i < 16; ++i)
    {
        void*p = malloc(512);
        g_dont_optimize = p;
        allocations.push_back(p);
        func2(allocations);
    }
}

void func1b(std::vector<void*>& allocations)
{
    for (int i = 0; i < 16; ++i)
    {
        void*p = malloc(256);
        g_dont_optimize = p;
        allocations.push_back(p);
        func2(allocations);
    }
}

TEST(dmMemProfile, TestTrace1)
{
    std::vector<void*> allocations;
    // We don't wan't any allocations in functions above...
    allocations.reserve(1024 * 1024);

    func1a(allocations);
    func1b(allocations);

    for (uint32_t i = 0; i < allocations.size(); ++i)
    {
        free(allocations[i]);
    }
}
#endif

#endif // SANITIZE ADDRESS/MEMORY

int main(int argc, char **argv)
{
    // Memprofile is active if a dummy argument is passed
    // We could use dmMemProfile::IsEnabled but we are testing.
    g_MemprofileActive = argc >= 3;

    dmMemProfile::Initialize();
    dmProfile::Initialize(128, 1024 * 1024, 16);

    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmProfile::Finalize();
    dmMemProfile::Finalize();
    return ret;
}
