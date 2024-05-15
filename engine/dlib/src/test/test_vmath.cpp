// Copyright 2020-2024 The Defold Foundation
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
#include "dlib/vmath.h"

const float epsilon = 0.0001f;

using namespace dmVMath;

TEST(dmVMath, QuatFromAngle)
{
    Quat q;
    Quat q1 = Quat::rotation(1, Vector3::xAxis());

    q = dmVMath::QuatFromAngle(0, 1);
    ASSERT_NEAR(0, dot(q - q1, q - q1), epsilon);

}

TEST(dmMath, TestQuatToEuler)
{
    const float epsilon = 0.015f;
    Vector3 euler;

    // Identity rotation
    euler = dmVMath::QuatToEuler(0.0f, 0.0f, 0.0f, 1.0f);
    ASSERT_EQ(0.0f, euler.getX());
    ASSERT_EQ(0.0f, euler.getY());
    ASSERT_EQ(0.0f, euler.getZ());

    // different degrees around different single axis
    const float half_rad_factor = (float) (M_PI / 360.0);
    for (uint32_t i = 0; i < 3; ++i)
    {
        for (float a = -355.0f; a < 360.0f; a += 5.0f)
        {
            float sa2 = sin(a * half_rad_factor);
            float ca2 = cos(a * half_rad_factor);
            float v[] = {0.0f, 0.0f, 0.0f};
            v[i] = sa2;
            euler = dmVMath::QuatToEuler(v[0], v[1], v[2], ca2);
            float expected_i = a;
            float expected_i_1 = 0.0f;
            float expected_i_n_1 = 0.0f;
            ASSERT_NEAR(expected_i, euler.getElem(i), epsilon);
            ASSERT_NEAR(expected_i_1, euler.getElem((i + 1) % 3), epsilon);
            ASSERT_NEAR(expected_i_n_1, euler.getElem((i + 2) % 3), epsilon);
            Quat exp_q = dmVMath::EulerToQuat(euler);
            ASSERT_NEAR(exp_q.getX(), v[0], epsilon);
            ASSERT_NEAR(exp_q.getY(), v[1], epsilon);
            ASSERT_NEAR(exp_q.getZ(), v[2], epsilon);
            ASSERT_NEAR(exp_q.getW(), ca2, epsilon);
        }
    }

    // rotation sequence consistency
    Matrix3 ref_mat;
    ref_mat = Matrix3::identity();
    Vector3 expected(90.0f, 22.5f, 45.0f);
    ref_mat *= Matrix3::rotation(expected.getY() * M_PI / 180.0f, Vector3(0.0f, 1.0f, 0.0f));
    ref_mat *= Matrix3::rotation(expected.getZ() * M_PI / 180.0f, Vector3(0.0f, 0.0f, 1.0f));
    ref_mat *= Matrix3::rotation(expected.getX() * M_PI / 180.0f, Vector3(1.0f, 0.0f, 0.0f));
    Quat q(ref_mat);
    q = normalize(q);
    euler = dmVMath::QuatToEuler(q.getX(), q.getY(), q.getZ(), q.getW());
    ASSERT_NEAR(expected.getX(), euler.getX(), epsilon);
    ASSERT_NEAR(expected.getY(), euler.getY(), epsilon);
    ASSERT_NEAR(expected.getZ(), euler.getZ(), epsilon);
}

TEST(dmMath, TestEulerToQuat)
{
    const float epsilon = 0.015f;

    // Identity rotation
    Vector3 zero(0.0f, 0.0f, 0.0f);
    Quat id = dmVMath::EulerToQuat(zero);
    ASSERT_EQ(0.0f, id.getX());
    ASSERT_EQ(0.0f, id.getY());
    ASSERT_EQ(0.0f, id.getZ());
    ASSERT_EQ(1.0f, id.getW());

    // different degrees around different single axis
    const float half_rad_factor = (float) (M_PI / 360.0);
    for (uint32_t i = 0; i < 3; ++i)
    {
        for (float a = -355.f; a < 360.0f; a += 5.0f)
        {
            Vector3 v(0.0f, 0.0f, 0.0f);
            v.setElem(i, a);
            Quat q = dmVMath::EulerToQuat(v);
            Quat expected(0.0f, 0.0f, 0.0f, cos(a * half_rad_factor));
            expected.setElem(i, sin(a * half_rad_factor));
            ASSERT_NEAR(expected.getX(), q.getX(), epsilon);
            ASSERT_NEAR(expected.getY(), q.getY(), epsilon);
            ASSERT_NEAR(expected.getZ(), q.getZ(), epsilon);
            ASSERT_NEAR(expected.getW(), q.getW(), epsilon);
            Vector3 exp_euler = dmVMath::QuatToEuler(q.getX(), q.getY(), q.getZ(), q.getW());
            ASSERT_NEAR(exp_euler.getX(), v[0], epsilon);
            ASSERT_NEAR(exp_euler.getY(), v[1], epsilon);
            ASSERT_NEAR(exp_euler.getZ(), v[2], epsilon);
        }
    }

    // rotation sequence consistency
    Matrix3 ref_mat;
    ref_mat = Matrix3::identity();
    Vector3 expected(90.0f, 22.5f, 45.0f);
    ref_mat *= Matrix3::rotation(expected.getY() * M_PI / 180.0f, Vector3(0.0f, 1.0f, 0.0f));
    ref_mat *= Matrix3::rotation(expected.getZ() * M_PI / 180.0f, Vector3(0.0f, 0.0f, 1.0f));
    ref_mat *= Matrix3::rotation(expected.getX() * M_PI / 180.0f, Vector3(1.0f, 0.0f, 0.0f));
    Quat q(ref_mat);
    Quat quat = dmVMath::EulerToQuat(expected);
    ASSERT_NEAR(quat.getX(), q.getX(), epsilon);
    ASSERT_NEAR(quat.getY(), q.getY(), epsilon);
    ASSERT_NEAR(quat.getZ(), q.getZ(), epsilon);
    ASSERT_NEAR(quat.getW(), q.getW(), epsilon);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
