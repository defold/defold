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
    dmPhysics::HWorld world = dmPhysics::NewWorld(Point3(-1000, -1000, -1000), Point3(1000, 1000, 1000), &SetVisualObjectState, 0);

    dmPhysics::HCollisionShape shape = dmPhysics::NewBoxShape(Vector3(1,1,1));
    dmPhysics::HRigidBody rb = dmPhysics::NewRigidBody(world, shape, 0, Quat::identity(), Point3(0,10,0), 1);

    dmPhysics::DeleteRigidBody(world, rb);
    dmPhysics::DeleteCollisionShape(shape);

    dmPhysics::DeleteWorld(world);
}

TEST(physics, GroundBoxCollision)
{
    dmPhysics::HWorld world = dmPhysics::NewWorld(Point3(-1000, -1000, -1000), Point3(1000, 1000, 1000), &SetVisualObjectState, 0);

    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    VisualObject ground_visual_object;
    dmPhysics::HCollisionShape ground_shape = dmPhysics::NewBoxShape(Vector3(100, ground_height_half_ext, 100));
    dmPhysics::HRigidBody ground_rb = dmPhysics::NewRigidBody(world, ground_shape, &ground_visual_object, Quat::identity(), Point3(0,0,0), 0.0f);

    VisualObject box_visual_object;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HRigidBody box_rb = dmPhysics::NewRigidBody(world, box_shape, &box_visual_object, Quat::identity(), Point3(0, 10.0f, 0), 1.0f);

    for (int i = 0; i < 200; ++i)
    {
        dmPhysics::StepWorld(world, 1.0f / 60.0f);
    }

//    Matrix4 m = dmPhysics::GetTransform(world, box_rb);
 //   Vector4 t = m.getCol(3);

    ASSERT_NEAR(ground_height_half_ext + box_half_ext, box_visual_object.m_Position.getY(), 0.01f);

    dmPhysics::DeleteRigidBody(world, ground_rb);
    dmPhysics::DeleteRigidBody(world, box_rb);
    dmPhysics::DeleteCollisionShape(ground_shape);
    dmPhysics::DeleteCollisionShape(box_shape);

    dmPhysics::DeleteWorld(world);
}

TEST(physics, ApplyForce)
{
    dmPhysics::HWorld world = dmPhysics::NewWorld(Point3(-1000, -1000, -1000), Point3(1000, 1000, 1000), &SetVisualObjectState, 0);
    float box_half_ext = 0.5f;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HRigidBody box_rb = dmPhysics::NewRigidBody(world, box_shape, 0x0, Quat::identity(), Point3(0, 10.0f, 0), 1.0f);
    Vector3 force(1.0f, 0.0f, 0.0f);
    dmPhysics::ApplyForce(box_rb, force, Vector3(0.0f, 0.0f, 0.0f));
    Vector3 total_force = dmPhysics::GetTotalForce(box_rb);
    ASSERT_NEAR(total_force.getX(), force.getX(), 0.01f);
    ASSERT_NEAR(total_force.getY(), force.getY(), 0.01f);
    ASSERT_NEAR(total_force.getZ(), force.getZ(), 0.01f);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
