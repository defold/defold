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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
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

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
