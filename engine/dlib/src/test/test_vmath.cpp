#include <gtest/gtest.h>
#include "dlib/vmath.h"

const float epsilon = 0.0001f;

using namespace Vectormath::Aos;

TEST(dmVMath, QuatFromAngle)
{
    Quat q;
    Quat q1 = Quat::rotation(1, Vector3::xAxis());

    q = dmVMath::QuatFromAngle(0, 1);
    ASSERT_NEAR(0, dot(q - q1, q - q1), epsilon);

}
#include <stdio.h>
TEST(dmMath, TestQuatToEuler)
{
    float epsilon = 0.02f;

    Vector3 euler;

    float rad_factor = M_PI / 360.0f; // contains half angle factor too
    // different degrees around different single axis
    for (uint32_t i = 0; i < 3; ++i)
    {
        for (float a = 0.0f; a < 105.0f; a += 10.0f)
        {
            float ap = dmMath::Min(a, 90.0f);
            float sa2 = sin(ap * rad_factor);
            float ca2 = cos(ap * rad_factor);
            float v[] = {0.0f, 0.0f, 0.0f};
            v[i] = sa2;
            euler = dmVMath::QuatToEuler(v[0], v[1], v[2], ca2);
            float expected_i = ap;
            float expected_i_1 = 0.0f;
            float expected_i_n_1 = 0.0f;
            if (i == 0 and ap >= 90.0f)
            {
                expected_i_1 = 90.0f;
            }
            ASSERT_NEAR(expected_i, euler.getElem(i), epsilon);
            ASSERT_NEAR(expected_i_1, euler.getElem((i + 1) % 3), epsilon);
            ASSERT_NEAR(expected_i_n_1, euler.getElem((i + 2) % 3), epsilon);
        }
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
