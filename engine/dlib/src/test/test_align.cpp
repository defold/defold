#include <stdint.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include "../dlib/align.h"

struct AlignStruct
{
    char DM_ALIGNED(128) x;
};

char DM_ALIGNED(256) g_Array[] = "foobar";

TEST(dmAlign, Alignment)
{
    ASSERT_EQ(128U, sizeof(AlignStruct));
    ASSERT_EQ(0U, ((uintptr_t) &g_Array[0]) & 255U);
}

TEST(dmAlign, Align)
{
    void* p = (void*) 0xaabb7;

    p = (void*) DM_ALIGN(p, 16);

    ASSERT_EQ(0xaabc0U, (uintptr_t) p);
}

TEST(dmAlign, AlignedMalloc)
{
    void* dummy = 0;
    ASSERT_EQ(dmAlign::RESULT_OK, dmAlign::Malloc(&dummy, 16, 1024));
    ASSERT_EQ(0, ((unsigned long)dummy % 16));
    dmAlign::Free(dummy);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
