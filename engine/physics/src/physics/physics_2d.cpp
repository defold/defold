// Copyright 2020-2025 The Defold Foundation
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


#include <stdlib.h> // qsort

#include "physics.h"

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include <box2d/box2d.h>
#include <box2d/src/world.h>

#include "physics_2d.h"

namespace dmPhysics
{
    using namespace dmVMath;

    Context2D::Context2D()
    : m_Worlds()
    , m_DebugCallbacks()
    , m_Socket(0)
    , m_Scale(1.0f)
    , m_InvScale(1.0f)
    , m_ContactImpulseLimit(0.0f)
    , m_TriggerEnterLimit(0.0f)
    , m_RayCastLimit(0)
    , m_TriggerOverlapCapacity(0)
    , m_AllowDynamicTransforms(0)
    {
        m_Gravity.x = 0.0f;
        m_Gravity.y = -10.0f;
    }

    World2D::World2D(HContext2D context, const NewWorldParams& params)
    : m_TriggerOverlaps(context->m_TriggerOverlapCapacity)
    , m_Context(context)
    , m_RayCastRequests()
    , m_DebugDraw(&context->m_DebugCallbacks)
    , m_GetWorldTransformCallback(params.m_GetWorldTransformCallback)
    , m_SetWorldTransformCallback(params.m_SetWorldTransformCallback)
    , m_AllowDynamicTransforms(context->m_AllowDynamicTransforms)
    {
        m_RayCastRequests.SetCapacity(context->m_RayCastLimit);

        b2WorldDef worldDef         = b2DefaultWorldDef();
        worldDef.gravity            = context->m_Gravity;

        // These values have changed, I think. It's in meter per second now, and our default values is like 1.0f
        // worldDef.maximumLinearSpeed = context->m_VelocityThreshold * context->m_Scale;

        m_WorldId = b2CreateWorld(&worldDef);

        m_Bodies.SetCapacity(32);
    }

    struct HullSet
    {
        struct Hull
        {
            uint16_t m_Index;
            uint16_t m_Count;
        };

        HullSet(const b2Vec2* vertices, uint32_t vertex_count,
                  const Hull* hulls, uint32_t hull_count)
        {
            uint32_t vertices_size = vertex_count * sizeof(vertices[0]);
            m_Vertices = (b2Vec2*) malloc(vertices_size);
            memcpy(m_Vertices, vertices, vertices_size);
            m_VertexCount = vertex_count;

            uint32_t hulls_size = hull_count * sizeof(hulls[0]);
            m_Hulls = (Hull*) malloc(hulls_size);
            memcpy(m_Hulls, hulls, hulls_size);
            m_HullCount = hull_count;
        }

        ~HullSet()
        {
            free(m_Vertices);
            free(m_Hulls);
        }

        b2Vec2*   m_Vertices;
        Hull*     m_Hulls;
        uint32_t  m_VertexCount;
        uint32_t  m_HullCount;

    private:
        HullSet(const HullSet&);
        HullSet& operator=(const HullSet&);
    };

    struct RayCastContext
    {
        RayCastContext()
        {
            memset(this, 0, sizeof(RayCastContext));
            m_CollisionMask = (uint16_t)~0u;
            m_CollisionGroup = (uint16_t)~0u;
        }

        HContext2D                m_Context;
        RayCastResponse           m_Response;
        dmArray<RayCastResponse>* m_Results;
        uint16_t                  m_CollisionMask;
        uint16_t                  m_CollisionGroup;
        void*                     m_IgnoredUserData;
        uint8_t                   m_ReturnAllResults : 1;
    };

    static float cast_ray_cb(b2ShapeId shape_id, b2Vec2 point, b2Vec2 normal, float fraction, void* context );

    template <typename T>
    static inline uint64_t ToOpaqueHandle(T id)
    {
        uint64_t opaque_id;
        memcpy(&opaque_id, &id, sizeof(id));
        return opaque_id;
    }

    HContext2D NewContext2D(const NewContextParams& params)
    {
        if (params.m_Scale < MIN_SCALE || params.m_Scale > MAX_SCALE)
        {
            dmLogFatal("Physics scale is outside the valid range %.2f - %.2f.", MIN_SCALE, MAX_SCALE);
            return 0x0;
        }
        Context2D* context = new Context2D();
        context->m_Worlds.SetCapacity(params.m_WorldCount);
        ToB2(params.m_Gravity, context->m_Gravity, params.m_Scale);
        context->m_Scale = params.m_Scale;
        context->m_InvScale = 1.0f / params.m_Scale;
        context->m_ContactImpulseLimit = params.m_ContactImpulseLimit * params.m_Scale;
        context->m_TriggerEnterLimit = params.m_TriggerEnterLimit * params.m_Scale;
        context->m_RayCastLimit = params.m_RayCastLimit2D;
        context->m_TriggerOverlapCapacity = params.m_TriggerOverlapCapacity;
        context->m_VelocityThreshold = params.m_VelocityThreshold;
        context->m_AllowDynamicTransforms = params.m_AllowDynamicTransforms;

        dmMessage::Result result = dmMessage::NewSocket(PHYSICS_SOCKET_NAME, &context->m_Socket);
        if (result != dmMessage::RESULT_OK && result != dmMessage::RESULT_SOCKET_EXISTS)
        {
            dmLogFatal("Could not create socket '%s'.", PHYSICS_SOCKET_NAME);
            DeleteContext2D(context);
            return 0x0;
        }
        return context;
    }

    void DeleteContext2D(HContext2D context)
    {
        if (!context->m_Worlds.Empty())
        {
            dmLogWarning("Deleting %ud 2d worlds since the context is deleted.", context->m_Worlds.Size());
            for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
                delete context->m_Worlds[i];
        }
        if (context->m_Socket != 0)
            dmMessage::DeleteSocket(context->m_Socket);
        delete context;
    }

    dmMessage::HSocket GetSocket2D(HContext2D context)
    {
        return context->m_Socket;
    }

    HWorld2D NewWorld2D(HContext2D context, const NewWorldParams& params)
    {
        if (context->m_Worlds.Full())
        {
            dmLogError("%s", "Physics world buffer full, world could not be created.");
            return 0x0;
        }
        World2D* world = new World2D(context, params);

        // From box2d docs:
        /// Enable/disable continuous collision between dynamic and static bodies. Generally you should keep continuous
        /// collision enabled to prevent fast moving objects from going through static objects. The performance gain from
        /// disabling continuous collision is minor.
        b2World_EnableContinuous(world->m_WorldId, true);

        context->m_Worlds.Push(world);
        return world;
    }

    void DeleteWorld2D(HContext2D context, HWorld2D world)
    {
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
            if (context->m_Worlds[i] == world)
                context->m_Worlds.EraseSwap(i);
        delete world;
    }

    void* GetWorldContext2D(HWorld2D world)
    {
        return (void*)&world->m_WorldId;
    }

    void* GetCollisionObjectContext2D(HCollisionObject2D collision_object)
    {
        return (void*) collision_object;
    }

    static dmArray<b2ContactData>& GetContactsBuffer(HWorld2D world, b2BodyId id)
    {
        int num_contacts = b2Body_GetContactCapacity(id);
        if (world->m_GetContactsScratchBuffer.Capacity() < num_contacts)
        {
            world->m_GetContactsScratchBuffer.SetCapacity(num_contacts);
        }

        if (num_contacts > 0)
        {
            num_contacts = b2Body_GetContactData(id, world->m_GetContactsScratchBuffer.Begin(), num_contacts);
        }

        world->m_GetContactsScratchBuffer.SetSize(num_contacts);

        return world->m_GetContactsScratchBuffer;
    }

    static dmArray<b2ShapeId>& GetSensorOverlapBuffer(HWorld2D world, b2ShapeId shapeId)
    {
        int num_overlaps = b2Shape_GetSensorCapacity(shapeId);

        if (world->m_GetSensorOverlapsScratchBuffer.Capacity() < num_overlaps)
        {
            world->m_GetSensorOverlapsScratchBuffer.SetCapacity(num_overlaps);
        }

        world->m_GetSensorOverlapsScratchBuffer.SetSize(num_overlaps);

        if (num_overlaps > 0)
        {
            num_overlaps = b2Shape_GetSensorOverlaps(shapeId, world->m_GetSensorOverlapsScratchBuffer.Begin(), num_overlaps);
        }
        return world->m_GetSensorOverlapsScratchBuffer;
    }

    static inline b2Vec2 FlipPoint(b2Vec2 p, float horizontal, float vertical)
    {
        p.x *= horizontal;
        p.y *= vertical;
        return p;
    }

    static void FlipPolygon(PolygonShapeData* shape, float horizontal, float vertical)
    {
        shape->m_Polygon.centroid = FlipPoint(shape->m_Polygon.centroid, horizontal, vertical);
        int count = shape->m_Polygon.count;

        for (int i = 0; i < count; ++i)
        {
            shape->m_Polygon.vertices[i] = FlipPoint(shape->m_Polygon.vertices[i], horizontal, vertical);
            shape->m_VerticesOriginal[i] = FlipPoint(shape->m_VerticesOriginal[i], horizontal, vertical);
        }

        // Switch the winding of the polygon
        for (int i = 0; i < count/2; ++i)
        {
            b2Vec2 tmp;
            tmp = shape->m_Polygon.vertices[i];
            shape->m_Polygon.vertices[i] = shape->m_Polygon.vertices[count-i-1];
            shape->m_Polygon.vertices[count-i-1] = tmp;

            tmp = shape->m_VerticesOriginal[i];
            shape->m_VerticesOriginal[i] = shape->m_VerticesOriginal[count-i-1];
            shape->m_VerticesOriginal[count-i-1] = tmp;
        }

        // Recalculate the normals
        for (int i = 0; i < count; ++i)
        {
            b2Vec2 v = b2Sub(shape->m_Polygon.vertices[(i+1)%count], shape->m_Polygon.vertices[i]);
            b2Vec2 n = b2Normalize(v);

            shape->m_Polygon.normals[i].x = n.y;
            shape->m_Polygon.normals[i].y = -n.x;
        }
    }

    static void FlipBody(HWorld2D world, HCollisionObject2D collision_object, float horizontal, float vertical)
    {
        Body* body = (Body*) collision_object;

        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            ShapeData* shape_data = body->m_Shapes[i];
            switch(shape_data->m_Type)
            {
                case SHAPE_TYPE_CIRCLE:
                {
                    CircleShapeData* circle_shape = (CircleShapeData*) shape_data;
                    circle_shape->m_Circle.center = FlipPoint(circle_shape->m_Circle.center, horizontal, vertical);
                    circle_shape->m_ShapeDataBase.m_CreationPosition = FlipPoint(circle_shape->m_ShapeDataBase.m_CreationPosition, horizontal, vertical);
                } break;
                case SHAPE_TYPE_POLYGON:
                {
                    FlipPolygon((PolygonShapeData*) shape_data, horizontal, vertical);
                } break;
                default: break;
            }
        }

        b2Body_SetAwake(body->m_BodyId, true);
    }

    void FlipH2D(HWorld2D world, HCollisionObject2D collision_object)
    {
        FlipBody(world, collision_object, -1, 1);
    }

    void FlipV2D(HWorld2D world, HCollisionObject2D collision_object)
    {
        FlipBody(world, collision_object, 1, -1);
    }

    bool IsWorldLocked(HWorld2D world)
    {
        // non-standard box2d function
        return b2World_IsLocked(world->m_WorldId);
    }

    static inline float GetUniformScale2D(dmTransform::Transform& transform)
    {
        const float* v = transform.GetScalePtr();
        return dmMath::Min(v[0], v[1]);
    }

    static void UpdateScale(HWorld2D world, Body* body)
    {
        dmTransform::Transform world_transform;
        (*world->m_GetWorldTransformCallback)(b2Body_GetUserData(body->m_BodyId), world_transform);

        float object_scale = GetUniformScale2D(world_transform);
        bool allow_sleep = true;

        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            ShapeData* shape_data = body->m_Shapes[i];
            b2ShapeId shape_id = shape_data->m_ShapeId;

            if (shape_data->m_LastScale == object_scale)
            {
                break;
            }

            shape_data->m_LastScale = object_scale;
            allow_sleep = false;

            if (shape_data->m_Type == SHAPE_TYPE_CIRCLE)
            {
                CircleShapeData* circle_shape_data = (CircleShapeData*) shape_data;
                circle_shape_data->m_Circle = b2Shape_GetCircle(shape_id);

                // creation scale for circles, is the initial radius
                circle_shape_data->m_Circle.radius   = shape_data->m_CreationScale * object_scale;
                circle_shape_data->m_Circle.center.x = shape_data->m_CreationPosition.x * object_scale;
                circle_shape_data->m_Circle.center.y = shape_data->m_CreationPosition.y * object_scale;
                b2Shape_SetCircle(shape_id, &circle_shape_data->m_Circle);
            }
            else if (shape_data->m_Type == SHAPE_TYPE_POLYGON)
            {
                PolygonShapeData* polygon_shape_data = (PolygonShapeData*) shape_data;
                polygon_shape_data->m_Polygon = b2Shape_GetPolygon(shape_id);

                float s = object_scale / shape_data->m_CreationScale;

                for( int i = 0; i < polygon_shape_data->m_Polygon.count; ++i)
                {
                    b2Vec2 p = polygon_shape_data->m_VerticesOriginal[i];
                    polygon_shape_data->m_Polygon.vertices[i].x = p.x * s;
                    polygon_shape_data->m_Polygon.vertices[i].y = p.y * s;
                }

                b2Shape_SetPolygon(shape_id, &polygon_shape_data->m_Polygon);
            }
        }

        if (!allow_sleep)
        {
            b2Body_SetAwake(body->m_BodyId, true);
        }
    }

    void StepWorld2D(HWorld2D world, const StepWorldContext& step_context)
    {
        float dt = step_context.m_DT;
        HContext2D context = world->m_Context;
        float scale = context->m_Scale;
        // Epsilon defining what transforms are considered noise and not
        // Values are picked by inspection, current rot value is roughly equivalent to 1 degree
        const float POS_EPSILON = 0.00005f * scale;
        const float ROT_EPSILON = 0.00007f;
        // Update transforms of kinematic bodies
        if (world->m_GetWorldTransformCallback)
        {
            DM_PROFILE("UpdateKinematic");

            for (int i=0; i < world->m_Bodies.Size(); ++i)
            {
                Body* body = world->m_Bodies[i];
                b2BodyId body_id = body->m_BodyId;
                b2BodyType body_type = b2Body_GetType(body_id);

                bool retrieve_gameworld_transform = world->m_AllowDynamicTransforms && body_type != b2_staticBody;

                // translate & rotation
                if (retrieve_gameworld_transform || body_type == b2_kinematicBody)
                {
                    void* body_user_data = b2Body_GetUserData(body_id);

                    Point3 old_position = GetWorldPosition2D(context, &body_id);
                    dmTransform::Transform world_transform;
                    (*world->m_GetWorldTransformCallback)(body_user_data, world_transform);
                    Point3 position = Point3(world_transform.GetTranslation());
                    // Ignore z-component
                    position.setZ(0.0f);
                    Quat rotation = world_transform.GetRotation();
                    float dp = distSqr(old_position, position);
                    float angle = atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ()));

                    b2Rot rot = b2Body_GetRotation(body_id);
                    float old_angle = b2Rot_GetAngle(rot);
                    float da = old_angle - angle;

                    if (dp > POS_EPSILON || fabsf(da) > ROT_EPSILON)
                    {
                        b2Vec2 b2_position;
                        ToB2(position, b2_position, scale);

                        b2Rot b2_rot = b2MakeRot(angle);
                        b2Body_SetTransform(body_id, b2_position, b2_rot);
                        b2Body_EnableSleep(body_id, false);
                    }
                    else
                    {
                        b2Body_EnableSleep(body_id, true);
                    }
                }

                // Scaling
                if(retrieve_gameworld_transform)
                {
                    UpdateScale(world, body);
                }
            }
        }
        // Step simulation
        {
            DM_PROFILE("StepSimulation");

            b2World_Step(world->m_WorldId, dt, 10);

            float inv_scale = world->m_Context->m_InvScale;
            // Update transforms of dynamic bodies
            if (world->m_SetWorldTransformCallback)
            {
                for (int i=0; i < world->m_Bodies.Size(); ++i)
                {
                    Body* body = world->m_Bodies[i];
                    b2BodyId body_id = body->m_BodyId;
                    b2BodyType body_type = b2Body_GetType(body_id);

                    if (body_type == b2_dynamicBody && b2Body_IsEnabled(body_id))
                    {
                        Point3 position;
                        FromB2(b2Body_GetPosition(body_id), position, inv_scale);
                        b2Rot rot = b2Body_GetRotation(body_id);
                        Quat rotation = Quat::rotationZ(b2Rot_GetAngle(rot));
                        (*world->m_SetWorldTransformCallback)(b2Body_GetUserData(body_id), position, rotation);
                    }
                }
            }
        }
        // Perform requested ray casts
        uint32_t size = world->m_RayCastRequests.Size();
        if (size > 0)
        {
            DM_PROFILE("RayCasts");

            RayCastContext ray_cast_context;
            ray_cast_context.m_Context = world->m_Context;

            b2QueryFilter filter = b2DefaultQueryFilter();

            for (uint32_t i = 0; i < size; ++i)
            {
                RayCastRequest& request = world->m_RayCastRequests[i];
                b2Vec2 from;
                ToB2(request.m_From, from, scale);
                b2Vec2 to;
                ToB2(request.m_To, to, scale);

                ray_cast_context.m_IgnoredUserData = request.m_IgnoredUserData;
                ray_cast_context.m_CollisionMask = request.m_Mask;
                ray_cast_context.m_Response.m_Hit = 0;

                filter.maskBits = request.m_Mask;
                b2World_CastRay(world->m_WorldId, from, to, filter, cast_ray_cb, &ray_cast_context);
                (*step_context.m_RayCastCallback)(ray_cast_context.m_Response, request, step_context.m_RayCastUserData);
            }

            world->m_RayCastRequests.SetSize(0);
        }

        b2SensorEvents sensor_events = b2World_GetSensorEvents(world->m_WorldId);

        if (step_context.m_CollisionCallback)
        {
            DM_PROFILE("CollisionCallbacks");
            for (int i=0; i < world->m_Bodies.Size(); ++i)
            {
                Body* body = world->m_Bodies[i];

                for (int j=0; j < body->m_ShapeCount; ++j)
                {
                    b2ShapeId shapeIdA = body->m_Shapes[j]->m_ShapeId;
                    if (!b2Shape_IsValid(shapeIdA))
                    {
                        continue;
                    }

                    if (b2Shape_IsSensor(shapeIdA))
                    {
                        dmArray<b2ShapeId>& overlaps = GetSensorOverlapBuffer(world, shapeIdA);

                        // Trigger collision callbacks for overlapping sensors
                        for (int k=0; k < overlaps.Size(); ++k)
                        {
                            b2ShapeId shapeIdB = overlaps[k];
                            if (!b2Shape_IsValid(shapeIdB))
                            {
                                continue;
                            }

                            step_context.m_CollisionCallback(
                                b2Shape_GetUserData(shapeIdA), b2Shape_GetFilter(shapeIdA).categoryBits,
                                b2Shape_GetUserData(shapeIdB), b2Shape_GetFilter(shapeIdB).categoryBits,
                                step_context.m_CollisionUserData);
                        }
                    }
                }
            }
        }

        // Report sensor enter/exit callbacks
        {
            DM_PROFILE("TriggerCallbacks");

            for (int i = 0; i < sensor_events.beginCount; ++i)
            {
                b2ShapeId shapeIdA = sensor_events.beginEvents[i].sensorShapeId;
                b2ShapeId shapeIdB = sensor_events.beginEvents[i].visitorShapeId;

                if (!(b2Shape_IsValid(shapeIdA) || b2Shape_IsValid(shapeIdB)))
                {
                    continue;
                }

                TriggerEnter data;
                data.m_UserDataA = b2Shape_GetUserData(shapeIdA);
                data.m_UserDataB = b2Shape_GetUserData(shapeIdB);
                data.m_GroupA    = b2Shape_GetFilter(shapeIdA).categoryBits;
                data.m_GroupB    = b2Shape_GetFilter(shapeIdB).categoryBits;

                step_context.m_TriggerEnteredCallback(data, step_context.m_TriggerExitedUserData);
            }

            for (int i = 0; i < sensor_events.endCount; ++i)
            {
                b2ShapeId shapeIdA = sensor_events.endEvents[i].sensorShapeId;
                b2ShapeId shapeIdB = sensor_events.endEvents[i].visitorShapeId;

                if (!(b2Shape_IsValid(shapeIdA) || b2Shape_IsValid(shapeIdB)))
                {
                    continue;
                }

                TriggerExit data;
                data.m_UserDataA = b2Shape_GetUserData(shapeIdA);
                data.m_UserDataB = b2Shape_GetUserData(shapeIdB);
                data.m_GroupA    = b2Shape_GetFilter(shapeIdA).categoryBits;
                data.m_GroupB    = b2Shape_GetFilter(shapeIdB).categoryBits;

                step_context.m_TriggerExitedCallback(data, step_context.m_TriggerExitedUserData);
            }
        }

    #if 0
        b2ContactEvents contact_events = b2World_GetContactEvents(world->m_WorldId);

        if (contact_events.beginCount > 0)
        {
            dmLogInfo("Contact begin events: %d", contact_events.beginCount);
        }

        if (contact_events.endCount > 0)
        {
            dmLogInfo("Contact end events: %d", contact_events.endCount);
        }

        if (contact_events.hitCount > 0)
        {
            dmLogInfo("Contact hit events: %d", contact_events.hitCount);
        }
    #endif

        // Post-solve callbacks
        if (step_context.m_CollisionCallback || step_context.m_ContactPointCallback)
        {
            DM_PROFILE("PostSolveCallbacks");

            float inv_scale = world->m_Context->m_InvScale;

            for (int i = 0; i < world->m_Bodies.Size(); ++i)
            {
                Body* body = world->m_Bodies[i];
                b2BodyId body_id = body->m_BodyId;
                dmArray<b2ContactData>& contacts = GetContactsBuffer(world, body_id);
                int num_contacts = contacts.Size();

                for (int j = 0; j < num_contacts; ++j)
                {
                    b2ContactData& contact = contacts[j];

                    float max_impulse = 0.0f;

                    for (int k = 0; k < contact.manifold.pointCount; ++k)
                    {
                        b2ManifoldPoint& manifold = contact.manifold.points[k];
                        max_impulse = dmMath::Max(max_impulse, manifold.maxNormalImpulse);
                    }
                    if (max_impulse < world->m_Context->m_ContactImpulseLimit)
                    {
                        continue;
                    }

                    if (step_context.m_CollisionCallback)
                    {
                        step_context.m_CollisionCallback(
                            b2Shape_GetUserData(contact.shapeIdA),
                            b2Shape_GetFilter(contact.shapeIdA).categoryBits,
                            b2Shape_GetUserData(contact.shapeIdB),
                            b2Shape_GetFilter(contact.shapeIdB).categoryBits,
                            step_context.m_CollisionUserData);
                    }

                    if (step_context.m_ContactPointCallback)
                    {
                        b2BodyId bodyIdA = b2Shape_GetBody(contact.shapeIdA);
                        b2BodyId bodyIdB = b2Shape_GetBody(contact.shapeIdB);
                        b2Vec2 vel_a = b2Body_GetLinearVelocity(bodyIdA);
                        b2Vec2 vel_b = b2Body_GetLinearVelocity(bodyIdB);
                        b2Vec2 rv    = b2Sub(vel_b, vel_a);

                        for (int k = 0; k < contact.manifold.pointCount; ++k)
                        {
                            b2ManifoldPoint& manifold_point = contact.manifold.points[k];

                            ContactPoint cp;
                            FromB2(manifold_point.point, cp.m_PositionA, inv_scale);
                            FromB2(manifold_point.point, cp.m_PositionB, inv_scale);
                            cp.m_UserDataA = b2Body_GetUserData(bodyIdA);
                            cp.m_UserDataB = b2Body_GetUserData(bodyIdB);

                            FromB2(contact.manifold.normal, cp.m_Normal, 1.0f); // Don't scale normal
                            FromB2(rv, cp.m_RelativeVelocity, inv_scale);

                            cp.m_Distance = -manifold_point.separation * inv_scale;
                            cp.m_AppliedImpulse = manifold_point.normalImpulse * inv_scale;
                            cp.m_MassA = b2Body_GetMass(bodyIdA);
                            cp.m_MassB = b2Body_GetMass(bodyIdB);
                            cp.m_GroupA = b2Shape_GetFilter(contact.shapeIdA).categoryBits;
                            cp.m_GroupB = b2Shape_GetFilter(contact.shapeIdB).categoryBits;
                            step_context.m_ContactPointCallback(cp, step_context.m_ContactPointUserData);
                        }
                    }
                }
            }
        }

        b2World_Draw(world->m_WorldId, &world->m_DebugDraw.m_DebugDraw);
    }

    void SetDrawDebug2D(HWorld2D world, bool draw_debug)
    {
        world->m_DebugDraw.m_DebugDraw.drawJoints          |= draw_debug;
        world->m_DebugDraw.m_DebugDraw.drawShapes          |= draw_debug;
        world->m_DebugDraw.m_DebugDraw.drawContactNormals  |= draw_debug;
        world->m_DebugDraw.m_DebugDraw.drawContactImpulses |= draw_debug;
    }

    HCollisionShape2D NewCircleShape2D(HContext2D context, float radius)
    {
        float scale = context->m_Scale;
        CircleShapeData* shape_data = new CircleShapeData();
        shape_data->m_ShapeDataBase.m_Type = SHAPE_TYPE_CIRCLE;
        shape_data->m_ShapeDataBase.m_CreationScale = radius * scale;
        shape_data->m_Circle.center = {0.0f, 0.0f};
        shape_data->m_Circle.radius = radius * scale;
        return shape_data;
    }

    HCollisionShape2D NewBoxShape2D(HContext2D context, const Vector3& half_extents)
    {
        float scale = context->m_Scale;
        PolygonShapeData* shape_data = new PolygonShapeData();
        shape_data->m_ShapeDataBase.m_Type = SHAPE_TYPE_POLYGON;
        shape_data->m_ShapeDataBase.m_CreationScale = scale;
        shape_data->m_Polygon = b2MakeBox(half_extents.getX() * scale, half_extents.getY() * scale);

        memcpy(shape_data->m_VerticesOriginal, shape_data->m_Polygon.vertices, sizeof(shape_data->m_Polygon.vertices));

        return shape_data;
    }

    static b2Vec2 ComputeCentroid(const b2Vec2* vs, int count)
    {
        assert(count >= 3);

        b2Vec2 c = {};
        float area = 0.0f;

        // pRef is the reference point for forming triangles.
        // It's location doesn't change the result (except for rounding error).
        b2Vec2 pRef = {};

        const float inv3 = 1.0f / 3.0f;

        for (int i = 0; i < count; ++i)
        {
            // Triangle vertices.
            b2Vec2 p1 = pRef;
            b2Vec2 p2 = vs[i];
            b2Vec2 p3 = i + 1 < count ? vs[i+1] : vs[0];

            b2Vec2 e1 = p2 - p1;
            b2Vec2 e2 = p3 - p1;

            float D = b2Cross(e1, e2);

            float triangleArea = 0.5f * D;
            area += triangleArea;

            // Area weighted centroid
            c += triangleArea * inv3 * (p1 + p2 + p3);
        }

        // Centroid
        assert(area > FLT_EPSILON);
        c *= 1.0f / area;
        return c;
    }

    static void MakePolygonFromVertices(PolygonShapeData* polygon, const b2Vec2* vertices, uint32_t count)
    {
        polygon->m_Polygon.count = count;

        // Copy vertices.
        for (int i = 0; i < count; ++i)
        {
            polygon->m_VerticesOriginal[i] = vertices[i];
            polygon->m_Polygon.vertices[i] = vertices[i];
        }

        // Compute normals. Ensure the edges have non-zero length.
        for (int i = 0; i < count; ++i)
        {
            int i1 = i;
            int i2 = i + 1 < count ? i + 1 : 0;
            b2Vec2 edge = vertices[i2] - vertices[i1];

            assert(b2LengthSquared(edge) > FLT_EPSILON * FLT_EPSILON);
            polygon->m_Polygon.normals[i] = b2CrossVS(edge, 1.0f);
            polygon->m_Polygon.normals[i] = b2Normalize(polygon->m_Polygon.normals[i]);
        }

        // Compute the polygon centroid.
        polygon->m_Polygon.centroid = ComputeCentroid(polygon->m_Polygon.vertices, count);
    }

    HCollisionShape2D NewPolygonShape2D(HContext2D context, const float* vertices, uint32_t vertex_count)
    {
        float scale = context->m_Scale;

        PolygonShapeData* shape_data = new PolygonShapeData();

        b2Vec2* v = new b2Vec2[vertex_count];
        for (uint32_t i = 0; i < vertex_count; ++i)
        {
            v[i].x = vertices[i * 2] * scale;
            v[i].y = vertices[i * 2 + 1] * scale;
        }

        MakePolygonFromVertices(shape_data, v, vertex_count);

        delete [] v;
        return shape_data;
    }

    HHullSet2D NewHullSet2D(HContext2D context, const float* vertices, uint32_t vertex_count,
                            const HullDesc* hulls, uint32_t hull_count)
    {
        assert(sizeof(HullDesc) == sizeof(const HullSet::Hull));
        // NOTE: We cast HullDesc* to const HullSet::Hull* here
        // We assume that they have the same physical layout
        // NOTE: No scaling of the vertices here since they are already assumed to be in a virtual "unit-space"
        HullSet* hull_set = new HullSet((const b2Vec2*) vertices, vertex_count, (const HullSet::Hull*) hulls, hull_count);
        return hull_set;
    }

    void DeleteHullSet2D(HHullSet2D hull_set)
    {
        delete (HullSet*) hull_set;
    }

    // These values are from the original Box2D implementation, I think the newer version has different constants.
    // i.e B2_LINEAR_SLOP

    /// A small length used as a collision and constraint tolerance. Usually it is
    /// chosen to be numerically significant, but visually insignificant.
    #define b2_linearSlop           0.005f
    /// The radius of the polygon/edge shape skin. This should not be modified. Making
    /// this smaller means polygons will have an insufficient buffer for continuous collision.
    /// Making it larger may create artifacts for vertex collision.
    #define b2_polygonRadius        (2.0f * b2_linearSlop)

    static inline void InitializeGridSHapeData(GridShapeData* shape_data)
    {
        uint32_t cell_count = shape_data->m_RowCount * shape_data->m_ColumnCount;
        uint32_t size       = sizeof(GridShapeData::Cell) * cell_count;
        shape_data->m_Cells = (GridShapeData::Cell*) malloc(size);
        // This will set all Cell#m_Index to 0xffffffff to indicate free cells
        memset(shape_data->m_Cells, 0xff, size);

        size = sizeof(HullFlags) * cell_count;
        shape_data->m_CellFlags = (HullFlags*) malloc(size);
        memset(shape_data->m_CellFlags, 0x0, size);

        size = sizeof(b2ShapeId) * cell_count;
        shape_data->m_CellPolygonShapes = (b2ShapeId*) malloc(size);
        memset(shape_data->m_CellPolygonShapes, 0x0, size);
    }

    HCollisionShape2D NewGridShape2D(HContext2D context, HHullSet2D hull_set,
                                     const Point3& position,
                                     uint32_t cell_width, uint32_t cell_height,
                                     uint32_t row_count, uint32_t column_count)
    {
        float scale = context->m_Scale;
        b2Vec2 p;
        ToB2(position, p, scale);

        GridShapeData* shape_data = new GridShapeData();
        shape_data->m_ShapeDataBase.m_Type = SHAPE_TYPE_GRID;

        shape_data->m_Position    = p;
        shape_data->m_HullSet     = (HullSet*) hull_set;
        shape_data->m_CellWidth   = cell_width * scale;
        shape_data->m_CellHeight  = cell_height * scale;
        shape_data->m_RowCount    = row_count;
        shape_data->m_ColumnCount = column_count;
        shape_data->m_Radius      = b2_polygonRadius;

        InitializeGridSHapeData(shape_data);

        return shape_data;
    }

    void ClearGridShapeHulls(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;

        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            if (body->m_Shapes[i]->m_Type == SHAPE_TYPE_GRID)
            {
                GridShapeData* grid_shape = (GridShapeData*) body->m_Shapes[i];

                uint32_t cellCount = grid_shape->m_RowCount * grid_shape->m_ColumnCount;
                memset(grid_shape->m_Cells, B2GRIDSHAPE_EMPTY_CELL, sizeof(GridShapeData::Cell) * cellCount);
                memset(grid_shape->m_CellFlags, 0x0, sizeof(HullFlags) * cellCount);
            }
        }
    }

    static inline GridShapeData* GetGridShapeData(Body* body, uint32_t index)
    {
        if (index >= body->m_ShapeCount)
        {
            return 0;
        }

        ShapeData* shape_data = body->m_Shapes[index];
        if (shape_data == 0 || shape_data->m_Type != SHAPE_TYPE_GRID)
        {
            return 0;
        }
        return (GridShapeData*) shape_data;
    }

    static int GetGridCellVertices(GridShapeData* shape_data, int index, b2Vec2* vertices)
    {
        // Original code checked if the shape was enabled, but I'm not sure we can do that now.
        //
        // if (!b2Body_IsEnabled(shape_data->m_CellBodyId))
        // {
        //     return 0;
        // }

        const GridShapeData::Cell& cell = shape_data->m_Cells[index];
        if (cell.m_Index == B2GRIDSHAPE_EMPTY_CELL)
        {
            return 0;
        }

        const HullSet::Hull& hull = shape_data->m_HullSet->m_Hulls[cell.m_Index];
        assert(hull.m_Count <= B2_MAX_POLYGON_VERTICES);

        int row = index / shape_data->m_ColumnCount;
        int col = index - (shape_data->m_ColumnCount * row);

        float halfWidth = shape_data->m_CellWidth * shape_data->m_ColumnCount * 0.5f;
        float halfHeight = shape_data->m_CellHeight * shape_data->m_RowCount * 0.5f;

        b2Vec2 t = {shape_data->m_CellWidth * col - halfWidth, shape_data->m_CellHeight * row - halfHeight};
        t.x += shape_data->m_CellWidth * 0.5f;
        t.y += shape_data->m_CellHeight * 0.5f;
        t += shape_data->m_Position;

        const HullFlags& flags = shape_data->m_CellFlags[index];
        float xScale = flags.m_FlipHorizontal ? -1.0f : 1.0f;
        float yScale = flags.m_FlipVertical ? -1.0f : 1.0f;
        float tmpX = 0.0;

        for (uint32_t i = 0; i < hull.m_Count; ++i)
        {
            vertices[i] = shape_data->m_HullSet->m_Vertices[hull.m_Index + i];
            if (flags.m_Rotate90)
            {
                // Clockwise rotation (x, y) -> (y, -x)
                // Also tile isn't necessary a square, that's why we should swap width and height
                // We apply flip before rotation, which means scale is part of size and should be swapped as well
                tmpX = vertices[i].x;
                vertices[i].x = vertices[i].y * (yScale * shape_data->m_CellWidth);
                vertices[i].y = -1 * tmpX * (xScale * shape_data->m_CellHeight);
            }
            else
            {
                vertices[i].x *= xScale * shape_data->m_CellWidth;
                vertices[i].y *= yScale * shape_data->m_CellHeight;
            }
            vertices[i] += t;
        }

        // Reverse order when single flipped
        if (flags.m_FlipHorizontal != flags.m_FlipVertical)
        {
            uint16_t halfCount = hull.m_Count / 2;
            for (uint32_t i = 0; i < halfCount; ++i)
            {
                b2Vec2& v1 = vertices[i];
                b2Vec2& v2 = vertices[hull.m_Count - 1 - i];
                b2Vec2 tmp = v1;
                v1 = v2;
                v2 = tmp;
            }
        }

        return hull.m_Count;
    }

    void CreateGridCellShape(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t child)
    {
        Body* body = (Body*) collision_object;
        GridShapeData* shape_data = GetGridShapeData(body, shape_index);

        if (ToOpaqueHandle(shape_data->m_CellPolygonShapes[child]) != 0)
        {
            return;
        }

        HullSet::Hull& hull = shape_data->m_HullSet->m_Hulls[shape_data->m_Cells[child].m_Index];
        assert(hull.m_Count <= B2_MAX_POLYGON_VERTICES);

        b2Vec2 vertices[B2_MAX_POLYGON_VERTICES];
        uint32_t count = GetGridCellVertices(shape_data, child, vertices);

        if (count > 0)
        {
            b2Hull polygon_hull = b2ComputeHull(vertices, count);
            b2Polygon polygon = b2MakePolygon(&polygon_hull, shape_data->m_Radius);
            shape_data->m_CellPolygonShapes[child] = b2CreatePolygonShape(shape_data->m_CellBodyId, &shape_data->m_ShapeDef, &polygon);
        }
    }

    bool SetGridShapeHull(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t row, uint32_t column, uint32_t hull, HullFlags flags)
    {
        Body* body = (Body*) collision_object;
        GridShapeData* shape_data = GetGridShapeData(body, shape_index);

        if (shape_data == 0)
        {
            return false;
        }

        HullFlags f = {};
        f.m_FlipHorizontal = flags.m_FlipHorizontal;
        f.m_FlipVertical   = flags.m_FlipVertical;
        f.m_Rotate90       = flags.m_Rotate90;

        uint32_t index = row * shape_data->m_ColumnCount + column;
        assert(index < shape_data->m_RowCount * shape_data->m_ColumnCount);

        GridShapeData::Cell* cell = &shape_data->m_Cells[index];
        cell->m_Index = hull;
        shape_data->m_CellFlags[index] = flags;

        uint64_t opaque_id = ToOpaqueHandle(shape_data->m_CellPolygonShapes[index]);

        // treat cells with an empty hull as an empty cell
        if (hull != B2GRIDSHAPE_EMPTY_CELL)
        {
            HullSet::Hull& h = shape_data->m_HullSet->m_Hulls[hull];
            if (h.m_Count == 0)
            {
                cell->m_Index = B2GRIDSHAPE_EMPTY_CELL;
            }
            if (opaque_id == 0)
            {
                CreateGridCellShape(collision_object, shape_index, index);
            }
        }
        else
        {
            // Clear the polygon
            if (opaque_id != 0)
            {
                b2DestroyShape(shape_data->m_CellPolygonShapes[index], false);
                memset(&shape_data->m_CellPolygonShapes[index], 0, sizeof(shape_data->m_CellPolygonShapes[index]));
            }
        }

        return true;
    }

    bool SetGridShapeEnable(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t enable)
    {
        Body* body = (Body*) collision_object;
        GridShapeData* shape_data = GetGridShapeData(body, shape_index);
        if (shape_data == 0)
        {
            return false;
        }

        // Enabling / disabling grid shapes will set the flag on the cell body and not the "main" body
        if (enable && !b2Body_IsEnabled(shape_data->m_CellBodyId))
        {
            b2Body_Enable(shape_data->m_CellBodyId);
        }
        else if (!enable && b2Body_IsEnabled(shape_data->m_CellBodyId))
        {
            b2Body_Disable(shape_data->m_CellBodyId);
        }
        return false;
    }

    void SetCollisionObjectFilter(HWorld2D world, HCollisionObject2D collision_object,
                                  uint32_t shape, uint32_t child,
                                  uint16_t group, uint16_t mask)
    {
        Body* body = (Body*) collision_object;
        if (shape >= body->m_ShapeCount)
        {
            return;
        }

        ShapeData* shape_data = body->m_Shapes[shape];

        if (shape_data->m_Type == SHAPE_TYPE_GRID)
        {
            GridShapeData* grid_shape = (GridShapeData*) shape_data;

            uint64_t opaque_id = ToOpaqueHandle(grid_shape->m_CellPolygonShapes[child]);

            // 0 is not a valid group, so if we have created a polygon shape already, we need to remove it.
            if (group == 0)
            {
                if (grid_shape->m_Cells[child].m_Index != B2GRIDSHAPE_EMPTY_CELL && opaque_id != 0)
                {
                    b2DestroyShape(grid_shape->m_CellPolygonShapes[child], false);
                    memset(&grid_shape->m_CellPolygonShapes[child], 0, sizeof(grid_shape->m_CellPolygonShapes[child]));
                }
            }
            else
            {
                if (opaque_id == 0)
                {
                    CreateGridCellShape(collision_object, shape, child);
                }
                b2Filter filter = b2Shape_GetFilter(grid_shape->m_CellPolygonShapes[child]);
                filter.categoryBits = group;
                filter.maskBits = mask;
                b2Shape_SetFilter(grid_shape->m_CellPolygonShapes[child], filter);
            }
        }
        else
        {
            b2Filter filter = b2Shape_GetFilter(shape_data->m_ShapeId);
            filter.categoryBits = group;
            filter.maskBits = mask;
            b2Shape_SetFilter(shape_data->m_ShapeId, filter);
        }
    }

    void DeleteCollisionShape2D(HCollisionShape2D _shape)
    {
        ShapeData* shape = (ShapeData*) _shape;

        switch (shape->m_Type)
        {
            case SHAPE_TYPE_CIRCLE:
                delete (CircleShapeData*) shape;
                break;
            case SHAPE_TYPE_POLYGON:
                delete (PolygonShapeData*) shape;
                break;
            case SHAPE_TYPE_GRID:
                delete (GridShapeData*) shape;
                break;
            default:
                assert(false);
        }
    }

    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D* shapes, uint32_t shape_count)
    {
        return NewCollisionObject2D(world, data, shapes, 0, 0, shape_count);
    }

    /*
     * Quaternion to complex number transform
     *
     * A quaternion around axis (0,0,1) with angle alpha is given by:
     *
     * q = (0, 0, sin(alpha/2), cos(alpha/2))   (1)
     *
     * The corresponding complex number is given by
     *
     * c = (cos(alpha/2), sin(alpha/2))         (2)
     *
     * The "double angle rule":
     *
     *  sin(2x) = 2sin(x)cos(x)
     * <=>
     *  sin(x) = 2sin(x/2)cos(x/2)              (3)
     *
     * and
     *
     *  cos(2x) = 1 - 2sin^2(x)
     * <=>
     *  cos(x) = 1 - 2sin^2(x/2)                (4)
     *
     *  The complex representation from in terms of a quaternion.
     *  Identify terms using (3) and (4)
     *
     *  c = (1 - 2 * q.z * q.z, 2 * q.z * q.w)
     *
     */

    static ShapeData* TransformCopyShape(HContext2D context,
                                       const ShapeData* shape,
                                       const Vector3& translation,
                                       const Quat& rotation,
                                       float scale)
    {
        b2Vec2 t;
        ToB2(translation, t, context->m_Scale * scale);
        b2Rot r;
        r.c = 1 - 2 * rotation.getZ() * rotation.getZ();
        r.s = 2 * rotation.getZ() * rotation.getW();

        b2Transform transform; // (t, r);
        transform.q = r;
        transform.p = t;

        ShapeData* ret = (ShapeData*) shape;

        switch(shape->m_Type)
        {
            case SHAPE_TYPE_CIRCLE:
            {
                CircleShapeData* circle_shape      = (CircleShapeData*) shape;
                CircleShapeData* circle_shape_prim = new CircleShapeData;

                circle_shape_prim->m_ShapeDataBase.m_Type = shape->m_Type;
                circle_shape_prim->m_Circle.center        = TransformScaleB2(transform, scale, circle_shape->m_Circle.center);
                circle_shape_prim->m_Circle.radius        = circle_shape->m_Circle.radius;

                if (context->m_AllowDynamicTransforms)
                {
                    circle_shape_prim->m_ShapeDataBase.m_CreationScale    = circle_shape_prim->m_Circle.radius;
                    circle_shape_prim->m_ShapeDataBase.m_CreationPosition = { transform.p.x / scale, transform.p.y / scale };
                }
                circle_shape_prim->m_Circle.radius *= scale;
                ret = (ShapeData*) circle_shape_prim;
            } break;
            case SHAPE_TYPE_POLYGON:
            {
                PolygonShapeData* poly_shape = (PolygonShapeData*) shape;
                PolygonShapeData* poly_shape_prim = new PolygonShapeData;
                memcpy(poly_shape_prim, poly_shape, sizeof(PolygonShapeData));

                b2Vec2 tmp[B2_MAX_POLYGON_VERTICES] = {};
                int32_t n = poly_shape->m_Polygon.count;
                for (int32_t i = 0; i < n; ++i)
                {
                    tmp[i] = TransformScaleB2(transform, scale, poly_shape->m_Polygon.vertices[i]);
                }

                MakePolygonFromVertices(poly_shape_prim, tmp, n);
                ret = (ShapeData*) poly_shape_prim;
            } break;
            case SHAPE_TYPE_GRID:
            {
                GridShapeData* grid_shape = (GridShapeData*) shape;
                GridShapeData* grid_shape_prim = new GridShapeData;
                memcpy(grid_shape_prim, grid_shape, sizeof(GridShapeData));

                InitializeGridSHapeData(grid_shape_prim);

                ret = (ShapeData*) grid_shape_prim;
            } break;
            default: break;
        }

        if (shape->m_Type != SHAPE_TYPE_CIRCLE)
        {
            ret->m_CreationScale = scale;
        }

        return ret;
    }

    static void FreeShape(ShapeData* shape)
    {
        switch(shape->m_Type)
        {
            case SHAPE_TYPE_CIRCLE:
            {
                b2DestroyShape(shape->m_ShapeId, false);
                CircleShapeData* circle_shape = (CircleShapeData*) shape;
                delete circle_shape;
            }
            break;
            case SHAPE_TYPE_POLYGON:
            {
                b2DestroyShape(shape->m_ShapeId, false);
                PolygonShapeData* poly_shape = (PolygonShapeData*) shape;
                delete poly_shape;
            }
            break;
            case SHAPE_TYPE_GRID:
            {
                GridShapeData* grid_shape = (GridShapeData*) shape;

                uint32_t cell_count = grid_shape->m_RowCount * grid_shape->m_ColumnCount;

                for (int i = 0; i < cell_count; ++i)
                {
                    if (grid_shape->m_Cells[i].m_Index != B2GRIDSHAPE_EMPTY_CELL)
                    {
                        b2DestroyShape(grid_shape->m_CellPolygonShapes[i], false);
                    }
                }

                b2DestroyBody(grid_shape->m_CellBodyId);

                delete grid_shape;
            } break;
            default: break;
        }
    }

    /*
     * NOTE: In order to support shape transform we create a copy of shapes using the function TransformCopyShape() above
     * This is required as the transform is part of the shape and due to absence of a compound shape, aka list shape
     * with per-child transform.
     */
    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D* shapes,
                                            Vector3* translations, Quat* rotations,
                                            uint32_t shape_count)
    {
        if (shape_count == 0)
        {
            dmLogError("Collision objects must have a shape.");
            return 0x0;
        }
        switch (data.m_Type)
        {
        case COLLISION_OBJECT_TYPE_DYNAMIC:
            if (data.m_Mass == 0.0f)
            {
                dmLogError("Collision objects can not be dynamic and have zero mass.");
                return 0x0;
            }
            break;
        default:
            if (data.m_Mass > 0.0f)
            {
                dmLogError("Only dynamic collision objects can have a positive mass.");
                return 0x0;
            }
            break;
        }

        HContext2D context = world->m_Context;
        b2BodyDef def = b2DefaultBodyDef();

        float scale = 1.0f;
        if (world->m_GetWorldTransformCallback != 0x0)
        {
            if (data.m_UserData != 0x0)
            {
                dmTransform::Transform world_transform;
                (*world->m_GetWorldTransformCallback)(data.m_UserData, world_transform);
                Point3 position = Point3(world_transform.GetTranslation());
                Quat rotation = Quat(world_transform.GetRotation());
                ToB2(position, def.position, context->m_Scale);

                def.rotation = b2MakeRot(atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ())));

                scale = GetUniformScale2D(world_transform);
            }
            else
            {
                dmLogWarning("Collision object created at origin, this will result in a performance hit if multiple objects are created there in the same frame.");
            }
        }
        switch (data.m_Type)
        {
            case dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC:
                def.type = b2_dynamicBody;
                break;
            case dmPhysics::COLLISION_OBJECT_TYPE_STATIC:
                def.type = b2_staticBody;
                break;
            default:
                def.type = b2_kinematicBody;
                break;
        }
        def.userData        = data.m_UserData;
        def.linearDamping   = data.m_LinearDamping;
        def.angularDamping  = data.m_AngularDamping;
        def.fixedRotation   = data.m_LockedRotation;
        def.isBullet        = data.m_Bullet;
        def.isEnabled       = data.m_Enabled;
        b2BodyId bodyId     = b2CreateBody(world->m_WorldId, &def);

        Body* body         = new Body();
        body->m_BodyId     = bodyId;
        body->m_Shapes     = (ShapeData**) malloc(shape_count * sizeof(ShapeData*));
        body->m_ShapeCount = shape_count;

        Vector3 zero_vec3 = Vector3(0);
        for (uint32_t i = 0; i < shape_count; ++i)
        {
            ShapeData* s = (ShapeData*) shapes[i];

            if (translations && rotations)
            {
                s = TransformCopyShape(context, s, translations[i], rotations[i], scale);
            }
            else
            {
                s = TransformCopyShape(context, s, zero_vec3, Quat::identity(), scale);
            }

            b2ShapeDef f_def          = b2DefaultShapeDef();
            f_def.userData            = data.m_UserData;
            f_def.filter.categoryBits = data.m_Group;
            f_def.filter.maskBits     = data.m_Mask;
            f_def.density             = 1.0f;
            f_def.friction            = data.m_Friction;
            f_def.restitution         = data.m_Restitution;
            f_def.isSensor            = data.m_Type == COLLISION_OBJECT_TYPE_TRIGGER;
            f_def.enableContactEvents = true;
            f_def.enableHitEvents     = true;

            switch (s->m_Type)
            {
                case SHAPE_TYPE_CIRCLE:
                {
                    CircleShapeData* c = (CircleShapeData*) s;
                    s->m_ShapeId = b2CreateCircleShape(bodyId, &f_def, &c->m_Circle);
                } break;
                case SHAPE_TYPE_POLYGON:
                {
                    PolygonShapeData* p = (PolygonShapeData*) s;
                    s->m_ShapeId = b2CreatePolygonShape(bodyId, &f_def, &p->m_Polygon);
                } break;
                case SHAPE_TYPE_GRID:
                {
                    GridShapeData* g = (GridShapeData*) s;
                    // Grid polygons are created later, so we need to store the definition for later.
                    memcpy(&g->m_ShapeDef, &f_def, sizeof(b2ShapeDef));

                    g->m_CellBodyId = b2CreateBody(world->m_WorldId, &def);
                } break;
                default:assert(0);
            }

            body->m_Shapes[i] = s;
        }

        if (world->m_Bodies.Full())
        {
            world->m_Bodies.OffsetCapacity(32);
        }

        world->m_Bodies.Push(body);

        UpdateMass2D(world, body, data.m_Mass);
        return body;
    }

    void DeleteCollisionObject2D(HWorld2D world, HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;

        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            FreeShape(body->m_Shapes[i]);
        }

        b2DestroyBody(body->m_BodyId);
        uint32_t num_bodies = world->m_Bodies.Size();
        for (int i = 0; i < num_bodies; ++i)
        {
            Body* other = world->m_Bodies[i];
            if (body == other)
            {
                world->m_Bodies.EraseSwap(i);
                break;
            }
        }
    }

    uint32_t GetCollisionShapes2D(HWorld2D world, HCollisionObject2D collision_object, HCollisionShape2D* out_buffer, uint32_t buffer_size)
    {
        Body* body = (Body*) collision_object;

        uint32_t size = dmMath::Min(buffer_size, (uint32_t) body->m_ShapeCount);
        for (uint32_t i = 0; i < size; ++i)
        {
            out_buffer[i] = body->m_Shapes[i];
        }
        return size;
    }

    HCollisionShape2D GetCollisionShape2D(HWorld2D world, HCollisionObject2D collision_object, uint32_t shape_index)
    {
        Body* body = (Body*) collision_object;
        return body->m_Shapes[shape_index];
    }

    void GetCollisionShapeRadius2D(HWorld2D world, HCollisionShape2D _shape, float* radius)
    {
        CircleShapeData* shape = (CircleShapeData*) _shape;
        assert(shape->m_ShapeDataBase.m_Type == SHAPE_TYPE_CIRCLE);
        *radius = shape->m_Circle.radius * world->m_Context->m_InvScale;
    }

    void SetCollisionShapeRadius2D(HWorld2D world, HCollisionShape2D _shape, float radius)
    {
        CircleShapeData* shape = (CircleShapeData*) _shape;
        assert(shape->m_ShapeDataBase.m_Type == SHAPE_TYPE_CIRCLE);

        shape->m_Circle.radius = radius * world->m_Context->m_Scale;
        shape->m_ShapeDataBase.m_CreationScale = shape->m_Circle.radius;

        b2Shape_SetCircle(shape->m_ShapeDataBase.m_ShapeId, &shape->m_Circle);
    }

    void SetCollisionShapeBoxDimensions2D(HWorld2D world, HCollisionShape2D _shape, Quat rotation, float w, float h)
    {
        ShapeData* shape = (ShapeData*) _shape;

        if (shape->m_Type == SHAPE_TYPE_POLYGON)
        {
            float angle = atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ()));
            float scale = world->m_Context->m_Scale;

            PolygonShapeData* polygon = (PolygonShapeData*) shape;
            b2Vec2 centroid = polygon->m_Polygon.centroid;
            polygon->m_Polygon = b2MakeBox(w * scale, h * scale);

            b2Transform transform;
            transform.p = centroid;
            transform.q = b2MakeRot(angle);

            for (int i = 0; i < polygon->m_Polygon.count; ++i)
            {
                polygon->m_Polygon.vertices[i] = b2TransformPoint(transform, polygon->m_Polygon.vertices[i]);
                polygon->m_Polygon.normals[i] = b2RotateVector(transform.q, polygon->m_Polygon.normals[i]);
            }

            memcpy(polygon->m_VerticesOriginal, polygon->m_Polygon.vertices, sizeof(b2Vec2) * polygon->m_Polygon.count);
        }
    }

    void GetCollisionShapeBoxDimensions2D(HWorld2D world, HCollisionShape2D _shape, Quat rotation, float& w, float& h)
    {
        ShapeData* shape = (ShapeData*) _shape;

        if (shape->m_Type == SHAPE_TYPE_POLYGON)
        {
            b2Vec2 t;
            ToB2(Vector3(0), t, world->m_Context->m_Scale);
            b2Rot r;

            r.c = 1 - 2 * rotation.getZ() * rotation.getZ();
            r.s = 2 * rotation.getX() * rotation.getY();

            b2Transform transform;
            transform.p = t;
            transform.q = r;

            PolygonShapeData* polygon_shape = (PolygonShapeData*) shape;
            b2Vec2* vertices = polygon_shape->m_Polygon.vertices;
            float min_x = INT32_MAX,
                  min_y = INT32_MAX,
                  max_x = -INT32_MAX,
                  max_y = -INT32_MAX;
            float inv_scale = world->m_Context->m_InvScale;
            for (int i = 0; i < polygon_shape->m_Polygon.count; i += 1)
            {
                b2Vec2 v1 = FromTransformScaleB2(transform, inv_scale, vertices[i]);
                min_x = dmMath::Min(min_x, v1.x);
                max_x = dmMath::Max(max_x, v1.x);
                min_y = dmMath::Min(min_y, v1.y);
                max_y = dmMath::Max(max_y, v1.y);
            }
            w = (max_x - min_x);
            h = (max_y - min_y);
        }
    }

    void SetCollisionObjectUserData2D(HCollisionObject2D collision_object, void* user_data)
    {
        Body* body = (Body*) collision_object;
        b2Body_SetUserData(body->m_BodyId, user_data);
    }

    void* GetCollisionObjectUserData2D(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        return b2Body_GetUserData(body->m_BodyId);
    }

    void ApplyForce2D(HContext2D context, HCollisionObject2D collision_object, const Vector3& force, const Point3& position)
    {
        float scale = context->m_Scale;
        b2Vec2 b2_force;
        ToB2(force, b2_force, scale);
        b2Vec2 b2_position;
        ToB2(position, b2_position, scale);

        Body* body = (Body*) collision_object;
        b2Body_ApplyForce(body->m_BodyId, b2_force, b2_position, true);
    }

    Vector3 GetTotalForce2D(HContext2D context, HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        b2Vec2 b2_force = b2Body_GetTotalForce(body->m_BodyId);
        Vector3 force;
        FromB2(b2_force, force, context->m_InvScale);
        return force;
    }

    Point3 GetWorldPosition2D(HContext2D context, HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        b2Vec2 b2_position = b2Body_GetPosition(body->m_BodyId);
        Point3 position;
        FromB2(b2_position, position, context->m_InvScale);
        return position;
    }

    Quat GetWorldRotation2D(HContext2D context, HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        b2Rot rot = b2Body_GetRotation(body->m_BodyId);
        float angle = b2Rot_GetAngle(rot);
        return Quat::rotationZ(angle);
    }

    Vector3 GetLinearVelocity2D(HContext2D context, HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        b2Vec2 b2_lin_vel = b2Body_GetLinearVelocity(body->m_BodyId);

        Vector3 lin_vel;
        FromB2(b2_lin_vel, lin_vel, context->m_InvScale);
        return lin_vel;
    }

    Vector3 GetAngularVelocity2D(HContext2D context, HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        float ang_vel = b2Body_GetAngularVelocity(body->m_BodyId);
        return Vector3(0.0f, 0.0f, ang_vel);
    }

    void SetLinearVelocity2D(HContext2D context, HCollisionObject2D collision_object, const Vector3& velocity)
    {
        Body* body = (Body*) collision_object;
        b2Vec2 b2_velocity;
        ToB2(velocity, b2_velocity, context->m_Scale);
        b2Body_SetLinearVelocity(body->m_BodyId, b2_velocity);
    }

    void SetAngularVelocity2D(HContext2D context, HCollisionObject2D collision_object, const Vector3& velocity)
    {
        Body* body = (Body*) collision_object;
        b2Body_SetAngularVelocity(body->m_BodyId, velocity.getZ());
    }

    bool IsEnabled2D(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        return b2Body_IsEnabled(body->m_BodyId);
    }

    void SetEnabled2D(HWorld2D world, HCollisionObject2D collision_object, bool enabled)
    {
        DM_PROFILE("SetEnabled2D");
        bool prev_enabled = IsEnabled2D(collision_object);
        // Avoid multiple adds/removes
        if (prev_enabled == enabled)
        {
            return;
        }

        Body* body = (Body*) collision_object;

        if (enabled)
            b2Body_Enable(body->m_BodyId);
        else
            b2Body_Disable(body->m_BodyId);

        if (enabled)
        {
            b2Body_SetAwake(body->m_BodyId, true);

            if (world->m_GetWorldTransformCallback)
            {
                dmTransform::Transform world_transform;
                (*world->m_GetWorldTransformCallback)(b2Body_GetUserData(body->m_BodyId), world_transform);
                Point3 position = Point3(world_transform.GetTranslation());
                Quat rotation = Quat(world_transform.GetRotation());
                float angle = atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ()));
                b2Vec2 b2_position;
                ToB2(position, b2_position, world->m_Context->m_Scale);

                b2Rot b2_rot = b2MakeRot(angle);
                b2Body_SetTransform(body->m_BodyId, b2_position, b2_rot);
            }
        }
        else
        {
            // Reset state
            b2Body_SetAwake(body->m_BodyId, false);
        }
    }

    bool IsSleeping2D(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        return b2Body_IsAwake(body->m_BodyId);
    }

    void Wakeup2D(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        b2Body_SetAwake(body->m_BodyId, true);
    }

    void SetLockedRotation2D(HCollisionObject2D collision_object, bool locked_rotation)
    {
        Body* body = (Body*) collision_object;
        b2Body_SetFixedRotation(body->m_BodyId, locked_rotation);
    }

    float GetLinearDamping2D(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        return b2Body_GetLinearDamping(body->m_BodyId);
    }

    void SetLinearDamping2D(HCollisionObject2D collision_object, float linear_damping)
    {
        Body* body = (Body*) collision_object;
        b2Body_SetLinearDamping(body->m_BodyId, linear_damping);
    }

    float GetAngularDamping2D(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        return b2Body_GetAngularDamping(body->m_BodyId);
    }

    void SetAngularDamping2D(HCollisionObject2D collision_object, float angular_damping)
    {
        Body* body = (Body*) collision_object;
        b2Body_SetAngularDamping(body->m_BodyId, angular_damping);
    }

    float GetMass2D(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        return b2Body_GetMass(body->m_BodyId);
    }

    bool IsBullet2D(HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;
        return b2Body_IsBullet(body->m_BodyId);
    }

    void SetBullet2D(HCollisionObject2D collision_object, bool value)
    {
        Body* body = (Body*) collision_object;
        b2Body_SetBullet(body->m_BodyId, value);
    }

    void SetGroup2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit)
    {
        Body* body = (Body*) collision_object;

        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            ShapeData* shape_data = body->m_Shapes[i];
            if (shape_data->m_Type != SHAPE_TYPE_GRID)
            {
                b2Filter filter = b2Shape_GetFilter(shape_data->m_ShapeId);
                filter.categoryBits = groupbit;
                b2Shape_SetFilter(shape_data->m_ShapeId, filter);
            }
        }
    }

    uint16_t GetGroup2D(HWorld2D world, HCollisionObject2D collision_object)
    {
        Body* body = (Body*) collision_object;

        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            ShapeData* shape_data = body->m_Shapes[i];
            if (shape_data->m_Type != SHAPE_TYPE_GRID)
            {
                return b2Shape_GetFilter(shape_data->m_ShapeId).categoryBits;
            }
        }
        return 0;
    }

    // updates a specific group bit of a collision object's current mask
    void SetMaskBit2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit, bool boolvalue)
    {
        Body* body = (Body*) collision_object;

        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            ShapeData* shape_data = body->m_Shapes[i];

            if (shape_data->m_Type != SHAPE_TYPE_GRID)
            {
                b2Filter filter = b2Shape_GetFilter(shape_data->m_ShapeId); // all non-grid shapes have only one filter item indexed at position 0
                if (boolvalue)
                    filter.maskBits |= groupbit;
                else
                    filter.maskBits &= ~groupbit;

                b2Shape_SetFilter(shape_data->m_ShapeId, filter);
            }
        }
    }

    bool GetMaskBit2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit)
    {
        Body* body = (Body*) collision_object;
        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            ShapeData* shape_data = body->m_Shapes[i];

            if (shape_data->m_Type != SHAPE_TYPE_GRID)
            {
                b2Filter filter = b2Shape_GetFilter(shape_data->m_ShapeId);
                return !!(filter.maskBits & groupbit);
            }
        }
        return false;
    }

    bool UpdateMass2D(HWorld2D world, HCollisionObject2D collision_object, float mass)
    {
        Body* body = (Body*) collision_object;

        if (b2Body_GetType(body->m_BodyId) != b2_dynamicBody) {
            return false;
        }

        float total_area = 0.0f;
        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            switch(body->m_Shapes[i]->m_Type)
            {
            case SHAPE_TYPE_POLYGON:
            {
                PolygonShapeData* polygon = (PolygonShapeData*) body->m_Shapes[i];
                b2MassData mass_data = b2ComputePolygonMass(&polygon->m_Polygon, 1.0f);
                total_area += mass_data.mass;
            } break;
            case SHAPE_TYPE_CIRCLE:
            {
                CircleShapeData* circle = (CircleShapeData*) body->m_Shapes[i];
                b2MassData mass_data = b2ComputeCircleMass(&circle->m_Circle, 1.0f);
                total_area += mass_data.mass;
            } break;
            case SHAPE_TYPE_GRID:
            {
                // TODO
                // Original code approximates the mass to a box, but I'm not even sure mass matters for a grid?
            }
            default:
                break;
            }
        }

        if (total_area <= 0.0f)
        {
            return false;
        }

        float new_density = mass / total_area;

        for (int i = 0; i < body->m_ShapeCount; ++i)
        {
            b2Shape_SetDensity(body->m_Shapes[i]->m_ShapeId, new_density, false);
        }

        b2Body_ApplyMassFromShapes(body->m_BodyId);

        return true;
    }

    void RequestRayCast2D(HWorld2D world, const RayCastRequest& request)
    {
        if (!world->m_RayCastRequests.Full())
        {
            // Verify that the ray is not 0-length
            // We need to remove the z-value before calculating length (DEF-1286)
            const Point3 from2d = Point3(request.m_From.getX(), request.m_From.getY(), 0.0);
            const Point3 to2d = Point3(request.m_To.getX(), request.m_To.getY(), 0.0);
            if (lengthSqr(to2d - from2d) <= 0.0f)
            {
                dmLogWarning("Ray had 0 length when ray casting, ignoring request.");
            }
            else
            {
                world->m_RayCastRequests.Push(request);
            }
        }
        else
        {
            dmLogWarning("Ray cast query buffer is full (%d), ignoring request. See 'physics.ray_cast_limit_2d' in game.project", world->m_RayCastRequests.Capacity());
        }
    }

    static int Sort_RayCastResponse(const dmPhysics::RayCastResponse* a, const dmPhysics::RayCastResponse* b)
    {
        float diff = a->m_Fraction - b->m_Fraction;
        if (diff == 0)
            return 0;
        return diff < 0 ? -1 : 1;
    }

    static float cast_ray_cb(b2ShapeId shape_id, b2Vec2 point, b2Vec2 normal, float fraction, void* context )
    {
        // Return value controls ray behavior:
        // -1: ignore this intersection
        // 0: terminate ray immediately
        // fraction: clip ray to this point
        // 1: continue ray to full length

        RayCastContext* ctx = (RayCastContext*) context;

        b2Filter shape_filter = b2Shape_GetFilter(shape_id);

        if (b2Shape_IsSensor(shape_id))
        {
            return -1.f;
        }
        if (b2Shape_GetUserData(shape_id) == ctx->m_IgnoredUserData)
        {
            return -1.f;
        }
        else if ((shape_filter.categoryBits & ctx->m_CollisionMask) && (shape_filter.maskBits & ctx->m_CollisionGroup))
        {
            ctx->m_Response.m_Hit = 1;
            ctx->m_Response.m_Fraction = fraction;
            ctx->m_Response.m_CollisionObjectGroup = shape_filter.categoryBits;
            ctx->m_Response.m_CollisionObjectUserData = b2Shape_GetUserData(shape_id);
            FromB2(normal, ctx->m_Response.m_Normal, 1.0f); // Don't scale normal
            FromB2(point, ctx->m_Response.m_Position, ctx->m_Context->m_InvScale);

            // Returning fraction means we're splitting the search area, effectively returning the closest ray
            // By returning 1, we're make sure each hit is reported.
            if (ctx->m_ReturnAllResults)
            {
                if (ctx->m_Results->Full())
                    ctx->m_Results->OffsetCapacity(32);
                ctx->m_Results->Push(ctx->m_Response);
                return 1.0f;
            }
            return fraction;
        }
        else
            return -1.f;
    }

    void RayCast2D(HWorld2D world, const RayCastRequest& request, dmArray<RayCastResponse>& results)
    {
        DM_PROFILE("RayCast2D");

        const Point3 from2d = Point3(request.m_From.getX(), request.m_From.getY(), 0.0);
        const Point3 to2d = Point3(request.m_To.getX(), request.m_To.getY(), 0.0);
        if (lengthSqr(to2d - from2d) <= 0.0f)
        {
            dmLogWarning("Ray had 0 length when ray casting, ignoring request.");
            return;
        }

        float scale = world->m_Context->m_Scale;

        b2Vec2 from;
        ToB2(from2d, from, scale);
        b2Vec2 to;
        ToB2(to2d, to, scale);

        // Box2d V3 requires a translation vector, not a point
        b2Vec2 translate = b2Sub(to, from);

        RayCastContext context;
        context.m_CollisionMask    = request.m_Mask;
        context.m_IgnoredUserData  = request.m_IgnoredUserData;
        context.m_Context          = world->m_Context;
        context.m_Results          = &results;
        context.m_ReturnAllResults = request.m_ReturnAllResults;

        b2QueryFilter filter = b2DefaultQueryFilter();
        filter.maskBits = request.m_Mask;
        filter.categoryBits = (uint64_t) -1; // This will search for all groups, but we could expose a more narrow search if needed!

        b2World_CastRay(world->m_WorldId, from, translate, filter, cast_ray_cb, &context);

        if (!request.m_ReturnAllResults)
        {
            if (context.m_Response.m_Hit)
            {
                if (results.Full())
                {
                    results.OffsetCapacity(1);
                }
                results.SetSize(1);
                results[0] = context.m_Response;
            }
        }
        else
        {
            qsort(results.Begin(), results.Size(), sizeof(dmPhysics::RayCastResponse), (int(*)(const void*, const void*))Sort_RayCastResponse);
        }
    }

    void SetGravity2D(HWorld2D world, const Vector3& gravity)
    {
        b2Vec2 gravity_b;
        ToB2(gravity, gravity_b, world->m_Context->m_Scale);
        b2World_SetGravity(world->m_WorldId, gravity_b);
    }

    Vector3 GetGravity2D(HWorld2D world)
    {
        b2Vec2 gravity_b = b2World_GetGravity(world->m_WorldId);
        Vector3 gravity;
        FromB2(gravity_b, gravity, world->m_Context->m_InvScale);
        return gravity;
    }

    void SetDebugCallbacks2D(HContext2D context, const DebugCallbacks& callbacks)
    {
        context->m_DebugCallbacks = callbacks;
    }

    void ReplaceShape2D(HContext2D context, HCollisionShape2D old_shape, HCollisionShape2D new_shape)
    {
        ShapeData* old_shape_data = (ShapeData*) old_shape;
        ShapeData* new_shape_data = (ShapeData*) new_shape;

        uint64_t old_shape_h = ToOpaqueHandle(old_shape_data->m_ShapeId);

        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
        {
            World2D* world = context->m_Worlds[i];

            for (int j = 0; j < world->m_Bodies.Size(); ++j)
            {
                Body* body = world->m_Bodies[j];

                for (int s = 0; s < body->m_ShapeCount; ++s)
                {
                    uint64_t shape_h = ToOpaqueHandle(body->m_Shapes[s]->m_ShapeId);

                    if (shape_h == old_shape_h)
                    {
                        // Err not sure what to do here
                        assert(0);
                    }
                }
            }

            /*
            for (b2Body* body = context->m_Worlds[i]->m_World.GetBodyList(); body; body = body->GetNext())
            {
                b2Fixture* fixture = body->GetFixtureList();
                float mass = body->GetMass();
                while (fixture)
                {
                    b2Fixture* next_fixture = fixture->GetNext();
                    if (fixture->GetShape() == old_shape)
                    {
                        b2FixtureDef def;
                        def.density = 1.0f;
                        def.filter = fixture->GetFilterData(0);
                        def.friction = fixture->GetFriction();
                        def.isSensor = fixture->IsSensor();
                        def.restitution = fixture->GetRestitution();
                        def.shape = (b2Shape*)new_shape;
                        def.userData = fixture->GetUserData();
                        b2Fixture* new_fixture = body->CreateFixture(&def);

                        // Copy filter data per child
                        b2Shape* new_b2_shape = (b2Shape*) new_shape;
                        b2Shape* old_b2_shape = fixture->GetShape();
                        if (new_b2_shape->m_filterPerChild)
                        {
                            uint32_t new_child_count = new_b2_shape->GetChildCount();
                            uint32_t old_child_count = old_b2_shape->GetChildCount();
                            for (uint32_t c = 0; c < new_child_count; ++c)
                            {
                                b2Filter filter;

                                if (c < old_child_count)
                                {
                                    filter = fixture->GetFilterData(c);
                                }
                                else
                                {
                                    // The new shape has more children than the old
                                    // Use filter data from the first child
                                    filter = fixture->GetFilterData(0);
                                }

                                new_fixture->SetFilterData(filter, c);
                            }
                        }

                        body->DestroyFixture(fixture);
                        body->SetActive(true);
                    }
                    fixture = next_fixture;
                }
                UpdateMass2D(body, mass);
            }
            */
        }
    }

    HJoint CreateJoint2D(HWorld2D world, HCollisionObject2D obj_a, const Point3& pos_a, HCollisionObject2D obj_b, const Point3& pos_b, dmPhysics::JointType type, const ConnectJointParams& params)
    {
        float scale = world->m_Context->m_Scale;
        b2Vec2 pa;
        ToB2(pos_a, pa, scale);
        b2Vec2 pb;
        ToB2(pos_b, pb, scale);

        b2BodyId* b2_obj_a = (b2BodyId*) obj_a;
        b2BodyId* b2_obj_b = (b2BodyId*) obj_b;

        b2JointId joint = {};

        switch (type)
        {
            case dmPhysics::JOINT_TYPE_SPRING:
                {
                    b2DistanceJointDef jointDef = b2DefaultDistanceJointDef();
                    jointDef.bodyIdA          = *b2_obj_a;
                    jointDef.bodyIdB          = *b2_obj_b;
                    jointDef.localAnchorA     = pa;
                    jointDef.localAnchorB     = pb;
                    jointDef.length           = params.m_SpringJointParams.m_Length * scale;
                    jointDef.hertz            = params.m_SpringJointParams.m_FrequencyHz;
                    jointDef.dampingRatio     = params.m_SpringJointParams.m_DampingRatio;
                    jointDef.collideConnected = params.m_CollideConnected;

                    joint = b2CreateDistanceJoint(world->m_WorldId, &jointDef);
                }
                break;
            case dmPhysics::JOINT_TYPE_FIXED:
                {
                    b2DistanceJointDef jointDef = b2DefaultDistanceJointDef();
                    jointDef.bodyIdA            = *b2_obj_a;
                    jointDef.bodyIdB            = *b2_obj_b;
                    jointDef.localAnchorA       = pa;
                    jointDef.localAnchorB       = pb;
                    jointDef.maxLength          = params.m_FixedJointParams.m_MaxLength * scale;
                    jointDef.collideConnected   = params.m_CollideConnected;

                    joint = b2CreateDistanceJoint(world->m_WorldId, &jointDef);
                }
                break;
            case dmPhysics::JOINT_TYPE_HINGE:
                {
                    b2RevoluteJointDef jointDef = b2DefaultRevoluteJointDef();
                    jointDef.bodyIdA          = *b2_obj_a;
                    jointDef.bodyIdB          = *b2_obj_b;
                    jointDef.localAnchorA     = pa;
                    jointDef.localAnchorB     = pb;
                    jointDef.referenceAngle   = params.m_HingeJointParams.m_ReferenceAngle;
                    jointDef.lowerAngle       = params.m_HingeJointParams.m_LowerAngle;
                    jointDef.upperAngle       = params.m_HingeJointParams.m_UpperAngle;
                    jointDef.maxMotorTorque   = params.m_HingeJointParams.m_MaxMotorTorque;
                    jointDef.motorSpeed       = params.m_HingeJointParams.m_MotorSpeed;
                    jointDef.enableLimit      = params.m_HingeJointParams.m_EnableLimit;
                    jointDef.enableMotor      = params.m_HingeJointParams.m_EnableMotor;
                    jointDef.collideConnected = params.m_CollideConnected;
                    joint = b2CreateRevoluteJoint(world->m_WorldId, &jointDef);
                }
                break;
            case dmPhysics::JOINT_TYPE_SLIDER:
                {
                    b2PrismaticJointDef jointDef = b2DefaultPrismaticJointDef();
                    jointDef.bodyIdA          = *b2_obj_a;
                    jointDef.bodyIdB          = *b2_obj_b;
                    jointDef.localAnchorA     = pa;
                    jointDef.localAnchorB     = pb;
                    b2Vec2 axis;
                    Vector3 apa(params.m_SliderJointParams.m_LocalAxisA[0], params.m_SliderJointParams.m_LocalAxisA[1], params.m_SliderJointParams.m_LocalAxisA[2]);
                    ToB2(apa, axis, 1.0f);
                    jointDef.localAxisA       = axis;
                    jointDef.referenceAngle   = params.m_SliderJointParams.m_ReferenceAngle;
                    jointDef.enableLimit      = params.m_SliderJointParams.m_EnableLimit;
                    jointDef.lowerTranslation = params.m_SliderJointParams.m_LowerTranslation * scale;
                    jointDef.upperTranslation = params.m_SliderJointParams.m_UpperTranslation * scale;
                    jointDef.enableMotor      = params.m_SliderJointParams.m_EnableMotor;
                    jointDef.maxMotorForce    = params.m_SliderJointParams.m_MaxMotorForce * scale;
                    jointDef.motorSpeed       = params.m_SliderJointParams.m_MotorSpeed;
                    jointDef.collideConnected = params.m_CollideConnected;
                    joint = b2CreatePrismaticJoint(world->m_WorldId, &jointDef);
                }
                break;
            case dmPhysics::JOINT_TYPE_WELD:
                {
                    b2WeldJointDef jointDef   = b2DefaultWeldJointDef();
                    jointDef.bodyIdA          = *b2_obj_a;
                    jointDef.bodyIdB          = *b2_obj_b;
                    jointDef.localAnchorA     = pa;
                    jointDef.localAnchorB     = pb;
                    jointDef.referenceAngle   = params.m_WeldJointParams.m_ReferenceAngle;
                    jointDef.linearHertz      = params.m_WeldJointParams.m_FrequencyHz;
                    // todo: angularHertz?
                    jointDef.linearDampingRatio = params.m_WeldJointParams.m_DampingRatio;
                    // todo: angularDampingRatio?
                    jointDef.collideConnected   = params.m_CollideConnected;
                    joint = b2CreateWeldJoint(world->m_WorldId, &jointDef);
                }
                break;
            case dmPhysics::JOINT_TYPE_WHEEL:
                {
                    b2WheelJointDef jointDef  = b2DefaultWheelJointDef();
                    jointDef.bodyIdA          = *b2_obj_a;
                    jointDef.bodyIdB          = *b2_obj_b;
                    jointDef.localAnchorA     = pa;
                    jointDef.localAnchorB     = pb;
                    b2Vec2 axis;
                    Vector3 apa(params.m_WheelJointParams.m_LocalAxisA[0], params.m_WheelJointParams.m_LocalAxisA[1], params.m_WheelJointParams.m_LocalAxisA[2]);
                    ToB2(apa, axis, 1.0f);
                    jointDef.localAxisA       = axis;
                    jointDef.maxMotorTorque   = params.m_WheelJointParams.m_MaxMotorTorque;
                    jointDef.motorSpeed       = params.m_WheelJointParams.m_MotorSpeed;
                    jointDef.enableMotor      = params.m_WheelJointParams.m_EnableMotor;
                    jointDef.hertz            = params.m_WheelJointParams.m_FrequencyHz;
                    jointDef.dampingRatio     = params.m_WheelJointParams.m_DampingRatio;
                    jointDef.collideConnected = params.m_CollideConnected;

                    joint = b2CreateWheelJoint(world->m_WorldId, &jointDef);
                }
                break;
            default:
                return 0x0;
        }

        b2JointId* joint_raw = new b2JointId();
        *joint_raw = joint;

        return joint_raw;
    }

    bool SetJointParams2D(HWorld2D world, HJoint joint, dmPhysics::JointType type, const ConnectJointParams& params)
    {
        float scale = world->m_Context->m_Scale;

        b2JointId* joint_raw = (b2JointId*) joint;

        switch (type)
        {
            case dmPhysics::JOINT_TYPE_SPRING:
                {
                    b2DistanceJoint_SetLength(*joint_raw, params.m_SpringJointParams.m_Length * scale);
                    b2DistanceJoint_SetSpringHertz(*joint_raw, params.m_SpringJointParams.m_FrequencyHz);
                    b2DistanceJoint_SetSpringDampingRatio(*joint_raw, params.m_SpringJointParams.m_DampingRatio);
                }
                break;
            case dmPhysics::JOINT_TYPE_FIXED:
                {
                    b2DistanceJoint_SetLengthRange(*joint_raw, b2DistanceJoint_GetMinLength(*joint_raw), params.m_FixedJointParams.m_MaxLength * scale);
                }
                break;
            case dmPhysics::JOINT_TYPE_HINGE:
                {
                    b2RevoluteJoint_SetLimits(*joint_raw, params.m_HingeJointParams.m_LowerAngle, params.m_HingeJointParams.m_UpperAngle);
                    b2RevoluteJoint_SetMaxMotorTorque(*joint_raw, params.m_HingeJointParams.m_MaxMotorTorque * scale);
                    b2RevoluteJoint_SetMotorSpeed(*joint_raw, params.m_HingeJointParams.m_MotorSpeed);
                    b2RevoluteJoint_EnableLimit(*joint_raw, params.m_HingeJointParams.m_EnableLimit);
                    b2RevoluteJoint_EnableMotor(*joint_raw, params.m_HingeJointParams.m_EnableMotor);
                }
                break;
            case dmPhysics::JOINT_TYPE_SLIDER:
                {
                    b2PrismaticJoint_EnableLimit(*joint_raw, params.m_SliderJointParams.m_EnableLimit);
                    b2PrismaticJoint_SetLimits(*joint_raw, params.m_SliderJointParams.m_LowerTranslation * scale, params.m_SliderJointParams.m_UpperTranslation * scale);
                    b2PrismaticJoint_EnableMotor(*joint_raw, params.m_SliderJointParams.m_EnableMotor);
                    b2PrismaticJoint_SetMaxMotorForce(*joint_raw, params.m_SliderJointParams.m_MaxMotorForce * scale);
                    b2PrismaticJoint_SetMotorSpeed(*joint_raw, params.m_SliderJointParams.m_MotorSpeed);
                }
                break;
            case dmPhysics::JOINT_TYPE_WELD:
                {
                    b2WeldJoint_SetLinearHertz(*joint_raw, params.m_WeldJointParams.m_FrequencyHz);
                    b2WeldJoint_SetLinearDampingRatio(*joint_raw, params.m_WeldJointParams.m_DampingRatio);
                }
                break;
            case dmPhysics::JOINT_TYPE_WHEEL:
                {
                    b2WheelJoint_SetMaxMotorTorque(*joint_raw, params.m_WheelJointParams.m_MaxMotorTorque);
                    b2WheelJoint_SetMotorSpeed(*joint_raw, params.m_WheelJointParams.m_MotorSpeed);
                    b2WheelJoint_EnableMotor(*joint_raw, params.m_WheelJointParams.m_EnableMotor);
                    b2WheelJoint_SetSpringHertz(*joint_raw, params.m_WheelJointParams.m_FrequencyHz);
                    b2WheelJoint_SetSpringDampingRatio(*joint_raw, params.m_WheelJointParams.m_DampingRatio);
                }
                break;
            default:
                return false;
        }

        return true;
    }

    bool GetJointParams2D(HWorld2D world, HJoint _joint, dmPhysics::JointType type, ConnectJointParams& params)
    {
        float inv_scale = world->m_Context->m_InvScale;

        b2JointId* joint_raw = (b2JointId*)_joint;
        params.m_CollideConnected = b2Joint_GetCollideConnected(*joint_raw);

        switch (type)
        {
            case dmPhysics::JOINT_TYPE_SPRING:
                {
                    params.m_SpringJointParams.m_Length =  b2DistanceJoint_GetLength(*joint_raw) * inv_scale;
                    params.m_SpringJointParams.m_FrequencyHz = b2DistanceJoint_GetSpringHertz(*joint_raw);
                    params.m_SpringJointParams.m_DampingRatio = b2DistanceJoint_GetSpringDampingRatio(*joint_raw);
                }
                break;
            case dmPhysics::JOINT_TYPE_FIXED:
                {
                    params.m_FixedJointParams.m_MaxLength = b2DistanceJoint_GetMaxLength(*joint_raw) * inv_scale;
                }
                break;
            case dmPhysics::JOINT_TYPE_HINGE:
                {
                    params.m_HingeJointParams.m_ReferenceAngle = b2RevoluteJoint_GetAngle(*joint_raw);
                    params.m_HingeJointParams.m_LowerAngle = b2RevoluteJoint_GetLowerLimit(*joint_raw);
                    params.m_HingeJointParams.m_UpperAngle = b2RevoluteJoint_GetUpperLimit(*joint_raw);
                    params.m_HingeJointParams.m_MaxMotorTorque = b2RevoluteJoint_GetMaxMotorTorque(*joint_raw) * inv_scale;
                    params.m_HingeJointParams.m_MotorSpeed = b2RevoluteJoint_GetMotorSpeed(*joint_raw);
                    params.m_HingeJointParams.m_EnableLimit = b2RevoluteJoint_IsLimitEnabled(*joint_raw);
                    params.m_HingeJointParams.m_EnableMotor = b2RevoluteJoint_IsMotorEnabled(*joint_raw);

                    // Read only properties
                    // params.m_HingeJointParams.m_JointAngle = typed_joint->GetJointAngle();
                    // params.m_HingeJointParams.m_JointSpeed = typed_joint->GetJointSpeed();
                }
                break;
            case dmPhysics::JOINT_TYPE_SLIDER:
                {
                    // b2Vec2 axis = typed_joint->GetLocalAxisA();
                    // params.m_SliderJointParams.m_LocalAxisA[0] = axis.x;
                    // params.m_SliderJointParams.m_LocalAxisA[1] = axis.y;
                    // params.m_SliderJointParams.m_LocalAxisA[2] = 0.0f;

                    // params.m_SliderJointParams.m_ReferenceAngle =   // typed_joint->GetReferenceAngle();
                    params.m_SliderJointParams.m_EnableLimit = b2PrismaticJoint_IsLimitEnabled(*joint_raw);
                    params.m_SliderJointParams.m_LowerTranslation = b2PrismaticJoint_GetLowerLimit(*joint_raw) * inv_scale;
                    params.m_SliderJointParams.m_UpperTranslation = b2PrismaticJoint_GetUpperLimit(*joint_raw) * inv_scale;
                    params.m_SliderJointParams.m_EnableMotor = b2PrismaticJoint_IsMotorEnabled(*joint_raw);
                    params.m_SliderJointParams.m_MaxMotorForce = b2PrismaticJoint_GetMaxMotorForce(*joint_raw) * inv_scale;
                    params.m_SliderJointParams.m_MotorSpeed = b2PrismaticJoint_GetMotorSpeed(*joint_raw);

                    // Read only properties
                    // params.m_SliderJointParams.m_JointTranslation = typed_joint->GetJointTranslation();
                    // params.m_SliderJointParams.m_JointSpeed = typed_joint->GetJointSpeed();
                }
                break;
            case dmPhysics::JOINT_TYPE_WELD:
                {
                    params.m_WeldJointParams.m_FrequencyHz = b2WeldJoint_GetLinearHertz(*joint_raw);
                    params.m_WeldJointParams.m_DampingRatio = b2WeldJoint_GetLinearDampingRatio(*joint_raw);

                    // Read only properties
                    // params.m_WeldJointParams.m_ReferenceAngle = typed_joint->GetReferenceAngle();
                }
                break;
            case dmPhysics::JOINT_TYPE_WHEEL:
                {
                    // b2Vec2 axis = typed_joint->GetLocalAxisA();
                    // params.m_WheelJointParams.m_LocalAxisA[0] = axis.x;
                    // params.m_WheelJointParams.m_LocalAxisA[1] = axis.y;
                    // params.m_WheelJointParams.m_LocalAxisA[2] = 0.0f;

                    params.m_WheelJointParams.m_MaxMotorTorque = b2WheelJoint_GetMaxMotorTorque(*joint_raw) * inv_scale;
                    params.m_WheelJointParams.m_MotorSpeed = b2WheelJoint_GetMotorSpeed(*joint_raw);
                    params.m_WheelJointParams.m_EnableMotor = b2WheelJoint_IsMotorEnabled(*joint_raw);
                    params.m_WheelJointParams.m_FrequencyHz = b2WheelJoint_GetSpringHertz(*joint_raw);
                    params.m_WheelJointParams.m_DampingRatio = b2WheelJoint_GetSpringDampingRatio(*joint_raw);

                    // Read only properties
                    // params.m_WheelJointParams.m_JointTranslation = typed_joint->GetJointTranslation();
                    // params.m_WheelJointParams.m_JointSpeed = typed_joint->GetJointSpeed();
                }
                break;
            default:
                return false;
        }

        return true;
    }

    void DeleteJoint2D(HWorld2D world, HJoint _joint)
    {
        assert(_joint);
        b2JointId* joint_raw = (b2JointId*)_joint;
        b2DestroyJoint(*joint_raw);
        delete joint_raw;
    }

    // NOTE: inv_dt is not used. It is calculated from the last step + substep time inside box2d
    bool GetJointReactionForce2D(HWorld2D world, HJoint _joint, Vector3& force)
    {
        float inv_scale = world->m_Context->m_InvScale;
        b2JointId* joint_raw = (b2JointId*)_joint;

        b2Vec2 bv2 = b2Joint_GetConstraintForce(*joint_raw);
        FromB2(bv2, force, inv_scale);
        return true;
    }

    // NOTE: inv_dt is not used. It is calculated from the last step + substep time inside box2d
    bool GetJointReactionTorque2D(HWorld2D world, HJoint _joint, float& torque)
    {
        float inv_scale = world->m_Context->m_InvScale;
        b2JointId* joint_raw = (b2JointId*)_joint;

        torque = b2Joint_GetConstraintTorque(*joint_raw) * inv_scale;
        return true;
    }
}
