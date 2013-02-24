#include <gtest/gtest.h>
#include "dlib/trig_lookup.h"
#include "dlib/math.h"

const float epsilon = 0.0001f;
const uint32_t iterations = 1000;
const float min_angle = -M_PI;
const float max_angle = 3 * M_PI;

TEST(dmTrigLookup, Sin)
{
    for (uint32_t i = 0; i < iterations; ++i)
    {
        float t = i / (float)iterations;
        float radians = (t - 1) * min_angle + t * max_angle;
        ASSERT_NEAR(sinf(radians), dmTrigLookup::Sin(radians), epsilon);
    }
}

TEST(dmTrigLookup, Cos)
{
    for (uint32_t i = 0; i < iterations; ++i)
    {
        float t = i / (float)iterations;
        float radians = (1-t) * min_angle + t * max_angle;
        ASSERT_NEAR(cosf(radians), dmTrigLookup::Cos(radians), epsilon);
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
