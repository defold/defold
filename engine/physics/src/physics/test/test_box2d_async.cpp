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

#include <dlib/time.h>

#include "../physics.h"
#include "../box2d/box2d_physics.h"
#include "../box2d/box2d_async_physics.h"
#include "../box2d/box2d_async_thread.h"

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

// --- Parity battery: the double-buffered path (worker thread, N-1 delivery) must produce the same
// simulation and callback stream as the synchronous path. Async runs one extra step to flush the
// final delivery, so both paths deliver the same number of steps and results match. ---

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
    if (async)
    {
        // N-1 pipeline: results are delivered on the following step, so one extra step flushes the
        // 150th step's delivery. Both paths then reflect 150 delivered steps and line up exactly.
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
    if (async) StepWorld2D(world, sc); // flush the final delivery (N-1)

    if (async) WaitForWorker2D(world);  // quiesce the worker so the twin read below does not race

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
    if (async) StepWorld2D(world, sc); // flush the final delivery (N-1)

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

static b2Vec2 RunApplyForceScene(bool async)
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

    SetGravity2D(world, dmVMath::Vector3(0.0f, 0.0f, 0.0f)); // isolate the applied force

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;
    for (int i = 0; i < 60; ++i)
    {
        // Push right through the body centre each frame (origin == centre of mass, so no torque).
        ApplyForce2D(context, ball, dmVMath::Vector3(20.0f, 0.0f, 0.0f), GetWorldPosition2D(context, ball));
        StepWorld2D(world, sc);
    }
    if (async) StepWorld2D(world, sc); // flush the final delivery (N-1)

    b2Vec2 pos = b2Body_GetPosition(ball->m_BodyId);

    DeleteCollisionObject2D(world, ball);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
    return pos;
}

// physics.apply_force must drive the body in the double-buffered path. The game world is never stepped
// there, so the force is accumulated on the game body and injected onto the twin before the worker
// steps; the body must move (silent no-op regression guard) and land where the sync path does.
TEST(AsyncForce, ApplyForceMovesBodyMatchesSync)
{
    b2Vec2 s = RunApplyForceScene(false);
    b2Vec2 a = RunApplyForceScene(true);

    ASSERT_GT(s.x, 1.0f);       // sync: force moved the body right
    ASSERT_GT(a.x, 1.0f);       // async: force actually did something
    ASSERT_NEAR(s.x, a.x, 0.01f);
    ASSERT_NEAR(s.y, a.y, 0.01f);
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
    WaitForWorker2D(world);   // quiesce the worker before reading the twin's shape

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

// --- Deleted-object filtering --------------------------------------------------------------------

// The deleted-object set: DeleteCollisionObject2D records the object's user-data key; the public
// accessors report membership, count, frame, and clear.
TEST(AsyncDeletedObjects, SetMechanics)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_UseDoubleBufferedWorlds = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);

    PBodyInfo info_a = { 0.0f, 0.0f };
    PBodyInfo info_b = { 1.0f, 0.0f };
    uint64_t key_a = (uint64_t)(uintptr_t) &info_a;
    uint64_t key_b = (uint64_t)(uintptr_t) &info_b;

    CollisionObjectData da; da.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; da.m_Mass = 1.0f; da.m_UserData = &info_a;
    CollisionObjectData db; db.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; db.m_Mass = 1.0f; db.m_UserData = &info_b;
    Body* a = (Body*)NewCollisionObject2D(world, da, &shape, 1);
    Body* b = (Body*)NewCollisionObject2D(world, db, &shape, 1);
    DrainPendingOps(world);

    ASSERT_EQ(0U, GetDeletedObjectCount2D(world));
    ASSERT_FALSE(IsObjectDeleted(world, key_a));

    DeleteCollisionObject2D(world, a);
    ASSERT_TRUE(IsObjectDeleted(world, key_a));
    ASSERT_FALSE(IsObjectDeleted(world, key_b));
    ASSERT_EQ(1U, GetDeletedObjectCount2D(world));

    SetDeletedObjectsFrame2D(world, 42);
    ASSERT_EQ(42U, GetDeletedObjectsFrame2D(world));

    ClearDeletedObjects2D(world);
    ASSERT_EQ(0U, GetDeletedObjectCount2D(world));
    ASSERT_FALSE(IsObjectDeleted(world, key_a));
    ASSERT_EQ(0U, GetDeletedObjectsFrame2D(world)); // frame reads 0 once the set is empty

    DeleteCollisionObject2D(world, b);
    DrainPendingOps(world);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// Delivery clears the deleted-object set: a delete recorded between frames is consumed by the next
// StepWorld2D (which runs the deliver phase), leaving the set empty for the following frame.
TEST(AsyncDeletedObjects, ClearedAfterDelivery)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ParityGetTransform;
    wp.m_UseDoubleBufferedWorlds   = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PBodyInfo floor_info = { 0.0f, -2.0f };
    PBodyInfo ball_info  = { 0.0f,  1.0f };

    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(5.0f, 0.3f, 0.0f));
    CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_info;
    Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_info;
    Body* ball = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;
    sc.m_CollisionCallback = ParityOnCollision;

    for (int i = 0; i < 80; ++i) StepWorld2D(world, sc); // ball lands and rests on the floor

    DeleteCollisionObject2D(world, ball);
    ASSERT_EQ(1U, GetDeletedObjectCount2D(world));

    StepWorld2D(world, sc);                       // deliver phase consumes and clears the set
    ASSERT_EQ(0U, GetDeletedObjectCount2D(world));

    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// A prepared event that references an object, once the object is deleted, has both a live buffer
// reference and a deleted-set hit: exactly the inputs DeliverPreparedEvents2D uses to skip it. The
// end-to-end runtime skip is verified by AsyncDeletedObjects.NoCallbackAfterDelete below.
TEST(AsyncDeletedObjects, StaleReferenceIsFlagged)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ParityGetTransform;
    wp.m_UseDoubleBufferedWorlds   = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PBodyInfo floor_info = { 0.0f, -2.0f };
    PBodyInfo ball_info  = { 0.0f,  1.0f };
    uint64_t ball_key = (uint64_t)(uintptr_t) &ball_info;

    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(5.0f, 0.3f, 0.0f));
    CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_info;
    Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_info;
    Body* ball = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;
    sc.m_CollisionCallback = ParityOnCollision;

    for (int i = 0; i < 80; ++i) StepWorld2D(world, sc); // ball rests on floor, buffer holds the contact
    WaitForWorker2D(world);                              // quiesce so the buffer read below does not race

    // The last step's buffer references the ball.
    bool ball_referenced = false;
    for (uint32_t i = 0; i < world->m_PreparedEvents.Size(); ++i)
    {
        PreparedCollisionEvent& ev = world->m_PreparedEvents[i];
        if (ev.m_BodyIdKeyA == ball_key || ev.m_BodyIdKeyB == ball_key)
        {
            ball_referenced = true;
            break;
        }
    }
    ASSERT_TRUE(ball_referenced);

    // Deleting the ball flags exactly those buffered references for skipping.
    DeleteCollisionObject2D(world, ball);
    ASSERT_TRUE(IsObjectDeleted(world, ball_key));

    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// Counts collision callbacks that reference a specific user-data pointer.
static void* g_watch_userdata;
static int   g_watch_hits;
static bool  WatchCollision(void* a, uint16_t, void* b, uint16_t, void*)
{
    if (a == g_watch_userdata || b == g_watch_userdata) g_watch_hits++;
    return true;
}

// End-to-end deleted-object skip: once an object is deleted, no further callback is delivered for it,
// even though the step collected before the delete references it. That buffer is delivered on
// the following step and must be filtered by the deleted-object set.
TEST(AsyncDeletedObjects, NoCallbackAfterDelete)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ParityGetTransform;
    wp.m_UseDoubleBufferedWorlds   = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PBodyInfo floor_info = { 0.0f, -2.0f };
    PBodyInfo ball_info  = { 0.0f,  1.0f };

    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(5.0f, 0.3f, 0.0f));
    CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_info;
    Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_info;
    Body* ball = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);

    g_watch_userdata = &ball_info;
    g_watch_hits     = 0;

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;
    sc.m_CollisionCallback = WatchCollision;

    for (int i = 0; i < 80; ++i) StepWorld2D(world, sc); // ball rests; its collisions are delivered
    ASSERT_GT(g_watch_hits, 0);
    int baseline = g_watch_hits;

    DeleteCollisionObject2D(world, ball); // the in-flight step's buffer references the ball
    for (int i = 0; i < 5; ++i) StepWorld2D(world, sc);

    // That buffer was delivered but filtered: no new callback fired for the deleted ball.
    ASSERT_EQ(baseline, g_watch_hits);

    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// --- Stability + churn --------------------------------------------------------------------------

// A stack of balls dropped onto a floor, stepped to rest.
static void RunStackScene(bool async, b2Vec2* out_positions, int ball_count)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ParityGetTransform;
    wp.m_UseDoubleBufferedWorlds   = async;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PBodyInfo floor_info = { 0.0f, -3.0f };
    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(5.0f, 0.3f, 0.0f));
    CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_info;
    Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

    static PBodyInfo ball_infos[8];
    Body* balls[8];
    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
    for (int i = 0; i < ball_count; ++i)
    {
        ball_infos[i].x = 0.0f;
        ball_infos[i].y = 1.0f + i * 1.2f; // stacked above the floor
        CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_infos[i];
        balls[i] = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);
    }

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;

    for (int i = 0; i < 300; ++i) StepWorld2D(world, sc);
    if (async) StepWorld2D(world, sc); // flush the final delivery (N-1)

    for (int i = 0; i < ball_count; ++i)
        out_positions[i] = b2Body_GetPosition(balls[i]->m_BodyId);

    for (int i = 0; i < ball_count; ++i) DeleteCollisionObject2D(world, balls[i]);
    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// The threaded async path settles a multi-body stack to the same resting configuration as the
// synchronous path (determinism + stability over a long run).
TEST(AsyncStability, StackMatchesSync)
{
    const int N = 4;
    b2Vec2 s[N], a[N];
    RunStackScene(false, s, N);
    RunStackScene(true,  a, N);

    for (int i = 0; i < N; ++i)
    {
        ASSERT_TRUE(s[i].y == s[i].y); // not NaN
        ASSERT_TRUE(a[i].y == a[i].y); // not NaN
        ASSERT_NEAR(s[i].x, a[i].x, 0.001f);
        ASSERT_NEAR(s[i].y, a[i].y, 0.001f);
    }
}

// Create/destroy churn concurrent with the worker: bodies are spawned and deleted right after each
// StepWorld2D returns, i.e. while that step's worker job is in flight. This stresses the body
// snapshot (the worker iterates the snapshot, never the mutating m_Bodies) and the deleted-object
// filter. The run must not crash, and after quiescing the twin count must match the live game bodies.
TEST(AsyncChurn, SpawnDeleteWhileStepping)
{
    HContext2D context = NewTestContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = ParityGetTransform;
    wp.m_UseDoubleBufferedWorlds   = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    PBodyInfo floor_info = { 0.0f, -3.0f };
    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(6.0f, 0.3f, 0.0f));
    CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_info;
    Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.4f);

    static PBodyInfo infos[256];
    Body* live[32];
    int   live_count = 0;
    int   next_info  = 0;

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;
    sc.m_CollisionCallback = ParityOnCollision; // exercise collect/deliver under churn

    g_collisions = 0;

    for (int f = 0; f < 200; ++f)
    {
        StepWorld2D(world, sc); // kicks the worker; the mutations below run while it is in flight

        if (live_count < 32 && next_info < 256)
        {
            infos[next_info].x = (float)((next_info % 7) - 3) * 0.5f;
            infos[next_info].y = 3.0f;
            CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &infos[next_info];
            live[live_count++] = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);
            next_info++;
        }

        if ((f % 3) == 0 && live_count > 0)
        {
            DeleteCollisionObject2D(world, live[0]);
            for (int k = 1; k < live_count; ++k) live[k-1] = live[k];
            live_count--;
        }
    }

    // Quiesce, apply the pending structural ops, and check the twin count matches the live bodies.
    WaitForWorker2D(world);
    DrainPendingOps(world);

    int expected_bodies = 1 + live_count; // floor + surviving balls
    ASSERT_EQ(expected_bodies, (int)world->m_Bodies.Size());
    ASSERT_EQ(expected_bodies, b2World_GetCounters(world->m_PhysicsWorldId).bodyCount);
    ASSERT_GT(g_collisions, 0); // the churn actually produced collisions

    for (int i = 0; i < live_count; ++i) DeleteCollisionObject2D(world, live[i]);
    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// --- box2d_async_thread substrate (no physics) ---------------------------------------------------

struct WorkerTestCtx
{
    volatile int m_Value;
    int          m_Add;
};

// A job that takes observable time (a real yield) before writing its result, so that if Wait did
// not block until completion the assertion after Wait would read the pre-job value.
static void AddJob(void* p)
{
    WorkerTestCtx* c = (WorkerTestCtx*)p;
    dmTime::Sleep(1000); // 1 ms
    c->m_Value += c->m_Add;
}

TEST(AsyncWorkerThread, IdleAfterCreate)
{
    AsyncWorker* w = NewAsyncWorker("test_async_worker", false);
    ASSERT_TRUE(AsyncWorkerIsIdle(w));
    DeleteAsyncWorker(w);
}

TEST(AsyncWorkerThread, SubmitWaitRunsJob)
{
    AsyncWorker* w = NewAsyncWorker("test_async_worker", false);
    WorkerTestCtx ctx = { 0, 7 };
    AsyncWorkerSubmit(w, AddJob, &ctx);
    AsyncWorkerWait(w);
    ASSERT_EQ(7, ctx.m_Value);          // Wait blocked until the job finished
    ASSERT_TRUE(AsyncWorkerIsIdle(w));  // idle again after completion
    DeleteAsyncWorker(w);
}

TEST(AsyncWorkerThread, ManySubmitWaitCycles)
{
    AsyncWorker* w = NewAsyncWorker("test_async_worker", false);
    WorkerTestCtx ctx = { 0, 1 };
    for (int i = 0; i < 64; ++i)
    {
        AsyncWorkerSubmit(w, AddJob, &ctx);
        AsyncWorkerWait(w);
    }
    ASSERT_EQ(64, ctx.m_Value);
    DeleteAsyncWorker(w);
}

TEST(AsyncWorkerThread, DestroyWhileIdleIsClean)
{
    AsyncWorker* w = NewAsyncWorker("test_async_worker", false);
    ASSERT_TRUE(AsyncWorkerIsIdle(w));
    DeleteAsyncWorker(w);
    ASSERT_TRUE(true); // reached teardown without hang/crash
}

TEST(AsyncWorkerThread, DestroyAfterSubmitWaitsForJob)
{
    AsyncWorker* w = NewAsyncWorker("test_async_worker", false);
    WorkerTestCtx ctx = { 0, 5 };
    AsyncWorkerSubmit(w, AddJob, &ctx);
    DeleteAsyncWorker(w);       // must drain the in-flight job before tearing down
    ASSERT_EQ(5, ctx.m_Value);
}

// --- Kinematic-pull gating ---

// A game object whose transform never tracks the physics body. In the engine, GetWorldTransform
// returns the game-object transform (which SetWorldTransform updates from physics); when that
// transform disagrees with the body, UpdateKinematicBodies2D pulls the body toward it. In the async
// path that pull would re-inject the twin's own transform onto the (sleeping) twin, which does not
// re-step it. This callback holds the transform fixed so the test can assert the body is driven by
// physics, not by the game-object transform.
static void FixedTransformGet(void* ud, dmTransform::Transform& t)
{
    (void)ud;
    t.SetIdentity();
    t.SetTranslation(dmVMath::Vector3(0.0f, 5.0f, 0.0f));
}

// A dynamic body in the async path must be driven by the physics simulation, not pulled back from
// its game-object transform. With allow_dynamic_transforms on and the game-object transform held at
// y=5, the ball must fall under gravity.
TEST(AsyncKinematic, DynamicBodyIsPhysicsDrivenNotPulled)
{
    NewContextParams cp;
    cp.m_WorldCount             = 4;
    cp.m_RayCastLimit2D         = 16;
    cp.m_TriggerOverlapCapacity = 16;
    cp.m_AllowDynamicTransforms = 1;
    HContext2D context = NewContext2D(cp);

    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = FixedTransformGet;
    wp.m_UseDoubleBufferedWorlds   = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData d;
    d.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC;
    d.m_Mass = 1.0f;
    Body* ball = (Body*)NewCollisionObject2D(world, d, &shape, 1);

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;

    for (int i = 0; i < 120; ++i) StepWorld2D(world, sc);
    StepWorld2D(world, sc); // flush the N-1 delivery

    // Physics-driven: the ball fell far below the fixed game-object y (5). If it were pulled back to
    // the game-object transform each step it would be frozen near y=5.
    b2Vec2 pos = b2Body_GetPosition(ball->m_BodyId);
    ASSERT_LT(pos.y, 0.0f);

    DeleteCollisionObject2D(world, ball);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// --- Engine-loop-faithful battery: model the game-object transform feedback -----------------------
// The engine's GetWorldTransform returns the game-object transform that SetWorldTransform wrote, and
// UpdateScale reads the object scale from it. These tests use that feedback (and allow_dynamic_transforms)
// so the async path is exercised through the game-object transform feedback, not static callbacks.

struct GObj
{
    dmVMath::Vector3 m_Pos;
    dmVMath::Quat    m_Rot;
    float            m_Scale;
};

static void GObjGet(void* ud, dmTransform::Transform& t)
{
    GObj* g = (GObj*)ud;
    t.SetIdentity();
    t.SetTranslation(g->m_Pos);
    t.SetRotation(g->m_Rot);
    t.SetUniformScale(g->m_Scale);
}

static void GObjSet(void* ud, const dmVMath::Point3& p, const dmVMath::Quat& r)
{
    GObj* g = (GObj*)ud;
    g->m_Pos = dmVMath::Vector3(p.getX(), p.getY(), 0.0f);
    g->m_Rot = r;
}

static HContext2D NewFeedbackContext()
{
    NewContextParams cp;
    cp.m_WorldCount             = 4;
    cp.m_RayCastLimit2D         = 16;
    cp.m_TriggerOverlapCapacity = 16;
    cp.m_AllowDynamicTransforms = 1; // enable the game-object transform/scale feedback path
    return NewContext2D(cp);
}

// Drop a ball onto a floor through the game-object transform feedback; report rest position, whether the body slept,
// and the residual jitter over 60 settled steps.
static b2Vec2 RunFeedbackDrop(bool async, bool* out_asleep, float* out_jitter)
{
    HContext2D context = NewFeedbackContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = GObjGet;
    wp.m_SetWorldTransformCallback = GObjSet;
    wp.m_UseDoubleBufferedWorlds   = async;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    GObj floor_o = { dmVMath::Vector3(0.0f, -3.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(5.0f, 0.3f, 0.0f));
    CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_o;
    Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

    GObj ball_o = { dmVMath::Vector3(0.0f, 2.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_o;
    Body* ball = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;

    for (int i = 0; i < 250; ++i) StepWorld2D(world, sc); // settle

    float jitter = 0.0f;
    b2Vec2 prev = b2Body_GetPosition(ball->m_BodyId);
    for (int i = 0; i < 60; ++i)
    {
        StepWorld2D(world, sc);
        b2Vec2 p = b2Body_GetPosition(ball->m_BodyId);
        jitter += b2Distance(p, prev);
        prev = p;
    }
    if (async) { StepWorld2D(world, sc); WaitForWorker2D(world); } // flush + quiesce for the reads

    *out_jitter = jitter;
    *out_asleep = async ? !b2Body_IsAwake(ball->m_PhysicsBodyId) : !b2Body_IsAwake(ball->m_BodyId);
    b2Vec2 rest = b2Body_GetPosition(ball->m_BodyId);

    DeleteCollisionObject2D(world, ball);
    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
    return rest;
}

// A settled body must sleep, hold still (no resting jitter), and rest where the synchronous path does.
TEST(AsyncFeedback, SettledSleepsStableMatchesSync)
{
    bool s_asleep, a_asleep;
    float s_jit, a_jit;
    b2Vec2 s = RunFeedbackDrop(false, &s_asleep, &s_jit);
    b2Vec2 a = RunFeedbackDrop(true,  &a_asleep, &a_jit);

    ASSERT_TRUE(s_asleep);          // sync body sleeps once settled
    ASSERT_TRUE(a_asleep);          // async twin sleeps once settled too
    ASSERT_LT(s_jit, 0.001f);
    ASSERT_LT(a_jit, 0.001f);       // no resting jitter
    ASSERT_NEAR(s.x, a.x, 0.01f);
    ASSERT_NEAR(s.y, a.y, 0.01f);
}

// Stepping in bursts (catch-up frames doing several fixed steps) must produce the same result as
// stepping one at a time: the N-1 pipeline must not lose or double a step.
TEST(AsyncCadence, BurstMatchesSingleSteps)
{
    // Helper lambda-free: run a drop delivering exactly `deliver` steps, grouping `burst` calls
    // "per frame" (grouping is cosmetic here but exercises consecutive calls without interleaving).
    b2Vec2 result[2];
    int g_coll[2];
    for (int variant = 0; variant < 2; ++variant)
    {
        int burst = variant == 0 ? 1 : 3;

        HContext2D context = NewFeedbackContext();
        NewWorldParams wp;
        wp.m_GetWorldTransformCallback = GObjGet;
        wp.m_SetWorldTransformCallback = GObjSet;
        wp.m_UseDoubleBufferedWorlds   = true;
        World2D* world = (World2D*)NewWorld2D(context, wp);

        GObj floor_o = { dmVMath::Vector3(0.0f, -3.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
        HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(5.0f, 0.3f, 0.0f));
        CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_o;
        Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

        GObj ball_o = { dmVMath::Vector3(0.3f, 2.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
        HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
        CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_o;
        Body* ball = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);

        g_collisions = 0;
        StepWorldContext sc;
        sc.m_DT = 1.0f / 60.0f;
        sc.m_Box2DSubStepCount = 4;
        sc.m_CollisionCallback = ParityOnCollision;

        const int deliver = 240;
        int delivered = 0;
        while (delivered < deliver)
        {
            for (int b = 0; b < burst && delivered < deliver; ++b, ++delivered)
                StepWorld2D(world, sc);
        }
        StepWorld2D(world, sc); // flush the final delivery

        result[variant] = b2Body_GetPosition(ball->m_BodyId);
        g_coll[variant] = g_collisions;

        DeleteCollisionObject2D(world, ball);
        DeleteCollisionObject2D(world, floor);
        DeleteCollisionShape2D(ball_shape);
        DeleteCollisionShape2D(floor_shape);
        DeleteWorld2D(context, world);
        DeleteContext2D(context);
    }

    ASSERT_NEAR(result[0].x, result[1].x, 0.001f);
    ASSERT_NEAR(result[0].y, result[1].y, 0.001f);
    ASSERT_EQ(g_coll[0], g_coll[1]); // same number of collision callbacks regardless of grouping
}

// A fast-moving body must land where the synchronous path lands.
TEST(AsyncVelocity, HighVelocityRestMatchesSync)
{
    b2Vec2 out[2];
    for (int variant = 0; variant < 2; ++variant)
    {
        bool async = variant == 1;
        HContext2D context = NewFeedbackContext();
        NewWorldParams wp;
        wp.m_GetWorldTransformCallback = GObjGet;
        wp.m_SetWorldTransformCallback = GObjSet;
        wp.m_UseDoubleBufferedWorlds   = async;
        World2D* world = (World2D*)NewWorld2D(context, wp);

        GObj floor_o = { dmVMath::Vector3(0.0f, -3.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
        HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(6.0f, 0.3f, 0.0f));
        CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_o;
        Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

        GObj ball_o = { dmVMath::Vector3(-4.0f, 2.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
        HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
        CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_o;
        Body* ball = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);
        b2Body_SetLinearVelocity(ball->m_BodyId, b2Vec2{40.0f, 0.0f}); // fast to the right

        StepWorldContext sc;
        sc.m_DT = 1.0f / 60.0f;
        sc.m_Box2DSubStepCount = 4;

        for (int i = 0; i < 200; ++i) StepWorld2D(world, sc);
        if (async) StepWorld2D(world, sc); // flush

        out[variant] = b2Body_GetPosition(ball->m_BodyId);

        DeleteCollisionObject2D(world, ball);
        DeleteCollisionObject2D(world, floor);
        DeleteCollisionShape2D(ball_shape);
        DeleteCollisionShape2D(floor_shape);
        DeleteWorld2D(context, world);
        DeleteContext2D(context);
    }
    ASSERT_NEAR(out[0].x, out[1].x, 0.02f);
    ASSERT_NEAR(out[0].y, out[1].y, 0.02f);
}

// Growing a body's game-object scale at runtime must resize the twin's collision shape to match.
TEST(AsyncShape, RuntimeResizeMirrorsToTwin)
{
    HContext2D context = NewFeedbackContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = GObjGet;
    wp.m_SetWorldTransformCallback = GObjSet;
    wp.m_UseDoubleBufferedWorlds   = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    GObj ball_o = { dmVMath::Vector3(0.0f, 0.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_o;
    Body* ball = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;

    StepWorld2D(world, sc);
    ball_o.m_Scale = 2.5f; // grow the object
    for (int i = 0; i < 5; ++i) StepWorld2D(world, sc);
    WaitForWorker2D(world);

    CircleShapeData* game_circle = (CircleShapeData*) ball->m_Shapes[0];
    b2ShapeId twin_shapes[1];
    int n = b2Body_GetShapes(ball->m_PhysicsBodyId, twin_shapes, 1);
    ASSERT_EQ(1, n);
    float twin_r = b2Shape_GetCircle(twin_shapes[0]).radius;

    ASSERT_NEAR(game_circle->m_Circle.radius, twin_r, 0.001f); // twin tracks the grown game shape

    DeleteCollisionObject2D(world, ball);
    DrainPendingOps(world);
    DeleteCollisionShape2D(ball_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// A kinematic body IS script-driven: moving its game object must carry into the simulation (the pull
// is gated only for dynamic bodies).
TEST(AsyncKinematic, KinematicFollowsGameObject)
{
    HContext2D context = NewFeedbackContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = GObjGet;
    wp.m_SetWorldTransformCallback = GObjSet;
    wp.m_UseDoubleBufferedWorlds   = true;
    World2D* world = (World2D*)NewWorld2D(context, wp);

    GObj obj = { dmVMath::Vector3(0.0f, 0.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
    HCollisionShape2D shape = NewCircleShape2D(context, 0.5f);
    CollisionObjectData d; d.m_Type = COLLISION_OBJECT_TYPE_KINEMATIC; d.m_Mass = 0.0f; d.m_UserData = &obj;
    Body* body = (Body*)NewCollisionObject2D(world, d, &shape, 1);

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;

    // Drive the game object to the right each step.
    for (int i = 0; i < 30; ++i)
    {
        obj.m_Pos.setX(obj.m_Pos.getX() + 0.1f);
        StepWorld2D(world, sc);
    }
    StepWorld2D(world, sc); // flush
    WaitForWorker2D(world);

    b2Vec2 game_pos = b2Body_GetPosition(body->m_BodyId);
    b2Vec2 twin_pos = b2Body_GetPosition(body->m_PhysicsBodyId);
    // The kinematic body followed the script-driven game object (moved well to the right), and the
    // twin matches the game body.
    ASSERT_GT(game_pos.x, 2.0f);
    ASSERT_NEAR(game_pos.x, twin_pos.x, 0.2f); // within one step of drive (N-1)

    DeleteCollisionObject2D(world, body);
    DrainPendingOps(world);
    DeleteCollisionShape2D(shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

// A spinning body's final orientation and rest must match the synchronous path.
TEST(AsyncAngular, SpinRestMatchesSync)
{
    float angle[2];
    for (int variant = 0; variant < 2; ++variant)
    {
        bool async = variant == 1;
        HContext2D context = NewFeedbackContext();
        NewWorldParams wp;
        wp.m_GetWorldTransformCallback = GObjGet;
        wp.m_SetWorldTransformCallback = GObjSet;
        wp.m_UseDoubleBufferedWorlds   = async;
        World2D* world = (World2D*)NewWorld2D(context, wp);

        // Box so angular velocity is meaningful.
        GObj obj = { dmVMath::Vector3(0.0f, 0.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
        HCollisionShape2D shape = NewBoxShape2D(context, dmVMath::Vector3(0.5f, 0.2f, 0.0f));
        CollisionObjectData d; d.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; d.m_Mass = 1.0f; d.m_UserData = &obj;
        Body* body = (Body*)NewCollisionObject2D(world, d, &shape, 1);
        b2Body_SetAngularVelocity(body->m_BodyId, 3.0f);
        b2Body_SetLinearVelocity(body->m_BodyId, b2Vec2{0.0f, 0.0f});
        b2Body_SetFixedRotation(body->m_BodyId, false);

        StepWorldContext sc;
        sc.m_DT = 1.0f / 60.0f;
        sc.m_Box2DSubStepCount = 4;

        for (int i = 0; i < 30; ++i) StepWorld2D(world, sc);
        if (async) StepWorld2D(world, sc); // flush

        angle[variant] = b2Rot_GetAngle(b2Body_GetRotation(body->m_BodyId));

        DeleteCollisionObject2D(world, body);
        DeleteCollisionShape2D(shape);
        DeleteWorld2D(context, world);
        DeleteContext2D(context);
    }
    ASSERT_NEAR(angle[0], angle[1], 0.01f);
}

// --- Mode parity: sync vs async-threaded vs async-inline -----------------------------------------
// Every scenario runs in three modes and the results are compared. The two async modes (worker
// thread vs inline) are the same N-1 pipeline and must match exactly; async must match sync (rest
// state within tolerance, callback counts exactly, using the one-step flush).

enum RunMode { RM_SYNC = 0, RM_ASYNC_THREADED = 1, RM_ASYNC_INLINE = 2 };

static void ConfigureMode(NewWorldParams& wp, RunMode m)
{
    wp.m_UseDoubleBufferedWorlds = (m != RM_SYNC);
    wp.m_AsyncWorkerInline       = (m == RM_ASYNC_INLINE);
}

struct SceneResult
{
    int    m_BallCount;
    b2Vec2 m_BallPos[4];
    int    m_Collisions;
    int    m_ContactPoints;
    int    m_TriggerEnters;
    int    m_TriggerExits;
};

// Balls drop through a static sensor onto a floor: exercises collision, contact-point, and trigger
// enter/exit callbacks plus resting behaviour, driven through the game-object transform feedback.
static SceneResult RunRichScene(RunMode mode)
{
    g_collisions = g_contact_points = g_trigger_enters = g_trigger_exits = 0;

    HContext2D context = NewFeedbackContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = GObjGet;
    wp.m_SetWorldTransformCallback = GObjSet;
    ConfigureMode(wp, mode);
    World2D* world = (World2D*)NewWorld2D(context, wp);

    GObj floor_o = { dmVMath::Vector3(0.0f, -3.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(5.0f, 0.3f, 0.0f));
    CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_o;
    Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

    GObj sensor_o = { dmVMath::Vector3(0.0f, -0.5f, 0.0f), dmVMath::Quat::identity(), 1.0f };
    HCollisionShape2D sensor_shape = NewBoxShape2D(context, dmVMath::Vector3(2.0f, 0.3f, 0.0f));
    CollisionObjectData snd; snd.m_Type = COLLISION_OBJECT_TYPE_TRIGGER; snd.m_Mass = 0.0f; snd.m_UserData = &sensor_o;
    Body* sensor = (Body*)NewCollisionObject2D(world, snd, &sensor_shape, 1);

    const int N = 3;
    GObj ball_o[N];
    Body* balls[N];
    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.5f);
    for (int i = 0; i < N; ++i)
    {
        ball_o[i].m_Pos = dmVMath::Vector3((i - 1) * 0.2f, 2.0f + i * 1.3f, 0.0f);
        ball_o[i].m_Rot = dmVMath::Quat::identity();
        ball_o[i].m_Scale = 1.0f;
        CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &ball_o[i];
        balls[i] = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);
    }

    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount      = 4;
    sc.m_CollisionCallback      = ParityOnCollision;
    sc.m_ContactPointCallback   = ParityOnContactPoint;
    sc.m_TriggerEnteredCallback = ParityOnTriggerEnter;
    sc.m_TriggerExitedCallback  = ParityOnTriggerExit;

    for (int i = 0; i < 220; ++i) StepWorld2D(world, sc);
    if (mode != RM_SYNC) StepWorld2D(world, sc); // flush the N-1 delivery

    SceneResult r;
    r.m_BallCount = N;
    for (int i = 0; i < N; ++i) r.m_BallPos[i] = b2Body_GetPosition(balls[i]->m_BodyId);
    r.m_Collisions    = g_collisions;
    r.m_ContactPoints = g_contact_points;
    r.m_TriggerEnters = g_trigger_enters;
    r.m_TriggerExits  = g_trigger_exits;

    for (int i = 0; i < N; ++i) DeleteCollisionObject2D(world, balls[i]);
    DeleteCollisionObject2D(world, sensor);
    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(sensor_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
    return r;
}

TEST(AsyncModeParity, RichSceneMatchesAcrossModes)
{
    SceneResult s = RunRichScene(RM_SYNC);
    SceneResult t = RunRichScene(RM_ASYNC_THREADED);
    SceneResult n = RunRichScene(RM_ASYNC_INLINE);

    // Non-vacuous: the scene exercised every callback path.
    ASSERT_GT(s.m_Collisions, 0);
    ASSERT_GT(s.m_ContactPoints, 0);
    ASSERT_GT(s.m_TriggerEnters, 0);
    ASSERT_GT(s.m_TriggerExits, 0);

    // Threaded vs inline async are the identical N-1 pipeline: results must match exactly.
    ASSERT_EQ(t.m_Collisions,    n.m_Collisions);
    ASSERT_EQ(t.m_ContactPoints, n.m_ContactPoints);
    ASSERT_EQ(t.m_TriggerEnters, n.m_TriggerEnters);
    ASSERT_EQ(t.m_TriggerExits,  n.m_TriggerExits);
    for (int i = 0; i < s.m_BallCount; ++i)
    {
        ASSERT_NEAR(t.m_BallPos[i].x, n.m_BallPos[i].x, 1e-5f);
        ASSERT_NEAR(t.m_BallPos[i].y, n.m_BallPos[i].y, 1e-5f);
    }

    // Async matches sync: same callback counts (flush aligns delivered steps), rest within tolerance.
    ASSERT_EQ(s.m_Collisions,    t.m_Collisions);
    ASSERT_EQ(s.m_ContactPoints, t.m_ContactPoints);
    ASSERT_EQ(s.m_TriggerEnters, t.m_TriggerEnters);
    ASSERT_EQ(s.m_TriggerExits,  t.m_TriggerExits);
    for (int i = 0; i < s.m_BallCount; ++i)
    {
        ASSERT_NEAR(s.m_BallPos[i].x, t.m_BallPos[i].x, 0.01f);
        ASSERT_NEAR(s.m_BallPos[i].y, t.m_BallPos[i].y, 0.01f);
    }
}

// Create/destroy churn across a run, compared threaded vs inline: the two async modes must end with
// the same body count and the same collision-callback total, and neither may crash.
static void RunChurn(RunMode mode, int* out_bodies, int* out_collisions)
{
    HContext2D context = NewFeedbackContext();
    NewWorldParams wp;
    wp.m_GetWorldTransformCallback = GObjGet;
    wp.m_SetWorldTransformCallback = GObjSet;
    ConfigureMode(wp, mode);
    World2D* world = (World2D*)NewWorld2D(context, wp);

    GObj floor_o = { dmVMath::Vector3(0.0f, -3.0f, 0.0f), dmVMath::Quat::identity(), 1.0f };
    HCollisionShape2D floor_shape = NewBoxShape2D(context, dmVMath::Vector3(6.0f, 0.3f, 0.0f));
    CollisionObjectData fd; fd.m_Type = COLLISION_OBJECT_TYPE_STATIC; fd.m_Mass = 0.0f; fd.m_UserData = &floor_o;
    Body* floor = (Body*)NewCollisionObject2D(world, fd, &floor_shape, 1);

    HCollisionShape2D ball_shape = NewCircleShape2D(context, 0.4f);
    static GObj infos[256];
    Body* live[32];
    int live_count = 0, next = 0;

    g_collisions = 0;
    StepWorldContext sc;
    sc.m_DT = 1.0f / 60.0f;
    sc.m_Box2DSubStepCount = 4;
    sc.m_CollisionCallback = ParityOnCollision;

    for (int f = 0; f < 200; ++f)
    {
        StepWorld2D(world, sc);
        if (live_count < 20 && next < 256)
        {
            infos[next].m_Pos = dmVMath::Vector3((float)((next % 7) - 3) * 0.5f, 3.0f, 0.0f);
            infos[next].m_Rot = dmVMath::Quat::identity();
            infos[next].m_Scale = 1.0f;
            CollisionObjectData bd; bd.m_Type = COLLISION_OBJECT_TYPE_DYNAMIC; bd.m_Mass = 1.0f; bd.m_UserData = &infos[next];
            live[live_count++] = (Body*)NewCollisionObject2D(world, bd, &ball_shape, 1);
            next++;
        }
        if ((f % 3) == 0 && live_count > 0)
        {
            DeleteCollisionObject2D(world, live[0]);
            for (int k = 1; k < live_count; ++k) live[k-1] = live[k];
            live_count--;
        }
    }

    WaitForWorker2D(world);
    DrainPendingOps(world);
    *out_bodies     = (int)world->m_Bodies.Size();
    *out_collisions = g_collisions;

    for (int i = 0; i < live_count; ++i) DeleteCollisionObject2D(world, live[i]);
    DeleteCollisionObject2D(world, floor);
    DeleteCollisionShape2D(ball_shape);
    DeleteCollisionShape2D(floor_shape);
    DeleteWorld2D(context, world);
    DeleteContext2D(context);
}

TEST(AsyncModeParity, ChurnThreadedMatchesInline)
{
    int tb, tc, nb, nc;
    RunChurn(RM_ASYNC_THREADED, &tb, &tc);
    RunChurn(RM_ASYNC_INLINE,   &nb, &nc);
    ASSERT_GT(tc, 0);       // churn produced collisions
    ASSERT_EQ(tb, nb);      // same surviving body count
    ASSERT_EQ(tc, nc);      // same collision-callback total
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
