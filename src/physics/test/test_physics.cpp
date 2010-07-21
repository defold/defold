#include <stdint.h>
#include <gtest/gtest.h>
#include "../physics.h"

using namespace Vectormath::Aos;

struct VisualObject
{
    VisualObject() : m_Position(0.0f, 0.0f, 0.0f), m_Rotation(0.0f, 0.0f, 0.0f, 1.0f), m_CollisionCount(0) {}
    Point3 m_Position;
    Quat   m_Rotation;
    int m_CollisionCount;
};

void GetWorldTransform(void* visual_object, Vectormath::Aos::Point3& position, Vectormath::Aos::Quat& rotation)
{
    if (visual_object != 0x0)
    {
        VisualObject* o = (VisualObject*) visual_object;
        position = o->m_Position;
        rotation = o->m_Rotation;
    }
    else
    {
        position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
        rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

void SetWorldTransform(void* visual_object, const Vectormath::Aos::Point3& position, const Vectormath::Aos::Quat& rotation)
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
        m_World = dmPhysics::NewWorld(Point3(-1000.0f, -1000.0f, -1000.0f), Point3(1000.0f, 1000.0f, 1000.0f), &GetWorldTransform, &SetWorldTransform);
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
    dmPhysics::HCollisionObject co = dmPhysics::NewCollisionObject(m_World, shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, 0x0);

    dmPhysics::DeleteCollisionObject(m_World, co);
    dmPhysics::DeleteCollisionShape(shape);
}

TEST_F(PhysicsTest, DynamicConstruction)
{
    dmPhysics::HCollisionShape shape = dmPhysics::NewBoxShape(Vector3(1.0f, 1.0f, 1.0f));
    dmPhysics::HCollisionObject co = dmPhysics::NewCollisionObject(m_World, shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, 0x0);

    ASSERT_EQ((void*)0, (void*)co);

    dmPhysics::DeleteCollisionObject(m_World, co);

    co = dmPhysics::NewCollisionObject(m_World, shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, 0x0);

    ASSERT_NE((void*)0, (void*)co);

    dmPhysics::DeleteCollisionObject(m_World, co);

    dmPhysics::DeleteCollisionShape(shape);
}

TEST_F(PhysicsTest, KinematicConstruction)
{
    dmPhysics::HCollisionShape shape = dmPhysics::NewBoxShape(Vector3(1.0f, 1.0f, 1.0f));
    dmPhysics::HCollisionObject co = dmPhysics::NewCollisionObject(m_World, shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC, 1, 1, 0x0);

    ASSERT_EQ((void*)0, (void*)co);

    dmPhysics::DeleteCollisionObject(m_World, co);

    co = dmPhysics::NewCollisionObject(m_World, shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC, 1, 1, 0x0);

    ASSERT_NE((void*)0, (void*)co);

    dmPhysics::DeleteCollisionObject(m_World, co);

    dmPhysics::DeleteCollisionShape(shape);
}

TEST_F(PhysicsTest, StaticConstruction)
{
    dmPhysics::HCollisionShape shape = dmPhysics::NewBoxShape(Vector3(1.0f, 1.0f, 1.0f));
    dmPhysics::HCollisionObject co = dmPhysics::NewCollisionObject(m_World, shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, 1, 1, 0x0);

    ASSERT_EQ((void*)0, (void*)co);

    dmPhysics::DeleteCollisionObject(m_World, co);

    co = dmPhysics::NewCollisionObject(m_World, shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, 1, 1, 0x0);

    ASSERT_NE((void*)0, (void*)co);

    dmPhysics::DeleteCollisionObject(m_World, co);

    dmPhysics::DeleteCollisionShape(shape);
}

TEST_F(PhysicsTest, TriggerConstruction)
{
    dmPhysics::HCollisionShape shape = dmPhysics::NewBoxShape(Vector3(1.0f, 1.0f, 1.0f));
    dmPhysics::HCollisionObject co = dmPhysics::NewCollisionObject(m_World, shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER, 1, 1, 0x0);

    ASSERT_EQ((void*)0, (void*)co);

    co = dmPhysics::NewCollisionObject(m_World, shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER, 1, 1, 0x0);

    ASSERT_NE((void*)0, (void*)co);

    dmPhysics::DeleteCollisionObject(m_World, co);

    dmPhysics::DeleteCollisionShape(shape);
}

TEST_F(PhysicsTest, WorldTransformCallbacks)
{
    VisualObject vo;

    // Dynamic RB

    vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::HCollisionShape shape = dmPhysics::NewBoxShape(Vector3(1.0f, 1.0f, 1.0f));

    dmPhysics::HCollisionObject dynamic_co = dmPhysics::NewCollisionObject(m_World, shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, &vo);

    ASSERT_EQ(0.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, dmPhysics::GetWorldPosition(dynamic_co).getY());

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);

    ASSERT_GT(0.0f, vo.m_Position.getY());
    ASSERT_GT(0.0f, dmPhysics::GetWorldPosition(dynamic_co).getY());

    dmPhysics::DeleteCollisionObject(m_World, dynamic_co);

    // Kinematic RB

    vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::HCollisionObject kinematic_co = dmPhysics::NewCollisionObject(m_World, shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC, 1, 1, &vo);

    ASSERT_EQ(0.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, dmPhysics::GetWorldPosition(kinematic_co).getY());

    vo.m_Position.setY(1.0f);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);

    ASSERT_EQ(1.0f, vo.m_Position.getY());
    ASSERT_EQ(1.0f, dmPhysics::GetWorldPosition(kinematic_co).getY());

    dmPhysics::DeleteCollisionObject(m_World, kinematic_co);

    // Static RB

    vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::HCollisionObject static_co = dmPhysics::NewCollisionObject(m_World, shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, 1, 1, &vo);

    ASSERT_EQ(0.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, dmPhysics::GetWorldPosition(static_co).getY());

    vo.m_Position.setY(1.0f);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);

    ASSERT_EQ(1.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, dmPhysics::GetWorldPosition(static_co).getY());

    dmPhysics::DeleteCollisionObject(m_World, static_co);

    // Trigger RB

    vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::HCollisionObject trigger_co = dmPhysics::NewCollisionObject(m_World, shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER, 1, 1, &vo);

    ASSERT_EQ(0.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, dmPhysics::GetWorldPosition(trigger_co).getY());

    vo.m_Position.setY(1.0f);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);

    ASSERT_EQ(1.0f, vo.m_Position.getY());
    ASSERT_EQ(1.0f, dmPhysics::GetWorldPosition(trigger_co).getY());

    dmPhysics::DeleteCollisionObject(m_World, trigger_co);

    dmPhysics::DeleteCollisionShape(shape);
}

TEST_F(PhysicsTest, GroundBoxCollision)
{
    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    VisualObject ground_visual_object;
    dmPhysics::HCollisionShape ground_shape = dmPhysics::NewBoxShape(Vector3(100, ground_height_half_ext, 100));
    dmPhysics::HCollisionObject ground_co = dmPhysics::NewCollisionObject(m_World, ground_shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, 1, 1, &ground_visual_object);

    VisualObject box_visual_object;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HCollisionObject box_co = dmPhysics::NewCollisionObject(m_World, box_shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, &box_visual_object);
    dmPhysics::SetCollisionObjectInitialTransform(box_co, Point3(0, 10.0f, 0), Quat::identity());

    for (int i = 0; i < 200; ++i)
    {
        dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    }

    ASSERT_NEAR(ground_height_half_ext + box_half_ext, box_visual_object.m_Position.getY(), 0.01f);

    dmPhysics::DeleteCollisionObject(m_World, ground_co);
    dmPhysics::DeleteCollisionObject(m_World, box_co);
    dmPhysics::DeleteCollisionShape(ground_shape);
    dmPhysics::DeleteCollisionShape(box_shape);
}

void CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
{
    VisualObject* vo = (VisualObject*)user_data_a;
    ++vo->m_CollisionCount;
    vo = (VisualObject*)user_data_b;
    ++vo->m_CollisionCount;
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
    dmPhysics::HCollisionObject ground_co = dmPhysics::NewCollisionObject(m_World, ground_shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, 1, 1, &ground_visual_object);

    VisualObject box_visual_object;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HCollisionObject box_co = dmPhysics::NewCollisionObject(m_World, box_shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, &box_visual_object);
    dmPhysics::SetCollisionObjectInitialTransform(box_co, Point3(0, 10.0f, 0), Quat::identity());

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

    ASSERT_LT(0, box_visual_object.m_CollisionCount);
    ASSERT_LT(0, ground_visual_object.m_CollisionCount);

    ASSERT_EQ(1, collision_count);
    ASSERT_EQ(2, contact_point_count);

    dmPhysics::DeleteCollisionObject(m_World, ground_co);
    dmPhysics::DeleteCollisionObject(m_World, box_co);
    dmPhysics::DeleteCollisionShape(ground_shape);
    dmPhysics::DeleteCollisionShape(box_shape);
}

void TriggerCollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
{
    VisualObject* vo = (VisualObject*)user_data_a;
    ++vo->m_CollisionCount;
    vo = (VisualObject*)user_data_b;
    ++vo->m_CollisionCount;
}

TEST_F(PhysicsTest, TriggerCollisions)
{
    float box_half_ext = 0.5f;

    // Test the test

    VisualObject static_vo;
    static_vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    static_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::HCollisionShape static_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HCollisionObject static_co = dmPhysics::NewCollisionObject(m_World, static_shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, 1, 1, &static_vo);

    VisualObject dynamic_vo;
    dynamic_vo.m_Position = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    dynamic_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::HCollisionShape dynamic_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HCollisionObject dynamic_co = dmPhysics::NewCollisionObject(m_World, dynamic_shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, &dynamic_vo);

    for (int i = 0; i < 20; ++i)
    {
        dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    }

    ASSERT_NEAR(1.0f, dynamic_vo.m_Position.getY(), 0.001f);
    ASSERT_EQ(0.0f, static_vo.m_Position.getY());

    dmPhysics::DeleteCollisionObject(m_World, static_co);
    dmPhysics::DeleteCollisionShape(static_shape);

    dmPhysics::DeleteCollisionObject(m_World, dynamic_co);

    // Test trigger collision: dynamic body moving into trigger

    dynamic_vo.m_Position = Vectormath::Aos::Point3(0.0f, 1.1f, 0.0f);
    dynamic_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dynamic_co = dmPhysics::NewCollisionObject(m_World, dynamic_shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, &dynamic_vo);

    VisualObject trigger_vo;
    trigger_vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    trigger_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::HCollisionShape trigger_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HCollisionObject trigger_co = dmPhysics::NewCollisionObject(m_World, trigger_shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER, 1, 1, &trigger_vo);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    dmPhysics::ForEachCollision(m_World, &TriggerCollisionCallback, 0x0, 0x0, 0x0);

    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    for (int i = 0; i < 20; ++i)
    {
        dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    }

    ASSERT_GT(1.0f - 0.1f, dynamic_vo.m_Position.getY());
    ASSERT_EQ(0.0f, trigger_vo.m_Position.getY());

    dmPhysics::ForEachCollision(m_World, &TriggerCollisionCallback, 0x0, 0x0, 0x0);
    ASSERT_LT(0, trigger_vo.m_CollisionCount);

    dmPhysics::DeleteCollisionObject(m_World, dynamic_co);
    dmPhysics::DeleteCollisionShape(dynamic_shape);

    dmPhysics::DeleteCollisionObject(m_World, trigger_co);

    // Test trigger collision: dynamic body moving into trigger

    static_vo = VisualObject();
    static_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    static_co = dmPhysics::NewCollisionObject(m_World, static_shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, 1, 1, &static_vo);

    trigger_vo.m_Position = Vectormath::Aos::Point3(0.0f, 1.1f, 0.0f);
    trigger_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    trigger_vo.m_CollisionCount = 0;
    trigger_co = dmPhysics::NewCollisionObject(m_World, trigger_shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER, 1, 1, &trigger_vo);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    dmPhysics::ForEachCollision(m_World, &TriggerCollisionCallback, 0x0, 0x0, 0x0);

    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    trigger_vo.m_Position.setY(0.8f);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    dmPhysics::ForEachCollision(m_World, &TriggerCollisionCallback, 0x0, 0x0, 0x0);

    ASSERT_LT(0, trigger_vo.m_CollisionCount);

    trigger_vo.m_CollisionCount = 0;

    trigger_vo.m_Position.setY(1.1f);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    dmPhysics::ForEachCollision(m_World, &TriggerCollisionCallback, 0x0, 0x0, 0x0);

    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    dmPhysics::DeleteCollisionObject(m_World, trigger_co);

    dmPhysics::DeleteCollisionObject(m_World, static_co);
    dmPhysics::DeleteCollisionShape(static_shape);

    // Test trigger collision: kinematic body moved into trigger

    VisualObject kinematic_vo;
    kinematic_vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    kinematic_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    kinematic_vo.m_CollisionCount = 0;
    dmPhysics::HCollisionShape kinematic_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HCollisionObject kinematic_co = dmPhysics::NewCollisionObject(m_World, kinematic_shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC, 1, 1, &kinematic_vo);

    trigger_vo.m_Position = Vectormath::Aos::Point3(0.0f, 1.1f, 0.0f);
    trigger_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    trigger_vo.m_CollisionCount = 0;
    trigger_co = dmPhysics::NewCollisionObject(m_World, trigger_shape, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER, 1, 1, &trigger_vo);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    dmPhysics::ForEachCollision(m_World, &TriggerCollisionCallback, 0x0, 0x0, 0x0);

    ASSERT_EQ(0, kinematic_vo.m_CollisionCount);
    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    kinematic_vo.m_Position.setY(0.5f);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    dmPhysics::ForEachCollision(m_World, &TriggerCollisionCallback, 0x0, 0x0, 0x0);

    ASSERT_EQ(1, kinematic_vo.m_CollisionCount);
    ASSERT_EQ(1, trigger_vo.m_CollisionCount);

    kinematic_vo.m_CollisionCount = 0;
    trigger_vo.m_CollisionCount = 0;

    kinematic_vo.m_Position.setY(0.0f);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
    dmPhysics::ForEachCollision(m_World, &TriggerCollisionCallback, 0x0, 0x0, 0x0);

    ASSERT_EQ(0, kinematic_vo.m_CollisionCount);
    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    dmPhysics::DeleteCollisionObject(m_World, kinematic_co);
    dmPhysics::DeleteCollisionShape(kinematic_shape);

    dmPhysics::DeleteCollisionObject(m_World, trigger_co);
    dmPhysics::DeleteCollisionShape(trigger_shape);
}

TEST_F(PhysicsTest, ApplyForce)
{
    float box_half_ext = 0.5f;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dmPhysics::HCollisionObject box_co = dmPhysics::NewCollisionObject(m_World, box_shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, 0x0);
    dmPhysics::SetCollisionObjectInitialTransform(box_co, Point3(0, 10.0f, 0), Quat::identity());
    Vector3 force(1.0f, 0.0f, 0.0f);
    dmPhysics::ApplyForce(box_co, force, Point3(0.0f, 0.0f, 0.0f));
    Vector3 total_force = dmPhysics::GetTotalForce(box_co);
    ASSERT_NEAR(total_force.getX(), force.getX(), 0.01f);
    ASSERT_NEAR(total_force.getY(), force.getY(), 0.01f);
    ASSERT_NEAR(total_force.getZ(), force.getZ(), 0.01f);

    dmPhysics::DeleteCollisionObject(m_World, box_co);
    dmPhysics::DeleteCollisionShape(box_shape);
}

struct RayCastResult
{
    bool m_Hit[2];
};

void RayCastResponse(bool hit, float hit_fraction, uint32_t user_id, void* user_data)
{
    if (hit)
    {
        RayCastResult* rcr = (RayCastResult*)user_data;
        rcr->m_Hit[user_id] = hit;
    }
}

TEST_F(PhysicsTest, RayCasting)
{
    float box_half_ext = 0.5f;
    dmPhysics::HCollisionShape box_shape = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    VisualObject vo;
    dmPhysics::HCollisionObject box_co = dmPhysics::NewCollisionObject(m_World, box_shape, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, 1, 1, &vo);

    RayCastResult result;
    memset(result.m_Hit, 0, sizeof(bool) * 2);

    dmPhysics::RayCastRequest request;
    request.m_From = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    request.m_To = Vectormath::Aos::Point3(0.0f, 0.51f, 0.0f);
    request.m_UserId = 0;
    request.m_UserData = &result;
    request.m_ResponseCallback = &RayCastResponse;

    dmPhysics::RequestRayCast(m_World, request);

    request.m_To = Vectormath::Aos::Point3(0.0f, 0.49f, 0.0f);
    request.m_UserId = 1;

    dmPhysics::RequestRayCast(m_World, request);

    dmPhysics::StepWorld(m_World, 1.0f / 60.0f);

    ASSERT_FALSE(result.m_Hit[0]);
    ASSERT_TRUE(result.m_Hit[1]);

    dmPhysics::DeleteCollisionObject(m_World, box_co);
    dmPhysics::DeleteCollisionShape(box_shape);
}

enum FilterGroup
{
    FILTER_GROUP_A,
    FILTER_GROUP_B,
    FILTER_GROUP_C,
    FILTER_GROUP_COUNT
};

void FilterCollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
{
    VisualObject* vo_a = (VisualObject*)user_data_a;
    ++vo_a->m_CollisionCount;
    VisualObject* vo_b = (VisualObject*)user_data_b;
    ++vo_b->m_CollisionCount;
    int* collision_count = (int*)user_data;
    int index_a = 0;
    int index_b = 0;
    int mask = 1;
    for (int i = 0; i < FILTER_GROUP_COUNT; ++i)
    {
        if (mask == group_a)
            index_a = i;
        if (mask == group_b)
            index_b = i;
        mask <<= 1;
    }
    collision_count[index_a] += 1;
    collision_count[index_b] += 1;
}

void FilterContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
{
    int* count = (int*)user_data;
    int index_a = 0;
    int index_b = 0;
    int mask = 1;
    for (int i = 0; i < FILTER_GROUP_COUNT; ++i)
    {
        if (mask == contact_point.m_GroupA)
            index_a = i;
        if (mask == contact_point.m_GroupB)
            index_b = i;
        mask <<= 1;
    }
    count[index_a] += 1;
    count[index_b] += 1;
}

TEST_F(PhysicsTest, CollisionFiltering)
{
    float box_half_ext = 0.5f;

    dmPhysics::HCollisionShape box_shape_a = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    VisualObject vo_a;
    vo_a.m_Position.setY(1.1f);
    uint16_t group = 1 << FILTER_GROUP_A;
    uint16_t mask = 1 << FILTER_GROUP_B;
    dmPhysics::HCollisionObject box_co_a = dmPhysics::NewCollisionObject(m_World, box_shape_a, 1.0f, dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC, group, mask, &vo_a);

    dmPhysics::HCollisionShape box_shape_b = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    VisualObject vo_b;
    vo_b.m_Position.setX(-0.6f);
    group = 1 << FILTER_GROUP_B;
    mask = 1 << FILTER_GROUP_A;
    dmPhysics::HCollisionObject box_co_b = dmPhysics::NewCollisionObject(m_World, box_shape_b, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, group, mask, &vo_b);

    dmPhysics::HCollisionShape box_shape_c = dmPhysics::NewBoxShape(Vector3(box_half_ext, box_half_ext, box_half_ext));
    VisualObject vo_c;
    vo_c.m_Position.setX(0.6f);
    group = 1 << FILTER_GROUP_C;
    mask = 1 << FILTER_GROUP_C;
    dmPhysics::HCollisionObject box_co_c = dmPhysics::NewCollisionObject(m_World, box_shape_c, 0.0f, dmPhysics::COLLISION_OBJECT_TYPE_STATIC, group, mask, &vo_c);

    int collision_count[FILTER_GROUP_COUNT];
    memset(collision_count, 0, FILTER_GROUP_COUNT * sizeof(int));
    int contact_point_count[FILTER_GROUP_COUNT];
    memset(contact_point_count, 0, FILTER_GROUP_COUNT * sizeof(int));
    for (int i = 0; i < 10; ++i)
    {
        dmPhysics::StepWorld(m_World, 1.0f / 60.0f);
        dmPhysics::ForEachCollision(m_World, FilterCollisionCallback, &collision_count, FilterContactPointCallback, &contact_point_count);
    }

    ASSERT_EQ(collision_count[FILTER_GROUP_A] + collision_count[FILTER_GROUP_B], vo_a.m_CollisionCount + vo_b.m_CollisionCount);
    ASSERT_EQ(0, vo_c.m_CollisionCount);
    ASSERT_EQ(collision_count[FILTER_GROUP_C], vo_c.m_CollisionCount);

    ASSERT_EQ(contact_point_count[FILTER_GROUP_A], contact_point_count[FILTER_GROUP_B]);
    ASSERT_EQ(0, contact_point_count[FILTER_GROUP_C]);

    dmPhysics::DeleteCollisionObject(m_World, box_co_a);
    dmPhysics::DeleteCollisionShape(box_shape_a);

    dmPhysics::DeleteCollisionObject(m_World, box_co_b);
    dmPhysics::DeleteCollisionShape(box_shape_b);

    dmPhysics::DeleteCollisionObject(m_World, box_co_c);
    dmPhysics::DeleteCollisionShape(box_shape_c);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
