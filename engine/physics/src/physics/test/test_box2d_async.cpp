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

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
