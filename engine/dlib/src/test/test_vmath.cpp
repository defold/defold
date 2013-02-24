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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
