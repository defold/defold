#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/math.h"

TEST(dmMath, MinMax)
{
    ASSERT_EQ(1, dmMin(1,2));
    ASSERT_EQ(1, dmMin(2,1));
    ASSERT_EQ(2, dmMax(1,2));
    ASSERT_EQ(2, dmMax(2,1));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
