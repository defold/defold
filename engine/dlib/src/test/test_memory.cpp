#include <stdint.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include "../dlib/memory.h"

TEST(dmMemory, AlignedMalloc)
{
    void* dummy = 0;

    ASSERT_EQ(dmMemory::RESULT_OK, dmMemory::AlignedMalloc(&dummy, 8, 1024));
    ASSERT_EQ(0, ((unsigned long)dummy % 8));
    dmMemory::AlignedFree(dummy);
    dummy = 0;

    ASSERT_EQ(dmMemory::RESULT_OK, dmMemory::AlignedMalloc(&dummy, 16, 1024));
    ASSERT_EQ(0, ((unsigned long)dummy % 16));
    dmMemory::AlignedFree(dummy);
    dummy = 0;

    ASSERT_EQ(dmMemory::RESULT_OK, dmMemory::AlignedMalloc(&dummy, 1024*16, 1024));
    ASSERT_EQ(0, ((unsigned long)dummy % 1024*16));
    dmMemory::AlignedFree(dummy);
    dummy = 0;

    // Trying to allocate with non-power-of-2 alignment should fail with RESULT_INVAL.
    ASSERT_EQ(dmMemory::RESULT_INVAL, dmMemory::AlignedMalloc(&dummy, 7, 1024));
    dmMemory::AlignedFree(dummy);
    dummy = 0;
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
