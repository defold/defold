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

#include "box2d_async_physics.h"
#include <string.h>

namespace dmPhysics
{
    static void PushOp(World2D* world, const PendingPhysicsOp& op)
    {
        if (world->m_PendingOps.Full())
        {
            world->m_PendingOps.OffsetCapacity(32);
        }
        world->m_PendingOps.Push(op);
    }

    // Copy one already-transformed game-world shape into POD operation data.
    static void FillOpShape(OpShapeData& out, const ShapeData* shape, const CollisionObjectData& data)
    {
        memset(&out, 0, sizeof(OpShapeData));
        out.type                  = (uint8_t)shape->m_Type;
        out.creation_scale        = shape->m_CreationScale;
        out.creation_position_x   = shape->m_CreationPosition.x;
        out.creation_position_y   = shape->m_CreationPosition.y;
        out.friction              = data.m_Friction;
        out.restitution           = data.m_Restitution;
        out.filter_group          = data.m_Group;
        out.filter_mask           = data.m_Mask;
        out.is_sensor             = data.m_Type == COLLISION_OBJECT_TYPE_TRIGGER;
        out.enable_contact_events = 1;
        out.enable_hit_events     = 1;
        out.enable_sensor_events  = 1;
        out.user_data             = data.m_UserData;

        switch (shape->m_Type)
        {
            case SHAPE_TYPE_CIRCLE:
            {
                const CircleShapeData* c = (const CircleShapeData*) shape;
                out.circle.center_x = c->m_Circle.center.x;
                out.circle.center_y = c->m_Circle.center.y;
                out.circle.radius   = c->m_Circle.radius;
            } break;
            case SHAPE_TYPE_POLYGON:
            {
                const PolygonShapeData* p = (const PolygonShapeData*) shape;
                out.polygon.vertex_count = (uint8_t)p->m_Polygon.count;
                out.polygon.centroid_x   = p->m_Polygon.centroid.x;
                out.polygon.centroid_y   = p->m_Polygon.centroid.y;
                out.centroid_original_x  = p->m_CentroidOriginal.x;
                out.centroid_original_y  = p->m_CentroidOriginal.y;
                for (int j = 0; j < p->m_Polygon.count; ++j)
                {
                    out.polygon.vertices_x[j]    = p->m_Polygon.vertices[j].x;
                    out.polygon.vertices_y[j]    = p->m_Polygon.vertices[j].y;
                    out.vertices_original_x[j]   = p->m_VerticesOriginal[j].x;
                    out.vertices_original_y[j]   = p->m_VerticesOriginal[j].y;
                }
            } break;
            default:
                // Grid shapes are not yet implemented for the operation queue.
                break;
        }
    }

    void EnqueueCreateBody(World2D* world, const b2BodyDef& def, const CollisionObjectData& data, Body* owner)
    {
        PendingPhysicsOp op;
        memset(&op, 0, sizeof(PendingPhysicsOp));
        op.m_Type = OP_CREATE_BODY;

        PendingPhysicsOp::OpData::CreateBody& b = op.m_Data.create_body;
        b.m_Owner          = owner;
        b.position_x       = def.position.x;   // already scaled at body creation
        b.position_y       = def.position.y;
        b.rotation_angle   = b2Rot_GetAngle(def.rotation);
        b.linear_velocity_x= def.linearVelocity.x;
        b.linear_velocity_y= def.linearVelocity.y;
        b.angular_velocity = def.angularVelocity;
        b.linear_damping   = def.linearDamping;
        b.angular_damping  = def.angularDamping;
        b.gravity_scale    = def.gravityScale;
        b.mass             = data.m_Mass;
        b.body_type        = (uint8_t)def.type;
        b.bullet           = def.isBullet ? 1 : 0;
        b.enabled          = def.isEnabled ? 1 : 0;
        b.locked_rotation  = def.fixedRotation ? 1 : 0;
        b.awake            = def.isAwake ? 1 : 0;
        b.sleeping_allowed = def.enableSleep ? 1 : 0;
        b.user_data        = def.userData;

        uint8_t shape_count = owner->m_ShapeCount;
        if (shape_count > MAX_OP_SHAPES)
        {
            shape_count = MAX_OP_SHAPES;
        }
        b.shape_count = shape_count;
        for (uint8_t i = 0; i < shape_count; ++i)
        {
            FillOpShape(b.shapes[i], owner->m_Shapes[i], data);
        }

        PushOp(world, op);
    }

    void EnqueueDestroyBody(World2D* world, Body* owner)
    {
        if (b2Body_IsValid(owner->m_PhysicsBodyId))
        {
            PendingPhysicsOp op;
            memset(&op, 0, sizeof(PendingPhysicsOp));
            op.m_Type = OP_DESTROY_BODY;
            op.m_Data.destroy_body.body_id = owner->m_PhysicsBodyId;
            PushOp(world, op);
            return;
        }

        // Twin not created: neutralize the pending create so the drain skips it and no
        // orphan body is left behind. The owner Body is freed by the caller right after this returns,
        // so its create op must not be applied.
        dmArray<PendingPhysicsOp>& q = world->m_PendingOps;
        for (uint32_t i = 0; i < q.Size(); ++i)
        {
            PendingPhysicsOp& op = q[i];
            if (op.m_Type == OP_CREATE_BODY && op.m_Data.create_body.m_Owner == owner)
            {
                op.m_Data.create_body.m_Owner = 0x0;
            }
        }
    }

    void DrainPendingOps(World2D* world)
    {
        dmArray<PendingPhysicsOp>& q = world->m_PendingOps;
        if (q.Empty())
        {
            return;
        }

        float scale = world->m_Context->m_Scale;
        for (uint32_t i = 0; i < q.Size(); ++i)
        {
            PendingPhysicsOp& op = q[i];
            if (op.m_Type == OP_CREATE_BODY)
            {
                Body* owner = (Body*) op.m_Data.create_body.m_Owner;
                if (owner == 0x0)
                {
                    continue; // cancelled create (see EnqueueDestroyBody)
                }
                owner->m_PhysicsBodyId = ApplyCreateBodyOp(world->m_PhysicsWorldId, op.m_Data.create_body, scale);
            }
            else
            {
                ApplyOperation(world->m_PhysicsWorldId, op, scale);
            }
        }
        q.SetSize(0);
    }
}
