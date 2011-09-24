/**
 * NOTE! This test is only present since the 3D physics does not yet support multiple
 * collision groups within the same collision object.
 */

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
, m_SetCollisionShapeGroupFunc(dmPhysics::SetCollisionShapeGroup2D)
, m_NewCollisionObjectFunc(dmPhysics::NewCollisionObject2D)
, m_DeleteCollisionObjectFunc(dmPhysics::DeleteCollisionObject2D)
, m_GetCollisionShapesFunc(dmPhysics::GetCollisionShapes2D)
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

typedef ::testing::Types<Test2D> TestTypes;
TYPED_TEST_CASE(PhysicsTest, TestTypes);

TYPED_TEST(PhysicsTest, MultipleGroups)
{
    VisualObject vo_a;
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 1 << 2;
    data.m_Mask = 1 << 3;
    typename TypeParam::CollisionShapeType shapes[2];
    float v1[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    shapes[0] = (*TestFixture::m_Test.m_NewConvexHullShapeFunc)(v1, 4);
    float v2[] = {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 0.0f, 2.0f};
    shapes[1] = (*TestFixture::m_Test.m_NewConvexHullShapeFunc)(v2, 4);
    (*TestFixture::m_Test.m_SetCollisionShapeGroupFunc)(shapes[1], 1 << 1);
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shapes, 2u);

    VisualObject vo_b;
    vo_b.m_Position = Point3(0.5f, 3.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 1 << 3;
    data.m_Mask = 1 << 2;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(Vector3(0.5f, 0.5f, 0.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    for (uint32_t i = 0; i < 40; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_GT(1.52f, vo_b.m_Position.getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shapes[0]);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shapes[1]);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
