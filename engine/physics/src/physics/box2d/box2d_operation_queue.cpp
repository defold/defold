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

#include "box2d_operation_queue.h"
#include "box2d_physics.h" // for the b2_polygonRadius define
#include <dlib/log.h>
#include <dlib/math.h>
#include <stdlib.h>

// Debug logging controls
// #define DEBUG_PHYSICS_OPERATIONS_LOGGING
#ifdef DEBUG_PHYSICS_OPERATIONS_LOGGING
    #define PHYSICS_OP_LOG(fmt, ...) dmLogInfo(fmt, ##__VA_ARGS__)
#else
    #define PHYSICS_OP_LOG(fmt, ...) (void)0
#endif

namespace dmPhysics
{
    // Shape creation data stored in physics world shape userData for dynamic scaling
    struct PhysicsShapeCreationData
    {
        ShapeType type;
        float creation_scale;
        float creation_position_x, creation_position_y;  // Circles only
        float vertices_original_x[B2_MAX_POLYGON_VERTICES];  // Polygons only
        float vertices_original_y[B2_MAX_POLYGON_VERTICES];
        float centroid_original_x, centroid_original_y;
    };
    // ===== OPERATION APPLICATION FUNCTIONS =====
    // These are pure functional operations that apply changes to a Box2D world.
    // They use b2BodyId directly - no lookups needed since Box2D manages bodies by ID.

    /**
     * Apply CREATE_BODY operation to world.
     * Creates body with properties and shapes from operation data.
     */
    b2BodyId ApplyCreateBodyOp(b2WorldId world_id, const PendingPhysicsOp::OpData::CreateBody& data,
                                   float scale)
    {
        // Create body definition from operation data
        b2BodyDef body_def = b2DefaultBodyDef();
        body_def.type = (b2BodyType)data.body_type;
        body_def.position = b2Vec2{data.position_x, data.position_y};  // Already scaled
        body_def.rotation = b2MakeRot(data.rotation_angle);
        body_def.linearVelocity = b2Vec2{data.linear_velocity_x, data.linear_velocity_y};  // Already scaled
        body_def.angularVelocity = data.angular_velocity;
        body_def.linearDamping = data.linear_damping;
        body_def.angularDamping = data.angular_damping;
        body_def.gravityScale = data.gravity_scale;
        body_def.isBullet = data.bullet;
        body_def.isEnabled = data.enabled;
        body_def.fixedRotation = data.locked_rotation;
        body_def.isAwake = data.awake;
        body_def.enableSleep = data.sleeping_allowed;
        body_def.userData = data.user_data;

        // Create body in world
        b2BodyId body_id = b2CreateBody(world_id, &body_def);

        uint64_t body_id_key = ((uint64_t)(uint32_t)body_id.index1 << 32) | (uint64_t)(uint16_t)body_id.generation;
        PHYSICS_OP_LOG("PHYSICS_OP: Created body in physics world (id={%d,%u,%u} key=0x%llx world=%d)",
                 body_id.index1, body_id.world0, body_id.generation, body_id_key, world_id.index1);

        // Create shapes from POD data
        for (uint32_t i = 0; i < data.shape_count; ++i)
        {
            const OpShapeData& shape_data = data.shapes[i];

            // Allocate creation data for dynamic scaling
            PhysicsShapeCreationData* creation_data = (PhysicsShapeCreationData*)malloc(sizeof(PhysicsShapeCreationData));
            creation_data->type = (ShapeType)shape_data.type;
            creation_data->creation_scale = shape_data.creation_scale;
            creation_data->creation_position_x = shape_data.creation_position_x;
            creation_data->creation_position_y = shape_data.creation_position_y;
            creation_data->centroid_original_x = shape_data.centroid_original_x;
            creation_data->centroid_original_y = shape_data.centroid_original_y;
            for (int j = 0; j < B2_MAX_POLYGON_VERTICES; ++j)
            {
                creation_data->vertices_original_x[j] = shape_data.vertices_original_x[j];
                creation_data->vertices_original_y[j] = shape_data.vertices_original_y[j];
            }

            // Create shape definition
            b2ShapeDef shape_def = b2DefaultShapeDef();
            shape_def.userData = creation_data;  // Store creation data in userData
            shape_def.filter.categoryBits = shape_data.filter_group;
            shape_def.filter.maskBits = shape_data.filter_mask;
            shape_def.density = 1.0f;
            shape_def.material.friction = shape_data.friction;
            shape_def.material.restitution = shape_data.restitution;
            shape_def.isSensor = shape_data.is_sensor;
            shape_def.enableContactEvents = shape_data.enable_contact_events;
            shape_def.enableHitEvents = shape_data.enable_hit_events;
            shape_def.enableSensorEvents = shape_data.enable_sensor_events;

            switch (shape_data.type)
            {
                case SHAPE_TYPE_CIRCLE:
                {
                    b2Circle circle;
                    circle.center = b2Vec2{shape_data.circle.center_x, shape_data.circle.center_y};
                    circle.radius = shape_data.circle.radius;
                    b2CreateCircleShape(body_id, &shape_def, &circle);
                    break;
                }

                case SHAPE_TYPE_POLYGON:
                {
                    b2Polygon polygon;
                    polygon.count = shape_data.polygon.vertex_count;
                    polygon.centroid = b2Vec2{shape_data.polygon.centroid_x, shape_data.polygon.centroid_y};
                    for (int j = 0; j < polygon.count; ++j)
                    {
                        polygon.vertices[j] = b2Vec2{shape_data.polygon.vertices_x[j],
                                                      shape_data.polygon.vertices_y[j]};
                    }
                    // Compute normals (Box2D requires this)
                    for (int j = 0; j < polygon.count; ++j)
                    {
                        int j1 = j;
                        int j2 = j + 1 < polygon.count ? j + 1 : 0;
                        b2Vec2 edge = b2Sub(polygon.vertices[j2], polygon.vertices[j1]);
                        polygon.normals[j] = b2Normalize(b2Vec2{edge.y, -edge.x});
                    }
                    // Match the source polygon's skin radius so the twin shape is geometrically
                    // identical to the game-world shape (b2MakeBox produces radius 0).
                    polygon.radius = shape_data.polygon.radius;
                    b2CreatePolygonShape(body_id, &shape_def, &polygon);
                    break;
                }

                case SHAPE_TYPE_GRID:
                {
                    // Grid shapes not yet implemented
                    dmLogWarning("ApplyCreateBodyOp: Grid shapes not yet implemented");
                    break;
                }

                default:
                    dmLogWarning("ApplyCreateBodyOp: Unknown shape type %d", shape_data.type);
                    break;
            }
        }

        // Update mass (Box2D will compute from shape densities)
        if (data.mass > 0.0f && body_def.type == b2_dynamicBody)
        {
            b2MassData mass_data = b2Body_GetMassData(body_id);
            if (mass_data.mass > 0.0f)
            {
                float scale_factor = data.mass / mass_data.mass;
                mass_data.mass = data.mass;
                mass_data.rotationalInertia *= scale_factor;  // Box2D v3 uses rotationalInertia not I
                b2Body_SetMassData(body_id, mass_data);
            }
        }

        return body_id;
    }

    /**
     * Apply DESTROY_BODY operation to world.
     * Destroys body and all attached shapes.
     * Frees creation data stored in shape userData.
     */
    static void ApplyDestroyBodyOp(b2WorldId world_id, b2BodyId body_id)
    {
        if (b2Body_IsValid(body_id))
        {
            uint64_t body_id_key = ((uint64_t)(uint32_t)body_id.index1 << 32) | (uint64_t)(uint16_t)body_id.generation;
            PHYSICS_OP_LOG("PHYSICS_OP: Destroying body from physics world (id={%d,%u,%u} key=0x%llx world=%d)",
                     body_id.index1, body_id.world0, body_id.generation, body_id_key, world_id.index1);

            // Free creation data from all shapes before destroying body
            int shape_capacity = 8;
            b2ShapeId shapes[8];
            int shape_count = b2Body_GetShapes(body_id, shapes, shape_capacity);

            for (int i = 0; i < shape_count; ++i)
            {
                if (b2Shape_IsValid(shapes[i]))
                {
                    PhysicsShapeCreationData* creation_data = (PhysicsShapeCreationData*)b2Shape_GetUserData(shapes[i]);
                    if (creation_data)
                    {
                        free(creation_data);
                    }
                }
            }

            b2DestroyBody(body_id);
            PHYSICS_OP_LOG("PHYSICS_OP: Body destroyed successfully (key=0x%llx)", body_id_key);
        }
        else
        {
            dmLogWarning("ApplyDestroyBodyOp: body ID is invalid");
        }
    }

    /**
     * Apply ENABLE_BODY or DISABLE_BODY operation to world.
     * Toggles body enabled state.
     */
    static void ApplyEnableBodyOp(b2WorldId world_id, b2BodyId body_id, bool enable)
    {
        if (!b2Body_IsValid(body_id))
        {
            dmLogWarning("ApplyEnableBodyOp: body ID is invalid");
            return;
        }

        uint64_t body_id_key = ((uint64_t)(uint32_t)body_id.index1 << 32) | (uint64_t)(uint16_t)body_id.generation;
        PHYSICS_OP_LOG("PHYSICS_OP: %s body (id={%d,%u,%u} key=0x%llx world=%d)",
                 enable ? "Enabling" : "Disabling",
                 body_id.index1, body_id.world0, body_id.generation, body_id_key, world_id.index1);

        if (enable)
        {
            b2Body_Enable(body_id);
        }
        else
        {
            b2Body_Disable(body_id);
        }
    }

    /**
     * Apply SCALE_BODY operation to world.
     * Scales all shapes attached to body using stored creation data.
     * Matches the game world UpdateScale implementation.
     */
    static void ApplyScaleBodyOp(b2WorldId world_id, b2BodyId body_id, float object_scale)
    {
        if (!b2Body_IsValid(body_id))
        {
            dmLogWarning("ApplyScaleBodyOp: body ID is invalid");
            return;
        }

        // Get all shapes on this body
        int shape_capacity = 8;  // MAX_OP_SHAPES
        b2ShapeId shapes[8];
        int shape_count = b2Body_GetShapes(body_id, shapes, shape_capacity);

        // Scale each shape using creation data
        for (int i = 0; i < shape_count; ++i)
        {
            b2ShapeId shape_id = shapes[i];
            if (!b2Shape_IsValid(shape_id))
            {
                continue;
            }

            // Get creation data from userData
            PhysicsShapeCreationData* creation_data = (PhysicsShapeCreationData*)b2Shape_GetUserData(shape_id);
            if (!creation_data)
            {
                dmLogWarning("ApplyScaleBodyOp: shape has no creation data in userData");
                continue;
            }

            switch (creation_data->type)
            {
                case SHAPE_TYPE_CIRCLE:
                {
                    b2Circle circle = b2Shape_GetCircle(shape_id);

                    // Match game world UpdateScale logic for circles:
                    // creation scale for circles, is the initial radius
                    circle.radius = creation_data->creation_scale * object_scale;
                    circle.center.x = creation_data->creation_position_x * object_scale;
                    circle.center.y = creation_data->creation_position_y * object_scale;

                    b2Shape_SetCircle(shape_id, &circle);
                    break;
                }

                case SHAPE_TYPE_POLYGON:
                {
                    b2Polygon polygon = b2Shape_GetPolygon(shape_id);

                    // Match game world UpdateScale logic for polygons:
                    float s = object_scale / creation_data->creation_scale;

                    for (int j = 0; j < polygon.count; ++j)
                    {
                        b2Vec2 p = {creation_data->vertices_original_x[j], creation_data->vertices_original_y[j]};
                        polygon.vertices[j].x = p.x * s;
                        polygon.vertices[j].y = p.y * s;
                    }

                    polygon.centroid.x = creation_data->centroid_original_x * s;
                    polygon.centroid.y = creation_data->centroid_original_y * s;

                    // Recompute normals (Box2D requires this)
                    for (int j = 0; j < polygon.count; ++j)
                    {
                        int j1 = j;
                        int j2 = j + 1 < polygon.count ? j + 1 : 0;
                        b2Vec2 edge = b2Sub(polygon.vertices[j2], polygon.vertices[j1]);
                        polygon.normals[j] = b2Normalize(b2Vec2{edge.y, -edge.x});
                    }

                    b2Shape_SetPolygon(shape_id, &polygon);
                    break;
                }

                case SHAPE_TYPE_GRID:
                {
                    dmLogWarning("ApplyScaleBodyOp: Grid shapes not yet implemented");
                    break;
                }

                default:
                    dmLogWarning("ApplyScaleBodyOp: Unknown shape type %d", creation_data->type);
                    break;
            }
        }

        // Wake up the body after scaling
        b2Body_SetAwake(body_id, true);
    }

    /**
     * Apply SET_GRAVITY operation to world.
     * Changes world gravity vector.
     */
    static void ApplySetGravityOp(b2WorldId world_id, const PendingPhysicsOp::OpData::SetGravity& data, float scale)
    {
        // Gravity values are already scaled when stored in operation
        b2Vec2 b2_gravity = {data.gravity_x, data.gravity_y};
        b2World_SetGravity(world_id, b2_gravity);
    }

    // ===== PUBLIC API =====

    void ApplyOperation(b2WorldId world_id, const PendingPhysicsOp& op, float scale)
    {
        switch (op.m_Type)
        {
            case OP_CREATE_BODY:
            {
                ApplyCreateBodyOp(world_id, op.m_Data.create_body, scale);
                break;
            }

            case OP_DESTROY_BODY:
            {
                ApplyDestroyBodyOp(world_id, op.m_Data.destroy_body.body_id);
                break;
            }

            case OP_ENABLE_BODY:
            {
                ApplyEnableBodyOp(world_id, op.m_Data.enable_body.body_id, true);
                break;
            }

            case OP_DISABLE_BODY:
            {
                ApplyEnableBodyOp(world_id, op.m_Data.disable_body.body_id, false);
                break;
            }

            case OP_SCALE_BODY:
            {
                ApplyScaleBodyOp(world_id, op.m_Data.scale_body.body_id, op.m_Data.scale_body.scale);
                break;
            }

            case OP_SET_GRAVITY:
            {
                ApplySetGravityOp(world_id, op.m_Data.set_gravity, scale);
                break;
            }

            default:
                dmLogError("ApplyOperation: Unimplemented operation type %d", op.m_Type);
                assert(false && "Unimplemented operation type");
                break;
        }
    }

    void ProcessOperationQueue(b2WorldId world_id, dmArray<PendingPhysicsOp>& queue, float scale)
    {
        // Apply all operations in order
        for (uint32_t i = 0; i < queue.Size(); ++i)
        {
            ApplyOperation(world_id, queue[i], scale);
        }

        // Clear queue after processing
        queue.SetSize(0);
    }
}
