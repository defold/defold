// Copyright 2020-2026 The Defold Foundation
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

#include <stdint.h>
#include <stdlib.h> // qsort

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dmsdk/dlib/vmath.h>

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include "physics_3d.h"

#include <stdio.h>

namespace dmPhysics
{
    using namespace dmVMath;

    /*
     * NOTE
     * This struct has the sole purpose of wrapping a collision object along with its collision group/mask.
     * The reason is that the way we do enabling/disabling (by adding/removing from the world)
     * looses the group/mask (bullet stores them in the broadphase-handle).
     * The first attempt was to use the collision flags ACTIVE_TAG vs DISABLE_SIMULATION, but that does not
     * work for static bodies and ghost objects (=triggers).
     * See: https://defold.fogbugz.com/default.asp?2085
     */
    struct CollisionObject3D
    {
        btCollisionObject* m_CollisionObject;
        uint16_t m_CollisionGroup;
        uint16_t m_CollisionMask;
    };

    static Point3 GetWorldPosition(HContext3D context, btCollisionObject* collision_object);
    static Quat GetWorldRotation(HContext3D context, btCollisionObject* collision_object);

    static btCollisionObject* GetCollisionObject(HCollisionObject3D co)
    {
        return ((CollisionObject3D*)co)->m_CollisionObject;
    }

    class MotionState : public btMotionState
    {
    public:
        MotionState(HContext3D context, void* user_data, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform)
        : m_Context(context)
        , m_UserData(user_data)
        , m_GetWorldTransform(get_world_transform)
        , m_SetWorldTransform(set_world_transform)
        {
        }

        virtual ~MotionState()
        {

        }

        virtual void getWorldTransform(btTransform& world_trans) const
        {
            if (m_GetWorldTransform != 0x0)
            {
                dmTransform::Transform world_transform;
                m_GetWorldTransform(m_UserData, world_transform);
                Point3 position = Point3(world_transform.GetTranslation());
                Quat rotation = Quat(world_transform.GetRotation());
                btVector3 origin;
                ToBt(position, origin, m_Context->m_Scale);
                world_trans.setOrigin(origin);
                world_trans.setRotation(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()));
            }
            else
            {
                world_trans = btTransform::getIdentity();
            }
        }

        virtual void setWorldTransform(const btTransform &worldTrans)
        {
            if (m_SetWorldTransform != 0x0)
            {
                btVector3 bt_pos = worldTrans.getOrigin();
                btQuaternion bt_rot = worldTrans.getRotation();

                Vector3 translation;
                FromBt(bt_pos, translation, m_Context->m_InvScale);
                Quat rot = Quat(bt_rot.getX(), bt_rot.getY(), bt_rot.getZ(), bt_rot.getW());
                m_SetWorldTransform(m_UserData, Point3(translation), rot);
            }
        }

    protected:
        HContext3D m_Context;
        void* m_UserData;
        GetWorldTransformCallback m_GetWorldTransform;
        SetWorldTransformCallback m_SetWorldTransform;
    };

    Context3D::Context3D()
    : m_Worlds()
    , m_DebugCallbacks()
    , m_Gravity(0.0f, -10.0f, 0.0f)
    , m_Socket(0)
    , m_Scale(1.0f)
    , m_InvScale(1.0f)
    , m_ContactImpulseLimit(0.0f)
    , m_TriggerEnterLimit(0.0f)
    , m_RayCastLimit(0)
    , m_TriggerOverlapCapacity(0)
    , m_AllowDynamicTransforms(0)
    {

    }

    World3D::World3D(HContext3D context, const NewWorldParams& params)
    : m_TriggerOverlaps(context->m_TriggerOverlapCapacity)
    , m_DebugDraw(&context->m_DebugCallbacks)
    , m_Context(context)
    , m_AllowDynamicTransforms(context->m_AllowDynamicTransforms)
    {
        m_CollisionConfiguration = new btDefaultCollisionConfiguration();
        m_Dispatcher = new btCollisionDispatcher(m_CollisionConfiguration);

        ///the maximum size of the collision world. Make sure objects stay within these boundaries
        ///Don't make the world AABB size too large, it will harm simulation quality and performance
        btVector3 world_aabb_min;
        ToBt(params.m_WorldMin, world_aabb_min, context->m_Scale);
        btVector3 world_aabb_max;
        ToBt(params.m_WorldMax, world_aabb_max, context->m_Scale);
        m_OverlappingPairCache = new btAxisSweep3(world_aabb_min,world_aabb_max, params.m_MaxCollisionObjectsCount);

        m_Solver = new btSequentialImpulseConstraintSolver;

        m_DynamicsWorld = new btDiscreteDynamicsWorld(m_Dispatcher, m_OverlappingPairCache, m_Solver, m_CollisionConfiguration);
        m_DynamicsWorld->setGravity(btVector3(context->m_Gravity.getX(), context->m_Gravity.getY(), context->m_Gravity.getZ()));
        m_DynamicsWorld->setDebugDrawer(&m_DebugDraw);

        m_GetWorldTransform = params.m_GetWorldTransformCallback;
        m_SetWorldTransform = params.m_SetWorldTransformCallback;

        m_RayCastRequests.SetCapacity(context->m_RayCastLimit);
        OverlapCacheInit(&m_TriggerOverlaps);
    }

    World3D::~World3D()
    {
        delete m_DynamicsWorld;
        delete m_Solver;
        delete m_OverlappingPairCache;
        delete m_Dispatcher;
        delete m_CollisionConfiguration;
    }

    HContext3D GetContext3D(HWorld3D world)
    {
        return world->m_Context;
    }

    static void ResponseFromRayCastResult(RayCastResponse& response, btScalar inv_scale, btScalar fraction,
                        const btVector3& point, const btVector3& normal, const btCollisionObject* co)
    {
        response.m_Hit = 1;
        response.m_Fraction = fraction;
        FromBt(point, response.m_Position, inv_scale);
        FromBt(normal, response.m_Normal, 1.0f); // don't scale normal

        if (co != 0x0)
        {
            response.m_CollisionObjectUserData = co->getUserPointer();
            response.m_CollisionObjectGroup = co->getBroadphaseHandle()->m_collisionFilterGroup;
        }
    }

    struct RayCastResultClosestCallback3D : public btCollisionWorld::ClosestRayResultCallback
    {
        RayCastResultClosestCallback3D(const btVector3& from, const btVector3& to, uint16_t mask, void* ignored_user_data)
        : btCollisionWorld::ClosestRayResultCallback(from, to)
        , m_IgnoredUserData(ignored_user_data)
        {
            // *all* groups for now, bullet will test this against the colliding object's mask
            m_collisionFilterGroup = ~0;
            m_collisionFilterMask = mask;
        }

        virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
        {
            if (rayResult.m_collisionObject->getUserPointer() == m_IgnoredUserData)
                return 1.0f;
            else if (!rayResult.m_collisionObject->hasContactResponse())
                return 1.0f;
            else
                return btCollisionWorld::ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
        }

        void*           m_IgnoredUserData;
        RayCastResponse m_Response;
    };

    // Grabbed from a more recent Bullet version for now
    /// BULLET (do not modify) ->
    struct AllHitsRayResultCallback : public btCollisionWorld::RayResultCallback
    {
        AllHitsRayResultCallback(const btVector3& rayFromWorld, const btVector3& rayToWorld)
            : m_rayFromWorld(rayFromWorld)
            , m_rayToWorld(rayToWorld)
        {
        }
        btAlignedObjectArray<const btCollisionObject*> m_collisionObjects;
        btAlignedObjectArray<btVector3> m_hitNormalWorld;
        btAlignedObjectArray<btVector3> m_hitPointWorld;
        btAlignedObjectArray<btScalar> m_hitFractions;
        btVector3 m_rayFromWorld;//used to calculate hitPointWorld from hitFraction
        btVector3 m_rayToWorld;

        virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
        {
                m_collisionObject = rayResult.m_collisionObject;
                m_collisionObjects.push_back(rayResult.m_collisionObject);
                btVector3 hitNormalWorld;
                if (normalInWorldSpace)
                {
                    hitNormalWorld = rayResult.m_hitNormalLocal;
                } else
                {
                    hitNormalWorld = m_collisionObject->getWorldTransform().getBasis()*rayResult.m_hitNormalLocal;
                }
                m_hitNormalWorld.push_back(hitNormalWorld);
                btVector3 hitPointWorld;
                hitPointWorld.setInterpolate3(m_rayFromWorld,m_rayToWorld,rayResult.m_hitFraction);
                m_hitPointWorld.push_back(hitPointWorld);
                m_hitFractions.push_back(rayResult.m_hitFraction);
                return m_closestHitFraction;
        }
    };
    /// <- END BULLET

    struct RayCastResultAllCallback3D : public AllHitsRayResultCallback
    {
        RayCastResultAllCallback3D(const btVector3& from, const btVector3& to, uint16_t mask, void* ignored_user_data)
            : AllHitsRayResultCallback(from, to)
            , m_IgnoredUserData(ignored_user_data)
        {
            // *all* groups for now, bullet will test this against the colliding object's mask
            m_collisionFilterGroup = ~0;
            m_collisionFilterMask = mask;
        }

        virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
        {
            if (rayResult.m_collisionObject->getUserPointer() == m_IgnoredUserData)
                return 1.0f;
            else if (!rayResult.m_collisionObject->hasContactResponse())
                return 1.0f;
            else
                return AllHitsRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
        }

        void* m_IgnoredUserData;
    };

    HContext3D NewContext3D(const NewContextParams& params)
    {
        if (params.m_Scale < MIN_SCALE || params.m_Scale > MAX_SCALE)
        {
            dmLogFatal("Physics scale is outside the valid range %.2f - %.2f.", MIN_SCALE, MAX_SCALE);
            return 0x0;
        }
        Context3D* context = new Context3D();
        ToBt(params.m_Gravity, context->m_Gravity, params.m_Scale);
        context->m_Worlds.SetCapacity(params.m_WorldCount);
        context->m_Scale = params.m_Scale;
        context->m_InvScale = 1.0f / params.m_Scale;
        context->m_ContactImpulseLimit = params.m_ContactImpulseLimit * params.m_Scale;
        context->m_TriggerEnterLimit = params.m_TriggerEnterLimit * params.m_Scale;
        context->m_RayCastLimit = params.m_RayCastLimit3D;
        context->m_TriggerOverlapCapacity = params.m_TriggerOverlapCapacity;
        context->m_AllowDynamicTransforms = params.m_AllowDynamicTransforms;
        dmMessage::Result result = dmMessage::NewSocket(PHYSICS_SOCKET_NAME, &context->m_Socket);
        if (result != dmMessage::RESULT_OK && result != dmMessage::RESULT_SOCKET_EXISTS)
        {
            dmLogFatal("Could not create socket '%s'.", PHYSICS_SOCKET_NAME);
            DeleteContext3D(context);
            return 0x0;
        }
        return context;
    }

    void DeleteContext3D(HContext3D context)
    {
        if (!context->m_Worlds.Empty())
        {
            dmLogWarning("Deleting %ud 3d worlds since the context is deleted.", context->m_Worlds.Size());
            for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
                delete context->m_Worlds[i];
        }
        if (context->m_Socket != 0)
            dmMessage::DeleteSocket(context->m_Socket);
        delete context;
    }

    dmMessage::HSocket GetSocket3D(HContext3D context)
    {
        return context->m_Socket;
    }

    HWorld3D NewWorld3D(HContext3D context, const NewWorldParams& params)
    {
        if (context->m_Worlds.Full())
        {
            dmLogError("%s", "Physics world buffer full, world could not be created.");
            return 0x0;
        }
        World3D* world = new World3D(context, params);
        context->m_Worlds.Push(world);
        return world;
    }

    static void ClearPendingRayCasts3D(HWorld3D world)
    {
        uint32_t size = world->m_RayCastRequests.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            const RayCastRequest& request = world->m_RayCastRequests[i];
            if (request.m_UserData)
            {
                free(request.m_UserData);
            }
        }
        world->m_RayCastRequests.SetSize(0);
    }

    void DeleteWorld3D(HContext3D context, HWorld3D world)
    {
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
            if (context->m_Worlds[i] == world)
                context->m_Worlds.EraseSwap(i);
        ClearPendingRayCasts3D(world);
        delete world;
    }

    void SetDrawDebug3D(HWorld3D world, bool draw_debug)
    {
        int debug_mode = 0;
        if (draw_debug)
        {
            debug_mode = btIDebugDraw::DBG_NoDebug
                | btIDebugDraw::DBG_DrawWireframe
                | btIDebugDraw::DBG_DrawAabb
                | btIDebugDraw::DBG_DrawFeaturesText
                | btIDebugDraw::DBG_DrawContactPoints
                | btIDebugDraw::DBG_DrawText
                | btIDebugDraw::DBG_ProfileTimings
                | btIDebugDraw::DBG_EnableSatComparison
                | btIDebugDraw::DBG_EnableCCD
                | btIDebugDraw::DBG_DrawConstraints
                | btIDebugDraw::DBG_DrawConstraintLimits;
        }

        world->m_DebugDraw.setDebugMode(debug_mode);
    }

    static void UpdateOverlapCache(OverlapCache* cache, HContext3D context, btDispatcher* dispatcher, const StepWorldContext& step_context);

    void StepWorld3D(HWorld3D world, const StepWorldContext& step_context)
    {
        HContext3D context = world->m_Context;
        float scale = context->m_Scale;
        // Epsilon defining what transforms are considered noise and not
        // Values are picked by inspection, current rot value is roughly equivalent to 1 degree
        const float POS_EPSILON = 0.00005f * scale;
        const float ROT_EPSILON = 0.00007f;
        // Update all trigger transforms before physics world step
        if (world->m_GetWorldTransform != 0x0)
        {
            DM_PROFILE("UpdateTriggers");
            int collision_object_count = world->m_DynamicsWorld->getNumCollisionObjects();
            btCollisionObjectArray& collision_objects = world->m_DynamicsWorld->getCollisionObjectArray();
            for (int i = 0; i < collision_object_count; ++i)
            {
                btCollisionObject* collision_object = collision_objects[i];

                bool retrieve_gameworld_transform = world->m_AllowDynamicTransforms && !collision_object->isStaticObject();

                if (collision_object->getInternalType() == btCollisionObject::CO_GHOST_OBJECT || collision_object->isKinematicObject() || retrieve_gameworld_transform)
                {
                    Point3 old_position = GetWorldPosition(context, collision_object);
                    Quat old_rotation = GetWorldRotation(context, collision_object);
                    dmTransform::Transform world_transform;
                    (*world->m_GetWorldTransform)(collision_object->getUserPointer(), world_transform);
                    Point3 position = Point3(world_transform.GetTranslation());
                    Quat rotation = Quat(world_transform.GetRotation());
                    float dp = distSqr(old_position, position);
                    float dr = norm(rotation - old_rotation);
                    if (dp > POS_EPSILON || dr > ROT_EPSILON)
                    {
                        btVector3 bt_pos;
                        ToBt(position, bt_pos, scale);
                        btTransform world_t(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()), bt_pos);
                        collision_object->setWorldTransform(world_t);
                        collision_object->activate(true);
                    }
                }

                // Scaling
                if (retrieve_gameworld_transform)
                {
                    dmTransform::Transform world_transform;
                    world->m_GetWorldTransform(collision_object->getUserPointer(), world_transform);

                    // The compound shape scale always defaults to 1
                    btCollisionShape* shape = collision_object->getCollisionShape();

                    float object_scale = world_transform.GetUniformScale();
                    float shape_scale = shape->getLocalScaling().getX();

                    if (object_scale != shape_scale)
                    {
                        shape->setLocalScaling(btVector3(object_scale,object_scale,object_scale));
                        if (!collision_object->isActive())
                            collision_object->activate(true);
                    }
                }
            }
        }

        {
            DM_PROFILE("StepSimulation");

            // Default behavior for Bullet3D is to use fixed timesteps with max_steps=1 and fixed_timestep=1.0f/60.0f
            float dt = step_context.m_DT;
            float fixed_timestep = step_context.m_FixedTimeStep ? dt : 1.0f/60.0f;
            int max_steps = step_context.m_MaxFixedTimeSteps;

            world->m_DynamicsWorld->stepSimulation(dt, max_steps, fixed_timestep);
        }

        // Handle ray cast requests
        uint32_t size = world->m_RayCastRequests.Size();
        if (size > 0)
        {
            DM_PROFILE("RayCasts");
            for (uint32_t i = 0; i < size; ++i)
            {
                const RayCastRequest& request = world->m_RayCastRequests[i];
                if (step_context.m_RayCastCallback == 0x0)
                {
                    dmLogWarning("Ray cast requested without any response callback, skipped.");
                    continue;
                }
                float scale = world->m_Context->m_Scale;
                btVector3 from;
                ToBt(request.m_From, from, scale);
                btVector3 to;
                ToBt(request.m_To, to, scale);
                RayCastResultClosestCallback3D result_callback(from, to, request.m_Mask, request.m_IgnoredUserData);
                world->m_DynamicsWorld->rayTest(from, to, result_callback);
                RayCastResponse response;
                response.m_Hit = result_callback.hasHit() ? 1 : 0;
                response.m_Fraction = result_callback.m_closestHitFraction;
                float inv_scale = world->m_Context->m_InvScale;
                FromBt(result_callback.m_hitPointWorld, response.m_Position, inv_scale);
                FromBt(result_callback.m_hitNormalWorld, response.m_Normal, 1.0f); // don't scale normal
                if (result_callback.m_collisionObject != 0x0)
                {
                    response.m_CollisionObjectUserData = result_callback.m_collisionObject->getUserPointer();
                    response.m_CollisionObjectGroup = result_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
                }
                step_context.m_RayCastCallback(response, request, step_context.m_RayCastUserData);
            }
            world->m_RayCastRequests.SetSize(0);
        }

        // Report collisions
        bool requests_collision_callbacks = true;
        bool requests_contact_callbacks = true;

        CollisionCallback collision_callback = step_context.m_CollisionCallback;
        ContactPointCallback contact_point_callback = step_context.m_ContactPointCallback;
        btDispatcher* dispatcher = world->m_DynamicsWorld->getDispatcher();
        float contact_impulse_limit = world->m_Context->m_ContactImpulseLimit;
        if (collision_callback != 0x0 || contact_point_callback != 0x0)
        {
            DM_PROFILE("CollisionCallbacks");
            int num_manifolds = dispatcher->getNumManifolds();
            for (int i = 0; i < num_manifolds && (requests_collision_callbacks || requests_contact_callbacks); ++i)
            {
                btPersistentManifold* contact_manifold = dispatcher->getManifoldByIndexInternal(i);
                btCollisionObject* object_a = static_cast<btCollisionObject*>(contact_manifold->getBody0());
                btCollisionObject* object_b = static_cast<btCollisionObject*>(contact_manifold->getBody1());

                if (!object_a->isActive() && !object_b->isActive())
                    continue;

                // verify that the impulse is large enough to be reported
                float max_impulse = 0.0f;
                int num_contacts = contact_manifold->getNumContacts();
                for (int j = 0; j < num_contacts && requests_contact_callbacks; ++j)
                {
                    btManifoldPoint& pt = contact_manifold->getContactPoint(j);
                    max_impulse = dmMath::Max(max_impulse, pt.getAppliedImpulse());
                }
                // early out if the impulse is too small to be reported
                if (max_impulse < contact_impulse_limit)
                    continue;

                if (collision_callback != 0x0 && requests_collision_callbacks && num_contacts > 0)
                {
                    requests_collision_callbacks = collision_callback(object_a->getUserPointer(), object_a->getBroadphaseHandle()->m_collisionFilterGroup, object_b->getUserPointer(), object_b->getBroadphaseHandle()->m_collisionFilterGroup, step_context.m_CollisionUserData);
                }

                bool is_trigger_contact = object_a->getInternalType() == btCollisionObject::CO_GHOST_OBJECT || object_b->getInternalType() == btCollisionObject::CO_GHOST_OBJECT;

                if (contact_point_callback != 0x0 && !is_trigger_contact)
                {
                    for (int j = 0; j < num_contacts && requests_contact_callbacks; ++j)
                    {
                        btManifoldPoint& pt = contact_manifold->getContactPoint(j);
                        btRigidBody* body_a = btRigidBody::upcast(object_a);
                        btRigidBody* body_b = btRigidBody::upcast(object_b);
                        ContactPoint point;
                        float inv_scale = world->m_Context->m_InvScale;
                        const btVector3& pt_a = pt.getPositionWorldOnA();
                        FromBt(pt_a, point.m_PositionA, inv_scale);
                        point.m_UserDataA = object_a->getUserPointer();
                        point.m_GroupA = object_a->getBroadphaseHandle()->m_collisionFilterGroup;
                        if (body_a)
                            point.m_MassA = 1.0f / body_a->getInvMass();
                        const btVector3& pt_b = pt.getPositionWorldOnB();
                        FromBt(pt_b, point.m_PositionB, inv_scale);
                        point.m_UserDataB = object_b->getUserPointer();
                        point.m_GroupB = object_b->getBroadphaseHandle()->m_collisionFilterGroup;
                        if (body_b)
                            point.m_MassB = 1.0f / body_b->getInvMass();
                        const btVector3& normal = pt.m_normalWorldOnB;
                        FromBt(-normal, point.m_Normal, 1.0f); // Don't scale normals
                        point.m_Distance = -pt.getDistance() * inv_scale;
                        point.m_AppliedImpulse = pt.getAppliedImpulse() * inv_scale;
                        Vector3 vel_a(0.0f, 0.0f, 0.0f);
                        if (body_a)
                        {
                            const btVector3& v = body_a->getLinearVelocity();
                            FromBt(v, vel_a, inv_scale);
                        }
                        Vector3 vel_b(0.0f, 0.0f, 0.0f);
                        if (body_b)
                        {
                            const btVector3& v = body_b->getLinearVelocity();
                            FromBt(v, vel_b, inv_scale);
                        }
                        point.m_RelativeVelocity = vel_a - vel_b;
                        requests_contact_callbacks = contact_point_callback(point, step_context.m_ContactPointUserData);
                    }
                }
            }
        }
        UpdateOverlapCache(&world->m_TriggerOverlaps, context, dispatcher, step_context);
        world->m_DynamicsWorld->debugDrawWorld();
    }

    void UpdateOverlapCache(OverlapCache* cache, HContext3D context, btDispatcher* dispatcher, const StepWorldContext& step_context)
    {
        DM_PROFILE("TriggerCallbacks");
        OverlapCacheReset(cache);
        OverlapCacheAddData add_data;
        add_data.m_TriggerEnteredCallback = step_context.m_TriggerEnteredCallback;
        add_data.m_TriggerEnteredUserData = step_context.m_TriggerEnteredUserData;
        int num_manifolds = dispatcher->getNumManifolds();
        for (int i = 0; i < num_manifolds; ++i)
        {
            btPersistentManifold* contact_manifold = dispatcher->getManifoldByIndexInternal(i);
            btCollisionObject* object_a = static_cast<btCollisionObject*>(contact_manifold->getBody0());
            btCollisionObject* object_b = static_cast<btCollisionObject*>(contact_manifold->getBody1());

            // Don't skip sleeping objects, in order to be able to catch exit events (also consistent with 2d physics)

            if (btGhostObject::upcast(object_a) != 0x0 || btGhostObject::upcast(object_b) != 0x0)
            {
                int contact_count = contact_manifold->getNumContacts();
                if (contact_count == 0)
                    continue;

                float max_distance = 0.0f;
                for (int j = 0; j < contact_count; ++j)
                {
                    const btManifoldPoint& point = contact_manifold->getContactPoint(j);
                    max_distance = dmMath::Max(max_distance, point.getDistance());
                }
                if (max_distance >= context->m_TriggerEnterLimit)
                {
                    add_data.m_ObjectA = (uint64_t) object_a;
                    add_data.m_UserDataA = object_a->getUserPointer();
                    add_data.m_ObjectB = (uint64_t) object_b;
                    add_data.m_UserDataB = object_b->getUserPointer();
                    add_data.m_GroupA = object_a->getBroadphaseHandle()->m_collisionFilterGroup;
                    add_data.m_GroupB = object_b->getBroadphaseHandle()->m_collisionFilterGroup;
                    OverlapCacheAdd(cache, add_data);
                }
            }
        }
        OverlapCachePruneData prune_data;
        prune_data.m_TriggerExitedCallback = step_context.m_TriggerExitedCallback;
        prune_data.m_TriggerExitedUserData = step_context.m_TriggerExitedUserData;
        OverlapCachePrune(cache, prune_data);
    }

    HCollisionShape3D NewSphereShape3D(HContext3D context, float radius)
    {
        return new btSphereShape(context->m_Scale * radius);
    }

    HCollisionShape3D NewBoxShape3D(HContext3D context, const Vector3& half_extents)
    {
        btVector3 dims;
        ToBt(half_extents, dims, context->m_Scale);
        return new btBoxShape(dims);
    }

    HCollisionShape3D NewCapsuleShape3D(HContext3D context, float radius, float height)
    {
        float scale = context->m_Scale;
        return new btCapsuleShape(scale * radius, scale * height);
    }

    HCollisionShape3D NewConvexHullShape3D(HContext3D context, const float* vertices, uint32_t vertex_count)
    {
        assert(sizeof(btScalar) == sizeof(float));
        float scale = context->m_Scale;
        const uint32_t elem_count = vertex_count * 3;
        float* v = new float[elem_count];
        for (uint32_t i = 0; i < elem_count; ++i)
        {
            v[i] = vertices[i] * scale;
        }
        btConvexHullShape* hull = new btConvexHullShape(v, vertex_count, sizeof(float) * 3);
        delete [] v;
        return hull;
    }

    void DeleteCollisionShape3D(HCollisionShape3D shape)
    {
        delete (btConvexShape*)shape;
    }

    bool g_ShapeGroupWarning = false;

    static btConvexShape* CloneShape(btConvexShape* shape)
    {
        switch(shape->getShapeType())
        {
            case SPHERE_SHAPE_PROXYTYPE:        return new btSphereShape(((btSphereShape*)shape)->getRadius()); break;
            case BOX_SHAPE_PROXYTYPE:           return new btBoxShape(((btBoxShape*)shape)->getHalfExtentsWithMargin()); break;
            case CAPSULE_SHAPE_PROXYTYPE:       return new btCapsuleShape(((btCapsuleShape*)shape)->getRadius(), 2.0f * ((btCapsuleShape*)shape)->getHalfHeight()); break;
            case CONVEX_HULL_SHAPE_PROXYTYPE:   return new btConvexHullShape((btScalar*)((btConvexHullShape*)shape)->getPoints(), ((btConvexHullShape*)shape)->getNumPoints()); break;
        }
        return shape;
    }

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data, HCollisionShape3D* shapes, uint32_t shape_count)
    {
        return NewCollisionObject3D(world, data, shapes, 0, 0, shape_count);
    }

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data, HCollisionShape3D* shapes,
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

        bool has_world_transform = world->m_GetWorldTransform != 0x0 && data.m_UserData;
        dmTransform::Transform world_transform;
        if (has_world_transform)
        {
            world->m_GetWorldTransform(data.m_UserData, world_transform);
        }

        float object_scale = 1.0f;
        if (has_world_transform)
        {
            object_scale = world_transform.GetUniformScale();
        }

        bool clone_shapes = world->m_AllowDynamicTransforms || object_scale != 1.0f;
        float scale = world->m_Context->m_Scale;
        btCompoundShape* compound_shape = new btCompoundShape(false);
        for (uint32_t i = 0; i < shape_count; ++i)
        {
            btConvexShape* shape = clone_shapes ? CloneShape((btConvexShape*)shapes[i]) : (btConvexShape*)shapes[i];

            if (translations && rotations)
            {
                const Vector3& trans = translations[i];
                const Quat& rot = rotations[i];

                btVector3 bt_trans;
                ToBt(trans, bt_trans, scale);
                btTransform transform(btQuaternion(rot.getX(), rot.getY(), rot.getZ(), rot.getW()), bt_trans);
                compound_shape->addChildShape(transform, shape);
            }
            else
            {
                compound_shape->addChildShape(btTransform::getIdentity(), shape);
            }
        }

        if (object_scale != 1.0f)
        {
            compound_shape->setLocalScaling(btVector3(object_scale, object_scale, object_scale));
        }

        btVector3 local_inertia(0.0f, 0.0f, 0.0f);
        if (data.m_Type == COLLISION_OBJECT_TYPE_DYNAMIC)
        {
            compound_shape->calculateLocalInertia(data.m_Mass, local_inertia);
        }

        btCollisionObject* collision_object = 0x0;
        if (data.m_Type != COLLISION_OBJECT_TYPE_TRIGGER)
        {
            MotionState* motion_state = new MotionState(world->m_Context, data.m_UserData, world->m_GetWorldTransform, world->m_SetWorldTransform);
            btRigidBody::btRigidBodyConstructionInfo rb_info(data.m_Mass, motion_state, compound_shape, local_inertia);
            rb_info.m_friction = data.m_Friction;
            rb_info.m_restitution = data.m_Restitution;
            rb_info.m_linearDamping = data.m_LinearDamping;
            rb_info.m_angularDamping = data.m_AngularDamping;
            btRigidBody* body = new btRigidBody(rb_info);
            float angular_factor = 1.0f;
            if (data.m_LockedRotation) {
                angular_factor = 0.0f;
            }
            body->setAngularFactor(angular_factor);
            switch (data.m_Type)
            {
            case COLLISION_OBJECT_TYPE_KINEMATIC:
                body->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
                break;
            case COLLISION_OBJECT_TYPE_STATIC:
                body->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
                break;
            default:
                break;
            }

            if (data.m_Enabled) {
                world->m_DynamicsWorld->addRigidBody(body, data.m_Group, data.m_Mask);
            }

            collision_object = body;
        }
        else
        {
            collision_object = new btGhostObject();
            btTransform world_t;
            if (has_world_transform)
            {
                Point3 position = Point3(world_transform.GetTranslation());
                Quat rotation = Quat(world_transform.GetRotation());
                btVector3 bt_pos;
                ToBt(position, bt_pos, world->m_Context->m_Scale);
                world_t = btTransform(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()), bt_pos);
            }
            else
            {
                world_t = btTransform::getIdentity();
            }
            collision_object->setWorldTransform(world_t);
            collision_object->setCollisionShape(compound_shape);
            collision_object->setCollisionFlags(collision_object->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            if (data.m_Enabled) {
                world->m_DynamicsWorld->addCollisionObject(collision_object, data.m_Group, data.m_Mask);
            }
        }
        collision_object->setUserPointer(data.m_UserData);
        CollisionObject3D* co = new CollisionObject3D();
        co->m_CollisionObject = collision_object;
        co->m_CollisionGroup = data.m_Group;
        co->m_CollisionMask = data.m_Mask;
        return co;
    }

    void DeleteCollisionObject3D(HWorld3D world, HCollisionObject3D collision_object)
    {
        CollisionObject3D* co = (CollisionObject3D*)collision_object;
        OverlapCacheRemove(&world->m_TriggerOverlaps, (uint64_t) co->m_CollisionObject);
        btCollisionObject* bt_co = co->m_CollisionObject;
        if (bt_co == 0x0)
            return;
        btCollisionShape* shape = bt_co->getCollisionShape();
        if (shape->isCompound())
        {
            delete shape;
        }
        btRigidBody* rigid_body = btRigidBody::upcast(bt_co);
        if (rigid_body != 0x0 && rigid_body->getMotionState())
            delete rigid_body->getMotionState();

        world->m_DynamicsWorld->removeCollisionObject(bt_co);
        delete bt_co;
        delete co;
    }

    HCollisionShape3D GetCollisionShape3D(HCollisionObject3D collision_object, uint32_t index)
    {
        btCollisionShape* shape = GetCollisionObject(collision_object)->getCollisionShape();

        if (shape->isCompound())
        {
            btCompoundShape* compound = (btCompoundShape*)shape;
            if (compound->getNumChildShapes() > index)
            {
                return compound->getChildShape(index);
            }
        }
        else if (index == 0)
        {
            return shape;
        }
        return 0;
    }

    uint32_t GetCollisionShapes3D(HCollisionObject3D collision_object, HCollisionShape3D* out_buffer, uint32_t buffer_size)
    {
        btCollisionShape* shape = GetCollisionObject(collision_object)->getCollisionShape();
        uint32_t n = 1;
        if (shape->isCompound())
        {
            btCompoundShape* compound = (btCompoundShape*)shape;
            n = compound->getNumChildShapes();
            for (uint32_t i = 0; i < n && i < buffer_size; ++i)
            {
                out_buffer[i] = compound->getChildShape(i);
            }
        }
        else if (buffer_size > 0)
        {
            out_buffer[0] = shape;
        }
        return n;
    }

    void SetCollisionObjectUserData3D(HCollisionObject3D collision_object, void* user_data)
    {
        GetCollisionObject(collision_object)->setUserPointer(user_data);
    }

    void* GetCollisionObjectUserData3D(HCollisionObject3D collision_object)
    {
        return GetCollisionObject(collision_object)->getUserPointer();
    }

    void ApplyForce3D(HContext3D context, HCollisionObject3D collision_object, const Vector3& force, const Point3& position)
    {
        btCollisionObject* bt_co = GetCollisionObject(collision_object);
        btRigidBody* rigid_body = btRigidBody::upcast(bt_co);
        if (rigid_body != 0x0 && !(rigid_body->isStaticOrKinematicObject()))
        {
            bool force_activate = false;
            rigid_body->activate(force_activate);
            btVector3 bt_force;
            ToBt(force, bt_force, context->m_Scale);
            btVector3 bt_position;
            ToBt(position, bt_position, context->m_Scale);
            rigid_body->applyForce(bt_force, bt_position - bt_co->getWorldTransform().getOrigin());
        }
    }

    Vector3 GetTotalForce3D(HContext3D context, HCollisionObject3D collision_object)
    {
        btRigidBody* rigid_body = btRigidBody::upcast(GetCollisionObject(collision_object));
        if (rigid_body != 0x0 && !(rigid_body->isStaticOrKinematicObject()))
        {
            const btVector3& bt_total_force = rigid_body->getTotalForce();
            Vector3 total_force;
            FromBt(bt_total_force, total_force, context->m_InvScale);
            return total_force;
        }
        else
        {
            return Vector3(0.0f, 0.0f, 0.0f);
        }
    }

    static Point3 GetWorldPosition(HContext3D context, btCollisionObject* collision_object)
    {
        const btVector3& bt_position = collision_object->getWorldTransform().getOrigin();
        Point3 position;
        FromBt(bt_position, position, context->m_InvScale);
        return position;
    }

    Point3 GetWorldPosition3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return GetWorldPosition(context, GetCollisionObject(collision_object));
    }

    static Quat GetWorldRotation(HContext3D context, btCollisionObject* collision_object)
    {
        btQuaternion rotation = collision_object->getWorldTransform().getRotation();
        return Quat(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
    }

    Quat GetWorldRotation3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return GetWorldRotation(context, GetCollisionObject(collision_object));
    }

    Vector3 GetLinearVelocity3D(HContext3D context, HCollisionObject3D collision_object)
    {
        Vector3 linear_velocity(0.0f, 0.0f, 0.0f);
        btRigidBody* body = btRigidBody::upcast(GetCollisionObject(collision_object));
        if (body != 0x0)
        {
            const btVector3& v = body->getLinearVelocity();
            FromBt(v, linear_velocity, context->m_InvScale);
        }
        return linear_velocity;
    }

    Vector3 GetAngularVelocity3D(HContext3D context, HCollisionObject3D collision_object)
    {
        Vector3 angular_velocity(0.0f, 0.0f, 0.0f);
        btRigidBody* body = btRigidBody::upcast(GetCollisionObject(collision_object));
        if (body != 0x0)
        {
            const btVector3& v = body->getAngularVelocity();
            FromBt(v, angular_velocity, 1.0f);
        }
        return angular_velocity;
    }

    void SetLinearVelocity3D(HContext3D context, HCollisionObject3D collision_object, const Vector3& velocity)
    {
        btRigidBody* body = btRigidBody::upcast(GetCollisionObject(collision_object));
        if (body != 0x0)
        {
            btVector3 bt_velocity;
            ToBt(velocity, bt_velocity, context->m_Scale);
            body->setLinearVelocity(bt_velocity);
        }
    }

    void SetAngularVelocity3D(HContext3D context, HCollisionObject3D collision_object, const Vector3& velocity)
    {
        btRigidBody* body = btRigidBody::upcast(GetCollisionObject(collision_object));
        if (body != 0x0)
        {
            btVector3 bt_velocity;
            ToBt(velocity, bt_velocity, 1.0f);
            body->setAngularVelocity(bt_velocity);
        }
    }

    bool IsEnabled3D(HCollisionObject3D collision_object)
    {
        btCollisionObject* co = GetCollisionObject(collision_object);
        return co->isInWorld();
    }

    void SetEnabled3D(HWorld3D world, HCollisionObject3D collision_object, bool enabled)
    {
        DM_PROFILE("SetEnabled3D");
        bool prev_enabled = IsEnabled3D(collision_object);
        // avoid multiple adds/removes
        if (prev_enabled == enabled)
            return;
        CollisionObject3D* co = (CollisionObject3D*)collision_object;
        btCollisionObject* bt_co = co->m_CollisionObject;
        if (enabled)
        {
            btRigidBody* body = btRigidBody::upcast(bt_co);
            if (body != 0x0)
            {
                // sync world transform
                if (world->m_GetWorldTransform != 0x0)
                {
                    dmTransform::Transform world_transform;
                    world->m_GetWorldTransform(body->getUserPointer(), world_transform);
                    Point3 position = Point3(world_transform.GetTranslation());
                    Quat rotation = Quat(world_transform.GetRotation());
                    btVector3 bt_position;
                    ToBt(position, bt_position, world->m_Context->m_Scale);
                    btTransform world_t(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()), bt_position);
                    body->setWorldTransform(world_t);
                }
                world->m_DynamicsWorld->addRigidBody(body, co->m_CollisionGroup, co->m_CollisionMask);
            }
            else
            {
                world->m_DynamicsWorld->addCollisionObject(bt_co, co->m_CollisionGroup, co->m_CollisionMask);
            }
        }
        else
        {
            btRigidBody* body = btRigidBody::upcast(bt_co);
            if (body != 0x0)
            {
                // Reset state
                body->clearForces();
                body->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
                body->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
                world->m_DynamicsWorld->removeRigidBody(body);
            }
            else
            {
                world->m_DynamicsWorld->removeCollisionObject(bt_co);
            }
        }
    }

    bool IsSleeping3D(HCollisionObject3D collision_object)
    {
        btCollisionObject* co = GetCollisionObject(collision_object);
        return co->getActivationState() == ISLAND_SLEEPING;
    }

    void Wakeup3D(HCollisionObject3D collision_object)
    {
        btCollisionObject* co = GetCollisionObject(collision_object);
        co->activate(true);
    }

    void SetLockedRotation3D(HCollisionObject3D collision_object, bool locked_rotation) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            if (locked_rotation) {
                body->setAngularFactor(0.0f);
                // Reset velocity
                body->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
            } else {
                body->setAngularFactor(1.0f);
            }
        }
    }

    float GetLinearDamping3D(HCollisionObject3D collision_object) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            return body->getLinearDamping();
        } else {
            return 0.0f;
        }
    }

    void SetLinearDamping3D(HCollisionObject3D collision_object, float linear_damping) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            body->setDamping(linear_damping, body->getAngularDamping());
        }
    }

    float GetAngularDamping3D(HCollisionObject3D collision_object) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            return body->getAngularDamping();
        } else {
            return 0.0f;
        }
    }

    void SetAngularDamping3D(HCollisionObject3D collision_object, float angular_damping) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            body->setDamping(body->getLinearDamping(), angular_damping);
        }
    }

    float GetMass3D(HCollisionObject3D collision_object) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0 && !body->isKinematicObject() && !body->isStaticObject()) {
            // Creating a dynamic body with 0 mass results in an error and the body is not created
            // Assert here just in case that would change
            assert(body->getInvMass() != 0.0f);
            return 1.0f / body->getInvMass();
        } else {
            return 0.0f;
        }
    }

	uint16_t GetGroup3D(HCollisionObject3D collision_object) {
		CollisionObject3D* co = (CollisionObject3D*) collision_object;
		uint16_t groupbit = co->m_CollisionGroup;
		return groupbit;
	}

    void SetGroup3D(HWorld3D world, HCollisionObject3D collision_object, uint16_t groupbit) {
		CollisionObject3D* co = (CollisionObject3D*) collision_object;
		btCollisionObject* bt_co = co->m_CollisionObject;

		bool enabled = IsEnabled3D(collision_object);

		if (!enabled) {
			// the collision object off the world. We just have to update the group property in CollisionObject3D. When the co is enabled, the group property will get applied.
			co->m_CollisionGroup = groupbit;
		} else {
			// we should remove and re-add the object to the 3d world to update group effectively
			btRigidBody* body = btRigidBody::upcast(bt_co); // rigidbody and collisionobject share the same pointer
			if (body != 0x0) {
				world->m_DynamicsWorld->removeRigidBody(body);
				co->m_CollisionGroup = groupbit;
				world->m_DynamicsWorld->addRigidBody(body, co->m_CollisionGroup, co->m_CollisionMask);
			} else {
				world->m_DynamicsWorld->removeCollisionObject(bt_co);
				co->m_CollisionGroup = groupbit;
				world->m_DynamicsWorld->addCollisionObject(bt_co, co->m_CollisionGroup, co->m_CollisionMask);
			}
		}
	}

	bool GetMaskBit3D(HCollisionObject3D collision_object, uint16_t groupbit) {
		CollisionObject3D* co = (CollisionObject3D*) collision_object;
		uint16_t maskbits = co->m_CollisionMask;
		return !!(maskbits & groupbit);
	}

    void SetMaskBit3D(HWorld3D world, HCollisionObject3D collision_object, uint16_t groupbit, bool boolvalue) {
		CollisionObject3D* co = (CollisionObject3D*) collision_object;
		btCollisionObject* bt_co = co->m_CollisionObject;

		bool enabled = IsEnabled3D(collision_object);

		//calculate new mask once
		uint16_t newmask = co->m_CollisionMask;
		if (boolvalue)
			newmask |= groupbit;
		else
			newmask &= ~groupbit;

		if (!enabled) {
			// the collision object off the world. We just have to update the mask property in CollisionObject3D. When the co is enabled, the mask property will get applied.
			co->m_CollisionMask = newmask;
		} else {
			// we should remove and re-add the object to the 3d world to update mask effectively
			btRigidBody* body = btRigidBody::upcast(bt_co); // rigidbody and collisionobject share the same pointer
			if (body != 0x0) {
				world->m_DynamicsWorld->removeRigidBody(body);
				co->m_CollisionMask = newmask;
				world->m_DynamicsWorld->addRigidBody(body, co->m_CollisionGroup, co->m_CollisionMask);
			} else {
				world->m_DynamicsWorld->removeCollisionObject(bt_co);
				co->m_CollisionMask = newmask;
				world->m_DynamicsWorld->addCollisionObject(bt_co, co->m_CollisionGroup, co->m_CollisionMask);
			}
		}
	}

    bool RequestRayCast3D(HWorld3D world, const RayCastRequest& request)
    {
        if (!world->m_RayCastRequests.Full())
        {
            // Verify that the ray is not 0-length
            if (lengthSqr(request.m_To - request.m_From) <= 0.0f)
            {
                dmLogWarning("Ray had 0 length when ray casting, ignoring request.");
                return false;
            }
            else
            {
                world->m_RayCastRequests.Push(request);
                return true;
            }
        }
        else
        {
            dmLogWarning("Ray cast query buffer is full (%d), ignoring request.", world->m_RayCastRequests.Capacity());
        }
        return false;
    }

    static int Sort_RayCastResponse(const dmPhysics::RayCastResponse* a, const dmPhysics::RayCastResponse* b)
    {
        float diff = a->m_Fraction - b->m_Fraction;
        if (diff == 0)
            return 0;
        return diff < 0 ? -1 : 1;
    }

    void RayCast3D(HWorld3D world, const RayCastRequest& request, dmArray<RayCastResponse>& results)
    {
        DM_PROFILE("RayCast3D");
        
        float scale = world->m_Context->m_Scale;
        btVector3 from;
        ToBt(request.m_From, from, scale);
        btVector3 to;
        ToBt(request.m_To, to, scale);
        
        if ((to - from).length2() <= 0.0f)
        {
            dmLogWarning("Ray had 0 length when ray casting after applying physics scale, ignoring request.");
            return;
        }

        float inv_scale = world->m_Context->m_InvScale;

        if (request.m_ReturnAllResults)
        {
            RayCastResultAllCallback3D result_callback(from, to, request.m_Mask, request.m_IgnoredUserData);
            world->m_DynamicsWorld->rayTest(from, to, result_callback);

            int size = result_callback.m_collisionObjects.size();

            if (results.Capacity() < size)
                results.SetCapacity(size);
            results.SetSize(size);

            for (int i = 0; i < size; ++i)
            {
                const btCollisionObject* co = result_callback.m_collisionObjects[i];
                const btVector3& point = result_callback.m_hitPointWorld[i];
                const btVector3& normal = result_callback.m_hitNormalWorld[i];
                const btScalar fraction = result_callback.m_hitFractions[i];

                ResponseFromRayCastResult(results[i], inv_scale, fraction, point, normal, co);
            }

            qsort(results.Begin(), results.Size(), sizeof(dmPhysics::RayCastResponse), (int(*)(const void*, const void*))Sort_RayCastResponse);
        }
        else
        {
            RayCastResultClosestCallback3D result_callback(from, to, request.m_Mask, request.m_IgnoredUserData);
            world->m_DynamicsWorld->rayTest(from, to, result_callback);

            if (result_callback.hasHit())
            {
                if (results.Full())
                    results.OffsetCapacity(1);
                results.SetSize(1);

                ResponseFromRayCastResult(results[0], inv_scale, result_callback.m_closestHitFraction, result_callback.m_hitPointWorld, result_callback.m_hitNormalWorld, result_callback.m_collisionObject);
            }
        }
    }

    void SetGravity3D(HWorld3D world, const Vector3& gravity)
    {
        HContext3D context = world->m_Context;
        ToBt(gravity, context->m_Gravity, context->m_Scale);
        world->m_DynamicsWorld->setGravity(btVector3(context->m_Gravity.getX(), context->m_Gravity.getY(), context->m_Gravity.getZ()));
    }

    Vector3 GetGravity3D(HWorld3D world)
    {
        HContext3D context = world->m_Context;
        Vector3 gravity;
        FromBt(context->m_Gravity, gravity, context->m_InvScale);
        return gravity;
    }

    void GetCollisionShapeRadius3D(HCollisionShape3D shape, float* radius)
    {
        btCollisionShape* bt_shape = (btCollisionShape*) shape;
        assert(bt_shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE);
        *radius = ((btSphereShape*) bt_shape)->getRadius();
    }

    void GetCollisionShapeCapsuleRadiusHeight3D(HCollisionShape3D shape, float* radius, float* half_height)
    {
        btCollisionShape* bt_shape = (btCollisionShape*) shape;
        assert(bt_shape->getShapeType() == CAPSULE_SHAPE_PROXYTYPE);
        btCapsuleShape* as_capsule = (btCapsuleShape*) bt_shape;
        *radius                    = as_capsule->getRadius();
        *half_height               = as_capsule->getHalfHeight();
    }

    void SetCollisionShapeRadius3D(HCollisionShape3D shape, float radius)
    {
        btCollisionShape* bt_shape = (btCollisionShape*) shape;
        assert(bt_shape->getShapeType() == SPHERE_SHAPE_PROXYTYPE);
        ((btSphereShape*) bt_shape)->setUnscaledRadius(radius);
    }

    void GetCollisionShapeHalfBoxExtents3D(HCollisionShape3D shape, float* xyz)
    {
        btCollisionShape* bt_shape = (btCollisionShape*) shape;
        assert(bt_shape->getShapeType() == BOX_SHAPE_PROXYTYPE);
        const btVector3& half_extents = ((btBoxShape*) bt_shape)->getHalfExtentsWithMargin();
        xyz[0] = half_extents.getX();
        xyz[1] = half_extents.getY();
        xyz[2] = half_extents.getZ();
    }

    void SetDebugCallbacks3D(HContext3D context, const DebugCallbacks& callbacks)
    {
        context->m_DebugCallbacks = callbacks;
    }

    void ReplaceShape3D(HCollisionObject3D object, HCollisionShape3D old_shape, HCollisionShape3D new_shape)
    {
        btCollisionObject* bt_object = GetCollisionObject(object);

        btCollisionShape* bt_current_shape = bt_object->getCollisionShape();

        if (bt_current_shape->isCompound())
        {
            btCompoundShape* compound_shape = (btCompoundShape*) bt_current_shape;
            uint32_t n = compound_shape->getNumChildShapes();
            for (uint32_t k = 0; k < n; ++k)
            {
                btCollisionShape* child = compound_shape->getChildShape(k);
                if (child == old_shape)
                {
                    btTransform t = compound_shape->getChildTransform(k);
                    compound_shape->removeChildShape(child);
                    compound_shape->addChildShape(t, (btConvexShape*)new_shape);
                    break;
                }
            }
        }
        else if (bt_current_shape == old_shape)
        {
            bt_object->setCollisionShape((btConvexShape*) new_shape);
        }
    }

    void ReplaceShape3D(HContext3D context, HCollisionShape3D old_shape, HCollisionShape3D new_shape)
    {
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
        {
            btCollisionObjectArray& objects = context->m_Worlds[i]->m_DynamicsWorld->getCollisionObjectArray();
            for (int j = 0; j < objects.size(); ++j)
            {
                btCollisionShape* shape = objects[j]->getCollisionShape();
                if (shape->isCompound())
                {
                    btCompoundShape* compound_shape = (btCompoundShape*)shape;
                    uint32_t n = compound_shape->getNumChildShapes();
                    for (uint32_t k = 0; k < n; ++k)
                    {
                        btCollisionShape* child = compound_shape->getChildShape(k);
                        if (child == old_shape)
                        {
                            btTransform t = compound_shape->getChildTransform(k);
                            compound_shape->removeChildShape(child);
                            compound_shape->addChildShape(t, (btConvexShape*)new_shape);
                            break;
                        }
                    }
                }
                else if (shape == old_shape)
                {
                    objects[j]->setCollisionShape((btConvexShape*)new_shape);
                    objects[j]->activate(true);
                }
            }
        }
    }
}
