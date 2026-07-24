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
#include <dlib/array.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../box2d/box2d_physics.h" // ShapeType / SHAPE_TYPE_*
#include "../box2d/box2d_operation_queue.h"

using namespace dmPhysics;

static PendingPhysicsOp MakeCreateCircleBody(float x, float y)
{
    PendingPhysicsOp op = {};
    op.m_Type = OP_CREATE_BODY;
    PendingPhysicsOp::OpData::CreateBody& b = op.m_Data.create_body;
    b.position_x = x;
    b.position_y = y;
    b.rotation_angle = 0.0f;
    b.gravity_scale = 1.0f;
    b.mass = 1.0f;
    b.body_type = (uint8_t)b2_dynamicBody;
    b.enabled = 1;
    b.awake = 1;
    b.sleeping_allowed = 1;
    b.shape_count = 1;

    OpShapeData& s = b.shapes[0];
    s.type = (uint8_t)SHAPE_TYPE_CIRCLE;
    s.circle.center_x = 0.0f;
    s.circle.center_y = 0.0f;
    s.circle.radius = 0.5f;
    s.creation_scale = 1.0f;
    s.friction = 0.1f;
    s.restitution = 0.0f;
    s.filter_group = 1;
    s.filter_mask = 0xffff;
    s.enable_contact_events = 1;
    return op;
}

// Capture the (single) body that moved during the last step, via the world's body move events.
static b2BodyId GetMovedBody(b2WorldId world)
{
    b2BodyEvents events = b2World_GetBodyEvents(world);
    if (events.moveCount > 0)
        return events.moveEvents[0].bodyId;
    return b2_nullBodyId;
}

TEST(OperationQueue, ProcessClearsQueue)
{
    b2WorldDef wd = b2DefaultWorldDef();
    b2WorldId world = b2CreateWorld(&wd);

    dmArray<PendingPhysicsOp> q;
    q.SetCapacity(4);
    q.Push(MakeCreateCircleBody(0.0f, 0.0f));
    ASSERT_EQ(1u, q.Size());
    ProcessOperationQueue(world, q, 1.0f);
    ASSERT_EQ(0u, q.Size());

    b2DestroyWorld(world);
}

TEST(OperationQueue, SetGravity)
{
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = b2Vec2{0.0f, 0.0f};
    b2WorldId world = b2CreateWorld(&wd);

    PendingPhysicsOp op = {};
    op.m_Type = OP_SET_GRAVITY;
    op.m_Data.set_gravity.gravity_x = 0.0f;
    op.m_Data.set_gravity.gravity_y = -7.5f;
    op.m_Data.set_gravity.gravity_z = 0.0f;
    ApplyOperation(world, op, 1.0f);

    b2Vec2 g = b2World_GetGravity(world);
    ASSERT_NEAR(0.0f, g.x, 0.0001f);
    ASSERT_NEAR(-7.5f, g.y, 0.0001f);

    b2DestroyWorld(world);
}

TEST(OperationQueue, CreateDisableEnableDestroy)
{
    b2WorldDef wd = b2DefaultWorldDef();
    wd.gravity = b2Vec2{0.0f, -10.0f};
    b2WorldId world = b2CreateWorld(&wd);

    // Create a dynamic body via the op queue.
    {
        dmArray<PendingPhysicsOp> q;
        q.SetCapacity(1);
        q.Push(MakeCreateCircleBody(0.0f, 0.0f));
        ProcessOperationQueue(world, q, 1.0f);
    }

    // Step so the (only) dynamic body moves under gravity; capture its physics-world id.
    b2World_Step(world, 1.0f / 60.0f, 4);
    b2BodyId body = GetMovedBody(world);
    ASSERT_TRUE(b2Body_IsValid(body));
    ASSERT_EQ((int)b2_dynamicBody, (int)b2Body_GetType(body));
    ASSERT_EQ(1, b2Body_GetShapeCount(body));
    ASSERT_TRUE(b2Body_IsEnabled(body));

    // Disable it.
    {
        PendingPhysicsOp op = {};
        op.m_Type = OP_DISABLE_BODY;
        op.m_Data.disable_body.body_id = body;
        ApplyOperation(world, op, 1.0f);
    }
    ASSERT_FALSE(b2Body_IsEnabled(body));

    // Re-enable it.
    {
        PendingPhysicsOp op = {};
        op.m_Type = OP_ENABLE_BODY;
        op.m_Data.enable_body.body_id = body;
        ApplyOperation(world, op, 1.0f);
    }
    ASSERT_TRUE(b2Body_IsEnabled(body));

    // Destroy it.
    {
        PendingPhysicsOp op = {};
        op.m_Type = OP_DESTROY_BODY;
        op.m_Data.destroy_body.body_id = body;
        ApplyOperation(world, op, 1.0f);
    }
    ASSERT_FALSE(b2Body_IsValid(body));

    b2DestroyWorld(world);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
