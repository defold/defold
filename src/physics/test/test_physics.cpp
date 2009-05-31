#include <stdint.h>
#include <gtest/gtest.h>
#include "../physics.h"

TEST(physics, Simple)
{
    Physics::HWorld world = Physics::NewWorld(Point3(-1000, -1000, -1000), Point3(1000, 1000, 1000));

    Physics::HCollisionShape shape = Physics::NewBoxShape(world, Point3(1,1,1));
    Physics::HRigidBody rb = Physics::NewRigidBody(world, shape, Quat::identity(), Point3(0,10,0), 1);

    Physics::DeleteRigidBody(world, rb);
    Physics::DeleteShape(world, shape);

    Physics::DeleteWorld(world);
}

TEST(physics, GroundBoxCollision)
{
    Physics::HWorld world = Physics::NewWorld(Point3(-1000, -1000, -1000), Point3(1000, 1000, 1000));

    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    Physics::HCollisionShape ground_shape = Physics::NewBoxShape(world, Point3(100, ground_height_half_ext, 100));
    Physics::HRigidBody ground_rb = Physics::NewRigidBody(world, ground_shape, Quat::identity(), Point3(0,0,0), 0.0f);

    Physics::HCollisionShape box_shape = Physics::NewBoxShape(world, Point3(box_half_ext, box_half_ext, box_half_ext));
    Physics::HRigidBody box_rb = Physics::NewRigidBody(world, box_shape, Quat::identity(), Point3(0, 10.0f, 0), 1.0f);

    for (int i = 0; i < 200; ++i)
    {
        Physics::StepWorld(world, 1.0f / 60.0f);
    }

    Matrix4 m = Physics::GetTransform(world, box_rb);
    Vector4 t = m.getCol(3);

    ASSERT_NEAR(ground_height_half_ext + box_half_ext, t.getY(), 0.01f);

    Physics::DeleteRigidBody(world, ground_rb);
    Physics::DeleteRigidBody(world, box_rb);
    Physics::DeleteShape(world, ground_shape);
    Physics::DeleteShape(world, box_shape);

    Physics::DeleteWorld(world);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
