#include <stdint.h>
#include <gtest/gtest.h>
#include "../physics.h"

using namespace Vectormath::Aos;

struct VisualObject
{
    Point3 m_Position;
    Quat   m_Rotation;
};

void GetWorldTransform(void* visual_object, void* callback_context, Vectormath::Aos::Point3& position, Vectormath::Aos::Quat& rotation)
{

}

void SetWorldTransform(void* visual_object, void* callback_context, const Vectormath::Aos::Point3& position, const Vectormath::Aos::Quat& rotation)
{
    if (!visual_object) return;
    VisualObject* o = (VisualObject*) visual_object;
    o->m_Position = position;
    o->m_Rotation = rotation;
}

class PhysicsTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_World = dmPhysics::NewWorld(Point3(-1000.0f, -1000.0f, -1000.0f), Point3(1000.0f, 1000.0f, 1000.0f), &GetWorldTransform, &SetWorldTransform, 0);
    }

    virtual void TearDown()
    {
        dmPhysics::DeleteWorld(m_World);
    }

    dmPhysics::HWorld m_World;
};

TEST_F(PhysicsTest, Simple)
{
    dmPhysics::HCollisionShape shape = dmPhysics::NewBoxShape(Vector3(1.0f, 1.0f, 1.0f));
    dmPhysics::HRigidBody rb = dmPhysics::NewRigidBody(m_World, shape, 0, 1.0f, false, 0x0);

    dmPhysics::DeleteRigidBody(m_World, rb);
    dmPhysics::DeleteCollisionShape(shape);
}

TEST_F(PhysicsTest, Kinematic)
{
    dmPhysics::HCollisionShape shape = dmPhysics::NewBoxShape(Vector3(1.0f, 1.0f, 1.0f));
    dmPhysics::HRigidBody rb = dmPhysics::NewRigidBody(m_World, shape, 0, 1.0f, true, 0x0);

    ASSERT_EQ((void*)0, (void*)rb);

    dmPhysics::DeleteRigidBody(m_World, rb);

    rb = dmPhysics::NewRigidBody(m_World, shape, 0, 0.0f, true, 0x0);

    ASSERT_NE((void*)0, (void*)rb);

    dmPhysics::DeleteRigidBody(m_World, rb);

    dmPhysics::DeleteCollisionShape(shape);
}

TEST_F(PhysicsTest, GroundBoxCollision)
{
    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    VisualObject ground_visual_object;
    dmPhysics::HCollisionShape ground_shape = dmPhysics::NewBoxShape(Vector3(100, ground_height_half_ext, 100));
    dmPhysics::HRigidBody ground_rb = dmPhysics::NewRigidBody(m_World, ground_shape, &ground_visual_object, 0.0f, false, 0x0);

    VisualObject box_visual_object;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HRigidBody box_rb = dmPhysics::NewRigidBody(m_World, box_shape, &box_visual_object, 1.0f, false, 0x0);
    dmPhysics::SetRigidBodyInitialTransform(box_rb, Point3(0, 10.0f, 0), Quat::identity());

    for (int i = 0; i < 200; ++i)
    {
        dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    }

    ASSERT_NEAR(ground_height_half_ext + box_half_ext, box_visual_object.m_Position.getY(), 0.01f);

    dmPhysics::DeleteRigidBody(m_World, ground_rb);
    dmPhysics::DeleteRigidBody(m_World, box_rb);
    dmPhysics::DeleteCollisionShape(ground_shape);
    dmPhysics::DeleteCollisionShape(box_shape);
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

TEST_F(PhysicsTest, CollisionCallbacks)
{
    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    VisualObject ground_visual_object;
    dmPhysics::HCollisionShape ground_shape = dmPhysics::NewBoxShape(Vector3(100, ground_height_half_ext, 100));
    bool ground_collision = false;
    dmPhysics::HRigidBody ground_rb = dmPhysics::NewRigidBody(m_World, ground_shape, &ground_visual_object, 0.0f, false, &ground_collision);

    VisualObject box_visual_object;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    bool box_collision = false;
    dmPhysics::HRigidBody box_rb = dmPhysics::NewRigidBody(m_World, box_shape, &box_visual_object, 1.0f, false, &box_collision);
    dmPhysics::SetRigidBodyInitialTransform(box_rb, Point3(0, 10.0f, 0), Quat::identity());

    int collision_count = 0;
    int contact_point_count = 0;
    for (int i = 0; i < 10; ++i)
    {
        dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
        dmPhysics::ForEachCollision(m_World, CollisionCallback, &collision_count, ContactPointCallback, &contact_point_count);
    }
    ASSERT_EQ(0, collision_count);
    ASSERT_EQ(0, contact_point_count);

    contact_point_count = 0;
    for (int i = 0; i < 200; ++i)
    {
        dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
        dmPhysics::ForEachCollision(m_World, 0x0, 0x0, ContactPointCallback, &contact_point_count);
    }
    ASSERT_GT(contact_point_count, 20);

    collision_count = 0;
    contact_point_count = 0;
    dmPhysics::ForEachCollision(m_World, CollisionCallback, &collision_count, ContactPointCallback, &contact_point_count);

    ASSERT_TRUE(ground_collision);
    ASSERT_TRUE(box_collision);

    ASSERT_EQ(1, collision_count);
    ASSERT_EQ(2, contact_point_count);

    dmPhysics::DeleteRigidBody(m_World, ground_rb);
    dmPhysics::DeleteRigidBody(m_World, box_rb);
    dmPhysics::DeleteCollisionShape(ground_shape);
    dmPhysics::DeleteCollisionShape(box_shape);
}

TEST_F(PhysicsTest, ApplyForce)
{
    float box_half_ext = 0.5f;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HRigidBody box_rb = dmPhysics::NewRigidBody(m_World, box_shape, 0x0, 1.0f, false, 0x0);
    dmPhysics::SetRigidBodyInitialTransform(box_rb, Point3(0, 10.0f, 0), Quat::identity());
    Vector3 force(1.0f, 0.0f, 0.0f);
    dmPhysics::ApplyForce(box_rb, force, Point3(0.0f, 0.0f, 0.0f));
    Vector3 total_force = dmPhysics::GetTotalForce(box_rb);
    ASSERT_NEAR(total_force.getX(), force.getX(), 0.01f);
    ASSERT_NEAR(total_force.getY(), force.getY(), 0.01f);
    ASSERT_NEAR(total_force.getZ(), force.getZ(), 0.01f);

    dmPhysics::DeleteRigidBody(m_World, box_rb);
    dmPhysics::DeleteCollisionShape(box_shape);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
