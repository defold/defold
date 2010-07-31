#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/math.h"

TEST(dmMath, ConstantsDefined)
{
    ASSERT_NEAR(0.0f, cosf(M_PI_2), 0.0000001f);
}

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

TEST(dmMath, Bezier)
{
    float delta = 0.05f;
    float epsilon = 0.00001f;

    ASSERT_EQ(2.0f, dmMath::LinearBezier(0.0f, 2.0f, 4.0f));
    ASSERT_EQ(3.0f, dmMath::LinearBezier(0.5f, 2.0f, 4.0f));
    ASSERT_EQ(4.0f, dmMath::LinearBezier(1.0f, 2.0f, 4.0f));
    ASSERT_NEAR(dmMath::LinearBezier(delta, 2.0f, 4.0f) - dmMath::LinearBezier(0.0f, 2.0f, 4.0f),
            dmMath::LinearBezier(1.0f, 2.0f, 4.0f) - dmMath::LinearBezier(1.0f - delta, 2.0f, 4.0f),
            epsilon);
    ASSERT_NEAR(dmMath::LinearBezier(delta, 2.0f, 4.0f) - dmMath::LinearBezier(0.0f, 2.0f, 4.0f),
            dmMath::LinearBezier(0.5f, 2.0f, 4.0f) - dmMath::LinearBezier(0.5f - delta, 2.0f, 4.0f),
            epsilon);
    ASSERT_NEAR(dmMath::LinearBezier(0.5f + delta, 2.0f, 4.0f) - dmMath::LinearBezier(0.5f, 2.0f, 4.0f),
            dmMath::LinearBezier(1.0f, 2.0f, 4.0f) - dmMath::LinearBezier(1.0f - delta, 2.0f, 4.0f),
            epsilon);

    ASSERT_EQ(2.0f, dmMath::QuadraticBezier(0.0f, 2.0f, 3.0f, 2.0f));
    ASSERT_EQ(2.5f, dmMath::QuadraticBezier(0.5f, 2.0f, 3.0f, 2.0f));
    ASSERT_EQ(2.0f, dmMath::QuadraticBezier(1.0f, 2.0f, 3.0f, 2.0f));
    ASSERT_NEAR(dmMath::QuadraticBezier(delta, 2.0f, 3.0f, 2.0f) - dmMath::QuadraticBezier(0.0f, 2.0f, 3.0f, 2.0f),
            dmMath::QuadraticBezier(1.0f - delta, 2.0f, 3.0f, 2.0f) - dmMath::QuadraticBezier(1.0f, 2.0f, 3.0f, 2.0f),
            epsilon);
    ASSERT_GT(dmMath::QuadraticBezier(delta, 2.0f, 3.0f, 2.0f) - dmMath::QuadraticBezier(0.0f, 2.0f, 3.0f, 2.0f),
            dmMath::QuadraticBezier(0.5f, 2.0f, 3.0f, 2.0f) - dmMath::QuadraticBezier(0.5f - delta, 2.0f, 3.0f, 2.0f));
    ASSERT_LT(dmMath::QuadraticBezier(0.5f, 2.0f, 3.0f, 2.0f) - dmMath::QuadraticBezier(0.5f + delta, 2.0f, 3.0f, 2.0f),
            dmMath::QuadraticBezier(1.0f - delta, 2.0f, 3.0f, 2.0f) - dmMath::QuadraticBezier(1.0f, 2.0f, 3.0f, 2.0f));

    ASSERT_EQ(2.0f, dmMath::CubicBezier(0.0f, 2.0f, 2.0f, 4.0f, 4.0f));
    ASSERT_EQ(3.0f, dmMath::CubicBezier(0.5f, 2.0f, 2.0f, 4.0f, 4.0f));
    ASSERT_EQ(4.0f, dmMath::CubicBezier(1.0f, 2.0f, 2.0f, 4.0f, 4.0f));
    ASSERT_NEAR(dmMath::CubicBezier(delta, 2.0f, 2.0f, 4.0f, 4.0f) - dmMath::CubicBezier(0.0f, 2.0f, 2.0f, 4.0f, 4.0f),
            dmMath::CubicBezier(1.0f, 2.0f, 2.0f, 4.0f, 4.0f) - dmMath::CubicBezier(1.0f - delta, 2.0f, 2.0f, 4.0f, 4.0f),
            epsilon);
    ASSERT_LT(dmMath::CubicBezier(delta, 2.0f, 2.0f, 4.0f, 4.0f) - dmMath::CubicBezier(0.0f, 2.0f, 2.0f, 4.0f, 4.0f),
            dmMath::CubicBezier(0.5f, 2.0f, 2.0f, 4.0f, 4.0f) - dmMath::CubicBezier(0.5f - delta, 2.0f, 2.0f, 4.0f, 4.0f));
    ASSERT_GT(dmMath::CubicBezier(0.5f + delta, 2.0f, 2.0f, 4.0f, 4.0f) - dmMath::CubicBezier(0.5f, 2.0f, 2.0f, 4.0f, 4.0f),
            dmMath::CubicBezier(1.0f, 2.0f, 2.0f, 4.0f, 4.0f) - dmMath::CubicBezier(1.0f - delta, 2.0f, 2.0f, 4.0f, 4.0f));
}

TEST(dmMath, Select)
{
    float a = 1.0f;
    float b = 2.0f;
    ASSERT_EQ(a, dmMath::Select(0.0f, a, b));
    ASSERT_EQ(a, dmMath::Select(1.0f, a, b));
    ASSERT_EQ(b, dmMath::Select(-1.0f, a, b));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
