#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/time.h"

TEST(dmTime, Sleep)
{
    dmTime::Sleep(1);
}

TEST(dmTime, GetTime)
{
    uint64_t start = dmTime::GetTime();
    dmTime::Sleep(200000);
    uint64_t end = dmTime::GetTime();
    ASSERT_NEAR(200000, end-start, 20000);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

