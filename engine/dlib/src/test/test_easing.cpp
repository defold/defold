#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <gtest/gtest.h>
#include "../dlib/easing.h"
#include "../dlib/math.h"

TEST(dmEasing, Linear)
{
    const float tol = 0.001f;
    ASSERT_EQ(0.0f, dmEasing::GetValue(dmEasing::TYPE_LINEAR, -1.0f));
    ASSERT_EQ(0.0f, dmEasing::GetValue(dmEasing::TYPE_LINEAR, 0.0f));
    ASSERT_EQ(1.0f, dmEasing::GetValue(dmEasing::TYPE_LINEAR, 1.0f));
    ASSERT_EQ(1.0f, dmEasing::GetValue(dmEasing::TYPE_LINEAR, 1.5f));

    ASSERT_EQ(0.0f, dmEasing::GetValue(dmEasing::TYPE_INOUTBOUNCE, -1.0f));
    ASSERT_EQ(0.0f, dmEasing::GetValue(dmEasing::TYPE_INOUTBOUNCE, 0.0f));
    ASSERT_EQ(1.0f, dmEasing::GetValue(dmEasing::TYPE_INOUTBOUNCE, 1.0f));
    ASSERT_EQ(1.0f, dmEasing::GetValue(dmEasing::TYPE_INOUTBOUNCE, 1.5f));

    ASSERT_NEAR(1.0876975059509277f, dmEasing::GetValue(dmEasing::TYPE_OUTBACK, 0.5f), tol);

    for (int i = 0; i < 100; ++i) {
        float t = i / 100.0f;
        float diff = t * t - dmEasing::GetValue(dmEasing::TYPE_INQUAD, t);
        // NOTE: We use 0.0001f as tolerance here. In this case
        // the precision is better than in general.
        ASSERT_NEAR(t * t, dmEasing::GetValue(dmEasing::TYPE_INQUAD, t), 0.0001f);
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
