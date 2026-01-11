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
#include <stdio.h>
#include <math.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
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
        // NOTE: We use 0.0001f as tolerance here. In this case
        // the precision is better than in general.
        ASSERT_NEAR(t * t, dmEasing::GetValue(dmEasing::TYPE_INQUAD, t), 0.0001f);
    }
}

TEST(dmEasing, CurstomCurve)
{
    dmVMath::FloatVector vector_empty(0);
    dmVMath::FloatVector vector_single(1);
    dmVMath::FloatVector vector(64);

    dmEasing::Curve curve(dmEasing::TYPE_FLOAT_VECTOR);

    // sample curve with zero entries
    curve.vector = &vector_empty;
    ASSERT_EQ(0.0f, dmEasing::GetValue(curve, 0.0f));
    ASSERT_EQ(0.0f, dmEasing::GetValue(curve, 0.5f));
    ASSERT_EQ(0.0f, dmEasing::GetValue(curve, 0.9f));
    ASSERT_EQ(0.0f, dmEasing::GetValue(curve, 1.0f));

    // sample curve with only one entry
    curve.vector = &vector_single;
    vector_single.values[0] = 0.7f;
    ASSERT_EQ(0.7f, dmEasing::GetValue(curve,-1.0f));
    ASSERT_EQ(0.7f, dmEasing::GetValue(curve, 0.0f));
    ASSERT_EQ(0.7f, dmEasing::GetValue(curve, 0.5f));
    ASSERT_EQ(0.7f, dmEasing::GetValue(curve, 0.9f));
    ASSERT_EQ(0.7f, dmEasing::GetValue(curve, 1.0f));
    ASSERT_EQ(0.7f, dmEasing::GetValue(curve, 2.0f));

    // INQUAD equivalent
    curve.vector = &vector;
    for (int i = 0; i < 64; ++i) {
        float t = i / 63.0f;
        vector.values[i] = t * t;
    }

    // sample outside interval
    ASSERT_EQ(0.0f, dmEasing::GetValue(curve, -1.0f));
    ASSERT_EQ(1.0f, dmEasing::GetValue(curve,  1.5f));

    // 1-to-1 samples
    for (int i = 0; i <= 63; ++i) {
        float t = i / 63.0f;
        ASSERT_NEAR(t * t, dmEasing::GetValue(curve, t), 0.0001f);
    }

    // fewer tests than sample points
    for (int i = 0; i <= 24; ++i) {
        float t = i / 24.0f;
        ASSERT_NEAR(t * t, dmEasing::GetValue(curve, t), 0.0001f);
    }

    // more tests than sample points
    for (int i = 0; i <= 200; ++i) {
        float t = i / 200.0f;
        ASSERT_NEAR(t * t, dmEasing::GetValue(curve, t), 0.0001f);
    }

    // LINEAR equivalent
    for (int i = 0; i < 64; ++i) {
        float t = i / 63.0f;
        vector.values[i] = t;
    }
    for (int i = 0; i <= 100; ++i) {
        float t = i / 100.0f;
        ASSERT_NEAR(t, dmEasing::GetValue(curve, t), 0.0001f);
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
