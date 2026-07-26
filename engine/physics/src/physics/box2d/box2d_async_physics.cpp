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
#include <math.h>

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
                out.polygon.radius       = p->m_Polygon.radius;
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

    // Make the physics-world twin's collision shapes geometrically identical to the game-world
    // shapes. The game shapes (body->m_Shapes) are the authoritative geometry the synchronous path
    // simulates - kept correct by UpdateScale and by shape recreation or resize. Reconstructing the
    // twin size from a stored creation-scale drifts from that (especially when a collision object is
    // recreated at a new size), so mirror the game geometry directly. Only shapes that actually
    // differ are updated, so a resting body's shapes are untouched and it can sleep.
    static void MirrorShapesToTwin(Body* body)
    {
        b2BodyId twin = body->m_PhysicsBodyId;

        // Mirror the collision filter so runtime group/mask changes (SetGroup2D / SetMaskBit2D) take
        // effect in the twin, which is the body actually simulated. Those setters apply the same filter
        // to every shape of the body, so the game filter can be copied onto every twin shape regardless
        // of b2Body_GetShapes enumeration order. Being order-independent, this is safe for multi-shape
        // bodies (unlike the index-based geometry mirroring below).
        if (body->m_ShapeCount > 0 && body->m_Shapes[0]->m_Type != SHAPE_TYPE_GRID)
        {
            b2Filter gf = b2Shape_GetFilter(body->m_Shapes[0]->m_ShapeId);
            b2ShapeId twin_all[MAX_OP_SHAPES];
            int tn = b2Body_GetShapes(twin, twin_all, MAX_OP_SHAPES);
            for (int i = 0; i < tn; ++i)
            {
                b2Filter tf = b2Shape_GetFilter(twin_all[i]);
                if (gf.categoryBits != tf.categoryBits || gf.maskBits != tf.maskBits || gf.groupIndex != tf.groupIndex)
                {
                    b2Shape_SetFilter(twin_all[i], gf);
                }
            }
        }

        // Geometry mirror: index-based, so b2Body_GetShapes order must match body->m_Shapes, which is
        // only guaranteed for single-shape bodies. Multi-shape geometry is not mirrored (a runtime size
        // change on a multi-shape body is not yet implemented for the async path).
        int cap = b2Body_GetShapeCount(twin);
        if (cap != 1 || body->m_ShapeCount != 1)
        {
            return;
        }
        b2ShapeId twin_shapes[MAX_OP_SHAPES];
        int n = b2Body_GetShapes(twin, twin_shapes, cap);
        for (int i = 0; i < n && i < (int) body->m_ShapeCount; ++i)
        {
            ShapeData* gs = body->m_Shapes[i];
            b2ShapeType tt = b2Shape_GetType(twin_shapes[i]);
            if (gs->m_Type == SHAPE_TYPE_CIRCLE)
            {
                if (tt != b2_circleShape)
                {
                    continue;
                }
                CircleShapeData* c = (CircleShapeData*) gs;
                b2Circle tc = b2Shape_GetCircle(twin_shapes[i]);
                if (fabsf(tc.radius   - c->m_Circle.radius)   > 1e-5f ||
                    fabsf(tc.center.x - c->m_Circle.center.x) > 1e-5f ||
                    fabsf(tc.center.y - c->m_Circle.center.y) > 1e-5f)
                {
                    tc.radius = c->m_Circle.radius;
                    tc.center = c->m_Circle.center;
                    b2Shape_SetCircle(twin_shapes[i], &tc);
                }
            }
            else if (gs->m_Type == SHAPE_TYPE_POLYGON)
            {
                if (tt != b2_polygonShape)
                {
                    continue;
                }
                PolygonShapeData* p = (PolygonShapeData*) gs;
                b2Polygon tp = b2Shape_GetPolygon(twin_shapes[i]);
                bool differ = tp.count != p->m_Polygon.count || fabsf(tp.radius - p->m_Polygon.radius) > 1e-5f;
                for (int j = 0; !differ && j < tp.count; ++j)
                {
                    if (fabsf(tp.vertices[j].x - p->m_Polygon.vertices[j].x) > 1e-5f ||
                        fabsf(tp.vertices[j].y - p->m_Polygon.vertices[j].y) > 1e-5f)
                    {
                        differ = true;
                    }
                }
                if (differ)
                {
                    b2Shape_SetPolygon(twin_shapes[i], &p->m_Polygon);
                }
            }
            // Grid shapes are not yet implemented for the async path.
        }
    }

    void SyncGameToPhysics(World2D* world)
    {
        for (uint32_t i = 0; i < world->m_Bodies.Size(); ++i)
        {
            Body* body = world->m_Bodies[i];
            if (!b2Body_IsValid(body->m_PhysicsBodyId))
            {
                continue;
            }

            b2Vec2 position = b2Body_GetPosition(body->m_BodyId);
            b2Rot  rotation = b2Body_GetRotation(body->m_BodyId);
            b2Body_SetTransform(body->m_PhysicsBodyId, position, rotation);
            b2Body_SetLinearVelocity(body->m_PhysicsBodyId, b2Body_GetLinearVelocity(body->m_BodyId));
            b2Body_SetAngularVelocity(body->m_PhysicsBodyId, b2Body_GetAngularVelocity(body->m_BodyId));

            // Inject any force/torque accumulated on the game body this frame onto the twin, which is
            // the body actually stepped, then clear it. The step consumes and clears the twin's own
            // accumulator, so each frame starts fresh.
            if (body->m_PendingForce.x != 0.0f || body->m_PendingForce.y != 0.0f || body->m_PendingTorque != 0.0f)
            {
                b2Body_ApplyForceToCenter(body->m_PhysicsBodyId, body->m_PendingForce, true);
                b2Body_ApplyTorque(body->m_PhysicsBodyId, body->m_PendingTorque, true);
                body->m_PendingForce  = b2Vec2_zero;
                body->m_PendingTorque = 0.0f;
            }

            // Keep the twin's collision geometry identical to the game shape (handles scale changes
            // and shape recreation that the stored-scale mirror path misses).
            MirrorShapesToTwin(body);
        }
    }

    void SyncPhysicsToGame(World2D* world)
    {
        float inv_scale = world->m_Context->m_InvScale;
        for (uint32_t i = 0; i < world->m_Bodies.Size(); ++i)
        {
            Body* body = world->m_Bodies[i];
            if (!b2Body_IsValid(body->m_PhysicsBodyId))
            {
                continue;
            }

            b2Vec2 position = b2Body_GetPosition(body->m_PhysicsBodyId);
            b2Rot  rotation = b2Body_GetRotation(body->m_PhysicsBodyId);
            b2Body_SetTransform(body->m_BodyId, position, rotation);
            b2Body_SetLinearVelocity(body->m_BodyId, b2Body_GetLinearVelocity(body->m_PhysicsBodyId));
            b2Body_SetAngularVelocity(body->m_BodyId, b2Body_GetAngularVelocity(body->m_PhysicsBodyId));

            if (world->m_SetWorldTransformCallback &&
                b2Body_GetType(body->m_BodyId) == b2_dynamicBody &&
                b2Body_IsEnabled(body->m_BodyId))
            {
                dmVMath::Point3 out_position;
                FromB2(position, out_position, inv_scale);
                dmVMath::Quat out_rotation = dmVMath::Quat::rotationZ(b2Rot_GetAngle(rotation));
                (*world->m_SetWorldTransformCallback)(b2Body_GetUserData(body->m_BodyId), out_position, out_rotation);
            }
        }
    }

    void EnqueueEnableBody(World2D* world, Body* owner, bool enable)
    {
        if (b2Body_IsValid(owner->m_PhysicsBodyId))
        {
            PendingPhysicsOp op;
            memset(&op, 0, sizeof(PendingPhysicsOp));
            op.m_Type = enable ? OP_ENABLE_BODY : OP_DISABLE_BODY;
            if (enable)
            {
                op.m_Data.enable_body.body_id = owner->m_PhysicsBodyId;
            }
            else
            {
                op.m_Data.disable_body.body_id = owner->m_PhysicsBodyId;
            }
            PushOp(world, op);
            return;
        }

        // Twin not created (enable/disable applied between create and the first drain): fold the
        // state into the pending create so the twin is created enabled/disabled as intended.
        // A standalone enable/disable op here would be dropped, since it needs a valid twin id.
        dmArray<PendingPhysicsOp>& q = world->m_PendingOps;
        for (uint32_t i = 0; i < q.Size(); ++i)
        {
            PendingPhysicsOp& op = q[i];
            if (op.m_Type == OP_CREATE_BODY && op.m_Data.create_body.m_Owner == owner)
            {
                op.m_Data.create_body.enabled = enable ? 1 : 0;
                return;
            }
        }
    }

    void EnqueueScaleBody(World2D* world, Body* owner, float scale)
    {
        if (!b2Body_IsValid(owner->m_PhysicsBodyId))
        {
            return;
        }
        PendingPhysicsOp op;
        memset(&op, 0, sizeof(PendingPhysicsOp));
        op.m_Type = OP_SCALE_BODY;
        op.m_Data.scale_body.body_id = owner->m_PhysicsBodyId;
        op.m_Data.scale_body.scale   = scale;
        PushOp(world, op);
    }

    void EnqueueSetGravity(World2D* world, float gravity_x, float gravity_y, float gravity_z)
    {
        PendingPhysicsOp op;
        memset(&op, 0, sizeof(PendingPhysicsOp));
        op.m_Type = OP_SET_GRAVITY;
        op.m_Data.set_gravity.gravity_x = gravity_x;
        op.m_Data.set_gravity.gravity_y = gravity_y;
        op.m_Data.set_gravity.gravity_z = gravity_z;
        PushOp(world, op);
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
                // The create op snapshots the body's shapes when it is enqueued. Anything that changed
                // the game body's shapes between enqueue and this drain (e.g. a filter/group change in
                // the object's init, before its first step) is not in the snapshot, so align the twin to
                // the game body's current shapes. This makes the twin born matching the game body,
                // with no first-step window where a mismatched filter or geometry is simulated.
                MirrorShapesToTwin(owner);
            }
            else
            {
                ApplyOperation(world->m_PhysicsWorldId, op, scale);
            }
        }
        q.SetSize(0);
    }
}
