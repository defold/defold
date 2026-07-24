// Copyright 2020-2026 The Defold Foundation
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

#include <box2d/box2d.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../physics.h"
#include "../box2d/box2d_physics.h"
#include "../box2d/box2d_async_physics.h"

using namespace dmPhysics;

static HContext2D NewTestContext()
{
    NewContextParams cp;
    cp.m_WorldCount             = 4;
    cp.m_RayCastLimit2D         = 16;
    cp.m_TriggerOverlapCapacity = 16;
    return NewContext2D(cp);
}

// Double-buffering off: the physics world handle stays null; only the game world is created.
TEST(AsyncWorld, SingleBufferedByDefault)
{
    HContext2D context = NewTestContext();

    NewWorldParams wp;
    ASSERT_FALSE(wp.m_UseDoubleBufferedWorlds);

    World2D* world = (World2D*)NewWorld2D(context, wp);
    ASSERT_TRUE(b2World_IsValid(world->m_WorldId));
    ASSERT_FALSE(b2World_IsValid(world->m_PhysicsWorldId));

    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// Double-buffering on: both the game world and the physics world are valid, and they are
// distinct Box2D worlds (different world0 index in their body-id space).
TEST(AsyncWorld, DoubleBufferedCreatesSecondWorld)
{
    HContext2D context = NewTestContext();

    NewWorldParams wp;
    wp.m_UseDoubleBufferedWorlds = true;

    World2D* world = (World2D*)NewWorld2D(context, wp);
    ASSERT_TRUE(b2World_IsValid(world->m_WorldId));
    ASSERT_TRUE(b2World_IsValid(world->m_PhysicsWorldId));

    // The two worlds are separate Box2D instances.
    b2BodyDef bd = b2DefaultBodyDef();
    b2BodyId game_body    = b2CreateBody(world->m_WorldId, &bd);
    b2BodyId physics_body = b2CreateBody(world->m_PhysicsWorldId, &bd);
    ASSERT_NE(game_body.world0, physics_body.world0);
    b2DestroyBody(game_body);
    b2DestroyBody(physics_body);

    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// Deleting a double-buffered world tears down both Box2D worlds without leaking or crashing.
TEST(AsyncWorld, DeleteDestroysBothWorlds)
{
    HContext2D context = NewTestContext();

    NewWorldParams wp;
    wp.m_UseDoubleBufferedWorlds = true;

    World2D* world = (World2D*)NewWorld2D(context, wp);
    b2WorldId game_id    = world->m_WorldId;
    b2WorldId physics_id = world->m_PhysicsWorldId;
    ASSERT_TRUE(b2World_IsValid(game_id));
    ASSERT_TRUE(b2World_IsValid(physics_id));

    DeleteWorld2D(context, world);
    ASSERT_FALSE(b2World_IsValid(game_id));
    ASSERT_FALSE(b2World_IsValid(physics_id));

    DeleteContext2D(context);
}

// Creating a collision object in a double-buffered world queues a physics-world twin; draining the
// queue creates it and maps its id onto the owning Body. Destroying mirrors the removal.
TEST(AsyncBodyMapping, CreateDrainDestroy)
{
    HContext2D context = NewTestContext();

    NewWorldParams wp;
    wp.m_UseDoubleBufferedWorlds = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);

    CollisionObjectData data;
    data.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;

    const int N = 3;
    Body* bodies[N];
    for (int i = 0; i < N; ++i)
    {
        bodies[i] = (Body*)NewCollisionObject2D(world, data, &shape, 1);
        // Game body exists immediately; physics twin is pending (not drained).
        ASSERT_TRUE(b2Body_IsValid(bodies[i]->m_BodyId));
        ASSERT_FALSE(b2Body_IsValid(bodies[i]->m_PhysicsBodyId));
    }

    DrainPendingOps(world);
    ASSERT_EQ(N, b2World_GetCounters(world->m_PhysicsWorldId).bodyCount);

    for (int i = 0; i < N; ++i)
    {
        ASSERT_TRUE(b2Body_IsValid(bodies[i]->m_BodyId));
        ASSERT_TRUE(b2Body_IsValid(bodies[i]->m_PhysicsBodyId));
        // The twin lives in a different Box2D world, so the ids are not interchangeable.
        ASSERT_NE(bodies[i]->m_BodyId.world0, bodies[i]->m_PhysicsBodyId.world0);
    }

    // Destroy the middle body; capture its twin id first since DeleteCollisionObject2D frees the Body.
    b2BodyId destroyed_twin = bodies[1]->m_PhysicsBodyId;
    DeleteCollisionObject2D(world, bodies[1]);
    DrainPendingOps(world);
    ASSERT_FALSE(b2Body_IsValid(destroyed_twin));

    // Surviving twins remain valid.
    ASSERT_TRUE(b2Body_IsValid(bodies[0]->m_PhysicsBodyId));
    ASSERT_TRUE(b2Body_IsValid(bodies[2]->m_PhysicsBodyId));

    DeleteCollisionObject2D(world, bodies[0]);
    DeleteCollisionObject2D(world, bodies[2]);
    DrainPendingOps(world);

    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// Creating then destroying a body before any drain cancels the pending create: no twin is left in
// the physics world.
TEST(AsyncBodyMapping, CreateDestroyBeforeDrain)
{
    HContext2D context = NewTestContext();

    NewWorldParams wp;
    wp.m_UseDoubleBufferedWorlds = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData data;
    data.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass = 1.0f;

    Body* body = (Body*)NewCollisionObject2D(world, data, &shape, 1);
    ASSERT_FALSE(b2Body_IsValid(body->m_PhysicsBodyId));

    // Destroy before draining: the create op is cancelled, so the drain creates nothing.
    DeleteCollisionObject2D(world, body);
    DrainPendingOps(world);

    ASSERT_EQ(0, b2World_GetCounters(world->m_PhysicsWorldId).bodyCount);

    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

struct SetTransformCapture
{
    dmVMath::Point3 m_Position;
    int             m_Calls;
};

static void CaptureSetTransform(void* user_data, const dmVMath::Point3& position, const dmVMath::Quat& rotation)
{
    SetTransformCapture* c = (SetTransformCapture*) user_data;
    c->m_Position = position;
    c->m_Calls++;
}

// SyncGameToPhysics copies the game body's state onto its twin; stepping the physics world and then
// SyncPhysicsToGame carries the result back onto the game body and fires the set-transform callback.
TEST(AsyncSync, GameToPhysicsAndBack)
{
    SetTransformCapture capture = {};
    capture.m_Position = dmVMath::Point3(0.0f, 0.0f, 0.0f);

    HContext2D context = NewTestContext();

    NewWorldParams wp;
    wp.m_UseDoubleBufferedWorlds  = true;
    wp.m_SetWorldTransformCallback = CaptureSetTransform;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData data;
    data.m_Type     = COLLISION_OBJECT_TYPE_DYNAMIC;
    data.m_Mass     = 1.0f;
    data.m_UserData = &capture;

    Body* body = (Body*)NewCollisionObject2D(world, data, &shape, 1);
    DrainPendingOps(world);
    ASSERT_TRUE(b2Body_IsValid(body->m_PhysicsBodyId));

    // Set a known state on the game body, then push it onto the twin.
    b2Body_SetTransform(body->m_BodyId, b2Vec2{3.0f, 5.0f}, b2Rot_identity);
    b2Body_SetLinearVelocity(body->m_BodyId, b2Vec2{1.0f, -2.0f});
    SyncGameToPhysics(world);

    b2Vec2 twin_pos = b2Body_GetPosition(body->m_PhysicsBodyId);
    b2Vec2 twin_vel = b2Body_GetLinearVelocity(body->m_PhysicsBodyId);
    ASSERT_NEAR(3.0f, twin_pos.x, 0.0001f);
    ASSERT_NEAR(5.0f, twin_pos.y, 0.0001f);
    ASSERT_NEAR(1.0f, twin_vel.x, 0.0001f);
    ASSERT_NEAR(-2.0f, twin_vel.y, 0.0001f);

    // Step only the physics world; the game body is untouched until the sync back.
    b2World_Step(world->m_PhysicsWorldId, 1.0f / 60.0f, 4);
    b2Vec2 stepped = b2Body_GetPosition(body->m_PhysicsBodyId);

    SyncPhysicsToGame(world);
    b2Vec2 game_pos = b2Body_GetPosition(body->m_BodyId);
    ASSERT_NEAR(stepped.x, game_pos.x, 0.0001f);
    ASSERT_NEAR(stepped.y, game_pos.y, 0.0001f);

    // The dynamic enabled body reported its stepped transform through the callback.
    ASSERT_EQ(1, capture.m_Calls);
    ASSERT_NEAR(stepped.x, capture.m_Position.getX(), 0.0001f);

    DeleteCollisionObject2D(world, body);
    DrainPendingOps(world);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// --- Parity battery: the double-buffered path must produce the same simulation and callback
// stream as the synchronous path when run synchronously (the worker thread lands later). ---

static int g_collisions;
static int g_contact_points;
static int g_trigger_enters;
static int g_trigger_exits;

static bool ParityOnCollision(void*, uint16_t, void*, uint16_t, void*)      { g_collisions++;      return true; }
static bool ParityOnContactPoint(const ContactPoint&, void*)               { g_contact_points++;  return true; }
static void ParityOnTriggerEnter(const TriggerEnter&, void*)               { g_trigger_enters++;  }
static void ParityOnTriggerExit(const TriggerExit&, void*)                 { g_trigger_exits++;   }

struct PBodyInfo { float x, y; };

static void ParityGetTransform(void* user_data, dmTransform::Transform& t)
{
    PBodyInfo* b = (PBodyInfo*) user_data;
    t.SetTranslation(dmVMath::Vector3(b->x, b->y, 0.0f));
    t.SetRotation(dmVMath::Quat::identity());
    t.SetUniformScale(1.0f);
}

struct ParityResult
{
    b2Vec2 m_FinalPos;
    int    m_Collisions;
    int    m_ContactPoints;
    int    m_TriggerEnters;
    int    m_TriggerExits;
};

// Ball dropped through a static sensor onto a static floor: exercises collision callbacks, contact
// points, and trigger enter/exit in one scene.
static ParityResult RunParityScene(bool async)
{
    g_collisions = g_contact_points = g_trigger_enters = g_trigger_exits = 0;

    HContext2D context = NewTestContext();

    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ParityGetTransform;
    wp.m_UseDoubleBufferedWorlds   = async;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PBodyInfo floor_info  = { 0.0f, -3.0f };
    PBodyInfo sensor_info = { 0.0f, -0.5f };
    PBodyInfo ball_info   = { 0.0f,  2.0f };

    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(5.0f, 0.3f, 0.0f));
    CollisionObjectData floor_data;
    floor_data.m_Type     = COLLISION_OBJECT_TYPE_STATIC;
    floor_data.m_Mass     = 0.0f;
    floor_data.m_UserData = &floor_info;
    Body* floor = (Body*)NewCollisionObject2D(world, floor_data, &floor_shape, 1);

    HCollisionShape2D sensor_shape = NewBoxShape2D(context, dmVMath::Vector3(1.0f, 0.3f, 0.0f));
    CollisionObjectData sensor_data;
    sensor_data.m_Type     = COLLISION_OBJECT_TYPE_TRIGGER;
    sensor_data.m_Mass     = 0.0f;
    sensor_data.m_UserData = &sensor_info;
    Body* sensor = (Body*)NewCollisionObject2D(world, sensor_data, &sensor_shape, 1);

    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData ball_data;
    ball_data.m_Type     = COLLISION_OBJECT_TYPE_DYNAMIC;
    ball_data.m_Mass     = 1.0f;
    ball_data.m_UserData = &ball_info;
    Body* ball = (Body*)NewCollisionObject2D(world, ball_data, &ball_shape, 1);

    StepWorldContext sc;
    sc.m_DT                     = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount      = 4;
    sc.m_CollisionCallback      = ParityOnCollision;
    sc.m_ContactPointCallback   = ParityOnContactPoint;
    sc.m_TriggerEnteredCallback = ParityOnTriggerEnter;
    sc.m_TriggerExitedCallback  = ParityOnTriggerExit;

    for (int i = 0; i < 150; ++i)
    {
        StepWorld2D(world, sc);
    }

    ParityResult r;
    r.m_FinalPos      = b2Body_GetPosition(ball->m_BodyId);
    r.m_Collisions    = g_collisions;
    r.m_ContactPoints = g_contact_points;
    r.m_TriggerEnters = g_trigger_enters;
    r.m_TriggerExits  = g_trigger_exits;

    DeleteCollisionObject2D(world, ball);
    DeleteCollisionObject2D(world, sensor);
    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(sensor_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
    return r;
}

TEST(AsyncParity, SyncVsAsyncScene)
{
    ParityResult s = RunParityScene(false);
    ParityResult a = RunParityScene(true);

    // The scene must actually exercise each callback path, or the parity check is vacuous.
    ASSERT_GT(s.m_Collisions, 0);
    ASSERT_GT(s.m_ContactPoints, 0);
    ASSERT_GT(s.m_TriggerEnters, 0);
    ASSERT_GT(s.m_TriggerExits, 0);

    ASSERT_EQ(s.m_Collisions, a.m_Collisions);
    ASSERT_EQ(s.m_ContactPoints, a.m_ContactPoints);
    ASSERT_EQ(s.m_TriggerEnters, a.m_TriggerEnters);
    ASSERT_EQ(s.m_TriggerExits, a.m_TriggerExits);

    ASSERT_NEAR(s.m_FinalPos.x, a.m_FinalPos.x, 0.001f);
    ASSERT_NEAR(s.m_FinalPos.y, a.m_FinalPos.y, 0.001f);
}

// --- Structural mutator parity: enable/disable, gravity, scale must mirror to the twin. ---

static b2Vec2 RunDisableScene(bool async, bool* out_game_enabled, bool* out_twin_enabled)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ParityGetTransform;
    wp.m_UseDoubleBufferedWorlds   = async;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PBodyInfo info = { 0.0f, 5.0f };
    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData d;
    d.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC;
    d.m_Mass = 1.0f;
    d.m_UserData = &info;
    Body* ball = (Body*)NewCollisionObject2D(world, d, &shape, 1);

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;

    for (int i = 0; i < 10; ++i) StepWorld2D(world, sc);
    SetEnabled2D(world, ball, false);
    for (int i = 0; i < 40; ++i) StepWorld2D(world, sc);

    b2Vec2 pos = b2Body_GetPosition(ball->m_BodyId);
    *out_game_enabled = b2Body_IsEnabled(ball->m_BodyId);
    *out_twin_enabled = async ? b2Body_IsEnabled(ball->m_PhysicsBodyId) : false;

    DeleteCollisionObject2D(world, ball);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
    return pos;
}

TEST(AsyncMutatorParity, Disable)
{
    bool s_game, s_twin, a_game, a_twin;
    b2Vec2 s = RunDisableScene(false, &s_game, &s_twin);
    b2Vec2 a = RunDisableScene(true,  &a_game, &a_twin);

    ASSERT_FALSE(s_game);   // disabled in both paths
    ASSERT_FALSE(a_game);
    ASSERT_FALSE(a_twin);   // twin was disabled through the op queue
    ASSERT_NEAR(s.x, a.x, 0.001f);
    ASSERT_NEAR(s.y, a.y, 0.001f);
}

static b2Vec2 RunGravityScene(bool async)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ParityGetTransform;
    wp.m_UseDoubleBufferedWorlds   = async;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PBodyInfo info = { 0.0f, 0.0f };
    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData d;
    d.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC;
    d.m_Mass = 1.0f;
    d.m_UserData = &info;
    Body* ball = (Body*)NewCollisionObject2D(world, d, &shape, 1);

    SetGravity2D(world, dmVMath::Vector3(10.0f, 0.0f, 0.0f)); // push right instead of down

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;
    for (int i = 0; i < 60; ++i) StepWorld2D(world, sc);

    b2Vec2 pos = b2Body_GetPosition(ball->m_BodyId);

    DeleteCollisionObject2D(world, ball);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
    return pos;
}

TEST(AsyncMutatorParity, Gravity)
{
    b2Vec2 s = RunGravityScene(false);
    b2Vec2 a = RunGravityScene(true);

    ASSERT_GT(s.x, 1.0f);   // moved right under the changed gravity
    ASSERT_NEAR(s.x, a.x, 0.001f);
    ASSERT_NEAR(s.y, a.y, 0.001f);
}

struct PScaleInfo { float x, y, scale; };

static void ScaleGetTransform(void* user_data, dmTransform::Transform& t)
{
    PScaleInfo* s = (PScaleInfo*) user_data;
    t.SetTranslation(dmVMath::Vector3(s->x, s->y, 0.0f));
    t.SetRotation(dmVMath::Quat::identity());
    t.SetUniformScale(s->scale);
}

// A scale change on a body (driven through the game-object transform) mirrors onto the twin's shape.
TEST(AsyncMutator, ScaleMirrorsToTwin)
{
    NewContextParams cp;
    cp.m_WorldCount             = 4;
    cp.m_RayCastLimit2D         = 16;
    cp.m_TriggerOverlapCapacity = 16;
    cp.m_AllowDynamicTransforms = 1; // so dynamic bodies pull scale from the game object
    HContext2D context = NewContext2D(cp);

    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ScaleGetTransform;
    wp.m_UseDoubleBufferedWorlds   = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PScaleInfo info = { 0.0f, 0.0f, 1.0f };
    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData d;
    d.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC;
    d.m_Mass = 1.0f;
    d.m_UserData = &info;
    Body* ball = (Body*)NewCollisionObject2D(world, d, &shape, 1);

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;

    StepWorld2D(world, sc);   // twin created at scale 1
    info.scale = 2.0f;        // grow the object
    for (int i = 0; i < 3; ++i) StepWorld2D(world, sc);

    CircleShapeData* game_circle = (CircleShapeData*) ball->m_Shapes[0];
    float game_radius = game_circle->m_Circle.radius;

    b2ShapeId twin_shapes[1];
    int n = b2Body_GetShapes(ball->m_PhysicsBodyId, twin_shapes, 1);
    ASSERT_EQ(1, n);
    b2Circle twin_circle = b2Shape_GetCircle(twin_shapes[0]);

    ASSERT_NEAR(1.0f, game_radius, 0.001f);            // 0.5 creation radius * 2.0 scale
    ASSERT_NEAR(game_radius, twin_circle.radius, 0.001f); // twin matches game

    DeleteCollisionObject2D(world, ball);
    DrainPendingOps(world);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// A body created disabled and then enabled before the first drain must have its twin created
// enabled: the enable is applied between create and drain, when the twin id does not exist, so
// it has to fold into the pending create rather than be dropped.
TEST(AsyncMutator, EnableBeforeDrainAppliesToTwin)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_UseDoubleBufferedWorlds = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData d;
    d.m_Type    = COLLISION_OBJECT_TYPE_DYNAMIC;
    d.m_Mass    = 1.0f;
    d.m_Enabled = 0; // created disabled

    Body* ball = (Body*)NewCollisionObject2D(world, d, &shape, 1);
    ASSERT_FALSE(b2Body_IsValid(ball->m_PhysicsBodyId)); // twin not created

    SetEnabled2D(world, ball, true); // enable before any drain
    DrainPendingOps(world);

    ASSERT_TRUE(b2Body_IsValid(ball->m_PhysicsBodyId));
    ASSERT_TRUE(b2Body_IsEnabled(ball->m_PhysicsBodyId)); // twin came up enabled

    DeleteCollisionObject2D(world, ball);
    DrainPendingOps(world);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
