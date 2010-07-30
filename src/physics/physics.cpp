#include <stdint.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/profile.h>

#include "physics.h"
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "debug_draw.h"

namespace dmPhysics
{
    DebugDraw m_DebugDraw;

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

    struct PhysicsWorld
    {
        PhysicsWorld(const Point3& world_min, const Point3& world_max, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform);
        ~PhysicsWorld();

        btDefaultCollisionConfiguration*        m_CollisionConfiguration;
        btCollisionDispatcher*                  m_Dispatcher;
        btAxisSweep3*                           m_OverlappingPairCache;
        btSequentialImpulseConstraintSolver*    m_Solver;
        btDiscreteDynamicsWorld*                m_DynamicsWorld;
        GetWorldTransformCallback               m_GetWorldTransform;
        SetWorldTransformCallback               m_SetWorldTransform;
        dmArray<RayCastRequest>                 m_RayCastRequests;
    };

    PhysicsWorld::PhysicsWorld(const Point3& world_min, const Point3& world_max, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform)
    {
        m_CollisionConfiguration = new btDefaultCollisionConfiguration();
        m_Dispatcher = new btCollisionDispatcher(m_CollisionConfiguration);

        ///the maximum size of the collision world. Make sure objects stay within these boundaries
        ///Don't make the world AABB size too large, it will harm simulation quality and performance
        btVector3 world_aabb_min(world_min.getX(), world_min.getY(), world_min.getZ());
        btVector3 world_aabb_max(world_max.getX(), world_max.getY(), world_max.getZ());
        int     maxProxies = 1024;
        m_OverlappingPairCache = new btAxisSweep3(world_aabb_min,world_aabb_max,maxProxies);

        m_Solver = new btSequentialImpulseConstraintSolver;

        m_DynamicsWorld = new btDiscreteDynamicsWorld(m_Dispatcher, m_OverlappingPairCache, m_Solver, m_CollisionConfiguration);
        m_DynamicsWorld->setGravity(btVector3(0,-10,0));
        m_DynamicsWorld->setDebugDrawer(&m_DebugDraw);

        m_GetWorldTransform = get_world_transform;
        m_SetWorldTransform = set_world_transform;

        m_RayCastRequests.SetCapacity(128);
    }

    PhysicsWorld::~PhysicsWorld()
    {
        delete m_DynamicsWorld;
        delete m_Solver;
        delete m_OverlappingPairCache;
        delete m_Dispatcher;
        delete m_CollisionConfiguration;
    }

    struct ProcessRayCastResultCallback : public btCollisionWorld::ClosestRayResultCallback
    {
        ProcessRayCastResultCallback(btVector3 from, btVector3 to, uint16_t mask, void* ignored_user_data)
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

    HWorld NewWorld(const Point3& world_min, const Point3& world_max, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform)
    {
        return new PhysicsWorld(world_min, world_max, get_world_transform, set_world_transform);
    }

    void DeleteWorld(HWorld world)
    {
        delete world;
    }

    void DebugRender(HWorld world)
    {
        world->m_DynamicsWorld->debugDrawWorld();
    }

    void StepWorld(HWorld world, float dt)
    {
        // Update all trigger transforms before physics world step
        if (world->m_GetWorldTransform != 0x0)
        {
            DM_PROFILE(Physics, "UpdateTriggers");
            int collision_object_count = world->m_DynamicsWorld->getNumCollisionObjects();
            for (int i = 0; i < collision_object_count; ++i)
            {
                btCollisionObject* collision_object = world->m_DynamicsWorld->getCollisionObjectArray()[i];
                if (collision_object->getInternalType() == btCollisionObject::CO_GHOST_OBJECT)
                {
                    Vectormath::Aos::Point3 position;
                    Vectormath::Aos::Quat rotation;
                    world->m_GetWorldTransform(collision_object->getUserPointer(), position, rotation);
                    btTransform world_transform(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()), btVector3(position.getX(), position.getY(), position.getZ()));
                    collision_object->setWorldTransform(world_transform);
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
                if (request.m_Callback == 0x0)
                {
                    dmLogWarning("Ray cast requested without any response callback, skipped.");
                    continue;
                }
                btVector3 from(request.m_From.getX(), request.m_From.getY(), request.m_From.getZ());
                btVector3 to(request.m_To.getX(), request.m_To.getY(), request.m_To.getZ());
                ProcessRayCastResultCallback result_callback(from, to, request.m_Mask, request.m_IgnoredUserData);
                world->m_DynamicsWorld->rayTest(from, to, result_callback);
                RayCastResponse response;
                response.m_Hit = result_callback.hasHit();
                response.m_Fraction = result_callback.m_closestHitFraction;
                response.m_Normal = Vectormath::Aos::Vector3(result_callback.m_hitNormalWorld.getX(), result_callback.m_hitNormalWorld.getY(), result_callback.m_hitNormalWorld.getZ());
                if (result_callback.m_collisionObject != 0x0)
                {
                    response.m_CollisionObjectUserData = result_callback.m_collisionObject->getUserPointer();
                    response.m_CollisionObjectGroup = result_callback.m_collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;
                }
                request.m_Callback(response, request);
            }
            world->m_RayCastRequests.SetSize(0);
        }
    }

    void ForEachCollision(HWorld world,
            CollisionCallback collision_callback,
            void* collision_callback_user_data,
            ContactPointCallback contact_point_callback,
            void* contact_point_callback_user_data)
    {
        // Assume world->stepSimulation or world->performDiscreteCollisionDetection has been called

        if (collision_callback != 0x0 || contact_point_callback != 0x0)
        {
            int num_manifolds = world->m_DynamicsWorld->getDispatcher()->getNumManifolds();
            for (int i = 0; i < num_manifolds; ++i)
            {
                btPersistentManifold* contact_manifold = world->m_DynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
                btCollisionObject* object_a = static_cast<btCollisionObject*>(contact_manifold->getBody0());
                btCollisionObject* object_b = static_cast<btCollisionObject*>(contact_manifold->getBody1());

                if (collision_callback != 0x0)
                {
                    collision_callback(object_a->getUserPointer(), object_a->getBroadphaseHandle()->m_collisionFilterGroup, object_b->getUserPointer(), object_b->getBroadphaseHandle()->m_collisionFilterGroup, collision_callback_user_data);
                }

                if (contact_point_callback)
                {
                    int num_contacts = contact_manifold->getNumContacts();
                    for (int j = 0; j < num_contacts; ++j)
                    {
                        btManifoldPoint& pt = contact_manifold->getContactPoint(j);
                        if (pt.getDistance() <= 0.0f)
                        {
                            ContactPoint point;
                            const btVector3& pt_a = pt.getPositionWorldOnA();
                            point.m_PositionA = Vectormath::Aos::Point3(pt_a.getX(), pt_a.getY(), pt_a.getZ());
                            point.m_UserDataA = object_a->getUserPointer();
                            point.m_GroupA = object_a->getBroadphaseHandle()->m_collisionFilterGroup;
                            const btVector3& pt_b = pt.getPositionWorldOnB();
                            point.m_PositionB = Vectormath::Aos::Point3(pt_b.getX(), pt_b.getY(), pt_b.getZ());
                            point.m_UserDataB = object_b->getUserPointer();
                            point.m_GroupB = object_b->getBroadphaseHandle()->m_collisionFilterGroup;
                            const btVector3& normal = pt.m_normalWorldOnB;
                            point.m_Normal = -Vectormath::Aos::Vector3(normal.getX(), normal.getY(), normal.getZ());
                            point.m_Distance = pt.getDistance();
                            contact_point_callback(point, contact_point_callback_user_data);
                        }
                    }
                }
            }
            // check ghosts
            if (collision_callback != 0x0)
            {
                int num_collision_objects = world->m_DynamicsWorld->getNumCollisionObjects();
                for (int i = 0; i < num_collision_objects; ++i)
                {
                    btCollisionObject* collision_object = world->m_DynamicsWorld->getCollisionObjectArray()[i];
                    btGhostObject* ghost_object = btGhostObject::upcast(collision_object);
                    if (ghost_object != 0x0)
                    {
                        int num_overlaps = ghost_object->getNumOverlappingObjects();
                        for (int j = 0; j < num_overlaps; ++j)
                        {
                            btCollisionObject* collidee = ghost_object->getOverlappingObject(j);
                            collision_callback(ghost_object->getUserPointer(), ghost_object->getBroadphaseHandle()->m_collisionFilterGroup, collidee->getUserPointer(), collidee->getBroadphaseHandle()->m_collisionFilterGroup, collision_callback_user_data);
                        }
                    }
                }
            }
        }
    }

    HCollisionShape NewSphereShape(float radius)
    {
        return new btSphereShape(radius);
    }

    HCollisionShape NewBoxShape(const Vector3& half_extents)
    {
        return new btBoxShape(btVector3(half_extents.getX(), half_extents.getY(), half_extents.getZ()));
    }

    HCollisionShape NewCapsuleShape(float radius, float height)
    {
        return new btCapsuleShape(radius, height);
    }

    HCollisionShape NewConvexHullShape(const float* vertices, uint32_t vertex_count)
    {
        assert(sizeof(btScalar) == sizeof(float));
        return new btConvexHullShape(vertices, vertex_count);
    }

    void DeleteCollisionShape(HCollisionShape shape)
    {
        delete shape;
    }

    HCollisionObject NewCollisionObject(HWorld world,
            HCollisionShape shape,
            float mass,
            CollisionObjectType collision_object_type,
            uint16_t group,
            uint16_t mask,
            void* user_data)
    {
        if (shape == 0x0)
        {
            dmLogError("Collision objects must have a shape.");
            return 0x0;
        }
        switch (collision_object_type)
        {
        case COLLISION_OBJECT_TYPE_DYNAMIC:
            if (mass == 0.0f)
            {
                dmLogError("Collision objects can not be dynamic and have zero mass.");
                return 0x0;
            }
            break;
        default:
            if (mass > 0.0f)
            {
                dmLogError("Only dynamic collision objects can have a positive mass.");
                return 0x0;
            }
            break;
        }

        btVector3 local_inertia(0.0f, 0.0f, 0.0f);
        if (collision_object_type == COLLISION_OBJECT_TYPE_DYNAMIC)
        {
            shape->calculateLocalInertia(mass, local_inertia);
        }

        btCollisionObject* collision_object = 0x0;
        if (collision_object_type != COLLISION_OBJECT_TYPE_TRIGGER)
        {
            MotionState* motion_state = new MotionState(user_data, world->m_GetWorldTransform, world->m_SetWorldTransform);
            btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state, shape, local_inertia);
            btRigidBody* body = new btRigidBody(rb_info);
            switch (collision_object_type)
            {
            case COLLISION_OBJECT_TYPE_KINEMATIC:
                body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                body->setActivationState(DISABLE_DEACTIVATION);
                break;
            case COLLISION_OBJECT_TYPE_STATIC:
                body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
                break;
            default:
                break;
            }

            world->m_DynamicsWorld->addRigidBody(body);
            body->getBroadphaseHandle()->m_collisionFilterGroup = group;
            body->getBroadphaseHandle()->m_collisionFilterMask = mask;

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
                world->m_GetWorldTransform(user_data, position, rotation);
                world_transform = btTransform(btQuaternion(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()), btVector3(position.getX(), position.getY(), position.getZ()));
            }
            else
            {
                world_transform = btTransform::getIdentity();
            }
            collision_object->setWorldTransform(world_transform);
            collision_object->setCollisionShape(shape);
            collision_object->setCollisionFlags(collision_object->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            world->m_DynamicsWorld->addCollisionObject(collision_object, group, mask);
        }
        collision_object->setUserPointer(user_data);
        return collision_object;
    }

    void DeleteCollisionObject(HWorld world, HCollisionObject collision_object)
    {
        if (collision_object == 0x0)
            return;
        btRigidBody* rigid_body = btRigidBody::upcast(collision_object);
        if (rigid_body != 0x0 && rigid_body->getMotionState())
            delete rigid_body->getMotionState();

        world->m_DynamicsWorld->removeCollisionObject(collision_object);
        delete collision_object;
    }

    void SetCollisionObjectInitialTransform(HCollisionObject collision_object, Vectormath::Aos::Point3 position, Vectormath::Aos::Quat orientation)
    {
        btVector3 bt_position(position.getX(), position.getY(), position.getZ());
        btQuaternion bt_orientation(orientation.getX(), orientation.getY(), orientation.getZ(), orientation.getW());
        btTransform world_transform(bt_orientation, bt_position);
        collision_object->setWorldTransform(world_transform);
    }

    void SetCollisionObjectUserData(HCollisionObject collision_object, void* user_data)
    {
        collision_object->setUserPointer(user_data);
    }

    void* GetCollisionObjectUserData(HCollisionObject collision_object)
    {
        return collision_object->getUserPointer();
    }

    void ApplyForce(HCollisionObject collision_object, Vector3 force, Point3 position)
    {
        btRigidBody* rigid_body = btRigidBody::upcast(collision_object);
        if (rigid_body != 0x0 && !(rigid_body->isStaticOrKinematicObject()))
        {
            bool force_activate = false;
            rigid_body->activate(force_activate);
            btVector3 bt_force(force.getX(), force.getY(), force.getZ());
            btVector3 bt_position(position.getX(), position.getY(), position.getZ());
            rigid_body->applyForce(bt_force, bt_position - collision_object->getWorldTransform().getOrigin());
        }
    }

    Vector3 GetTotalForce(HCollisionObject collision_object)
    {
        btRigidBody* rigid_body = btRigidBody::upcast(collision_object);
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

    Vectormath::Aos::Point3 GetWorldPosition(HCollisionObject collision_object)
    {
        btVector3 position = collision_object->getWorldTransform().getOrigin();
        return Vectormath::Aos::Point3(position.getX(), position.getY(), position.getZ());
    }

    Vectormath::Aos::Quat GetWorldRotation(HCollisionObject collision_object)
    {
        btQuaternion rotation = collision_object->getWorldTransform().getRotation();
        return Vectormath::Aos::Quat(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
    }

    RayCastRequest::RayCastRequest()
    : m_From(0.0f, 0.0f, 0.0f)
    , m_To(0.0f, 0.0f, 0.0f)
    , m_UserId(0)
    , m_Mask(~0)
    , m_IgnoredUserData((void*)~0) // unlikely user data to ignore
    , m_UserData(0x0)
    , m_Callback(0x0)
    {

    }

    RayCastResponse::RayCastResponse()
    : m_Hit(false)
    , m_Fraction(1.0f)
    , m_Normal(0.0f, 0.0f, 0.0f)
    , m_CollisionObjectUserData(0x0)
    , m_CollisionObjectGroup(0)
    {

    }

    void RequestRayCast(HWorld world, const RayCastRequest& request)
    {
        if (!world->m_RayCastRequests.Full())
            world->m_RayCastRequests.Push(request);
        else
            dmLogWarning("Ray cast query buffer is full (%d), ignoring request.", world->m_RayCastRequests.Capacity());
    }

    void SetDebugRenderer(void* ctx, RenderLine render_line)
    {
        m_DebugDraw.SetRenderLine(ctx, render_line);
    }
}
