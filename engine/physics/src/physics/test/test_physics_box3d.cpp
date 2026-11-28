// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/transform.h>
#include <dlib/array.h>
#include <dmsdk/physics/box3d.hpp>
#include "../physics.h"
#include "../box3d/physics_box3d.h"

using namespace dmPhysics;
using namespace dmVMath;

struct Actor
{
    dmTransform::Transform m_Transform;
};
static void ReadPose(void* owner, dmTransform::Transform& transform)
{
    transform = ((Actor*)owner)->m_Transform;
}

static void WritePose(void* owner, const Point3& position, const Quat& rotation)
{
    Actor* actor = (Actor*)owner;
    actor->m_Transform.SetTranslation(Vector3(position));
    actor->m_Transform.SetRotation(rotation);
}

struct Hits
{
    void* m_Floor;
    void* m_Ball;
    int   m_Begins;
    int   m_Impacts;
};
static bool Begin(void* a, uint16_t group_a, void* b, uint16_t group_b, void* context)
{
    Hits* hits = (Hits*)context;
    EXPECT_TRUE((a == hits->m_Floor && b == hits->m_Ball) || (b == hits->m_Floor && a == hits->m_Ball));
    ++hits->m_Begins;
    return true;
}

static bool Hit(const ContactPoint& point, void* context)
{
    Hits* hits = (Hits*)context;
    EXPECT_GT(point.m_AppliedImpulse, 0.0f);
    ++hits->m_Impacts;
    return true;
}

static void Simulate(bool subscribe_floor, bool subscribe_ball, bool component_events)
{
    NewContextParams params;
    params.m_Scale = 0.5f;
    HContext3D context = NewContext3D(params);
    ASSERT_NE((HContext3D)0, context);
    NewWorldParams world_params;
    world_params.m_GetWorldTransformCallback = ReadPose;
    world_params.m_SetWorldTransformCallback = WritePose;
    HWorld3D world = NewWorld3D(context, world_params);
    ASSERT_NE((HWorld3D)0, world);

    Actor floor, ball;
    floor.m_Transform.SetIdentity();
    floor.m_Transform.SetScale(Vector3(10.0f, 1.0f, 10.0f));
    ball.m_Transform.SetIdentity();
    ball.m_Transform.SetTranslation(Vector3(2.0f, 4.0f, 0.0f));
    CollisionObjectData def;
    def.m_UserData = &floor;
    def.m_Type = COLLISION_OBJECT_TYPE_STATIC;
    def.m_Mass = 0.0f;
    def.m_EventContact = def.m_EventCollision = subscribe_floor;
    HCollisionShape3D box = NewBoxShape3D(context, Vector3(0.5f));
    HCollisionObject3D platform = NewCollisionObject3D(world, def, &box, 1);
    ASSERT_NE((HCollisionObject3D)0, platform);
    DeleteCollisionShape3D(box);
    def.m_UserData = &ball;
    def.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC;
    def.m_Mass = 3.0f;
    def.m_Enabled = false;
    def.m_EventContact = def.m_EventCollision = subscribe_ball;
    HCollisionShape3D sphere = NewSphereShape3D(context, 0.5f);
    HCollisionObject3D projectile = NewCollisionObject3D(world, def, &sphere, 1);
    ASSERT_NE((HCollisionObject3D)0, projectile);
    DeleteCollisionShape3D(sphere);
    SetLinearVelocity3D(context, projectile, Vector3(0.0f, -1.0f, 0.0f));
    SetEnabled3D(world, projectile, true);
    ASSERT_NEAR(-1.0f, GetLinearVelocity3D(context, projectile).getY(), 0.0001f);
    ASSERT_NEAR(3.0f, GetMass3D(projectile), 0.001f);

    Hits hits = { &floor, &ball, 0, 0 };
    StepWorldContext step;
    step.m_DT = 1.0f / 30.0f;
    step.m_CollisionCallback = Begin;
    step.m_CollisionUserData = &hits;
    step.m_ContactPointCallback = Hit;
    step.m_ContactPointUserData = &hits;
    b3WorldId wid = *(b3WorldId*)GetWorldContext3D(world);
    ASSERT_EQ(1, b3World_GetWorkerCount(wid));
    SetBox3DPaused(wid, true);
    ASSERT_FALSE(StepWorld3D(world, step));
    ASSERT_EQ(4.0f, ball.m_Transform.GetTranslation().getY());
    SetBox3DPaused(wid, false);
    SetBox3DComponentEvents(wid, component_events);
    int native_hits = 0;
    for (int i = 0; i < 90; ++i)
    {
        ASSERT_TRUE(StepWorld3D(world, step));
        native_hits += b3World_GetContactEvents(wid).hitCount;
    }

    ASSERT_NEAR(1.0f, ball.m_Transform.GetTranslation().getY(), 0.035f);
    int expected = subscribe_floor || subscribe_ball ? 1 : 0;
    ASSERT_EQ(expected, native_hits);
    expected = component_events ? expected : 0;
    ASSERT_EQ(expected, hits.m_Begins);
    ASSERT_EQ(expected, hits.m_Impacts);
    b3BodyId id = *(b3BodyId*)GetCollisionObjectContext3D(projectile);
    DeleteCollisionObject3D(world, projectile);
    ASSERT_FALSE(b3Body_IsValid(id));
    DeleteCollisionObject3D(world, platform);
    DeleteWorld3D(context, world);
    ASSERT_FALSE(b3World_IsValid(wid));
    DeleteContext3D(context);
}

// A scaled component floor stops the projectile and an interested pair gets one event per impact.
TEST(Box3D, ComponentContact)
{
    Simulate(false, true, true);
}

// Both components subscribe to the same impact and receive one pair in the listener.
TEST(Box3D, BothComponentsSubscribe)
{
    Simulate(true, true, true);
}

// Turning both subscriptions off suppresses callbacks while preserving the physical collision.
TEST(Box3D, UnsubscribedContact)
{
    Simulate(false, false, true);
}

// A native observer still receives the hit after component event delivery is disabled.
TEST(Box3D, NativeEventsOnly)
{
    Simulate(true, true, false);
}

// Factory-created bodies retain their assigned motion through enable and update their game object.
TEST(Box3D, BodyMotionBeforeEnable)
{
    NewContextParams params;
    params.m_Scale = 0.25f;
    HContext3D context = NewContext3D(params);
    NewWorldParams world_params;
    world_params.m_GetWorldTransformCallback = ReadPose;
    world_params.m_SetWorldTransformCallback = WritePose;
    HWorld3D world = NewWorld3D(context, world_params);
    Actor actor;
    actor.m_Transform.SetIdentity();
    CollisionObjectData def;
    def.m_UserData = &actor;
    def.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC;
    def.m_Mass = 1.0f;
    def.m_Enabled = false;
    HCollisionShape3D sphere = NewSphereShape3D(context, 1.0f);
    HCollisionObject3D object = NewCollisionObject3D(world, def, &sphere, 1);
    b3BodyId id = *(b3BodyId*)GetCollisionObjectContext3D(object);
    SetBox3DAwake(id, false);

    Box3DBodyMotion motion;
    motion.m_Position = Vector3(2.0f, -4.0f, 8.0f);
    motion.m_Rotation = Quat::rotationY(0.7f);
    motion.m_LinearVelocity = Vector3(-3.0f, 5.0f, 1.0f);
    motion.m_AngularVelocity = Vector3(1.0f, 2.0f, -1.0f);
    SetBox3DBodyMotion(id, motion, true);
    ASSERT_FALSE(b3Body_IsEnabled(id));
    ASSERT_NEAR(0.0f, Length(actor.m_Transform.GetTranslation() - motion.m_Position), 0.0001f);
    ASSERT_NEAR(1.0f, dot(actor.m_Transform.GetRotation(), motion.m_Rotation), 0.0001f);
    for (int enabled = 0; enabled <= 1; ++enabled)
    {
        SetEnabled3D(world, object, enabled != 0);
        Box3DBodyMotion actual;
        GetBox3DBodyMotion(id, &actual);
        ASSERT_NEAR(0.0f, Length(actual.m_Position - motion.m_Position), 0.0001f);
        ASSERT_NEAR(1.0f, dot(actual.m_Rotation, motion.m_Rotation), 0.0001f);
        ASSERT_NEAR(0.0f, Length(actual.m_LinearVelocity - motion.m_LinearVelocity), 0.0001f);
        ASSERT_NEAR(0.0f, Length(actual.m_AngularVelocity - motion.m_AngularVelocity), 0.0001f);
    }

    ASSERT_TRUE(b3Body_IsAwake(id));

    DeleteCollisionObject3D(world, object);
    DeleteCollisionShape3D(sphere);
    DeleteWorld3D(context, world);
    DeleteContext3D(context);
}

// One configured helper participates in addition to the caller on a threaded platform.
TEST(Box3D, BackgroundWorkerCount)
{
    const char* argv[] = { "test_physics_box3d", "--config=box3d.worker_count=1" };
    NewContextParams params;
    ASSERT_EQ(dmConfigFile::RESULT_OK, dmConfigFile::LoadFromBuffer(0, 0, 2, argv, &params.m_Config));
    HContext3D context = NewContext3D(params);
    ASSERT_NE((HContext3D)0, context);
    HWorld3D world = NewWorld3D(context, NewWorldParams());
    ASSERT_NE((HWorld3D)0, world);

    b3WorldId wid = *(b3WorldId*)GetWorldContext3D(world);
#if !defined(DM_USE_SINGLE_THREAD) && (defined(_WIN32) || defined(__linux__) || defined(__APPLE__) || defined(__EMSCRIPTEN_PTHREADS__))
    ASSERT_EQ(2, b3World_GetWorkerCount(wid));
#else
    ASSERT_EQ(1, b3World_GetWorkerCount(wid));
#endif
    StepWorldContext step;
    step.m_DT = 1.0f / 30.0f;
    ASSERT_TRUE(StepWorld3D(world, step));
    DeleteWorld3D(context, world);
    DeleteContext3D(context);
    dmConfigFile::Delete(params.m_Config);
}

// A yawed cylinder resting on a box keeps its face contacts on the next collision query.
TEST(Box3D, SharedPrismAxisFaceContact)
{
    b3BoxHull floor = b3MakeBoxHull(2.0f, 0.5f, 2.0f);
    b3HullData* cylinder = b3CreateCylinder(1.0f, 0.5f, -0.5f, 8);
    ASSERT_NE((b3HullData*)0, cylinder);
    b3Transform pose = { { 0.3f, 0.98f, -0.1f }, b3MakeQuatFromAxisAngle(b3Vec3_axisY, 0.37f) };
    b3LocalManifoldPoint points[4];
    b3LocalManifold manifold = {};
    manifold.points = points;
    b3SATCache cache = {};
    for (int i = 0; i < 2; ++i)
    {
        b3CollideHulls(&manifold, 4, &floor.base, cylinder, pose, &cache);
        EXPECT_EQ(4, manifold.pointCount);
        EXPECT_NEAR(1.0f, manifold.normal.y, 0.0001f);
        EXPECT_EQ(i, (int)cache.hit);
        for (int j = 0; j < manifold.pointCount; ++j)
        {
            EXPECT_NEAR(-0.02f, points[j].separation, 0.0001f);
        }
    }

    b3DestroyHull(cylinder);
}

// Two differently rotated hulls can meet on edges even when their body rotations match.
TEST(Box3D, DifferentPrismAxesEdgeContact)
{
    b3Transform rotation_y = b3Transform_identity;
    rotation_y.q = b3MakeQuatFromAxisAngle(b3Vec3_axisY, 0.25f * B3_PI);
    b3Transform rotation_z = b3Transform_identity;
    rotation_z.q = b3MakeQuatFromAxisAngle(b3Vec3_axisZ, 0.25f * B3_PI);
    b3BoxHull a = b3MakeTransformedBoxHull(0.5f, 0.5f, 0.5f, rotation_y);
    b3BoxHull b = b3MakeTransformedBoxHull(0.5f, 0.5f, 0.5f, rotation_z);
    b3Transform pose = b3Transform_identity;
    pose.p.x = 1.4f;

    b3LocalManifoldPoint points[4];
    b3LocalManifold manifold = {};
    manifold.points = points;
    b3SATCache cache = {};
    b3CollideHulls(&manifold, 4, &a.base, &b.base, pose, &cache);
    ASSERT_EQ(1, manifold.pointCount);
    EXPECT_EQ((int)b3_edgePairAxis, (int)cache.type);
    EXPECT_NEAR(1.0f, manifold.normal.x, 0.0001f);
    EXPECT_NEAR(1.4f - sqrtf(2.0f), points[0].separation, 0.0001f);
}

struct StackFrame
{
    b3Vec3 m_Velocity[3];
    b3Quat m_Rotation[3];
    b3Pos  m_Position[3];
    int    m_Begins;
    int    m_Ends;
    int    m_Hits;
};

static void CaptureStack(bool prepare, bool rotate, StackFrame* frames, int count)
{
    HContext3D context = NewContext3D(NewContextParams());
    HWorld3D world = NewWorld3D(context, NewWorldParams());
    b3WorldId id = *(b3WorldId*)GetWorldContext3D(world);
    SetBox3DComponentEvents(id, false);
    b3BoxHull box = b3MakeBoxHull(0.5f, 0.5f, 0.5f);
    b3ShapeDef shape_def = b3DefaultShapeDef();
    shape_def.enableContactEvents = true;
    shape_def.enableHitEvents = true;
    shape_def.invokeContactCreation = true;
    b3BodyId bodies[3];
    for (int i = 0; i < 3; ++i)
    {
        b3BodyDef def = b3DefaultBodyDef();
        def.type = i == 0 ? b3_staticBody : b3_dynamicBody;
        def.position.y = (float)i;
        def.isAwake = false;
        bodies[i] = b3CreateBody(id, &def);
        b3CreateHullShape(bodies[i], &shape_def, &box.base);
    }

    if (prepare)
    {
        PrepareBox3DSleepingContacts(id);
    }

    StepWorldContext step;
    step.m_DT = 1.0f / 30.0f;
    for (int tick = 0; tick < count; ++tick)
    {
        if (tick == 5)
        {
            // One pair still matches. The other moved between loading and the first impact.
            b3Pos pos = b3Body_GetPosition(bodies[2]);
            pos.x += 0.125f;
            b3Body_SetTransform(bodies[2], pos, b3Quat_identity);
            if (rotate)
            {
                b3Quat yaw = b3MakeQuatFromAxisAngle(b3Vec3_axisY, 0.7351f);
                b3WorldTransform pivot = { b3Pos_zero, yaw };
                for (int i = 0; i < 3; ++i)
                {
                    b3Pos p = b3Body_GetPosition(bodies[i]);
                    b3Body_SetTransform(bodies[i], b3TransformWorldPoint(pivot, b3SubPos(p, b3Pos_zero)), yaw);
                }
            }

            b3Body_SetAwake(bodies[1], true);
            const b3Vec3 impulse = { 0.0f, -2.0f, 0.0f };
            b3Body_ApplyLinearImpulseToCenter(bodies[2], impulse, true);
        }

        ASSERT_TRUE(StepWorld3D(world, step));
        const b3ContactEvents events = b3World_GetContactEvents(id);
        StackFrame& frame = frames[tick];
        frame.m_Begins = events.beginCount;
        frame.m_Ends = events.endCount;
        frame.m_Hits = events.hitCount;
        for (int i = 0; i < 3; ++i)
        {
            frame.m_Velocity[i] = b3Body_GetLinearVelocity(bodies[i]);
            frame.m_Rotation[i] = b3Body_GetRotation(bodies[i]);
            frame.m_Position[i] = b3Body_GetPosition(bodies[i]);
        }

        if (tick < 5)
        {
            ASSERT_FALSE(b3Body_IsAwake(bodies[1]));
            ASSERT_FALSE(b3Body_IsAwake(bodies[2]));
            ASSERT_EQ(0, events.beginCount + events.endCount + events.hitCount);
            ASSERT_EQ(2.0f, frame.m_Position[2].y);
        }
    }

    DeleteWorld3D(context, world);
    DeleteContext3D(context);
}

// Preparing sleeping contacts must preserve the trajectory and events after wake and rotation.
TEST(Box3D, PrepareSleepingContacts)
{
    StackFrame reference[24] = {};
    StackFrame prepared[24] = {};
    for (int rotate = 0; rotate < 2; ++rotate)
    {
        CaptureStack(false, rotate != 0, reference, 24);
        CaptureStack(true, rotate != 0, prepared, 24);
        ASSERT_GT(reference[5].m_Begins, 0);
        ASSERT_EQ(0, memcmp(reference, prepared, sizeof(reference)));
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
