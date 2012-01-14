#include <stdint.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/profile.h>

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

#include "physics_3d.h"

namespace dmPhysics
{
    using namespace Vectormath::Aos;

    class MotionState : public btMotionState
    {
    public:
        MotionState(void* user_data, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform)
        : m_UserData(user_data)
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
                Vectormath::Aos::Point3 position;
                Vectormath::Aos::Quat rotation;
                m_GetWorldTransform(m_UserData, position, rotation);
                world_trans.setOrigin(btVector3(position.getX(), position.getY(), position.getZ()));
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

                Point3 pos = Point3(bt_pos.getX(), bt_pos.getY(), bt_pos.getZ());
                Quat rot = Quat(bt_rot.getX(), bt_rot.getY(), bt_rot.getZ(), bt_rot.getW());
                m_SetWorldTransform(m_UserData, pos, rot);
            }
        }

    protected:
        void* m_UserData;
        GetWorldTransformCallback m_GetWorldTransform;
        SetWorldTransformCallback m_SetWorldTransform;
    };

    Context3D::Context3D()
    : m_Worlds()
    , m_DebugCallbacks()
    , m_Gravity(0.0f, -10.0f, 0.0f)
    , m_Socket(0)
    {

    }

    World3D::World3D(HContext3D context, const NewWorldParams& params)
    : m_DebugDraw(&context->m_DebugCallbacks)
    , m_Context(context)
    {
        m_CollisionConfiguration = new btDefaultCollisionConfiguration();
        m_Dispatcher = new btCollisionDispatcher(m_CollisionConfiguration);

        ///the maximum size of the collision world. Make sure objects stay within these boundaries
        ///Don't make the world AABB size too large, it will harm simulation quality and performance
        btVector3 world_aabb_min(params.m_WorldMin.getX(), params.m_WorldMin.getY(), params.m_WorldMin.getZ());
        btVector3 world_aabb_max(params.m_WorldMax.getX(), params.m_WorldMax.getY(), params.m_WorldMax.getZ());
        int     maxProxies = 1024;
        m_OverlappingPairCache = new btAxisSweep3(world_aabb_min,world_aabb_max,maxProxies);

        m_Solver = new btSequentialImpulseConstraintSolver;

        m_DynamicsWorld = new btDiscreteDynamicsWorld(m_Dispatcher, m_OverlappingPairCache, m_Solver, m_CollisionConfiguration);
        m_DynamicsWorld->setGravity(btVector3(context->m_Gravity.getX(), context->m_Gravity.getY(), context->m_Gravity.getZ()));
        m_DynamicsWorld->setDebugDrawer(&m_DebugDraw);

        m_GetWorldTransform = params.m_GetWorldTransformCallback;
        m_SetWorldTransform = params.m_SetWorldTransformCallback;

        m_RayCastRequests.SetCapacity(128);
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

    HContext3D NewContext3D(const NewContextParams& params)
    {
        Context3D* context = new Context3D();
        context->m_Gravity = params.m_Gravity;
        context->m_Worlds.SetCapacity(params.m_WorldCount);
        dmMessage::Result result = dmMessage::NewSocket(PHYSICS_SOCKET_NAME, &context->m_Socket);
        if (result != dmMessage::RESULT_OK)
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

    void DeleteWorld3D(HContext3D context, HWorld3D world)
    {
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
            if (context->m_Worlds[i] == world)
                context->m_Worlds.EraseSwap(i);
        delete world;
    }

    void DrawDebug3D(HWorld3D world)
    {
        world->m_DynamicsWorld->debugDrawWorld();
    }

    void StepWorld3D(HWorld3D world, const StepWorldContext& context)
    {
        float dt = context.m_DT;
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
                    Point3 old_position = GetWorldPosition3D(collision_object);
                    Quat old_rotation = GetWorldRotation3D(collision_object);
                    Vectormath::Aos::Point3 position;
                    Vectormath::Aos::Quat rotation;
                    world->m_GetWorldTransform(collision_object->getUserPointer(), position, rotation);
                    btTransform world_transform(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()), btVector3(position.getX(), position.getY(), position.getZ()));
                    collision_object->setWorldTransform(world_transform);
                    if ((distSqr(old_position, position) > 0.0f || lengthSqr(Vector4(rotation - old_rotation)) > 0.0f))
                    {
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
                if (context.m_RayCastCallback == 0x0)
                {
                    dmLogWarning("Ray cast requested without any response callback, skipped.");
                    continue;
                }
                btVector3 from(request.m_From.getX(), request.m_From.getY(), request.m_From.getZ());
                btVector3 to(request.m_To.getX(), request.m_To.getY(), request.m_To.getZ());
                ProcessRayCastResultCallback3D result_callback(from, to, request.m_Mask, request.m_IgnoredUserData);
                world->m_DynamicsWorld->rayTest(from, to, result_callback);
                RayCastResponse response;
                response.m_Hit = result_callback.hasHit() ? 1 : 0;
                response.m_Fraction = result_callback.m_closestHitFraction;
                response.m_Position = Vectormath::Aos::Point3(result_callback.m_hitPointWorld.getX(), result_callback.m_hitPointWorld.getY(), result_callback.m_hitPointWorld.getZ());
                response.m_Normal = Vectormath::Aos::Vector3(result_callback.m_hitNormalWorld.getX(), result_callback.m_hitNormalWorld.getY(), result_callback.m_hitNormalWorld.getZ());
                if (result_callback.m_collisionObject != 0x0)
                {
                    response.m_CollisionObjectUserData = result_callback.m_collisionObject->getUserPointer();
                    response.m_CollisionObjectGroup = result_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
                }
                context.m_RayCastCallback(response, request, context.m_RayCastUserData);
            }
            world->m_RayCastRequests.SetSize(0);
        }

        // Report collisions
        bool requests_collision_callbacks = true;
        bool requests_contact_callbacks = true;

        CollisionCallback collision_callback = context.m_CollisionCallback;
        ContactPointCallback contact_point_callback = context.m_ContactPointCallback;
        btDispatcher* dispatcher = world->m_DynamicsWorld->getDispatcher();
        if (collision_callback != 0x0 || contact_point_callback != 0x0)
        {
            DM_PROFILE(Physics, "CollisionCallbacks");
            int num_manifolds = dispatcher->getNumManifolds();
            for (int i = 0; i < num_manifolds; ++i)
            {
                btPersistentManifold* contact_manifold = dispatcher->getManifoldByIndexInternal(i);
                btCollisionObject* object_a = static_cast<btCollisionObject*>(contact_manifold->getBody0());
                btCollisionObject* object_b = static_cast<btCollisionObject*>(contact_manifold->getBody1());

                if (!object_a->isActive() && !object_b->isActive())
                    continue;

                if (collision_callback != 0x0 && requests_collision_callbacks)
                {
                    requests_collision_callbacks = collision_callback(object_a->getUserPointer(), object_a->getBroadphaseHandle()->m_collisionFilterGroup, object_b->getUserPointer(), object_b->getBroadphaseHandle()->m_collisionFilterGroup, context.m_CollisionUserData);
                    if (!requests_collision_callbacks && !requests_contact_callbacks)
                        return;
                }

                if (contact_point_callback != 0x0)
                {
                    int num_contacts = contact_manifold->getNumContacts();
                    for (int j = 0; j < num_contacts && requests_contact_callbacks; ++j)
                    {
                        btManifoldPoint& pt = contact_manifold->getContactPoint(j);
                        btRigidBody* body_a = btRigidBody::upcast(object_a);
                        btRigidBody* body_b = btRigidBody::upcast(object_b);
                        ContactPoint point;
                        const btVector3& pt_a = pt.getPositionWorldOnA();
                        point.m_PositionA = Vectormath::Aos::Point3(pt_a.getX(), pt_a.getY(), pt_a.getZ());
                        point.m_UserDataA = object_a->getUserPointer();
                        point.m_GroupA = object_a->getBroadphaseHandle()->m_collisionFilterGroup;
                        if (body_a)
                            point.m_InvMassA = body_a->getInvMass();
                        const btVector3& pt_b = pt.getPositionWorldOnB();
                        point.m_PositionB = Vectormath::Aos::Point3(pt_b.getX(), pt_b.getY(), pt_b.getZ());
                        point.m_UserDataB = object_b->getUserPointer();
                        point.m_GroupB = object_b->getBroadphaseHandle()->m_collisionFilterGroup;
                        if (body_b)
                            point.m_InvMassB = body_b->getInvMass();
                        const btVector3& normal = pt.m_normalWorldOnB;
                        point.m_Normal = -Vectormath::Aos::Vector3(normal.getX(), normal.getY(), normal.getZ());
                        point.m_Distance = -pt.getDistance();
                        point.m_AppliedImpulse = pt.getAppliedImpulse();
                        Vectormath::Aos::Vector3 vel_a(0.0f, 0.0f, 0.0f);
                        if (body_a)
                        {
                            const btVector3& v = body_a->getLinearVelocity();
                            vel_a = Vectormath::Aos::Vector3(v.getX(), v.getY(), v.getZ());
                        }
                        Vectormath::Aos::Vector3 vel_b(0.0f, 0.0f, 0.0f);
                        if (body_b)
                        {
                            const btVector3& v = body_b->getLinearVelocity();
                            vel_b = Vectormath::Aos::Vector3(v.getX(), v.getY(), v.getZ());
                        }
                        point.m_RelativeVelocity = vel_a - vel_b;
                        requests_contact_callbacks = contact_point_callback(point, context.m_ContactPointUserData);
                        if (!requests_collision_callbacks && !requests_contact_callbacks)
                            return;
                    }
                }
            }
            // check ghosts
            if (collision_callback != 0x0 && requests_collision_callbacks)
            {
                int num_collision_objects = world->m_DynamicsWorld->getNumCollisionObjects();
                btCollisionObjectArray& collision_objects = world->m_DynamicsWorld->getCollisionObjectArray();
                for (int i = 0; i < num_collision_objects; ++i)
                {
                    btCollisionObject* collision_object = collision_objects[i];
                    btGhostObject* ghost_object = btGhostObject::upcast(collision_object);
                    if (ghost_object != 0x0)
                    {
                        int num_overlaps = ghost_object->getNumOverlappingObjects();
                        for (int j = 0; j < num_overlaps; ++j)
                        {
                            btCollisionObject* collidee = ghost_object->getOverlappingObject(j);
                            requests_collision_callbacks = collision_callback(ghost_object->getUserPointer(), ghost_object->getBroadphaseHandle()->m_collisionFilterGroup, collidee->getUserPointer(), collidee->getBroadphaseHandle()->m_collisionFilterGroup, context.m_CollisionUserData);
                            if (!requests_collision_callbacks)
                                return;
                        }
                    }
                }
            }
        }
    }

    HCollisionShape3D NewSphereShape3D(float radius)
    {
        return new btSphereShape(radius);
    }

    HCollisionShape3D NewBoxShape3D(const Vector3& half_extents)
    {
        return new btBoxShape(btVector3(half_extents.getX(), half_extents.getY(), half_extents.getZ()));
    }

    HCollisionShape3D NewCapsuleShape3D(float radius, float height)
    {
        return new btCapsuleShape(radius, height);
    }

    HCollisionShape3D NewConvexHullShape3D(const float* vertices, uint32_t vertex_count)
    {
        assert(sizeof(btScalar) == sizeof(float));
        return new btConvexHullShape(vertices, vertex_count / 3, sizeof(float) * 3);
    }

    void DeleteCollisionShape3D(HCollisionShape3D shape)
    {
        delete (btConvexShape*)shape;
    }

    bool g_ShapeGroupWarning = false;

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data, HCollisionShape3D* shapes, uint32_t shape_count)
    {
        return NewCollisionObject3D(world, data, shapes, 0, 0, shape_count);
    }

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data, HCollisionShape3D* shapes,
                                            Vectormath::Aos::Vector3* translations, Vectormath::Aos::Quat* rotations,
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

        btCompoundShape* compound_shape = new btCompoundShape(false);
        for (uint32_t i = 0; i < shape_count; ++i)
        {
            if (translations && rotations)
            {
                const Vectormath::Aos::Vector3& trans = translations[i];
                const Vectormath::Aos::Quat& rot = rotations[i];

                btTransform transform(btQuaternion(rot.getX(), rot.getY(), rot.getZ(), rot.getW()),
                                      btVector3(trans.getX(), trans.getY(), trans.getZ()));
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
            MotionState* motion_state = new MotionState(data.m_UserData, world->m_GetWorldTransform, world->m_SetWorldTransform);
            btRigidBody::btRigidBodyConstructionInfo rb_info(data.m_Mass, motion_state, compound_shape, local_inertia);
            rb_info.m_friction = data.m_Friction;
            rb_info.m_restitution = data.m_Restitution;
            btRigidBody* body = new btRigidBody(rb_info);
            switch (data.m_Type)
            {
            case COLLISION_OBJECT_TYPE_KINEMATIC:
                // bodies are constructed as static by default in bullet so clear that flag (btCollisionObject.cpp:28)
                body->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
                break;
            case COLLISION_OBJECT_TYPE_STATIC:
                body->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
                break;
            default:
                break;
            }

            world->m_DynamicsWorld->addRigidBody(body);
            body->getBroadphaseHandle()->m_collisionFilterGroup = data.m_Group;
            body->getBroadphaseHandle()->m_collisionFilterMask = data.m_Mask;

            collision_object = body;
        }
        else
        {
            collision_object = new btGhostObject();
            btTransform world_transform;
            if (world->m_GetWorldTransform != 0x0)
            {
                Vectormath::Aos::Point3 position;
                Vectormath::Aos::Quat rotation;
                world->m_GetWorldTransform(data.m_UserData, position, rotation);
                world_transform = btTransform(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()), btVector3(position.getX(), position.getY(), position.getZ()));
            }
            else
            {
                world_transform = btTransform::getIdentity();
            }
            collision_object->setWorldTransform(world_transform);
            collision_object->setCollisionShape(compound_shape);
            collision_object->setCollisionFlags(collision_object->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            world->m_DynamicsWorld->addCollisionObject(collision_object, data.m_Group, data.m_Mask);
        }
        collision_object->setUserPointer(data.m_UserData);
        return collision_object;
    }

    void DeleteCollisionObject3D(HWorld3D world, HCollisionObject3D collision_object)
    {
        btCollisionObject* bt_co = (btCollisionObject*)collision_object;
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
        delete (btCollisionObject*)collision_object;
    }

    uint32_t GetCollisionShapes3D(HCollisionObject3D collision_object, HCollisionShape3D* out_buffer, uint32_t buffer_size)
    {
        btCollisionShape* shape = ((btCollisionObject*)collision_object)->getCollisionShape();
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
        ((btCollisionObject*)collision_object)->setUserPointer(user_data);
    }

    void* GetCollisionObjectUserData3D(HCollisionObject3D collision_object)
    {
        return ((btCollisionObject*)collision_object)->getUserPointer();
    }

    void ApplyForce3D(HCollisionObject3D collision_object, const Vector3& force, const Point3& position)
    {
        btCollisionObject* bt_co = (btCollisionObject*)collision_object;
        btRigidBody* rigid_body = btRigidBody::upcast(bt_co);
        if (rigid_body != 0x0 && !(rigid_body->isStaticOrKinematicObject()))
        {
            bool force_activate = false;
            rigid_body->activate(force_activate);
            btVector3 bt_force(force.getX(), force.getY(), force.getZ());
            btVector3 bt_position(position.getX(), position.getY(), position.getZ());
            rigid_body->applyForce(bt_force, bt_position - bt_co->getWorldTransform().getOrigin());
        }
    }

    Vector3 GetTotalForce3D(HCollisionObject3D collision_object)
    {
        btRigidBody* rigid_body = btRigidBody::upcast((btCollisionObject*)collision_object);
        if (rigid_body != 0x0 && !(rigid_body->isStaticOrKinematicObject()))
        {
            const btVector3& bt_total_force = rigid_body->getTotalForce();
            return Vector3(bt_total_force.getX(), bt_total_force.getY(), bt_total_force.getZ());
        }
        else
        {
            return Vector3(0.0f, 0.0f, 0.0f);
        }
    }

    Vectormath::Aos::Point3 GetWorldPosition3D(HCollisionObject3D collision_object)
    {
        btVector3 position = ((btCollisionObject*)collision_object)->getWorldTransform().getOrigin();
        return Vectormath::Aos::Point3(position.getX(), position.getY(), position.getZ());
    }

    Vectormath::Aos::Quat GetWorldRotation3D(HCollisionObject3D collision_object)
    {
        btQuaternion rotation = ((btCollisionObject*)collision_object)->getWorldTransform().getRotation();
        return Vectormath::Aos::Quat(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
    }

    Vectormath::Aos::Vector3 GetLinearVelocity3D(HCollisionObject3D collision_object)
    {
        Vectormath::Aos::Vector3 linear_velocity(0.0f, 0.0f, 0.0f);
        btRigidBody* body = btRigidBody::upcast((btCollisionObject*)collision_object);
        if (body != 0x0)
        {
            const btVector3& v = body->getLinearVelocity();
            linear_velocity = Vectormath::Aos::Vector3(v.getX(), v.getY(), v.getZ());
        }
        return linear_velocity;
    }

    Vectormath::Aos::Vector3 GetAngularVelocity3D(HCollisionObject3D collision_object)
    {
        Vectormath::Aos::Vector3 angular_velocity(0.0f, 0.0f, 0.0f);
        btRigidBody* body = btRigidBody::upcast((btCollisionObject*)collision_object);
        if (body != 0x0)
        {
            const btVector3& v = body->getAngularVelocity();
            angular_velocity = Vectormath::Aos::Vector3(v.getX(), v.getY(), v.getZ());
        }
        return angular_velocity;
    }

    bool IsEnabled3D(HCollisionObject3D collision_object)
    {
        btRigidBody* body = btRigidBody::upcast((btCollisionObject*)collision_object);
        return body->isInWorld();
    }

    void SetEnabled3D(HWorld3D world, HCollisionObject3D collision_object, bool enabled)
    {
        bool prev_enabled = IsEnabled3D(collision_object);
        // avoid multiple adds/removes
        if (prev_enabled == enabled)
            return;
        btRigidBody* body = btRigidBody::upcast((btCollisionObject*)collision_object);
        if (enabled)
        {
            // Reset state
            body->clearForces();
            body->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
            body->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
            if (world->m_GetWorldTransform != 0x0)
            {
                Vectormath::Aos::Point3 position;
                Vectormath::Aos::Quat rotation;
                world->m_GetWorldTransform(body->getUserPointer(), position, rotation);
                btTransform world_transform(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()), btVector3(position.getX(), position.getY(), position.getZ()));
                body->setWorldTransform(world_transform);
            }
            world->m_DynamicsWorld->addRigidBody(body);
        }
        else
        {
            world->m_DynamicsWorld->removeRigidBody(body);
        }
    }

    bool IsSleeping3D(HCollisionObject3D collision_object)
    {
        btRigidBody* body = btRigidBody::upcast((btCollisionObject*)collision_object);
        return body->getActivationState() == ISLAND_SLEEPING;
    }

    void RequestRayCast3D(HWorld3D world, const RayCastRequest& request)
    {
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

    void SetDebugCallbacks3D(HContext3D context, const DebugCallbacks& callbacks)
    {
        context->m_DebugCallbacks = callbacks;
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
