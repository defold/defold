#include "test_physics.h"

#include <dlib/math.h>

using namespace Vectormath::Aos;

VisualObject::VisualObject()
: m_Position(0.0f, 0.0f, 0.0f)
, m_Rotation(0.0f, 0.0f, 0.0f, 1.0f)
, m_CollisionCount(0)
{

}

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

bool CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
{
    VisualObject* vo = (VisualObject*)user_data_a;
    ++vo->m_CollisionCount;
    vo = (VisualObject*)user_data_b;
    ++vo->m_CollisionCount;
    int* count = (int*)user_data;
    if (*count < 20)
    {
        *count += 1;
        return true;
    }
    else
    {
        return false;
    }
}

bool ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
{
    int* count = (int*)user_data;
    if (*count < 20)
    {
        *count += 1;
        return true;
    }
    else
    {
        return false;
    }
}

Test3D::Test3D()
: m_NewContextFunc(dmPhysics::NewContext3D)
, m_DeleteContextFunc(dmPhysics::DeleteContext3D)
, m_NewWorldFunc(dmPhysics::NewWorld3D)
, m_DeleteWorldFunc(dmPhysics::DeleteWorld3D)
, m_StepWorldFunc(dmPhysics::StepWorld3D)
, m_DrawDebugFunc(dmPhysics::DrawDebug3D)
, m_NewBoxShapeFunc(dmPhysics::NewBoxShape3D)
, m_NewSphereShapeFunc(dmPhysics::NewSphereShape3D)
, m_NewCapsuleShapeFunc(dmPhysics::NewCapsuleShape3D)
, m_NewConvexHullShapeFunc(dmPhysics::NewConvexHullShape3D)
, m_DeleteCollisionShapeFunc(dmPhysics::DeleteCollisionShape3D)
, m_NewCollisionObjectFunc(dmPhysics::NewCollisionObject3D)
, m_DeleteCollisionObjectFunc(dmPhysics::DeleteCollisionObject3D)
, m_GetCollisionShapeFunc(dmPhysics::GetCollisionShape3D)
, m_SetCollisionObjectUserDataFunc(dmPhysics::SetCollisionObjectUserData3D)
, m_GetCollisionObjectUserDataFunc(dmPhysics::GetCollisionObjectUserData3D)
, m_ApplyForceFunc(dmPhysics::ApplyForce3D)
, m_GetTotalForceFunc(dmPhysics::GetTotalForce3D)
, m_GetWorldPositionFunc(dmPhysics::GetWorldPosition3D)
, m_GetWorldRotationFunc(dmPhysics::GetWorldRotation3D)
, m_GetLinearVelocityFunc(dmPhysics::GetLinearVelocity3D)
, m_GetAngularVelocityFunc(dmPhysics::GetAngularVelocity3D)
, m_RequestRayCastFunc(dmPhysics::RequestRayCast3D)
, m_SetDebugCallbacksFunc(dmPhysics::SetDebugCallbacks3D)
, m_ReplaceShapeFunc(dmPhysics::ReplaceShape3D)
, m_Vertices(new float[4*3])
, m_VertexCount(4)
, m_PolygonRadius(0.001f)
{
    m_Vertices[0] = 0.0f; m_Vertices[1] = 0.0f; m_Vertices[2] = 0.0f;
    m_Vertices[3] = 1.0f; m_Vertices[4] = 0.0f; m_Vertices[5] = 0.0f;
    m_Vertices[6] = 0.0f; m_Vertices[7] = 1.0f; m_Vertices[8] = 0.0f;
    m_Vertices[9] = 0.0f; m_Vertices[10] = 0.0f; m_Vertices[11] = 1.0f;
}

Test3D::~Test3D()
{
    delete [] m_Vertices;
}

Test2D::Test2D()
: m_NewContextFunc(dmPhysics::NewContext2D)
, m_DeleteContextFunc(dmPhysics::DeleteContext2D)
, m_NewWorldFunc(dmPhysics::NewWorld2D)
, m_DeleteWorldFunc(dmPhysics::DeleteWorld2D)
, m_StepWorldFunc(dmPhysics::StepWorld2D)
, m_DrawDebugFunc(dmPhysics::DrawDebug2D)
, m_NewBoxShapeFunc(dmPhysics::NewBoxShape2D)
, m_NewSphereShapeFunc(dmPhysics::NewCircleShape2D)
, m_NewCapsuleShapeFunc(0x0)
, m_NewConvexHullShapeFunc(dmPhysics::NewPolygonShape2D)
, m_DeleteCollisionShapeFunc(dmPhysics::DeleteCollisionShape2D)
, m_NewCollisionObjectFunc(dmPhysics::NewCollisionObject2D)
, m_DeleteCollisionObjectFunc(dmPhysics::DeleteCollisionObject2D)
, m_GetCollisionShapeFunc(dmPhysics::GetCollisionShape2D)
, m_SetCollisionObjectUserDataFunc(dmPhysics::SetCollisionObjectUserData2D)
, m_GetCollisionObjectUserDataFunc(dmPhysics::GetCollisionObjectUserData2D)
, m_ApplyForceFunc(dmPhysics::ApplyForce2D)
, m_GetTotalForceFunc(dmPhysics::GetTotalForce2D)
, m_GetWorldPositionFunc(dmPhysics::GetWorldPosition2D)
, m_GetWorldRotationFunc(dmPhysics::GetWorldRotation2D)
, m_GetLinearVelocityFunc(dmPhysics::GetLinearVelocity2D)
, m_GetAngularVelocityFunc(dmPhysics::GetAngularVelocity2D)
, m_RequestRayCastFunc(dmPhysics::RequestRayCast2D)
, m_SetDebugCallbacksFunc(dmPhysics::SetDebugCallbacks2D)
, m_ReplaceShapeFunc(dmPhysics::ReplaceShape2D)
, m_Vertices(new float[3*2])
, m_VertexCount(3)
, m_PolygonRadius(b2_polygonRadius)
{
    m_Vertices[0] = 0.0f; m_Vertices[1] = 0.0f;
    m_Vertices[2] = 1.0f; m_Vertices[3] = 0.0f;
    m_Vertices[4] = 0.0f; m_Vertices[5] = 1.0f;
}

Test2D::~Test2D()
{
    delete [] m_Vertices;
}

typedef ::testing::Types<Test3D, Test2D> TestTypes;
TYPED_TEST_CASE(PhysicsTest, TestTypes);

TYPED_TEST(PhysicsTest, BoxShape)
{
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(1.0f, 1.0f, 1.0f));
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, SphereShape)
{
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewSphereShapeFunc)(1.0f);
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, CapsuleShape)
{
    if (TestFixture::m_Test.m_NewCapsuleShapeFunc != 0x0)
    {
        dmPhysics::CollisionObjectData data;
        typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewCapsuleShapeFunc)(0.5f, 1.0f);
        typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

        (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);
        (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    }
}

TYPED_TEST(PhysicsTest, ConvexHullShape)
{
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewConvexHullShapeFunc)(TestFixture::m_Test.m_Vertices, TestFixture::m_Test.m_VertexCount);
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, DynamicConstruction)
{
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(1.0f, 1.0f, 1.0f));
    data.m_Mass = 0.0f;
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_EQ((void*)0, (void*)co);

    data.m_Mass = 1.0f;
    co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_NE((void*)0, (void*)co);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);

    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, KinematicConstruction)
{
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(1.0f, 1.0f, 1.0f));
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_EQ((void*)0, (void*)co);

    data.m_Mass = 0.0f;
    co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_NE((void*)0, (void*)co);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);

    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, StaticConstruction)
{
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(1.0f, 1.0f, 1.0f));
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_EQ((void*)0, (void*)co);

    data.m_Mass = 0.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_NE((void*)0, (void*)co);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);

    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, TriggerConstruction)
{
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(1.0f, 1.0f, 1.0f));
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER;
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_EQ((void*)0, (void*)co);

    data.m_Mass = 0.0f;
    co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_NE((void*)0, (void*)co);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);

    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, WorldTransformCallbacks)
{
    VisualObject vo;

    // Dynamic RB

    Vectormath::Aos::Point3 start_pos(1.0f, 2.0f, 0.0f);
    float pi_4 = M_PI * 0.25f;
    Vectormath::Aos::Quat start_rot(0.0f, 0.0f, sin(pi_4), cos(pi_4));
    vo.m_Position = start_pos;
    vo.m_Rotation = start_rot;
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(1.0f, 1.0f, 1.0f));
    data.m_UserData = &vo;
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    Vectormath::Aos::Point3 body_pos = (*TestFixture::m_Test.m_GetWorldPositionFunc)(dynamic_co);
    for (int i = 0; i < 3; ++i)
    {
        ASSERT_EQ(start_pos.getElem(i), vo.m_Position.getElem(i));
        ASSERT_EQ(start_pos.getElem(i), body_pos.getElem(i));
    }
    Vectormath::Aos::Quat body_rot = (*TestFixture::m_Test.m_GetWorldRotationFunc)(dynamic_co);
    for (int i = 0; i < 4; ++i)
    {
        ASSERT_EQ(start_rot.getElem(i), vo.m_Rotation.getElem(i));
        ASSERT_NEAR(start_rot.getElem(i), body_rot.getElem(i), 0.0000001f);
    }

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_GT(start_pos.getY(), vo.m_Position.getY());
    ASSERT_GT(start_pos.getY(), (*TestFixture::m_Test.m_GetWorldPositionFunc)(dynamic_co).getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);

    // Kinematic RB

    vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    data.m_Mass = 0.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    typename TypeParam::CollisionObjectType kinematic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_EQ(0.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, (*TestFixture::m_Test.m_GetWorldPositionFunc)(kinematic_co).getY());

    vo.m_Position.setY(1.0f);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(1.0f, vo.m_Position.getY());
    ASSERT_EQ(1.0f, (*TestFixture::m_Test.m_GetWorldPositionFunc)(kinematic_co).getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, kinematic_co);

    // Static RB

    vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_EQ(0.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, (*TestFixture::m_Test.m_GetWorldPositionFunc)(static_co).getY());

    vo.m_Position.setY(1.0f);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(1.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, (*TestFixture::m_Test.m_GetWorldPositionFunc)(static_co).getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);

    // Trigger RB

    vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER;
    typename TypeParam::CollisionObjectType trigger_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_EQ(0.0f, vo.m_Position.getY());
    ASSERT_EQ(0.0f, (*TestFixture::m_Test.m_GetWorldPositionFunc)(trigger_co).getY());

    vo.m_Position.setY(1.0f);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(1.0f, vo.m_Position.getY());
    ASSERT_EQ(1.0f, (*TestFixture::m_Test.m_GetWorldPositionFunc)(trigger_co).getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, trigger_co);

    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, GroundBoxCollision)
{
    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    VisualObject ground_visual_object;
    dmPhysics::CollisionObjectData ground_data;
    typename TypeParam::CollisionShapeType ground_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(100, ground_height_half_ext, 100));
    ground_data.m_Mass = 0.0f;
    ground_data.m_Restitution = 0.0f;
    ground_data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    ground_data.m_UserData = &ground_visual_object;
    typename TypeParam::CollisionObjectType ground_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, ground_data, ground_shape);

    Point3 start_position(0.0f, 2.0f, 0.0f);
    VisualObject box_visual_object;
    box_visual_object.m_Position = start_position;
    dmPhysics::CollisionObjectData box_data;
    box_data.m_Restitution = 0.0f;
    typename TypeParam::CollisionShapeType box_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    box_data.m_UserData = &box_visual_object;
    typename TypeParam::CollisionObjectType box_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, box_data, box_shape);

    for (int i = 0; i < 200; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_NEAR(ground_height_half_ext + box_half_ext, box_visual_object.m_Position.getY(), 2.0f * TestFixture::m_Test.m_PolygonRadius);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, ground_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(ground_shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(box_shape);
}

TYPED_TEST(PhysicsTest, CollisionCallbacks)
{
    float ground_height_half_ext = 1.0f;
    float box_half_ext = 0.5f;

    VisualObject ground_visual_object;
    dmPhysics::CollisionObjectData ground_data;
    typename TypeParam::CollisionShapeType ground_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(100, ground_height_half_ext, 100));
    ground_data.m_Mass = 0.0f;
    ground_data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    ground_data.m_UserData = &ground_visual_object;
    typename TypeParam::CollisionObjectType ground_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, ground_data, ground_shape);

    VisualObject box_visual_object;
    box_visual_object.m_Position = Point3(0, 10, 0);
    dmPhysics::CollisionObjectData box_data;
    typename TypeParam::CollisionShapeType box_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    box_data.m_UserData = &box_visual_object;
    typename TypeParam::CollisionObjectType box_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, box_data, box_shape);

    box_visual_object.m_CollisionCount = 0;
    ground_visual_object.m_CollisionCount = 0;
    TestFixture::m_CollisionCount = 0;
    TestFixture::m_ContactPointCount = 0;
    for (int i = 0; i < 10; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }
    ASSERT_EQ(0, TestFixture::m_CollisionCount);
    ASSERT_EQ(0, TestFixture::m_ContactPointCount);

    float last_y = 0.0f;
    for (int i = 0; i < 200 && box_visual_object.m_Position.getY() != last_y; ++i)
    {
        last_y = box_visual_object.m_Position.getY();
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }
    ASSERT_EQ(20, TestFixture::m_CollisionCount);
    ASSERT_EQ(20, TestFixture::m_ContactPointCount);

    ASSERT_LT(0, box_visual_object.m_CollisionCount);
    ASSERT_LT(0, ground_visual_object.m_CollisionCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, ground_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(ground_shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(box_shape);
}

bool TriggerCollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
{
    VisualObject* vo = (VisualObject*)user_data_a;
    ++vo->m_CollisionCount;
    vo = (VisualObject*)user_data_b;
    ++vo->m_CollisionCount;
    return true;
}

TYPED_TEST(PhysicsTest, TriggerCollisions)
{
    float box_half_ext = 0.5f;

    // Test the test

    VisualObject static_vo;
    static_vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    static_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::CollisionObjectData static_data;
    typename TypeParam::CollisionShapeType static_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    static_data.m_Mass = 0.0f;
    static_data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    static_data.m_UserData = &static_vo;
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, static_data, static_shape);

    VisualObject dynamic_vo;
    dynamic_vo.m_Position = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    dynamic_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::CollisionObjectData dynamic_data;
    typename TypeParam::CollisionShapeType dynamic_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    dynamic_data.m_UserData = &dynamic_vo;
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, dynamic_data, dynamic_shape);

    for (int i = 0; i < 20; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_NEAR(1.0f, dynamic_vo.m_Position.getY(), 2.0f * TestFixture::m_Test.m_PolygonRadius);
    ASSERT_EQ(0.0f, static_vo.m_Position.getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(static_shape);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);

    // Test trigger collision: dynamic body moving into trigger

    dynamic_vo.m_Position = Vectormath::Aos::Point3(0.0f, 1.1f, 0.0f);
    dynamic_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, dynamic_data, dynamic_shape);

    VisualObject trigger_vo;
    trigger_vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    trigger_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    dmPhysics::CollisionObjectData trigger_data;
    typename TypeParam::CollisionShapeType trigger_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    trigger_data.m_Mass = 0.0f;
    trigger_data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER;
    trigger_data.m_UserData = &trigger_vo;
    typename TypeParam::CollisionObjectType trigger_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, trigger_data, trigger_shape);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    for (int i = 0; i < 20; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_GT(1.0f - 0.1f, dynamic_vo.m_Position.getY());
    ASSERT_EQ(0.0f, trigger_vo.m_Position.getY());
    ASSERT_LT(0, trigger_vo.m_CollisionCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(dynamic_shape);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, trigger_co);

    // Test trigger collision: dynamic body moving into trigger

    static_vo = VisualObject();
    static_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, static_data, static_shape);

    trigger_vo.m_Position = Vectormath::Aos::Point3(0.0f, 1.1f, 0.0f);
    trigger_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    trigger_vo.m_CollisionCount = 0;
    trigger_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, trigger_data, trigger_shape);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    trigger_vo.m_Position.setY(0.8f);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_LT(0, trigger_vo.m_CollisionCount);

    trigger_vo.m_CollisionCount = 0;

    trigger_vo.m_Position.setY(1.1f);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, trigger_co);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(static_shape);

    // Test trigger collision: kinematic body moved into trigger

    VisualObject kinematic_vo;
    kinematic_vo.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    kinematic_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    kinematic_vo.m_CollisionCount = 0;
    dmPhysics::CollisionObjectData kinematic_data;
    typename TypeParam::CollisionShapeType kinematic_shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    kinematic_data.m_Mass = 0.0f;
    kinematic_data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    kinematic_data.m_UserData = &kinematic_vo;
    typename TypeParam::CollisionObjectType kinematic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, kinematic_data, kinematic_shape);

    trigger_vo.m_Position = Vectormath::Aos::Point3(0.0f, 1.1f, 0.0f);
    trigger_vo.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    trigger_vo.m_CollisionCount = 0;
    trigger_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, trigger_data, trigger_shape);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(0, kinematic_vo.m_CollisionCount);
    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    kinematic_vo.m_Position.setY(0.5f);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(1, kinematic_vo.m_CollisionCount);
    ASSERT_EQ(1, trigger_vo.m_CollisionCount);

    kinematic_vo.m_CollisionCount = 0;
    trigger_vo.m_CollisionCount = 0;

    kinematic_vo.m_Position.setY(0.0f);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(0, kinematic_vo.m_CollisionCount);
    ASSERT_EQ(0, trigger_vo.m_CollisionCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, kinematic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(kinematic_shape);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, trigger_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(trigger_shape);
}

TYPED_TEST(PhysicsTest, ApplyForce)
{
    float box_half_ext = 0.5f;
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    typename TypeParam::CollisionObjectType box_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    Vector3 lin_vel = (*TestFixture::m_Test.m_GetLinearVelocityFunc)(box_co);
    ASSERT_EQ(0.0f, lengthSqr(lin_vel));
    Vector3 ang_vel = (*TestFixture::m_Test.m_GetAngularVelocityFunc)(box_co);
    ASSERT_EQ(0.0f, lengthSqr(ang_vel));

    Vector3 force(0.0f, 1.0f, 0.0f);
    (*TestFixture::m_Test.m_ApplyForceFunc)(box_co, force, Point3(0.5f*box_half_ext, 0.0f, 0.0f));
    Vector3 total_force = (*TestFixture::m_Test.m_GetTotalForceFunc)(box_co);
    ASSERT_NEAR(total_force.getX(), force.getX(), 0.01f);
    ASSERT_NEAR(total_force.getY(), force.getY(), 0.01f);
    ASSERT_NEAR(total_force.getZ(), force.getZ(), 0.01f);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    lin_vel = (*TestFixture::m_Test.m_GetLinearVelocityFunc)(box_co);
    ASSERT_NE(0.0f, lengthSqr(lin_vel));
    ang_vel = (*TestFixture::m_Test.m_GetAngularVelocityFunc)(box_co);
    ASSERT_NE(0.0f, lengthSqr(ang_vel));

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

struct RayCastResult
{
    dmPhysics::RayCastResponse m_Response;
    void* m_UserData;
};

void RayCastCallback(const dmPhysics::RayCastResponse& response, const dmPhysics::RayCastRequest& request, void* user_data)
{
    RayCastResult* rcr = (RayCastResult*)request.m_UserData;
    rcr[request.m_UserId].m_Response = response;
    rcr[request.m_UserId].m_UserData = request.m_UserData;
}

TYPED_TEST(PhysicsTest, EmptyRayCasting)
{
    RayCastResult result;
    memset(&result, 0, sizeof(RayCastResult));
    // Avoid false positives
    result.m_Response.m_Hit = true;

    dmPhysics::RayCastRequest request;
    request.m_From = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    request.m_To = Vectormath::Aos::Point3(0.0f, 0.51f, 0.0f);
    request.m_UserId = 0;
    request.m_UserData = &result;

    (*TestFixture::m_Test.m_RequestRayCastFunc)(TestFixture::m_World, request);

    TestFixture::m_StepWorldContext.m_RayCastCallback = RayCastCallback;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_FALSE(result.m_Response.m_Hit);
}

TYPED_TEST(PhysicsTest, RayCasting)
{
    float box_half_ext = 0.5f;
    VisualObject vo;
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data.m_Mass = 0.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_UserData = &vo;
    typename TypeParam::CollisionObjectType box_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    RayCastResult result[2];
    memset(result, 0, sizeof(RayCastResult) * 2);

    dmPhysics::RayCastRequest request;
    request.m_From = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    request.m_To = Vectormath::Aos::Point3(0.0f, 0.51f + TestFixture::m_Test.m_PolygonRadius, 0.0f);
    request.m_UserId = 0;
    request.m_UserData = result;

    (*TestFixture::m_Test.m_RequestRayCastFunc)(TestFixture::m_World, request);

    request.m_To = Vectormath::Aos::Point3(0.0f, 0.49f, 0.0f);
    request.m_UserId = 1;

    (*TestFixture::m_Test.m_RequestRayCastFunc)(TestFixture::m_World, request);

    TestFixture::m_StepWorldContext.m_RayCastCallback = RayCastCallback;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);


    ASSERT_FALSE(result[0].m_Response.m_Hit);
    ASSERT_TRUE(result[1].m_Response.m_Hit);
    ASSERT_GT(1.0f, result[1].m_Response.m_Fraction);
    ASSERT_NEAR(0.0f, result[1].m_Response.m_Position.getX(), 0.00001f);
    ASSERT_NEAR(0.5f, result[1].m_Response.m_Position.getY(), 0.00001f);
    ASSERT_NEAR(0.0f, result[1].m_Response.m_Position.getZ(), 0.00001f);
    ASSERT_NEAR(0.0f, result[1].m_Response.m_Normal.getX(), 0.00001f);
    ASSERT_NEAR(1.0f, result[1].m_Response.m_Normal.getY(), 0.00001f);
    ASSERT_NEAR(0.0f, result[1].m_Response.m_Normal.getZ(), 0.00001f);
    ASSERT_EQ((void*)&vo, (void*)result[1].m_Response.m_CollisionObjectUserData);
    ASSERT_EQ(1, result[1].m_Response.m_CollisionObjectGroup);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, InsideRayCasting)
{
    float box_half_ext = 0.5f;
    VisualObject vo;
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data.m_Mass = 0.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_UserData = &vo;
    typename TypeParam::CollisionObjectType box_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    RayCastResult result[2];
    memset(result, 0, sizeof(result));

    dmPhysics::RayCastRequest request;
    request.m_From = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    request.m_To = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    request.m_UserId = 0;
    request.m_UserData = result;

    (*TestFixture::m_Test.m_RequestRayCastFunc)(TestFixture::m_World, request);

    request.m_To = Vectormath::Aos::Point3(0.0f, 0.49f, 0.0f);
    request.m_UserId = 1;

    (*TestFixture::m_Test.m_RequestRayCastFunc)(TestFixture::m_World, request);

    TestFixture::m_StepWorldContext.m_RayCastCallback = RayCastCallback;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_FALSE(result[0].m_Response.m_Hit);
    ASSERT_FALSE(result[1].m_Response.m_Hit);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, IgnoreRayCasting)
{
    float box_half_ext = 0.5f;
    VisualObject vo;
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data.m_Mass = 0.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_UserData = &vo;
    typename TypeParam::CollisionObjectType box_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    RayCastResult result;
    memset(&result, 0, sizeof(RayCastResult));

    dmPhysics::RayCastRequest request;
    request.m_From = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    request.m_To = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    request.m_UserId = 0;
    request.m_IgnoredUserData = &vo;
    request.m_UserData = &result;

    (*TestFixture::m_Test.m_RequestRayCastFunc)(TestFixture::m_World, request);

    TestFixture::m_StepWorldContext.m_RayCastCallback = RayCastCallback;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_FALSE(result.m_Response.m_Hit);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, TriggerRayCasting)
{
    float box_half_ext = 0.5f;
    VisualObject vo;
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data.m_Mass = 0.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER;
    data.m_UserData = &vo;
    typename TypeParam::CollisionObjectType box_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    RayCastResult result;
    memset(&result, 0, sizeof(RayCastResult));

    dmPhysics::RayCastRequest request;
    request.m_From = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    request.m_To = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    request.m_UserId = 0;
    request.m_UserData = &result;

    (*TestFixture::m_Test.m_RequestRayCastFunc)(TestFixture::m_World, request);

    TestFixture::m_StepWorldContext.m_RayCastCallback = RayCastCallback;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    // We rather have a 0.0f hit here, but bullet thinks otherwise
    ASSERT_FALSE(result.m_Response.m_Hit);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

TYPED_TEST(PhysicsTest, FilteredRayCasting)
{
    float box_half_ext = 0.5f;

    enum Groups
    {
        GROUP_A = 1 << 0,
        GROUP_B = 1 << 1
    };

    VisualObject vo_a;
    dmPhysics::CollisionObjectData data_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data_a.m_Mass = 0.0f;
    data_a.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data_a.m_Group = GROUP_A;
    data_a.m_Mask = GROUP_A;
    data_a.m_UserData = &vo_a;
    typename TypeParam::CollisionObjectType box_co_a = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data_a, shape_a);

    VisualObject vo_b;
    vo_b.m_Position.setY(-1.0f);
    dmPhysics::CollisionObjectData data_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data_b.m_Mass = 0.0f;
    data_b.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data_b.m_Group = GROUP_B;
    data_b.m_Mask = GROUP_B;
    data_b.m_UserData = &vo_b;
    typename TypeParam::CollisionObjectType box_co_b = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data_b, shape_b);

    RayCastResult result;
    memset(&result, 0, sizeof(RayCastResult));

    dmPhysics::RayCastRequest request;
    request.m_From = Vectormath::Aos::Point3(0.0f, 1.0f, 0.0f);
    request.m_To = Vectormath::Aos::Point3(0.0f, -1.0f, 0.0f);
    request.m_UserId = 0;
    request.m_UserData = &result;
    request.m_Mask = GROUP_B;

    (*TestFixture::m_Test.m_RequestRayCastFunc)(TestFixture::m_World, request);

    TestFixture::m_StepWorldContext.m_RayCastCallback = RayCastCallback;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_TRUE(result.m_Response.m_Hit);
    ASSERT_EQ(GROUP_B, result.m_Response.m_CollisionObjectGroup);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co_b);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
}

enum FilterGroup
{
    FILTER_GROUP_A,
    FILTER_GROUP_B,
    FILTER_GROUP_C,
    FILTER_GROUP_COUNT
};

bool FilterCollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
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
    return true;
}

bool FilterContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
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
    return true;
}

TYPED_TEST(PhysicsTest, CollisionFiltering)
{
    float box_half_ext = 0.5f;

    VisualObject vo_a;
    vo_a.m_Position.setY(1.1f);
    dmPhysics::CollisionObjectData data_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data_a.m_Group = 1 << FILTER_GROUP_A;
    data_a.m_Mask = 1 << FILTER_GROUP_B;
    data_a.m_UserData = &vo_a;
    typename TypeParam::CollisionObjectType box_co_a = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data_a, shape_a);

    VisualObject vo_b;
    vo_b.m_Position.setX(-0.6f);
    dmPhysics::CollisionObjectData data_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data_b.m_Group = 1 << FILTER_GROUP_B;
    data_b.m_Mask = 1 << FILTER_GROUP_A;
    data_b.m_Mass = 0.0f;
    data_b.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data_b.m_UserData = &vo_b;
    typename TypeParam::CollisionObjectType box_co_b = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data_b, shape_b);

    VisualObject vo_c;
    vo_c.m_Position.setX(0.6f);
    dmPhysics::CollisionObjectData data_c;
    typename TypeParam::CollisionShapeType shape_c = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(box_half_ext, box_half_ext, box_half_ext));
    data_c.m_Group = 1 << FILTER_GROUP_C;
    data_c.m_Mask = 1 << FILTER_GROUP_C;
    data_c.m_Mass = 0.0f;
    data_c.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data_c.m_UserData = &vo_c;
    typename TypeParam::CollisionObjectType box_co_c = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data_c, shape_c);

    int collision_count[FILTER_GROUP_COUNT];
    memset(collision_count, 0, FILTER_GROUP_COUNT * sizeof(int));
    int contact_point_count[FILTER_GROUP_COUNT];
    memset(contact_point_count, 0, FILTER_GROUP_COUNT * sizeof(int));

    TestFixture::m_StepWorldContext.m_CollisionCallback = FilterCollisionCallback;
    TestFixture::m_StepWorldContext.m_CollisionUserData = collision_count;
    TestFixture::m_StepWorldContext.m_ContactPointCallback = FilterContactPointCallback;
    TestFixture::m_StepWorldContext.m_ContactPointUserData = contact_point_count;
    for (int i = 0; i < 10; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_EQ(collision_count[FILTER_GROUP_A] + collision_count[FILTER_GROUP_B], vo_a.m_CollisionCount + vo_b.m_CollisionCount);
    ASSERT_EQ(0, vo_c.m_CollisionCount);
    ASSERT_EQ(collision_count[FILTER_GROUP_C], vo_c.m_CollisionCount);

    ASSERT_EQ(contact_point_count[FILTER_GROUP_A], contact_point_count[FILTER_GROUP_B]);
    ASSERT_EQ(0, contact_point_count[FILTER_GROUP_C]);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co_b);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co_c);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_c);
}

TYPED_TEST(PhysicsTest, ReplaceShapes)
{
    float size = 0.5f;
    typename TypeParam::CollisionShapeType shape1 = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(size, size, size));
    typename TypeParam::CollisionShapeType shape2 = (*TestFixture::m_Test.m_NewSphereShapeFunc)(size);
    dmPhysics::CollisionObjectData data_a;
    typename TypeParam::CollisionObjectType box_co_a = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data_a, shape1);
    ASSERT_EQ(shape1, (*TestFixture::m_Test.m_GetCollisionShapeFunc)(box_co_a));
    (*TestFixture::m_Test.m_ReplaceShapeFunc)(TestFixture::m_Context, shape1, shape2);
    ASSERT_EQ(shape2, (*TestFixture::m_Test.m_GetCollisionShapeFunc)(box_co_a));
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, box_co_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape1);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape2);
}

TYPED_TEST(PhysicsTest, UserData)
{
    VisualObject vo_a;
    VisualObject vo_b;
    dmPhysics::CollisionObjectData data;
    data.m_UserData = &vo_a;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(1.0f, 1.0f, 1.0f));
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_EQ((void*)&vo_a, (*TestFixture::m_Test.m_GetCollisionObjectUserDataFunc)(co));
    (*TestFixture::m_Test.m_SetCollisionObjectUserDataFunc)(co, (void*)&vo_b);

    ASSERT_EQ((void*)&vo_b, (*TestFixture::m_Test.m_GetCollisionObjectUserDataFunc)(co));

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

void DrawLines(Vectormath::Aos::Point3* points, uint32_t point_count, Vectormath::Aos::Vector4 color, void* user_data)
{
    bool* drew = (bool*)user_data;
    *drew = true;
}

TYPED_TEST(PhysicsTest, DrawDebug)
{
    bool drew = false;
    dmPhysics::DebugCallbacks callbacks;
    callbacks.m_DrawLines = DrawLines;
    callbacks.m_UserData = &drew;
    (*TestFixture::m_Test.m_SetDebugCallbacksFunc)(TestFixture::m_Context, callbacks);

    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(1.0f, 1.0f, 1.0f));
    typename TypeParam::CollisionObjectType co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shape);

    ASSERT_FALSE(drew);

    (*TestFixture::m_Test.m_DrawDebugFunc)(TestFixture::m_World);

    ASSERT_TRUE(drew);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
