#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/math.h"

TEST(dmMath, MinMax)
{
    ASSERT_EQ(1, dmMath::Min(1,2));
    ASSERT_EQ(1, dmMath::Min(2,1));
    ASSERT_EQ(2, dmMath::Max(1,2));
    ASSERT_EQ(2, dmMath::Max(2,1));
}

TEST(dmMath, Clamp)
{
    ASSERT_EQ(1, dmMath::Clamp(1, 0, 2));
    ASSERT_EQ(0, dmMath::Clamp(-1, 0, 2));
    ASSERT_EQ(2, dmMath::Clamp(3, 0, 2));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
