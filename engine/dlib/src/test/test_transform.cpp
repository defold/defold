#include <gtest/gtest.h>
#include "dlib/transform.h"

using namespace Vectormath::Aos;
using namespace dmTransform;

#define EPSILON 0.001f
#define ASSERT_V3_NEAR(expected, actual)\
    ASSERT_NEAR(expected.getX(), actual.getX(), EPSILON);\
    ASSERT_NEAR(expected.getY(), actual.getY(), EPSILON);\
    ASSERT_NEAR(expected.getZ(), actual.getZ(), EPSILON);
#define ASSERT_V4_NEAR(expected, actual)\
    ASSERT_NEAR(expected.getX(), actual.getX(), EPSILON);\
    ASSERT_NEAR(expected.getY(), actual.getY(), EPSILON);\
    ASSERT_NEAR(expected.getZ(), actual.getZ(), EPSILON);\
    ASSERT_NEAR(expected.getW(), actual.getW(), EPSILON);
#define ASSERT_TRANSFORMS1_NEAR(expected, actual)\
    ASSERT_V3_NEAR(expected.GetTranslation(), actual.GetTranslation());\
    ASSERT_V4_NEAR(expected.GetRotation(), actual.GetRotation());\
    ASSERT_NEAR(expected.GetScale(), actual.GetScale(), EPSILON);

TEST(dmTransform, Apply)
{
    TransformS1 t(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ(M_PI_2),
            2.0f);

    ASSERT_V3_NEAR(Vector3(0.0f, 2.0f, 0.0f), dmTransform::Apply(t, Vector3(1.0f, 0.0f, 0.0f)));
    ASSERT_V3_NEAR(Point3(1.0f, 2.0f, 0.0f), dmTransform::Apply(t, Point3(1.0f, 0.0f, 0.0f)));
}

TEST(dmTransform, ApplyNoScaleZ)
{
    TransformS1 t(Vector3(0.0f, 0.0f, 0.0f),
            Quat::identity(),
            2.0f);

    ASSERT_V3_NEAR(Vector3(2.0f, 0.0f, 1.0f), dmTransform::ApplyNoScaleZ(t, Vector3(1.0f, 0.0f, 1.0f)));
    ASSERT_V3_NEAR(Point3(2.0f, 0.0f, 1.0f), dmTransform::ApplyNoScaleZ(t, Point3(1.0f, 0.0f, 1.0f)));
}

TEST(dmTransform, Multiply)
{
    TransformS1 t0(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ(M_PI_2),
            2.0f);

    TransformS1 res = Mul(t0, t0);
    ASSERT_V3_NEAR(Vector3(1.0f, 2.0f, 0.0f), res.GetTranslation());
    ASSERT_V4_NEAR(Quat::rotationZ(M_PI), res.GetRotation());
    ASSERT_EQ(4.0f, res.GetScale());
}

TEST(dmTransform, MultiplyNoScaleZ)
{
    TransformS1 t0(Vector3(0.0f, 0.0f, 1.0f),
            Quat::identity(),
            2.0f);

    TransformS1 res = Mul(t0, t0);
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 3.0f), res.GetTranslation());
    res = MulNoScaleZ(t0, t0);
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 2.0f), res.GetTranslation());
}

TEST(dmTransform, Inverse)
{
    TransformS1 i;
    i.SetIdentity();

    TransformS1 t0(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ(M_PI_2),
            2.0f);

    // Identity inverse is identity
    ASSERT_TRANSFORMS1_NEAR(i, Inv(i));

    // Inverted inverse is original
    ASSERT_TRANSFORMS1_NEAR(t0, Inv(Inv(t0)));

    // Inverted product is identity
    ASSERT_TRANSFORMS1_NEAR(i, Mul(t0, Inv(t0)));
    ASSERT_TRANSFORMS1_NEAR(i, Mul(Inv(t0), t0));
}

#undef EPSILON
#undef ASSERT_V3_NEAR
#undef ASSERT_V4_NEAR
#undef ASSERT_TRANSFORMS1_NEAR

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
