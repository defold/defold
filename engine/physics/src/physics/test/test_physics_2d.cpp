/**
 * NOTE! This test is only present since the 3D physics does not yet support multiple
 * collision groups within the same collision object nor support for the custom grid-shape.
 */

#include "test_physics.h"

#include <vector>
#include <dlib/math.h>

using namespace Vectormath::Aos;

dmPhysics::HullFlags EMPTY_FLAGS;

VisualObject::VisualObject()
: m_Position(0.0f, 0.0f, 0.0f)
, m_Rotation(0.0f, 0.0f, 0.0f, 1.0f)
, m_Scale(1.0f)
, m_CollisionCount(0)
, m_FirstCollisionGroup(0)
{

}

void GetWorldTransform(void* visual_object, dmTransform::Transform& world_transform)
{
    if (visual_object != 0x0)
    {
        VisualObject* o = (VisualObject*) visual_object;
        world_transform.SetTranslation(Vector3(o->m_Position));
        world_transform.SetRotation(o->m_Rotation);
        world_transform.SetUniformScale(o->m_Scale);
    }
    else
    {
        world_transform.SetIdentity();
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
    if (vo->m_CollisionCount == 0)
        vo->m_FirstCollisionGroup = group_a;
    ++vo->m_CollisionCount;

    vo = (VisualObject*)user_data_b;
    if (vo->m_CollisionCount == 0)
        vo->m_FirstCollisionGroup = group_b;
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
    *count += 1;
    return true;
}

Test2D::Test2D()
: m_NewContextFunc(dmPhysics::NewContext2D)
, m_DeleteContextFunc(dmPhysics::DeleteContext2D)
, m_NewWorldFunc(dmPhysics::NewWorld2D)
, m_DeleteWorldFunc(dmPhysics::DeleteWorld2D)
, m_StepWorldFunc(dmPhysics::StepWorld2D)
, m_SetDrawDebugFunc(dmPhysics::SetDrawDebug2D)
, m_NewBoxShapeFunc(dmPhysics::NewBoxShape2D)
, m_NewSphereShapeFunc(dmPhysics::NewCircleShape2D)
, m_NewCapsuleShapeFunc(0x0)
, m_NewConvexHullShapeFunc(dmPhysics::NewPolygonShape2D)
, m_DeleteCollisionShapeFunc(dmPhysics::DeleteCollisionShape2D)
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
, m_ContactPointTestFunc(dmPhysics::ContactPointTest2D)
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
    shapes[0] = (*TestFixture::m_Test.m_NewConvexHullShapeFunc)(TestFixture::m_Context, v1, 4);
    float v2[] = {0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 0.0f, 2.0f};
    shapes[1] = (*TestFixture::m_Test.m_NewConvexHullShapeFunc)(TestFixture::m_Context, v2, 4);
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, shapes, 2u);
    dmPhysics::SetCollisionObjectFilter(static_co, 1, 0, 1 << 1, 1 << 3);

    VisualObject vo_b;
    vo_b.m_Position = Point3(0.5f, 3.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 1 << 3;
    data.m_Mask = 1 << 2;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 0.5f, 0.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    for (uint32_t i = 0; i < 40; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_GT(1.5f + 2.0f * TestFixture::m_Test.m_PolygonRadius / PHYSICS_SCALE, vo_b.m_Position.getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shapes[0]);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shapes[1]);
}

TYPED_TEST(PhysicsTest, GridShapePolygon)
{
    int32_t rows = 4;
    int32_t columns = 10;
    int32_t cell_width = 16;
    int32_t cell_height = 16;
    float grid_radius = b2_polygonRadius;

    for (int32_t j = 0; j < columns; ++j)
    {
        VisualObject vo_a;
        vo_a.m_Position = Point3(0, 0, 0);
        dmPhysics::CollisionObjectData data;
        data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
        data.m_Mass = 0.0f;
        data.m_UserData = &vo_a;
        data.m_Group = 0xffff;
        data.m_Mask = 0xffff;
        data.m_Restitution = 0.0f;

        const float hull_vertices[] = {  // 1x1 around origo
                                        -0.5f, -0.5f,
                                         0.5f, -0.5f,
                                         0.5f,  0.5f,
                                        -0.5f,  0.5f,

                                         // 1x0.5 with top aligned to x-axis
                                        -0.5f, -0.5f,
                                         0.5f, -0.5f,
                                         0.5f,  0.0f,
                                        -0.5f,  0.0f };

        const dmPhysics::HullDesc hulls[] = { {0, 4}, {4, 4} };
        dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 8, hulls, 2);
        dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
        typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

        for (int32_t row = 0; row < rows; ++row)
        {
            for (int32_t col = 0; col < columns; ++col)
            {
                dmPhysics::SetGridShapeHull(grid_co, 0, row, col, 0, EMPTY_FLAGS);
            }
        }

        VisualObject vo_b;
        vo_b.m_Position = Point3(-columns * cell_width * 0.5f + cell_width * 0.5f + cell_width * j, 50.0f, 0.0f);
        data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
        data.m_Mass = 1.0f;
        data.m_UserData = &vo_b;
        data.m_Group = 0xffff;
        data.m_Mask = 0xffff;
        data.m_Restitution = 0.0f;
        typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 0.5f, 0.0f));
        typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

        for (int32_t col = 0; col < columns; ++col)
        {
            uint32_t child = columns * (rows - 1) + col;
            // Set group for last row to [0, 1, 2, 4, ...]. The box should not collide with the first object in the last row.
            // See ASSERT_NEAR below for the special case
            dmPhysics::SetCollisionObjectFilter(grid_co, 0, child, (1 << col) >> 1, 0xffff);
        }

        // Set the last cell in the last row to the smaller hull
        dmPhysics::SetGridShapeHull(grid_co, 0, rows - 1, columns - 1, 1, EMPTY_FLAGS);

        // Remove the cell just before the last cell
        dmPhysics::SetGridShapeHull(grid_co, 0, rows - 1, columns - 2, dmPhysics::GRIDSHAPE_EMPTY_CELL, EMPTY_FLAGS);

        // Remove the cell temporarily and add after first simulation.
        // Test this to make sure that the broad-phase is properly updated
        dmPhysics::SetGridShapeHull(grid_co, 0, rows - 1, 2, dmPhysics::GRIDSHAPE_EMPTY_CELL, EMPTY_FLAGS);

        for (uint32_t i = 0; i < 400; ++i)
        {
            (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

            // Set cell to hull 1 removed above. See comment above
            if (i == 0)
            {
                dmPhysics::SetGridShapeHull(grid_co, 0, rows - 1, 2, 0, EMPTY_FLAGS);
            }
        }

        if (j == 0)
        {
            // Box should fall through last row to next
            ASSERT_NEAR(16.0f + grid_radius + 0.5f, vo_b.m_Position.getY(), 0.05f);
            ASSERT_EQ(0xffff, vo_a.m_FirstCollisionGroup);
        }
        else
        {
            // Should collide with last row

            if (j == columns - 1)
            {
                // Special case for last cell, the smaller hull
                ASSERT_NEAR(24.0f + grid_radius + 0.5f, vo_b.m_Position.getY(), 0.05f);
            }
            else if (j == columns - 2)
            {
                // Special case for the cell just before the last cell. The cell is removed
                // and the box should fall-through
                ASSERT_NEAR(16.0f + grid_radius + 0.5f, vo_b.m_Position.getY(), 0.05f);
            }
            else
            {
                // "General" case
                ASSERT_NEAR(32.0f + grid_radius + 0.5f, vo_b.m_Position.getY(), 0.05f);
            }

            if (j == columns - 2)
            {
                // Fall through case (removed hull), group should be 0xffff i
                ASSERT_EQ(0xffff, vo_a.m_FirstCollisionGroup);
            }
            else
            {
                ASSERT_EQ((1 << j) >> 1, vo_a.m_FirstCollisionGroup);
            }
        }

        (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
        (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
        (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
        (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
        dmPhysics::DeleteHullSet2D(hull_set);
    }
}

TYPED_TEST(PhysicsTest, GridShapeSphere)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 4;
    int32_t columns = 10;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(1, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {  // 1x1 around origo
                                    -0.5f, -0.5f,
                                     0.5f, -0.5f,
                                     0.5f,  0.5f,
                                    -0.5f,  0.5f,

                                     // 1x0.5 with top aligned to x-axis
                                    -0.5f, -0.5f,
                                     0.5f, -0.5f,
                                     0.5f,  0.0f,
                                    -0.5f,  0.0f };

    const dmPhysics::HullDesc hulls[] = { {0, 4}, {4, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 8, hulls, 2);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    for (int32_t row = 0; row < rows; ++row)
    {
        for (int32_t col = 0; col < columns; ++col)
        {
            dmPhysics::SetGridShapeHull(grid_co, 0, row, col, 0, EMPTY_FLAGS);
        }
    }

    VisualObject vo_b;
    vo_b.m_Position = Point3(1, 33.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 0.5f);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    for (uint32_t i = 0; i < 30; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_NEAR(32.0f + 0.5f, vo_b.m_Position.getY(), 0.05f);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

bool TriggerCollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
{
    VisualObject* vo = (VisualObject*)user_data_a;
    if (vo->m_CollisionCount == 0)
        vo->m_FirstCollisionGroup = group_a;
    ++vo->m_CollisionCount;

    vo = (VisualObject*)user_data_b;
    if (vo->m_CollisionCount == 0)
        vo->m_FirstCollisionGroup = group_b;
    ++vo->m_CollisionCount;
    int* count = (int*)user_data;
    *count += 1;
    return true;
}

TYPED_TEST(PhysicsTest, GridShapeTrigger)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 1;
    int32_t columns = 2;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {  // 1x1 around origo
                                    -0.5f, -0.5f,
                                     0.5f, -0.5f,
                                     0.5f,  0.5f,
                                    -0.5f,  0.5f };

    const dmPhysics::HullDesc hulls[] = { {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 1);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 0, 0, EMPTY_FLAGS);
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 1, 0, EMPTY_FLAGS);

    VisualObject vo_b;
    vo_b.m_Position = Point3(4.0f, 4.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_TRIGGER;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.1f, 0.1f, 0.1f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    TestFixture::m_StepWorldContext.m_CollisionCallback = TriggerCollisionCallback;
    for (int i = 0; i < 500; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
        ASSERT_EQ(i+1, TestFixture::m_CollisionCount);
        ASSERT_EQ(0, TestFixture::m_ContactPointCount);
    }

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

struct CrackUserData
{
    CrackUserData()
    {
        memset(this, 0, sizeof(CrackUserData));
    }

    VisualObject* m_Target;
    Vector3 m_Normal;
    Vector3 m_AccumNormal;
    float m_Distance;
    uint32_t m_Count;
};

bool CrackContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
{
    CrackUserData* data = (CrackUserData*)user_data;
    data->m_Normal = contact_point.m_Normal;
    data->m_Distance = contact_point.m_Distance;
    data->m_AccumNormal += contact_point.m_Normal;
    ++data->m_Count;
    if (data->m_Target == contact_point.m_UserDataA)
    {
        data->m_Normal = -data->m_Normal;
    }
    return true;
}

// Tests colliding a thin box between two cells of a grid shape, due to a bug that disregards such collisions
// Note: We changed the implementation of the tilegrid to only do polygon checks (as opposed to edge checks when this test was written)
// The current implementation makes the resolve normals point sideways instead
TYPED_TEST(PhysicsTest, GridShapeCrack)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 1;
    int32_t columns = 2;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {-0.5f, -0.5f,
                                    0.5f, -0.5f,
                                    0.5f,  0.5f,
                                   -0.5f,  0.5f};

    const dmPhysics::HullDesc hulls[] = { {0, 4}, {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 2);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    for (int32_t row = 0; row < rows; ++row)
    {
        for (int32_t col = 0; col < columns; ++col)
        {
            dmPhysics::SetGridShapeHull(grid_co, 0, row, col, 0, EMPTY_FLAGS);
        }
    }

    VisualObject vo_b;
    vo_b.m_Position = Point3(0.0f, 12.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(2.0f, 8.0f, 8.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    CrackUserData crack_data;
    crack_data.m_Target = &vo_b;
    TestFixture::m_StepWorldContext.m_ContactPointCallback = CrackContactPointCallback;
    TestFixture::m_StepWorldContext.m_ContactPointUserData = &crack_data;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    float eps = 0.000001f;
    ASSERT_EQ(2u, crack_data.m_Count);
    ASSERT_EQ(1.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    // We get two conflicting normals (left + right x axis)
    ASSERT_EQ(0.0f, crack_data.m_AccumNormal.getX());
    ASSERT_EQ(0.0f, crack_data.m_AccumNormal.getY());
    ASSERT_EQ(0.0f, crack_data.m_AccumNormal.getZ());
    ASSERT_NEAR(2.0f, crack_data.m_Distance, eps);
    ASSERT_EQ(2, vo_a.m_CollisionCount);
    ASSERT_EQ(2, vo_b.m_CollisionCount);

    vo_b.m_Position = Point3(0.0f, 8.1f, 0.0f);
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(1.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    vo_b.m_Position = Point3(0.0f, 7.9f, 0.0f);
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(1.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

// We changed tilegrids from doing edge checks to polygon checks
// in order to get more solid collisions
TYPED_TEST(PhysicsTest, PolygonShape)
{
    int32_t rows = 1;
    int32_t columns = 2;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {-0.5f, -0.5f,
                                    0.5f, -0.5f,
                                    0.5f,  0.5f,
                                   -0.5f,  0.5f};

    const dmPhysics::HullDesc hulls[] = { {0, 4}, {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 2);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    for (int32_t row = 0; row < rows; ++row)
    {
        for (int32_t col = 0; col < columns; ++col)
        {
            dmPhysics::SetGridShapeHull(grid_co, 0, row, col, 0, EMPTY_FLAGS);
        }
    }

    VisualObject vo_b;
    vo_b.m_Position = Point3(5.0f, 8.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(2.0f, 2.0f, 8.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    CrackUserData crack_data;
    crack_data.m_Target = &vo_b;
    TestFixture::m_StepWorldContext.m_ContactPointCallback = CrackContactPointCallback;
    TestFixture::m_StepWorldContext.m_ContactPointUserData = &crack_data;

    float eps = 0.000001f;

    // Put it completely within one cell, but closer to one of the non-shared edges, and it should resolve that direction
    vo_b.m_Position = Point3(5.0f, 7.9f, 0.0f);
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(1u, crack_data.m_Count);
    ASSERT_EQ(0.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(1.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    ASSERT_NEAR(2.1f, crack_data.m_Distance, eps);
    ASSERT_EQ(1, vo_a.m_CollisionCount);
    ASSERT_EQ(1, vo_b.m_CollisionCount);

    vo_b.m_Position = Point3(5.0f, 0.0f, 0.0f);
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(-1.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    ASSERT_NEAR(7.0f, crack_data.m_Distance, eps);

    vo_b.m_Position = Point3(16.0f - 5.0f, 0.0f, 0.0f);
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(1.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    ASSERT_NEAR(7.0f, crack_data.m_Distance, eps);

    vo_b.m_Position = Point3(8.0f, 7.0f, 0.0f);
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(0.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(1.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    ASSERT_NEAR(3.0f, crack_data.m_Distance, eps);
    vo_b.m_Position = Point3(8.0f, -7.0f, 0.0f);
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(0.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(-1.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    ASSERT_NEAR(3.0f, crack_data.m_Distance, eps);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

// Tests colliding a thin box at the corner of a cell of a grid shape
TYPED_TEST(PhysicsTest, GridShapeCorner)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 2;
    int32_t columns = 1;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {-0.5f, -0.5f,
                                    0.5f, -0.5f,
                                    0.5f,  0.5f,
                                   -0.5f,  0.5f};

    const dmPhysics::HullDesc hulls[] = { {0, 4}, {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 2);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    for (int32_t row = 0; row < rows; ++row)
    {
        for (int32_t col = 0; col < columns; ++col)
        {
            dmPhysics::SetGridShapeHull(grid_co, 0, row, col, 0, EMPTY_FLAGS);
        }
    }

    VisualObject vo_b;
    vo_b.m_Position = Point3(-10.0f, 24.0f, 0.0f) + Vector3(0.15f, -0.1f, 0.0);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(2.0f, 8.0f, 8.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    CrackUserData crack_data;
    crack_data.m_Target = &vo_b;
    TestFixture::m_StepWorldContext.m_ContactPointCallback = CrackContactPointCallback;
    TestFixture::m_StepWorldContext.m_ContactPointUserData = &crack_data;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    float eps = 0.000001f;
    ASSERT_EQ(1u, crack_data.m_Count);
    ASSERT_EQ(0.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(1.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    ASSERT_NEAR(0.1f, crack_data.m_Distance, eps);
    ASSERT_EQ(1, vo_a.m_CollisionCount);
    ASSERT_EQ(1, vo_b.m_CollisionCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

// Tests colliding a sphere against a grid shape (edge) to verify collision distance
TYPED_TEST(PhysicsTest, GridShapeSphereDistance)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 1;
    int32_t columns = 3;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {-0.5f, -0.5f,
                                    0.5f, -0.5f,
                                    0.5f,  0.5f,
                                   -0.5f,  0.5f};

    const dmPhysics::HullDesc hulls[] = { {0, 4}, {0, 4}, {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 3);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    for (int32_t row = 0; row < rows; ++row)
    {
        for (int32_t col = 0; col < columns; ++col)
        {
            dmPhysics::SetGridShapeHull(grid_co, 0, row, col, 0, EMPTY_FLAGS);
        }
    }

    VisualObject vo_b;
    vo_b.m_Position = Point3(0.0f, 14.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 8.0f);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    CrackUserData crack_data;
    crack_data.m_Target = &vo_b;
    TestFixture::m_StepWorldContext.m_ContactPointCallback = CrackContactPointCallback;
    TestFixture::m_StepWorldContext.m_ContactPointUserData = &crack_data;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    float eps = 0.000001f;
    ASSERT_EQ(1u, crack_data.m_Count);
    ASSERT_EQ(0.0f, crack_data.m_Normal.getX());
    ASSERT_EQ(1.0f, crack_data.m_Normal.getY());
    ASSERT_EQ(0.0f, crack_data.m_Normal.getZ());
    ASSERT_NEAR(2.0f, crack_data.m_Distance, eps);
    ASSERT_EQ(1, vo_a.m_CollisionCount);
    ASSERT_EQ(1, vo_b.m_CollisionCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

// Tests that many grid shapes can be constructed and having an increasing number of tiles per shape (earlier bug)
TYPED_TEST(PhysicsTest, GridShapeMultiLayered)
{
    int32_t rows = 1;
    int32_t columns = 3;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    const float hull_vertices[] = {-0.5f, -0.5f,
                                    0.5f, -0.5f,
                                    0.5f,  0.5f,
                                   -0.5f,  0.5f};

    const dmPhysics::HullDesc hulls[] = { {0, 4}, {0, 4}, {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 3);
    dmPhysics::HCollisionShape2D grid_shapes[2];
    grid_shapes[0] = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, 1);
    grid_shapes[1] = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, 2);
    dmPhysics::CollisionObjectData data;
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, grid_shapes, 2u);

    for (int32_t layer = 0; layer < 2; ++layer)
    {
        for (int32_t row = 0; row < rows; ++row)
        {
            // increasing number of columns per layer (testing earlier bug)
            columns = layer + 1;
            for (int32_t col = 0; col < columns; ++col)
            {
                dmPhysics::SetGridShapeHull(grid_co, layer, row, col, 0, EMPTY_FLAGS);
            }
        }
    }

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shapes[0]);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shapes[1]);
    dmPhysics::DeleteHullSet2D(hull_set);
}


void RayCastCallback(const dmPhysics::RayCastResponse& response, const dmPhysics::RayCastRequest& request, void* user_data)
{
   std::vector<dmPhysics::RayCastResponse>* responses = (std::vector<dmPhysics::RayCastResponse>*) user_data;
   if (response.m_Hit)
   {
       (*responses).push_back(response);
   }
}

TYPED_TEST(PhysicsTest, GridShapeRayCast)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 2;
    int32_t columns = 2;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(1, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {  // 1x1 around origo
                                    -0.5f, -0.5f,
                                     0.5f, -0.5f,
                                     0.5f,  0.5f,
                                    -0.5f,  0.5f };

    const dmPhysics::HullDesc hulls[] = { {0, 4}, {4, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 1);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    for (int32_t row = 0; row < rows; ++row)
    {
        for (int32_t col = 0; col < columns; ++col)
        {
            dmPhysics::SetGridShapeHull(grid_co, 0, row, col, 0, EMPTY_FLAGS);
        }
    }

    std::vector<dmPhysics::RayCastResponse> responses;
    dmPhysics::RayCastRequest ray_request;
    ray_request.m_From = Point3(-100, 4, 0);
    ray_request.m_To = Point3(100, 4, 0);
    ray_request.m_IgnoredUserData = 0;
    ray_request.m_UserData = 0;
    ray_request.m_Mask = 0xffff;
    ray_request.m_UserId = 0;
    TestFixture::m_Test.m_RequestRayCastFunc(TestFixture::m_World, ray_request);

    TestFixture::m_StepWorldContext.m_RayCastCallback = RayCastCallback;
    TestFixture::m_StepWorldContext.m_RayCastUserData = (void*) &responses;

    responses.clear();
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(1U, responses.size());
    ASSERT_NEAR(-15.0f, responses[0].m_Position.getX(), 0.0001f);

    // Clear all hulls
    for (int32_t row = 0; row < rows; ++row)
    {
        for (int32_t col = 0; col < columns; ++col)
        {
            dmPhysics::SetGridShapeHull(grid_co, 0, row, col, dmPhysics::GRIDSHAPE_EMPTY_CELL, EMPTY_FLAGS);
        }
    }

    // Do ray-cast. Should not hit anything
    responses.clear();
    TestFixture::m_Test.m_RequestRayCastFunc(TestFixture::m_World, ray_request);
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(0U, responses.size());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

TYPED_TEST(PhysicsTest, ScaledCircle)
{
    VisualObject vo_a;
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 0.5f, 0.0f));
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_a, 1u);

    VisualObject vo_b;
    vo_b.m_Position.setY(1.5f);
    vo_b.m_Scale = 2.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 0.5f);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_b, 1u);

    for (uint32_t i = 0; i < 40; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_LT(1.5f, vo_b.m_Position.getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
}

TYPED_TEST(PhysicsTest, ScaledBox)
{
    VisualObject vo_a;
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 0.5f, 0.0f));
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_a, 1u);

    VisualObject vo_b;
    vo_b.m_Position.setY(1.5f);
    vo_b.m_Scale = 2.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 0.5f, 0.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_b, 1u);

    for (uint32_t i = 0; i < 40; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_LT(1.5f, vo_b.m_Position.getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
}

TYPED_TEST(PhysicsTest, ScaledGrid)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 1;
    int32_t columns = 1;
    int32_t cell_width = 1;
    int32_t cell_height = 1;

    VisualObject vo_a;
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 0.5f, 0.0f));
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_a, 1u);

    VisualObject vo_b;
    vo_b.m_Position.setY(1.5f);
    vo_b.m_Scale = 2.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    const float hull_vertices[] = {  // 1x1 around origo
                                    -0.5f, -0.5f,
                                     0.5f, -0.5f,
                                     0.5f,  0.5f,
                                    -0.5f,  0.5f };

    const dmPhysics::HullDesc hulls[] = { {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 1);
    dmPhysics::HCollisionShape2D shape_b = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_b, 1u);
    dmPhysics::SetGridShapeHull(dynamic_co, 0, 0, 0, 0, EMPTY_FLAGS);

    for (uint32_t i = 0; i < 40; ++i)
    {
        (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    }

    ASSERT_LT(1.5f, vo_b.m_Position.getY());

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
    dmPhysics::DeleteHullSet2D(hull_set);
}

// Test that contacts are removed when a grid shape hull cell is cleared while having an adjacent body
TYPED_TEST(PhysicsTest, ClearGridShapeHull)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 2;
    int32_t columns = 2;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {  // 1x1 around origo
                                    -0.5f, -0.5f,
                                     0.5f, -0.5f,
                                     0.5f,  0.5f,
                                    -0.5f,  0.5f };

    const dmPhysics::HullDesc hulls[] = { {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 1);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    // An "L" of set hulls
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 0, 0, EMPTY_FLAGS);
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 1, 0, EMPTY_FLAGS);
    dmPhysics::SetGridShapeHull(grid_co, 0, 1, 0, 0, EMPTY_FLAGS);
    dmPhysics::SetGridShapeHull(grid_co, 0, 1, 1, ~0u, EMPTY_FLAGS); // upper right corner is cleared

    VisualObject vo_b;
    // the sphere is centered in the upper right quadrant, which is cleared
    vo_b.m_Position = Point3(8.0f, 8.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 8.0f);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    // TODO Asserting for 2 here is actually a bug, it should be 1 for one body in collision with 1 other
    // Reported here: https://defold.fogbugz.com/default.asp?2091
    ASSERT_EQ(2, TestFixture::m_CollisionCount);
    ASSERT_EQ(2, TestFixture::m_ContactPointCount);

    // Clear the "top" of the "L"
    dmPhysics::SetGridShapeHull(grid_co, 0, 1, 0, ~0u, EMPTY_FLAGS);

    // Reset counts
    TestFixture::m_CollisionCount = 0;
    TestFixture::m_ContactPointCount = 0;

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);

    ASSERT_EQ(1, TestFixture::m_CollisionCount);
    ASSERT_EQ(1, TestFixture::m_ContactPointCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

// Test that a grid shape hull cell set to an empty hull is treated as a cleared cell
TYPED_TEST(PhysicsTest, GridShapeEmptyHull)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 1;
    int32_t columns = 1;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {  // 1x1 around origo
                                    -0.5f, -0.5f,
                                     0.5f, -0.5f,
                                     0.5f,  0.5f,
                                    -0.5f,  0.5f };

    // Single empty hull
    const dmPhysics::HullDesc hulls[] = { {0, 0} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 1);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    // A single cell pointing to the empty hull above
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 0, 0, EMPTY_FLAGS);

    VisualObject vo_b;
    // the box is slightly above, half way inside the empty hull
    vo_b.m_Position = Point3(0.0f, 8.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(8.0f, 8.0f, 0.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(0, TestFixture::m_CollisionCount);
    ASSERT_EQ(0, TestFixture::m_ContactPointCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}

// Test that grid shape cells are flipped correctly
TYPED_TEST(PhysicsTest, GridShapeFlipped)
{
    /*
     * Simplified version of GridShapePolygon
     */
    int32_t rows = 1;
    int32_t columns = 1;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    const float hull_vertices[] = {  // 1x1 around origo
                                     0.0f,  0.0f,
                                     0.5f,  0.0f,
                                     0.5f,  0.5f,
                                     0.0f,  0.5f };

    // Single hull
    const dmPhysics::HullDesc hulls[] = { {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 1);
    dmPhysics::HCollisionShape2D grid_shape = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType grid_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &grid_shape, 1u);

    dmPhysics::HullFlags flags = EMPTY_FLAGS;

    // A single cell pointing to the empty hull above
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 0, 0, flags);

    VisualObject vo_b;
    // the sphere is in the upper right quadrant
    vo_b.m_Position = Point3(9.0f, 9.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_KINEMATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_b;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;
    typename TypeParam::CollisionShapeType shape = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 8.0f);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape, 1u);

    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(1, TestFixture::m_CollisionCount);

    flags.m_FlipHorizontal = 1;
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 0, 0, flags);

    TestFixture::m_CollisionCount = 0;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(0, TestFixture::m_CollisionCount);

    flags.m_FlipHorizontal = 0;
    flags.m_FlipVertical = 1;
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 0, 0, flags);

    TestFixture::m_CollisionCount = 0;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(0, TestFixture::m_CollisionCount);

    flags.m_FlipHorizontal = 1;
    flags.m_FlipVertical = 1;
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 0, 0, flags);

    TestFixture::m_CollisionCount = 0;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(0, TestFixture::m_CollisionCount);

    flags.m_FlipHorizontal = 0;
    flags.m_FlipVertical = 0;
    dmPhysics::SetGridShapeHull(grid_co, 0, 0, 0, 0, flags);

    TestFixture::m_CollisionCount = 0;
    (*TestFixture::m_Test.m_StepWorldFunc)(TestFixture::m_World, TestFixture::m_StepWorldContext);
    ASSERT_EQ(1, TestFixture::m_CollisionCount);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, grid_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(grid_shape);
    dmPhysics::DeleteHullSet2D(hull_set);
}


struct ContactPointTestUserData
{
    int m_Count;
    dmPhysics::ContactPoint m_LastCP;
};

static void ContactPointTestCallback(const dmPhysics::ContactPoint& cp, void* user_ctx)
{
    ContactPointTestUserData* user_data = (ContactPointTestUserData*)user_ctx;
    user_data->m_Count++;
    user_data->m_LastCP = cp;
}

TYPED_TEST(PhysicsTest, ContactTestBox)
{
    VisualObject vo_a;
    dmPhysics::CollisionObjectData data;
    vo_a.m_Position = Point3(5.0f, 5.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 2.0f, 0.0f));
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_a, 1u);

    VisualObject vo_b;
    vo_b.m_Position.setY(1.5f);
    vo_b.m_Scale = 1.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 0.5f, 0.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_b, 1u);

    ContactPointTestUserData ctx;

    Quat rotation = Quat::rotation(1, Vector3::xAxis());

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(0,0,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,1.5,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5.5,5,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(2, ctx.m_Count);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
}

TYPED_TEST(PhysicsTest, ContactTestSphere)
{
    VisualObject vo_a;
    dmPhysics::CollisionObjectData data;
    vo_a.m_Position = Point3(5.0f, 5.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 1.0f);
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_a, 1u);

    VisualObject vo_b;
    vo_b.m_Scale = 1.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 1.0f);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_b, 1u);

    ContactPointTestUserData ctx;
    ctx.m_Count = 0;

    Quat rotation = Quat::rotation(1, Vector3::xAxis());

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(0,0,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,2,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,3.5,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(1, ctx.m_Count);
    ASSERT_EQ(4.25f, ctx.m_LastCP.m_PositionA.getY());
    ASSERT_EQ(4.25f, ctx.m_LastCP.m_PositionB.getY());
    ASSERT_EQ(0.5f, ctx.m_LastCP.m_Distance);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,3,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(1, ctx.m_Count);
    ASSERT_EQ(4.0f, ctx.m_LastCP.m_PositionA.getY());
    ASSERT_EQ(4.0f, ctx.m_LastCP.m_PositionB.getY());
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Distance);


    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
}


TYPED_TEST(PhysicsTest, ContactTestBoxSphere)
{
    VisualObject vo_a;
    dmPhysics::CollisionObjectData data;
    vo_a.m_Position = Point3(5.0f, 5.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 2.0f, 0.0f));
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_a, 1u);

    VisualObject vo_b;
    vo_b.m_Scale = 1.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 1.0f);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_b, 1u);

    ContactPointTestUserData ctx;
    ctx.m_Count = 0;

    Quat rotation = Quat::rotation(1, Vector3::xAxis());

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,1,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,2,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(1, ctx.m_Count);
    ASSERT_NEAR(3.0f, ctx.m_LastCP.m_PositionA.getY(), 0.02f);
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Normal.getX());
    ASSERT_EQ(-1.0f, ctx.m_LastCP.m_Normal.getY());
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Distance);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(3,4,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(3.5,4,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(1, ctx.m_Count);
    ASSERT_NEAR(4.5f, ctx.m_LastCP.m_PositionA.getX(), 0.02f);
    ASSERT_EQ(-1.0f, ctx.m_LastCP.m_Normal.getX());
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Normal.getY());
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Distance);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
}

TYPED_TEST(PhysicsTest, ContactTestSphereBox)
{
    VisualObject vo_a;
    dmPhysics::CollisionObjectData data;
    vo_a.m_Position = Point3(5.0f, 5.0f, 0.0f);
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    typename TypeParam::CollisionShapeType shape_a = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 1.0f);
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_a, 1u);

    VisualObject vo_b;
    vo_b.m_Position.setY(1.5f);
    vo_b.m_Scale = 1.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewBoxShapeFunc)(TestFixture::m_Context, Vector3(0.5f, 0.5f, 0.0f));
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_b, 1u);

    ContactPointTestUserData ctx;
    ctx.m_Count = 0;

    Quat rotation = Quat::rotation(1, Vector3::xAxis());

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,1,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,3.5f,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(1, ctx.m_Count);
    ASSERT_NEAR(4.0f, ctx.m_LastCP.m_PositionA.getY(), 0.02f);
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Normal.getX());
    ASSERT_EQ(-1.0f, ctx.m_LastCP.m_Normal.getY());
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Distance);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(3,7,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,6.5f,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(1, ctx.m_Count);
    ASSERT_NEAR(6.0f, ctx.m_LastCP.m_PositionA.getY(), 0.02f);
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Normal.getX());
    ASSERT_EQ(1.0f, ctx.m_LastCP.m_Normal.getY());
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Distance);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
}


TYPED_TEST(PhysicsTest, ContactTestTileGridSphere)
{
    int32_t rows = 1;
    int32_t columns = 2;
    int32_t cell_width = 16;
    int32_t cell_height = 16;

    VisualObject vo_a;
    vo_a.m_Position = Point3(0, 0, 0);
    dmPhysics::CollisionObjectData data;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_STATIC;
    data.m_Mass = 0.0f;
    data.m_UserData = &vo_a;
    data.m_Group = 0xffff;
    data.m_Mask = 0xffff;

    // The tile center is (0,0)
    const float hull_vertices[] = {-0.5f, -0.5f,
                                    0.5f, -0.5f,
                                    0.5f,  0.5f,
                                   -0.5f,  0.5f};

    const dmPhysics::HullDesc hulls[] = { {0, 4}, {0, 4} };
    dmPhysics::HHullSet2D hull_set = dmPhysics::NewHullSet2D(TestFixture::m_Context, hull_vertices, 4, hulls, 2);
    dmPhysics::HCollisionShape2D shape_a = dmPhysics::NewGridShape2D(TestFixture::m_Context, hull_set, Point3(0,0,0), cell_width, cell_height, rows, columns);
    typename TypeParam::CollisionObjectType static_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_a, 1u);

    for (int32_t row = 0; row < rows; ++row)
    {
        for (int32_t col = 0; col < columns; ++col)
        {
            dmPhysics::SetGridShapeHull(static_co, 0, row, col, 0, EMPTY_FLAGS);
        }
    }

    VisualObject vo_b;
    vo_b.m_Scale = 1.0f;
    data.m_Type = dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;
    data.m_UserData = &vo_b;
    typename TypeParam::CollisionShapeType shape_b = (*TestFixture::m_Test.m_NewSphereShapeFunc)(TestFixture::m_Context, 1.0f);
    typename TypeParam::CollisionObjectType dynamic_co = (*TestFixture::m_Test.m_NewCollisionObjectFunc)(TestFixture::m_World, data, &shape_b, 1u);

    ContactPointTestUserData ctx;
    ctx.m_Count = 0;

    Quat rotation = Quat::rotation(1, Vector3::xAxis());

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(0,10,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,9.5,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(0, ctx.m_Count);

    ctx.m_Count = 0;
    (*TestFixture::m_Test.m_ContactPointTestFunc)(TestFixture::m_Context, dynamic_co, Point3(5,9,0), rotation, ContactPointTestCallback, (void*)&ctx );
    ASSERT_EQ(1, ctx.m_Count);
    ASSERT_NEAR(8.0f, ctx.m_LastCP.m_PositionA.getY(), 0.02f);
    ASSERT_NEAR(8.0f, ctx.m_LastCP.m_PositionB.getY(), 0.02f);
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Normal.getX());
    ASSERT_EQ(1.0f, ctx.m_LastCP.m_Normal.getY());
    ASSERT_EQ(0.0f, ctx.m_LastCP.m_Distance);

    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, static_co);
    (*TestFixture::m_Test.m_DeleteCollisionObjectFunc)(TestFixture::m_World, dynamic_co);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_a);
    (*TestFixture::m_Test.m_DeleteCollisionShapeFunc)(shape_b);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
