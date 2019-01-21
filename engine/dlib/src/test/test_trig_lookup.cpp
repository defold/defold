#include "testutil.h"
#include "dlib/trig_lookup.h"
#include "dlib/math.h"

const uint32_t iterations = 5000;
const float min_angle = -3 * M_PI;
const float max_angle = 3 * M_PI;
// Error epsilon
// NOTE If this is changed, the documentation in trig_lookup_template.h also needs to be updated.
const float epsilon = 0.001f;

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
