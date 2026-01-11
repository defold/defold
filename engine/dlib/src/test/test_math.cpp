// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "dlib/math.h"

TEST(dmMath, ConstantsDefined)
{
    ASSERT_NEAR(0.0f, cosf((float) M_PI_2), 0.0000001f);
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

TEST(dmMath, Abs)
{
    ASSERT_EQ(0.0f, dmMath::Abs(0.0f));
    ASSERT_EQ(1.0f, dmMath::Abs(1.0f));
    ASSERT_EQ(1.0f, dmMath::Abs(-1.0f));
}

// out is [min, max, mean]
void TestRand(float (*rand)(uint32_t* seed), uint32_t* seed, float* out)
{
    uint32_t iterations = 1000000;
    float min = 1.0f;
    float max = -1.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < iterations; ++i)
    {
        float r = rand(seed);
        if (min > r)
            min = r;
        if (max < r)
            max = r;
        sum += r;
    }
    out[0] = min;
    out[1] = max;
    out[2] = sum / iterations;
}

TEST(dmMath, Rand)
{
    uint32_t seed = 0;

    float out[3];
    float epsilon = 0.0001f;

    TestRand(dmMath::Rand01, &seed, out);
    ASSERT_NEAR(0.0f, out[0], epsilon);
    ASSERT_NEAR(1.0f, out[1], epsilon);
    ASSERT_NEAR(0.5f, out[2], epsilon * 10);

    TestRand(dmMath::RandOpen01, &seed, out);
    ASSERT_NEAR(0.0f, out[0], epsilon);
    ASSERT_NEAR(1.0f, out[1], epsilon);
    ASSERT_GT(1.0f, out[1]);
    ASSERT_NEAR(0.5f, out[2], epsilon * 10);

    TestRand(dmMath::Rand11, &seed, out);
    ASSERT_NEAR(-1.0f, out[0], epsilon);
    ASSERT_NEAR(1.0f, out[1], epsilon);
    ASSERT_NEAR(0.0f, out[2], epsilon * 20);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
