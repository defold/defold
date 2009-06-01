#include <stdint.h>
#include <gtest/gtest.h>
#include "../physics.h"

struct VisualObject
{
    Quat   m_Orientation;
    Point3 m_Position;
};

void SetVisualObjectState(void* context, void* visual_object, const Quat& orientation, const Point3& position)
{
    if (!visual_object) return;
    VisualObject* o = (VisualObject*) visual_object;
    o->m_Orientation = orientation;
    o->m_Position = position;
}

TEST(physics, Simple)
{
    Physics::HWorld world = Physics::NewWorld(Point3(-1000, -1000, -1000), Point3(1000, 1000, 1000), &SetVisualObjectState, 0);

    Physics::HCollisionShape shape = Physics::NewBoxShape(Vector3(1,1,1));
    Physics::HRigidBody rb = Physics::NewRigidBody(world, shape, 0, Quat::identity(), Point3(0,10,0), 1);

    Physics::DeleteRigidBody(world, rb);
    Physics::DeleteCollisionShape(shape);

    Physics::DeleteWorld(world);
}

TEST(physics, GroundBoxCollision)
{
    Physics::HWorld world = Physics::NewWorld(Point3(-1000, -1000, -1000), Point3(1000, 1000, 1000), &SetVisualObjectState, 0);

    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    VisualObject ground_visual_object;
    Physics::HCollisionShape ground_shape = Physics::NewBoxShape(Vector3(100, ground_height_half_ext, 100));
    Physics::HRigidBody ground_rb = Physics::NewRigidBody(world, ground_shape, &ground_visual_object, Quat::identity(), Point3(0,0,0), 0.0f);

    VisualObject box_visual_object;
    Physics::HCollisionShape box_shape = Physics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    Physics::HRigidBody box_rb = Physics::NewRigidBody(world, box_shape, &box_visual_object, Quat::identity(), Point3(0, 10.0f, 0), 1.0f);

    for (int i = 0; i < 200; ++i)
    {
        Physics::StepWorld(world, 1.0f / 60.0f);
    }

//    Matrix4 m = Physics::GetTransform(world, box_rb);
 //   Vector4 t = m.getCol(3);

    ASSERT_NEAR(ground_height_half_ext + box_half_ext, box_visual_object.m_Position.getY(), 0.01f);

    Physics::DeleteRigidBody(world, ground_rb);
    Physics::DeleteRigidBody(world, box_rb);
    Physics::DeleteCollisionShape(ground_shape);
    Physics::DeleteCollisionShape(box_shape);

    Physics::DeleteWorld(world);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
