#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/time.h"

TEST(Time, dmSleep)
{
    dmSleep(1);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

