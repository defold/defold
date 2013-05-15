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

TEST(dmMath, TestQuatToEuler)
{
    float epsilon = 0.02f;

    Vector3 euler;

    float half_rad_factor = M_PI / 360.0f;
    // different degrees around different single axis
    for (uint32_t i = 0; i < 3; ++i)
    {
        for (float a = 0.0f; a < 105.0f; a += 10.0f)
        {
            float ap = dmMath::Min(a, 90.0f);
            float sa2 = sin(ap * half_rad_factor);
            float ca2 = cos(ap * half_rad_factor);
            float v[] = {0.0f, 0.0f, 0.0f};
            v[i] = sa2;
            euler = dmVMath::QuatToEuler(v[0], v[1], v[2], ca2);
            float expected_i = ap;
            float expected_i_1 = 0.0f;
            float expected_i_n_1 = 0.0f;
            if (i == 0 && ap >= 90.0f)
            {
                expected_i_1 = 90.0f;
            }
            ASSERT_NEAR(expected_i, euler.getElem(i), epsilon);
            ASSERT_NEAR(expected_i_1, euler.getElem((i + 1) % 3), epsilon);
            ASSERT_NEAR(expected_i_n_1, euler.getElem((i + 2) % 3), epsilon);
        }
    }
}

TEST(dmMath, TestEulerToQuat)
{
    float epsilon = 0.001f;

    Vector3 euler;

    float half_rad_factor = M_PI / 360.0f;
    // different degrees around different single axis
    for (uint32_t i = 0; i < 3; ++i)
    {
        for (float a = 0.0f; a < 105.0f; a += 10.0f)
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
        }
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
