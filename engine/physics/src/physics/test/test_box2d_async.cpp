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

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
