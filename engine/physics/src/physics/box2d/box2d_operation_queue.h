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

#ifndef DM_BOX2D_OPERATION_QUEUE_H
#define DM_BOX2D_OPERATION_QUEUE_H

#include <stdint.h>
#include <box2d/box2d.h>
#include <dlib/array.h>

namespace dmPhysics
{
    // Structural physics operations queued by the main thread and applied to the physics world
    // at a safe point (when the async worker is idle). Applying a create/destroy/enable/scale/gravity
    // operation from these PODs never dereferences a live game-world object, so the queue can be
    // drained without racing the worker.
    enum PendingPhysicsOpType
    {
        OP_CREATE_BODY,
        OP_DESTROY_BODY,
        OP_ENABLE_BODY,
        OP_DISABLE_BODY,
        OP_SCALE_BODY,
        OP_SET_GRAVITY,
    };

    // Maximum shapes per collision object (mirrors MAX_SHAPE_COUNT in physics.h)
    static const int MAX_OP_SHAPES = 8;

    // POD shape data for the operation queue (no pointers into engine structures)
    struct OpShapeData
    {
        uint8_t type;  // ShapeType enum

        // Current geometry (scaled)
        union {
            // SHAPE_TYPE_CIRCLE
            struct {
                float center_x, center_y;
                float radius;
            } circle;

            // SHAPE_TYPE_POLYGON
            struct {
                float vertices_x[B2_MAX_POLYGON_VERTICES];
                float vertices_y[B2_MAX_POLYGON_VERTICES];
                float centroid_x, centroid_y;
                uint8_t vertex_count;
            } polygon;

            // SHAPE_TYPE_GRID - not yet implemented
        };

        // Original geometry for dynamic scaling (stored outside the union)
        float creation_scale;  // For all shapes
        float creation_position_x, creation_position_y;  // For circles
        float vertices_original_x[B2_MAX_POLYGON_VERTICES];  // For polygons
        float vertices_original_y[B2_MAX_POLYGON_VERTICES];
        float centroid_original_x, centroid_original_y;

        // Shape properties
        float friction;
        float restitution;
        uint16_t filter_group;
        uint16_t filter_mask;
        uint8_t is_sensor;
        uint8_t enable_contact_events;
        uint8_t enable_hit_events;
        uint8_t enable_sensor_events;
        void* user_data;
    };

    struct PendingPhysicsOp
    {
        PendingPhysicsOpType m_Type;

        struct OpData {
            // OP_CREATE_BODY
            struct CreateBody {
                void* m_Owner;           // Owning game-world Body*, written back with the twin id at drain.
                                         // POD tag only: ApplyOperation never dereferences it.
                b2BodyId body_id;        // Body ID created in the game world
                float position_x, position_y;
                float rotation_angle;
                float linear_velocity_x, linear_velocity_y;
                float angular_velocity;
                float linear_damping;
                float angular_damping;
                float gravity_scale;
                float mass;              // For the UpdateMass call
                uint8_t body_type;       // b2BodyType
                uint8_t bullet : 1;
                uint8_t enabled : 1;
                uint8_t locked_rotation : 1;
                uint8_t awake : 1;
                uint8_t sleeping_allowed : 1;
                uint8_t : 3;
                void* user_data;

                OpShapeData shapes[MAX_OP_SHAPES];
                uint8_t shape_count;
            } create_body;

            // OP_DESTROY_BODY
            struct DestroyBody {
                b2BodyId body_id;
            } destroy_body;

            // OP_ENABLE_BODY
            struct EnableBody {
                b2BodyId body_id;
            } enable_body;

            // OP_DISABLE_BODY
            struct DisableBody {
                b2BodyId body_id;
            } disable_body;

            // OP_SCALE_BODY
            struct ScaleBody {
                b2BodyId body_id;
                float scale;
            } scale_body;

            // OP_SET_GRAVITY
            struct SetGravity {
                float gravity_x, gravity_y, gravity_z;
            } set_gravity;
        } m_Data;
    };

    /**
     * Apply a CREATE_BODY operation to a world and return the created Box2D body id.
     * Pure b2-only: reads only the POD operation data (never dereferences m_Owner).
     * The async drain uses the returned id to populate the owning Body's physics-world twin.
     *
     * @param world_id The Box2D world to create the body in
     * @param data The create-body operation data
     * @param scale Physics scale factor
     * @return The created body id
     */
    b2BodyId ApplyCreateBodyOp(b2WorldId world_id, const PendingPhysicsOp::OpData::CreateBody& data, float scale);

    /**
     * Apply a single pending physics operation to the specified world.
     * Pure functional: modifies only the Box2D world state, using b2BodyId directly from the
     * operation data (no body lookups needed).
     *
     * @param world_id The Box2D world to apply the operation to
     * @param op The operation to apply
     * @param scale Physics scale factor for position/velocity conversions
     */
    void ApplyOperation(b2WorldId world_id, const PendingPhysicsOp& op, float scale);

    /**
     * Apply an entire queue of pending operations, in order, to the specified world.
     * Clears the queue after processing.
     *
     * @param world_id The Box2D world to apply operations to
     * @param queue The queue of pending operations (cleared after processing)
     * @param scale Physics scale factor for position/velocity conversions
     */
    void ProcessOperationQueue(b2WorldId world_id, dmArray<PendingPhysicsOp>& queue, float scale);
}

#endif // DM_BOX2D_OPERATION_QUEUE_H
