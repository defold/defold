#include <stdint.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include <dmsdk/extension/extension.h>
#include <dmsdk/physics/physics.h>

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include "physics_3d.h"

namespace dmPhysics3D
{
    using namespace Vectormath::Aos;
    using namespace dmPhysics;

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
        short m_CollisionGroup;
        short m_CollisionMask;
    };

    static Vectormath::Aos::Point3 GetWorldPosition(HContext _context, btCollisionObject* collision_object);
    static Vectormath::Aos::Quat GetWorldRotation(HContext _context, btCollisionObject* collision_object);

    static btCollisionObject* GetCollisionObject(HCollisionObject co)
    {
        return ((CollisionObject3D*)co)->m_CollisionObject;
    }

    class MotionState : public btMotionState
    {
    public:
        MotionState(Context3D* context, void* user_data, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform)
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
                Vectormath::Aos::Point3 position = Vectormath::Aos::Point3(world_transform.GetTranslation());
                Vectormath::Aos::Quat rotation = Vectormath::Aos::Quat(world_transform.GetRotation());
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
        Context3D* m_Context;
        void* m_UserData;
        GetWorldTransformCallback m_GetWorldTransform;
        SetWorldTransformCallback m_SetWorldTransform;
    };

    Context3D::Context3D()
    : m_Worlds()
    , m_DebugCallbacks()
    , m_Gravity(0.0f, -10.0f, 0.0f)
    , m_Scale(1.0f)
    , m_InvScale(1.0f)
    , m_ContactImpulseLimit(0.0f)
    , m_TriggerEnterLimit(0.0f)
    , m_RayCastLimit(0)
    , m_TriggerOverlapCapacity(0)
    {

    }

    World3D::World3D(Context3D* context, const NewWorldParams& params)
    : m_DebugDraw(&context->m_DebugCallbacks)
    , m_Context(context)
    , m_TriggerOverlaps(context->m_TriggerOverlapCapacity)
    {
        m_CollisionConfiguration = new btDefaultCollisionConfiguration();
        m_Dispatcher = new btCollisionDispatcher(m_CollisionConfiguration);

        ///the maximum size of the collision world. Make sure objects stay within these boundaries
        ///Don't make the world AABB size too large, it will harm simulation quality and performance
        btVector3 world_aabb_min;
        ToBt(params.m_WorldMin, world_aabb_min, context->m_Scale);
        btVector3 world_aabb_max;
        ToBt(params.m_WorldMax, world_aabb_max, context->m_Scale);
        int maxProxies = 1024;
        m_OverlappingPairCache = new btAxisSweep3(world_aabb_min,world_aabb_max,maxProxies);

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

    struct ProcessRayCastResultCallback3D : public btCollisionWorld::ClosestRayResultCallback
    {
        ProcessRayCastResultCallback3D(const btVector3& from, const btVector3& to, uint16_t mask, void* ignored_user_data)
        : btCollisionWorld::ClosestRayResultCallback(from, to)
        , m_IgnoredUserData(ignored_user_data)
        {
            // *all* groups for now, bullet will test this against the colliding object's mask
            m_collisionFilterGroup = ~0;
            m_collisionFilterMask = mask;
        }

        virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult,bool normalInWorldSpace)
        {
            if (rayResult.m_collisionObject->getUserPointer() == m_IgnoredUserData)
                return 1.0f;
            else if (!rayResult.m_collisionObject->hasContactResponse())
                return 1.0f;
            else
                return btCollisionWorld::ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
        }

        void* m_IgnoredUserData;
    };

    static HContext NewContext3D(const NewContextParams& params)
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
        return context;
    }

    static void DeleteContext3D(HContext _context)
    {
        Context3D* context = (Context3D*)_context;
        if (!context->m_Worlds.Empty())
        {
            dmLogWarning("Deleting %ud 3d worlds since the context is deleted.", context->m_Worlds.Size());
            for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
                delete context->m_Worlds[i];
        }
        delete context;
    }

    static HWorld NewWorld3D(HContext _context, const NewWorldParams& params)
    {
        Context3D* context = (Context3D*)_context;
        if (context->m_Worlds.Full())
        {
            dmLogError("%s", "Physics world buffer full, world could not be created.");
            return 0x0;
        }
        World3D* world = new World3D(context, params);
        context->m_Worlds.Push(world);
        return world;
    }

    static void DeleteWorld3D(HContext _context, HWorld world)
    {
        Context3D* context = (Context3D*)_context;
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
            if (context->m_Worlds[i] == world)
                context->m_Worlds.EraseSwap(i);
        delete (World3D*)world;
    }

    static void SetDrawDebug3D(HWorld _world, bool draw_debug)
    {
        World3D* world = (World3D*)_world;
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

    static void UpdateOverlapCache(OverlapCache* cache, HContext context, btDispatcher* dispatcher, const StepWorldContext& step_context);

    static void StepWorld3D(HWorld _world, const StepWorldContext& step_context)
    {
        World3D* world = (World3D*)_world;

        float dt = step_context.m_DT;
        Context3D* context = world->m_Context;
        float scale = context->m_Scale;
        // Epsilon defining what transforms are considered noise and not
        // Values are picked by inspection, current rot value is roughly equivalent to 1 degree
        const float POS_EPSILON = 0.00005f * scale;
        const float ROT_EPSILON = 0.00007f;
        // Update all trigger transforms before physics world step
        if (world->m_GetWorldTransform != 0x0)
        {
            DM_PROFILE(Physics, "UpdateTriggers");
            int collision_object_count = world->m_DynamicsWorld->getNumCollisionObjects();
            btCollisionObjectArray& collision_objects = world->m_DynamicsWorld->getCollisionObjectArray();
            for (int i = 0; i < collision_object_count; ++i)
            {
                btCollisionObject* collision_object = collision_objects[i];
                if (collision_object->getInternalType() == btCollisionObject::CO_GHOST_OBJECT || collision_object->isKinematicObject())
                {
                    Point3 old_position = GetWorldPosition(context, collision_object);
                    Quat old_rotation = GetWorldRotation(context, collision_object);
                    dmTransform::Transform world_transform;
                    (*world->m_GetWorldTransform)(collision_object->getUserPointer(), world_transform);
                    Vectormath::Aos::Point3 position = Vectormath::Aos::Point3(world_transform.GetTranslation());
                    Vectormath::Aos::Quat rotation = Vectormath::Aos::Quat(world_transform.GetRotation());
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
            }
        }

        {
            DM_PROFILE(Physics, "StepSimulation");
            // Step simulation
            // TODO: Max substeps = 1 for now...
            world->m_DynamicsWorld->stepSimulation(dt, 1);
        }

        // Handle ray cast requests
        uint32_t size = world->m_RayCastRequests.Size();
        if (size > 0)
        {
            DM_PROFILE(Physics, "RayCasts");
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
                ProcessRayCastResultCallback3D result_callback(from, to, request.m_Mask, request.m_IgnoredUserData);
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
            DM_PROFILE(Physics, "CollisionCallbacks");
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

                if (collision_callback != 0x0 && requests_collision_callbacks)
                {
                    requests_collision_callbacks = collision_callback(object_a->getUserPointer(), object_a->getBroadphaseHandle()->m_collisionFilterGroup, object_b->getUserPointer(), object_b->getBroadphaseHandle()->m_collisionFilterGroup, step_context.m_CollisionUserData);
                }

                if (contact_point_callback != 0x0)
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
                        Vectormath::Aos::Vector3 vel_a(0.0f, 0.0f, 0.0f);
                        if (body_a)
                        {
                            const btVector3& v = body_a->getLinearVelocity();
                            FromBt(v, vel_a, inv_scale);
                        }
                        Vectormath::Aos::Vector3 vel_b(0.0f, 0.0f, 0.0f);
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

    static void UpdateOverlapCache(OverlapCache* cache, HContext _context, btDispatcher* dispatcher, const StepWorldContext& step_context)
    {
        Context3D* context = (Context3D*)_context;

        DM_PROFILE(Physics, "TriggerCallbacks");
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

            if (!object_a->isActive() || !object_b->isActive())
                continue;

            if (btGhostObject::upcast(object_a) != 0x0 || btGhostObject::upcast(object_b) != 0x0)
            {
                float max_distance = 0.0f;
                int contact_count = contact_manifold->getNumContacts();
                for (int j = 0; j < contact_count; ++j)
                {
                    const btManifoldPoint& point = contact_manifold->getContactPoint(j);
                    max_distance = dmMath::Max(max_distance, point.getDistance());
                }
                if (max_distance >= context->m_TriggerEnterLimit)
                {
                    float* f = object_a->getWorldTransform().getOrigin().m_floats;
                    f = object_b->getWorldTransform().getOrigin().m_floats;
                    add_data.m_ObjectA = object_a;
                    add_data.m_UserDataA = object_a->getUserPointer();
                    add_data.m_ObjectB = object_b;
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

    static HCollisionShape NewSphereShape3D(HContext _context, float radius)
    {
        Context3D* context = (Context3D*)_context;
        return new btSphereShape(context->m_Scale * radius);
    }

    static HCollisionShape NewBoxShape3D(HContext _context, const Vector3& half_extents)
    {
        Context3D* context = (Context3D*)_context;
        btVector3 dims;
        ToBt(half_extents, dims, context->m_Scale);
        return new btBoxShape(dims);
    }

    static HCollisionShape NewCapsuleShape3D(HContext _context, float radius, float height)
    {
        Context3D* context = (Context3D*)_context;
        float scale = context->m_Scale;
        return new btCapsuleShape(scale * radius, scale * height);
    }

    static HCollisionShape NewConvexHullShape3D(HContext _context, const float* vertices, uint32_t vertex_count)
    {
        Context3D* context = (Context3D*)_context;
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

    static void DeleteCollisionShape3D(HCollisionShape shape)
    {
        delete (btConvexShape*)shape;
    }

    static HCollisionObject NewCollisionObjectTransform3D(HWorld _world, const CollisionObjectData& data, HCollisionShape* shapes,
                                            Vectormath::Aos::Vector3* translations, Vectormath::Aos::Quat* rotations,
                                            uint32_t shape_count)
    {
        World3D* world = (World3D*)_world;
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

        float scale = world->m_Context->m_Scale;
        btCompoundShape* compound_shape = new btCompoundShape(false);
        for (uint32_t i = 0; i < shape_count; ++i)
        {
            if (translations && rotations)
            {
                const Vectormath::Aos::Vector3& trans = translations[i];
                const Vectormath::Aos::Quat& rot = rotations[i];

                btVector3 bt_trans;
                ToBt(trans, bt_trans, scale);
                btTransform transform(btQuaternion(rot.getX(), rot.getY(), rot.getZ(), rot.getW()), bt_trans);
                compound_shape->addChildShape(transform, (btConvexShape*)shapes[i]);
            }
            else
            {
                compound_shape->addChildShape(btTransform::getIdentity(), (btConvexShape*)shapes[i]);
            }
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
            if (world->m_GetWorldTransform != 0x0)
            {
                dmTransform::Transform world_transform;
                world->m_GetWorldTransform(data.m_UserData, world_transform);
                Vectormath::Aos::Point3 position = Vectormath::Aos::Point3(world_transform.GetTranslation());
                Vectormath::Aos::Quat rotation = Vectormath::Aos::Quat(world_transform.GetRotation());
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

    static HCollisionObject NewCollisionObject3D(HWorld _world, const CollisionObjectData& data, HCollisionShape* shapes, uint32_t shape_count)
    {
        return NewCollisionObjectTransform3D(_world, data, shapes, 0, 0, shape_count);
    }

    static void DeleteCollisionObject3D(HWorld _world, HCollisionObject collision_object)
    {
        World3D* world = (World3D*)_world;
        CollisionObject3D* co = (CollisionObject3D*)collision_object;
        OverlapCacheRemove(&world->m_TriggerOverlaps, co->m_CollisionObject);
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

    static uint32_t GetCollisionShapes3D(HCollisionObject collision_object, HCollisionShape* out_buffer, uint32_t buffer_size)
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

    static void SetCollisionObjectUserData3D(HCollisionObject collision_object, void* user_data)
    {
        GetCollisionObject(collision_object)->setUserPointer(user_data);
    }

    static void* GetCollisionObjectUserData3D(HCollisionObject collision_object)
    {
        return GetCollisionObject(collision_object)->getUserPointer();
    }

    static void ApplyForce3D(HContext _context, HCollisionObject collision_object, const Vector3& force, const Point3& position)
    {
        Context3D* context = (Context3D*)_context;
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

    static Vector3 GetTotalForce3D(HContext _context, HCollisionObject collision_object)
    {
        Context3D* context = (Context3D*)_context;
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

    static Vectormath::Aos::Point3 GetWorldPosition(HContext _context, btCollisionObject* collision_object)
    {
        Context3D* context = (Context3D*)_context;
        const btVector3& bt_position = collision_object->getWorldTransform().getOrigin();
        Vectormath::Aos::Point3 position;
        FromBt(bt_position, position, context->m_InvScale);
        return position;
    }

    static Vectormath::Aos::Point3 GetWorldPosition3D(HContext _context, HCollisionObject collision_object)
    {
        return GetWorldPosition(_context, GetCollisionObject(collision_object));
    }

    static static Vectormath::Aos::Quat GetWorldRotation(HContext _context, btCollisionObject* collision_object)
    {
        btQuaternion rotation = collision_object->getWorldTransform().getRotation();
        return Vectormath::Aos::Quat(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
    }

    static Vectormath::Aos::Quat GetWorldRotation3D(HContext _context, HCollisionObject collision_object)
    {
        return GetWorldRotation(_context, GetCollisionObject(collision_object));
    }

    static Vectormath::Aos::Vector3 GetLinearVelocity3D(HContext _context, HCollisionObject collision_object)
    {
        Context3D* context = (Context3D*)_context;
        Vectormath::Aos::Vector3 linear_velocity(0.0f, 0.0f, 0.0f);
        btRigidBody* body = btRigidBody::upcast(GetCollisionObject(collision_object));
        if (body != 0x0)
        {
            const btVector3& v = body->getLinearVelocity();
            FromBt(v, linear_velocity, context->m_InvScale);
        }
        return linear_velocity;
    }

    static Vectormath::Aos::Vector3 GetAngularVelocity3D(HContext _context, HCollisionObject collision_object)
    {
        Context3D* context = (Context3D*)_context;
        Vectormath::Aos::Vector3 angular_velocity(0.0f, 0.0f, 0.0f);
        btRigidBody* body = btRigidBody::upcast(GetCollisionObject(collision_object));
        if (body != 0x0)
        {
            const btVector3& v = body->getAngularVelocity();
            FromBt(v, angular_velocity, 1.0f);
        }
        return angular_velocity;
    }

    static bool IsEnabled3D(HCollisionObject collision_object)
    {
        btCollisionObject* co = GetCollisionObject(collision_object);
        return co->isInWorld();
    }

    static void SetEnabled3D(HWorld _world, HCollisionObject collision_object, bool enabled)
    {
        World3D* world = (World3D*)_world;
        DM_PROFILE(Physics, "SetEnabled");
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
                    Vectormath::Aos::Point3 position = Vectormath::Aos::Point3(world_transform.GetTranslation());
                    Vectormath::Aos::Quat rotation = Vectormath::Aos::Quat(world_transform.GetRotation());
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

    static bool IsSleeping3D(HCollisionObject collision_object)
    {
        btCollisionObject* co = GetCollisionObject(collision_object);
        return co->getActivationState() == ISLAND_SLEEPING;
    }

    static void SetLockedRotation3D(HCollisionObject collision_object, bool locked_rotation) {
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

    static float GetLinearDamping3D(HCollisionObject collision_object) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            return body->getLinearDamping();
        } else {
            return 0.0f;
        }
    }

    static void SetLinearDamping3D(HCollisionObject collision_object, float linear_damping) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            body->setDamping(linear_damping, body->getAngularDamping());
        }
    }

    static float GetAngularDamping3D(HCollisionObject collision_object) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            return body->getAngularDamping();
        } else {
            return 0.0f;
        }
    }

    static void SetAngularDamping3D(HCollisionObject collision_object, float angular_damping) {
        btCollisionObject* co = GetCollisionObject(collision_object);
        btRigidBody* body = btRigidBody::upcast(co);
        if (body != 0x0) {
            body->setDamping(body->getLinearDamping(), angular_damping);
        }
    }

    static float GetMass3D(HCollisionObject collision_object) {
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

    static void RequestRayCast3D(HWorld _world, const RayCastRequest& request)
    {
        World3D* world = (World3D*)_world;
        if (!world->m_RayCastRequests.Full())
        {
            // Verify that the ray is not 0-length
            if (Vectormath::Aos::lengthSqr(request.m_To - request.m_From) <= 0.0f)
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
            dmLogWarning("Ray cast query buffer is full (%d), ignoring request.", world->m_RayCastRequests.Capacity());
        }
    }

    static void SetDebugCallbacks3D(HContext _context, const DebugCallbacks& callbacks)
    {
        Context3D* context = (Context3D*)_context;
        context->m_DebugCallbacks = callbacks;
    }

    static void ReplaceShape3D(HContext _context, HCollisionShape old_shape, HCollisionShape new_shape)
    {
        Context3D* context = (Context3D*)_context;
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


static dmExtension::Result AppInitializeExtension(dmExtension::AppParams* params)
{
    dmLogWarning("Registered dmBullet3D Extension\n");

    dmPhysics::ExtensionDesc* desc = (dmPhysics::ExtensionDesc*) malloc( sizeof(dmPhysics::ExtensionDesc) );

    desc->NewContext     = dmPhysics3D::NewContext3D;     //)(const NewContextParams& params);
    desc->DeleteContext  = dmPhysics3D::DeleteContext3D;      //)(HContext context);

    desc->NewWorld       = dmPhysics3D::NewWorld3D;       //)(HContext context, const NewWorldParams& params);
    desc->DeleteWorld    = dmPhysics3D::DeleteWorld3D;        //)(HContext context, HWorld world);
    desc->StepWorld      = dmPhysics3D::StepWorld3D;        //)(HWorld world, const StepWorldContext& context);
    desc->SetDrawDebug   = dmPhysics3D::SetDrawDebug3D;     //)(HWorld world, bool draw_debug);

    desc->NewSphereShape = dmPhysics3D::NewSphereShape3D;     //)(HContext context, float radius);

    desc->NewBoxShape    = dmPhysics3D::NewBoxShape3D;        //)(HContext context, const Vectormath::Aos::Vector3& half_extents);

    desc->NewCapsuleShape    = 0;//dmPhysics3D::NewCapsuleShape3D;        //)(HContext context, float radius, float height);

    desc->NewConvexHullShape = dmPhysics3D::NewConvexHullShape3D; //;dmPhysics3D::NewConvexHullShape3D;     //)(HContext context, const float* vertices, uint32_t vertex_count);

    // Specific for the 2d implementation
    desc->NewHullSet2D       = 0;//dmPhysics3D::NewHullSet3D;       //)(HContext context, const float* vertices, uint32_t vertex_count, const HullDesc* hulls, uint32_t hull_count);
    desc->DeleteHullSet2D    = 0;//dmPhysics3D::DeleteHullSet3D;        //)(HHullSet2D hull_set);
    desc->NewGridShape2D     = 0;//dmPhysics3D::NewGridShape3D;     //)(HContext context, HHullSet2D hull_set, const Vectormath::Aos::Point3& position, uint32_t cell_width, uint32_t cell_height, uint32_t row_count, uint32_t column_count);
    desc->SetGridShapeHull   = 0;//dmPhysics3D::SetGridShapeHull3D;       //)(HCollisionObject collision_object, uint32_t shape_index, uint32_t row, uint32_t column, uint32_t hull, HullFlags flags);

    desc->SetCollisionObjectFilter   = 0;//dmPhysics3D::SetCollisionObjectFilter3D;       //)(HCollisionObject collision_object, uint32_t shape, uint32_t child, uint16_t group, uint16_t mask);
    desc->DeleteCollisionShape       = dmPhysics3D::DeleteCollisionShape3D;       //)(HCollisionShape shape);

    desc->NewCollisionObject         = dmPhysics3D::NewCollisionObject3D;     //)(HWorld world, const CollisionObjectData& data, HCollisionShape* shapes, uint32_t shape_count);
    desc->NewCollisionObjectTransform= dmPhysics3D::NewCollisionObjectTransform3D;        //)(HWorld world, const CollisionObjectData& data, HCollisionShape* shapes, Vectormath::Aos::Vector3* translations, Vectormath::Aos::Quat* rotations, uint32_t shape_count);

    desc->DeleteCollisionObject      = dmPhysics3D::DeleteCollisionObject3D;      //)(HWorld world, HCollisionObject collision_object);
    desc->GetCollisionShapes         = dmPhysics3D::GetCollisionShapes3D;     //)(HCollisionObject collision_object, HCollisionShape* out_buffer, uint32_t buffer_size);
    desc->SetCollisionObjectUserData = dmPhysics3D::SetCollisionObjectUserData3D;     //)(HCollisionObject collision_object, void* user_data);
    desc->GetCollisionObjectUserData = dmPhysics3D::GetCollisionObjectUserData3D;     //)(HCollisionObject collision_object);
    desc->ApplyForce                 = dmPhysics3D::ApplyForce3D;     //)(HContext context, HCollisionObject collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position);

    desc->GetTotalForce          = dmPhysics3D::GetTotalForce3D;      //)(HContext context, HCollisionObject collision_object);
    desc->GetWorldPosition       = dmPhysics3D::GetWorldPosition3D;       //)(HContext context, HCollisionObject collision_object);
    desc->GetWorldRotation       = dmPhysics3D::GetWorldRotation3D;       //)(HContext context, HCollisionObject collision_object);
    desc->GetLinearVelocity      = dmPhysics3D::GetLinearVelocity3D;      //)(HContext context, HCollisionObject collision_object);
    desc->GetAngularVelocity     = dmPhysics3D::GetAngularVelocity3D;     //)(HContext context, HCollisionObject collision_object);

    desc->IsEnabled              = dmPhysics3D::IsEnabled3D;      //)(HCollisionObject collision_object);
    desc->SetEnabled             = dmPhysics3D::SetEnabled3D;     //)(HWorld world, HCollisionObject collision_object, bool enabled);
    desc->IsSleeping             = dmPhysics3D::IsSleeping3D;     //)(HCollisionObject collision_object);
    desc->SetLockedRotation      = dmPhysics3D::SetLockedRotation3D;      //)(HCollisionObject collision_object, bool locked_rotation);
    desc->GetLinearDamping       = dmPhysics3D::GetLinearDamping3D;       //)(HCollisionObject collision_object);
    desc->SetLinearDamping       = dmPhysics3D::SetLinearDamping3D;       //)(HCollisionObject collision_object, float linear_damping);
    desc->GetAngularDamping      = dmPhysics3D::GetAngularDamping3D;      //)(HCollisionObject collision_object);
    desc->SetAngularDamping      = dmPhysics3D::SetAngularDamping3D;      //)(HCollisionObject collision_object, float angular_damping);
    desc->GetMass                = dmPhysics3D::GetMass3D;        //)(HCollisionObject collision_object);
    desc->RequestRayCast         = dmPhysics3D::RequestRayCast3D;     //)(HWorld world, const RayCastRequest& request);
    desc->SetDebugCallbacks      = dmPhysics3D::SetDebugCallbacks3D;      //)(HContext context, const DebugCallbacks& callbacks);
    desc->ReplaceShape           = dmPhysics3D::ReplaceShape3D;       //)(HContext context, HCollisionShape old_shape, HCollisionShape new_shape);

    // TODO: only register once! (e.e handle hot reload)
    dmPhysics::Register(desc, sizeof(dmPhysics::ExtensionDesc), "dmBullet3D", true);

    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}


DM_DECLARE_EXTENSION(dmBullet3D, "dmBulletD", AppInitializeExtension, AppFinalizeExtension, InitializeExtension, 0, 0, FinalizeExtension);
