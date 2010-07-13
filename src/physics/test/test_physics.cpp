#include <stdint.h>
#include <gtest/gtest.h>
#include "../physics.h"

using namespace Vectormath::Aos;

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
    dmPhysics::HRigidBody rb = dmPhysics::NewRigidBody(world, shape, 0, 1, 0x0);

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
    dmPhysics::HRigidBody ground_rb = dmPhysics::NewRigidBody(world, ground_shape, &ground_visual_object, 0.0f, 0x0);

    VisualObject box_visual_object;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HRigidBody box_rb = dmPhysics::NewRigidBody(world, box_shape, &box_visual_object, 1.0f, 0x0);
    dmPhysics::SetRigidBodyInitialTransform(box_rb, Point3(0, 10.0f, 0), Quat::identity());

    for (int i = 0; i < 200; ++i)
    {
        dmPhysics::StepWorld(world, 1.0f / 60.0f);
    }

    ASSERT_NEAR(ground_height_half_ext + box_half_ext, box_visual_object.m_Position.getY(), 0.01f);

    dmPhysics::DeleteRigidBody(world, ground_rb);
    dmPhysics::DeleteRigidBody(world, box_rb);
    dmPhysics::DeleteCollisionShape(ground_shape);
    dmPhysics::DeleteCollisionShape(box_shape);

    dmPhysics::DeleteWorld(world);
}

void CollisionCallback(void* user_data_a, void* user_data_b, void* user_data)
{
    bool* collision_a = (bool*)user_data_a;
    *collision_a = true;
    bool* collision_b = (bool*)user_data_b;
    *collision_b = true;
    int* count = (int*)user_data;
    *count = *count + 1;
}

void ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
{
    int* count = (int*)user_data;
    *count = *count + 1;
}

TEST(physics, CollisionCallbacks)
{
    dmPhysics::HWorld world = dmPhysics::NewWorld(Point3(-1000, -1000, -1000), Point3(1000, 1000, 1000), &SetVisualObjectState, 0);

    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    VisualObject ground_visual_object;
    dmPhysics::HCollisionShape ground_shape = dmPhysics::NewBoxShape(Vector3(100, ground_height_half_ext, 100));
    bool ground_collision = false;
    dmPhysics::HRigidBody ground_rb = dmPhysics::NewRigidBody(world, ground_shape, &ground_visual_object, 0.0f, &ground_collision);

    VisualObject box_visual_object;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    bool box_collision = false;
    dmPhysics::HRigidBody box_rb = dmPhysics::NewRigidBody(world, box_shape, &box_visual_object, 1.0f, &box_collision);
    dmPhysics::SetRigidBodyInitialTransform(box_rb, Point3(0, 10.0f, 0), Quat::identity());

    int collision_count = 0;
    int contact_point_count = 0;
    for (int i = 0; i < 10; ++i)
    {
        dmPhysics::StepWorld(world, 1.0f / 60.0f);
        dmPhysics::ForEachCollision(world, CollisionCallback, &collision_count, ContactPointCallback, &contact_point_count);
    }
    ASSERT_EQ(0, collision_count);
    ASSERT_EQ(0, contact_point_count);

    contact_point_count = 0;
    for (int i = 0; i < 200; ++i)
    {
        dmPhysics::StepWorld(world, 1.0f / 60.0f);
        dmPhysics::ForEachCollision(world, 0x0, 0x0, ContactPointCallback, &contact_point_count);
    }
    ASSERT_GT(contact_point_count, 20);

    collision_count = 0;
    contact_point_count = 0;
    dmPhysics::ForEachCollision(world, CollisionCallback, &collision_count, ContactPointCallback, &contact_point_count);

    ASSERT_TRUE(ground_collision);
    ASSERT_TRUE(box_collision);

    ASSERT_EQ(1, collision_count);
    ASSERT_EQ(2, contact_point_count);

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
    dmPhysics::HRigidBody box_rb = dmPhysics::NewRigidBody(world, box_shape, 0x0, 1.0f, 0x0);
    dmPhysics::SetRigidBodyInitialTransform(box_rb, Point3(0, 10.0f, 0), Quat::identity());
    Vector3 force(1.0f, 0.0f, 0.0f);
    dmPhysics::ApplyForce(box_rb, force, Point3(0.0f, 0.0f, 0.0f));
    Vector3 total_force = dmPhysics::GetTotalForce(box_rb);
    ASSERT_NEAR(total_force.getX(), force.getX(), 0.01f);
    ASSERT_NEAR(total_force.getY(), force.getY(), 0.01f);
    ASSERT_NEAR(total_force.getZ(), force.getZ(), 0.01f);

    dmPhysics::DeleteRigidBody(world, box_rb);
    dmPhysics::DeleteCollisionShape(box_shape);
    dmPhysics::DeleteWorld(world);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
