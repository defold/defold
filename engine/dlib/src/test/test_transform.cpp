#include <gtest/gtest.h>
#include "dlib/transform.h"
#include "dlib/math.h"

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
#define ASSERT_TRANSFORM_NEAR(expected, actual)\
    ASSERT_V3_NEAR(expected.GetTranslation(), actual.GetTranslation());\
    ASSERT_V4_NEAR(expected.GetRotation(), actual.GetRotation());\
    ASSERT_V3_NEAR(expected.GetScale(), actual.GetScale());

TEST(dmTransform, Apply)
{
    TransformS1 ts1(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ((float) M_PI_2),
            2.0f);

    ASSERT_V3_NEAR(Vector3(0.0f, 2.0f, 0.0f), dmTransform::Apply(ts1, Vector3(1.0f, 0.0f, 0.0f)));
    ASSERT_V3_NEAR(Point3(1.0f, 2.0f, 0.0f), dmTransform::Apply(ts1, Point3(1.0f, 0.0f, 0.0f)));

    Transform t(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ((float) M_PI_2),
            Vector3(2.0f, 2.0f, 2.0f));

    ASSERT_V3_NEAR(Vector3(0.0f, 2.0f, 0.0f), dmTransform::Apply(t, Vector3(1.0f, 0.0f, 0.0f)));
    ASSERT_V3_NEAR(Point3(1.0f, 2.0f, 0.0f), dmTransform::Apply(t, Point3(1.0f, 0.0f, 0.0f)));
}

TEST(dmTransform, ApplyNoScaleZ)
{
    TransformS1 ts1(Vector3(0.0f, 0.0f, 0.0f),
            Quat::identity(),
            2.0f);

    ASSERT_V3_NEAR(Vector3(2.0f, 0.0f, 1.0f), dmTransform::ApplyNoScaleZ(ts1, Vector3(1.0f, 0.0f, 1.0f)));
    ASSERT_V3_NEAR(Point3(2.0f, 0.0f, 1.0f), dmTransform::ApplyNoScaleZ(ts1, Point3(1.0f, 0.0f, 1.0f)));

    Transform t(Vector3(0.0f, 0.0f, 0.0f),
            Quat::identity(),
            Vector3(2.0f, 2.0f, 2.0f));

    ASSERT_V3_NEAR(Vector3(2.0f, 0.0f, 1.0f), dmTransform::ApplyNoScaleZ(t, Vector3(1.0f, 0.0f, 1.0f)));
    ASSERT_V3_NEAR(Point3(2.0f, 0.0f, 1.0f), dmTransform::ApplyNoScaleZ(t, Point3(1.0f, 0.0f, 1.0f)));
}

TEST(dmTransform, Multiply)
{
    TransformS1 ts10(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ((float) M_PI_2),
            2.0f);

    TransformS1 ress1 = Mul(ts10, ts10);
    ASSERT_V3_NEAR(Vector3(1.0f, 2.0f, 0.0f), ress1.GetTranslation());
    ASSERT_V4_NEAR(Quat::rotationZ((float) M_PI), ress1.GetRotation());
    ASSERT_EQ(4.0f, ress1.GetScale());

    Transform t0(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ((float) M_PI_2),
            Vector3(2.0f, 2.0f, 2.0f));

    Transform res = Mul(t0, t0);
    ASSERT_V3_NEAR(Vector3(1.0f, 2.0f, 0.0f), res.GetTranslation());
    ASSERT_V4_NEAR(Quat::rotationZ((float) M_PI), res.GetRotation());
    ASSERT_V3_NEAR(Vector3(4.0f, 4.0f, 4.0f), res.GetScale());
}

TEST(dmTransform, MultiplyNoScaleZ)
{
    TransformS1 ts10(Vector3(0.0f, 0.0f, 1.0f),
            Quat::identity(),
            2.0f);

    TransformS1 ress1 = Mul(ts10, ts10);
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 3.0f), ress1.GetTranslation());
    ress1 = MulNoScaleZ(ts10, ts10);
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 2.0f), ress1.GetTranslation());

    Transform t0(Vector3(0.0f, 0.0f, 1.0f),
            Quat::identity(),
            Vector3(2.0f, 2.0f, 2.0f));

    Transform res = Mul(t0, t0);
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 3.0f), res.GetTranslation());
    res = MulNoScaleZ(t0, t0);
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 2.0f), res.GetTranslation());
}

TEST(dmTransform, MultiplyNoScaleZMatrix)
{
    Transform ts10(Vector3(0.0f, 0.0f, 1.0f),
            Quat::identity(),
            Vector3(2.0f, 2.0f, 2.0f));

    Matrix4 ts10mtx = ToMatrix4(ts10);
    Matrix4 ress1 = ts10mtx * ts10mtx;
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 3.0f), ress1.getCol(3));
    ress1 = MulNoScaleZ(ts10mtx, ts10mtx);
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 2.0f), ress1.getCol(3));

    Transform t0(Vector3(0.0f, 0.0f, 1.0f),
            Quat::identity(),
            Vector3(2.0f, 2.0f, 2.0f));

    Matrix4 t0mtx = ToMatrix4(t0);
    Matrix4 res = t0mtx * t0mtx;
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 3.0f), res.getCol(3));
    res = MulNoScaleZ(t0mtx, t0mtx);
    ASSERT_V3_NEAR(Vector3(0.0f, 0.0f, 2.0f), res.getCol(3));
}

TEST(dmTransform, Conversion)
{
    const int count = 4;
    Vector3 vecs[count] = {
        Vector3(-1, -2, -3),
        Vector3(5, 0.3f, 3),
        Vector3(0.09f, -3, 1),
        Vector3(1, 2, 0.5f)
    };

    // try all the combinations of above vectors as pos, rot and scale
    // and make sure transformation transform->matrix->transform generates
    // the same transfrom
    for (int i=0;i<count;i++) // pos
    {
        for (int j=0;j<count;j++) // rotaxis
        {
            for (int k=0;k<count;k++) // scale
            {
                Vector3 axis = vecs[j] * (1.0f / length(vecs[j]));
                Vector3 scale(dmMath::Abs(vecs[k].getX()), dmMath::Abs(vecs[k].getY()), dmMath::Abs(vecs[k].getZ()));
                Transform t0(vecs[i], normalize(Quat(axis, 1.0f)), scale);

                Matrix4 mtx = ToMatrix4(t0);
                Transform t1 = ToTransform(mtx);

                Vector3 test(-10,20,-30);
                Vector4 res = mtx * test;
                Vector3 res3(res.getX(), res.getY(), res.getZ());
                ASSERT_V3_NEAR(Apply(t0, test), res3);
                ASSERT_V3_NEAR(Apply(t0, test), Apply(t1, test));
            }
        }
    }
}

TEST(dmTransform, Inverse)
{
    TransformS1 is1;
    is1.SetIdentity();

    TransformS1 t0s1(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ((float) M_PI_2),
            2.0f);

    // Identity inverse is identity
    ASSERT_TRANSFORMS1_NEAR(is1, Inv(is1));

    // Inverted inverse is original
    ASSERT_TRANSFORMS1_NEAR(t0s1, Inv(Inv(t0s1)));

    // Inverted product is identity
    ASSERT_TRANSFORMS1_NEAR(is1, Mul(t0s1, Inv(t0s1)));
    ASSERT_TRANSFORMS1_NEAR(is1, Mul(Inv(t0s1), t0s1));

    Transform i;
    i.SetIdentity();

    Transform t0(Vector3(1.0f, 0.0f, 0.0f),
            Quat::rotationZ((float) M_PI_2),
            Vector3(2.0f, 2.0f, 2.0f));

    // Identity inverse is identity
    ASSERT_TRANSFORM_NEAR(i, Inv(i));

    // Inverted inverse is original
    ASSERT_TRANSFORM_NEAR(t0, Inv(Inv(t0)));

    // Inverted product is identity
    ASSERT_TRANSFORM_NEAR(i, Mul(t0, Inv(t0)));
    ASSERT_TRANSFORM_NEAR(i, Mul(Inv(t0), t0));
}

#undef EPSILON
#undef ASSERT_V3_NEAR
#undef ASSERT_V4_NEAR
#undef ASSERT_TRANSFORMS1_NEAR
#undef ASSERT_TRANSFORM_NEAR

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
