#include <stdint.h>

#include <dlib/log.h>

#include "physics.h"
#include "btBulletDynamicsCommon.h"
#include "debug_draw.h"

namespace dmPhysics
{
    DebugDraw m_DebugDraw;

    using namespace Vectormath::Aos;

    class MotionState : public btMotionState
    {
    public:
        MotionState(void* visual_object, void* callback_context, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform)
        : m_VisualObject(visual_object)
        , m_CallbackContext(callback_context)
        , m_GetWorldTransform(get_world_transform)
        , m_SetWorldTransform(set_world_transform)
        {
        }

        virtual ~MotionState() {
        }

        virtual void getWorldTransform(btTransform& world_trans) const
        {
            if (m_GetWorldTransform != 0x0)
            {
                Vectormath::Aos::Point3 position;
                Vectormath::Aos::Quat rotation;
                m_GetWorldTransform(m_VisualObject, m_CallbackContext, position, rotation);
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
                m_SetWorldTransform(m_VisualObject, m_CallbackContext, pos, rot);
            }
        }

    protected:
        void*           m_VisualObject;
        void*           m_CallbackContext;
        GetWorldTransformCallback  m_GetWorldTransform;
        SetWorldTransformCallback  m_SetWorldTransform;
    };

    struct PhysicsWorld
    {
        PhysicsWorld(const Point3& world_min, const Point3& world_max, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform, void* callback_context);
        ~PhysicsWorld();

        btDefaultCollisionConfiguration*        m_CollisionConfiguration;
        btCollisionDispatcher*                  m_Dispatcher;
        btAxisSweep3*                           m_OverlappingPairCache;
        btSequentialImpulseConstraintSolver*    m_Solver;
        btDiscreteDynamicsWorld*                m_DynamicsWorld;
        GetWorldTransformCallback               m_GetWorldTransform;
        SetWorldTransformCallback               m_SetWorldTransform;
        void*                                   m_CallbackContext;
    };

    PhysicsWorld::PhysicsWorld(const Point3& world_min, const Point3& world_max, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform, void* callback_context)
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
        m_CallbackContext = callback_context;
    }

    PhysicsWorld::~PhysicsWorld()
    {
        delete m_DynamicsWorld;
        delete m_Solver;
        delete m_OverlappingPairCache;
        delete m_Dispatcher;
        delete m_CollisionConfiguration;
    }

    HWorld NewWorld(const Point3& world_min, const Point3& world_max, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform, void* callback_context)
    {
        return new PhysicsWorld(world_min, world_max, get_world_transform, set_world_transform, callback_context);
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
        // TODO: Max substeps = 1 for now...
        world->m_DynamicsWorld->stepSimulation(dt, 1);
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
                btPersistentManifold* contactManifold = world->m_DynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
                btCollisionObject* object_a = static_cast<btCollisionObject*>(contactManifold->getBody0());
                btCollisionObject* object_b = static_cast<btCollisionObject*>(contactManifold->getBody1());

                if (collision_callback != 0x0)
                {
                    collision_callback(object_a->getUserPointer(), object_b->getUserPointer(), collision_callback_user_data);
                }

                if (contact_point_callback)
                {
                    int num_contacts = contactManifold->getNumContacts();
                    for (int j = 0; j < num_contacts; ++j)
                    {
                        btManifoldPoint& pt = contactManifold->getContactPoint(j);
                        if (pt.getDistance() <= 0.0f)
                        {
                            ContactPoint point;
                            const btVector3& pt_a = pt.getPositionWorldOnA();
                            point.m_PositionA = Vectormath::Aos::Point3(pt_a.getX(), pt_a.getY(), pt_a.getZ());
                            const btVector3& pt_b = pt.getPositionWorldOnB();
                            point.m_PositionB = Vectormath::Aos::Point3(pt_b.getX(), pt_b.getY(), pt_b.getZ());
                            const btVector3& normal = pt.m_normalWorldOnB;
                            point.m_Normal = -Vectormath::Aos::Vector3(normal.getX(), normal.getY(), normal.getZ());
                            point.m_Distance = pt.getDistance();
                            point.m_UserDataA = object_a->getUserPointer();
                            point.m_UserDataB = object_b->getUserPointer();
                            contact_point_callback(point, contact_point_callback_user_data);
                        }
                    }
                }
            }
        }
    }

    HCollisionShape NewBoxShape(const Vector3& half_extents)
    {
        return new btBoxShape(btVector3(half_extents.getX(), half_extents.getY(), half_extents.getZ()));
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

    HRigidBody NewRigidBody(HWorld world, HCollisionShape shape,
                            void* visual_object,
                            float mass,
                            RigidBodyType rigid_body_type,
                            void* user_data)
    {
        switch (rigid_body_type)
        {
        case RIGID_BODY_TYPE_DYNAMIC:
            if (mass == 0.0f)
            {
                dmLogError("Rigid bodies can not be dynamic and have zero mass.");
                return 0x0;
            }
            break;
        default:
            if (mass > 0.0f)
            {
                dmLogError("Only dynamic rigid bodies can have a positive mass.");
                return 0x0;
            }
            break;
        }

        btVector3 local_inertia(0.0f, 0.0f, 0.0f);
        if (rigid_body_type == RIGID_BODY_TYPE_DYNAMIC)
        {
            shape->calculateLocalInertia(mass, local_inertia);
        }

        MotionState* motion_state = new MotionState(visual_object, world->m_CallbackContext, world->m_GetWorldTransform, world->m_SetWorldTransform);
        btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state, shape, local_inertia);
        // TODO: Investigate if RIGID_BODY_TYPE_TRIGGER should be instantiated from a btGhostObject instead of rigid body with no contact response
        // Pros:
        // * Light weight object, one of its purposes are triggers
        // Cons:
        // * No motion state and hence no automatic callback to update its position => must be done manually from the trigger-component or similar
        btRigidBody* body = new btRigidBody(rb_info);
        switch (rigid_body_type)
        {
        case RIGID_BODY_TYPE_KINEMATIC:
            body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            body->setActivationState(DISABLE_DEACTIVATION);
            break;
        case RIGID_BODY_TYPE_STATIC:
            body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
            break;
        case RIGID_BODY_TYPE_TRIGGER:
            body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT | btCollisionObject::CF_NO_CONTACT_RESPONSE);
            break;
        default:
            break;
        }
        body->setUserPointer(user_data);
        world->m_DynamicsWorld->addRigidBody(body);
        return body;
    }

    void DeleteRigidBody(HWorld world, HRigidBody rigid_body)
    {
        if (rigid_body == 0x0)
            return;
        if (rigid_body->getMotionState())
            delete rigid_body->getMotionState();

        world->m_DynamicsWorld->removeCollisionObject(rigid_body);
        delete rigid_body;
    }

    void SetRigidBodyInitialTransform(HRigidBody rigid_body, Vectormath::Aos::Point3 position, Vectormath::Aos::Quat orientation)
    {
        btMotionState* motion_state = rigid_body->getMotionState();
        assert(motion_state);
        btVector3 bt_position(position.getX(), position.getY(), position.getZ());
        btQuaternion bt_orientation(orientation.getX(), orientation.getY(), orientation.getZ(), orientation.getW());
        btTransform world_transform(bt_orientation, bt_position);
        motion_state->setWorldTransform(world_transform);
        rigid_body->setWorldTransform(world_transform);
    }

    void SetRigidBodyUserData(HRigidBody rigid_body, void* user_data)
    {
        rigid_body->setUserPointer(user_data);
    }

    void* GetRigidBodyUserData(HRigidBody rigid_body)
    {
        return rigid_body->getUserPointer();
    }

    void ApplyForce(HRigidBody rigid_body, Vector3 force, Point3 position)
    {
        btVector3 bt_force(force.getX(), force.getY(), force.getZ());
        btVector3 bt_position(position.getX(), position.getY(), position.getZ());
    	rigid_body->applyForce(bt_force, bt_position - rigid_body->getWorldTransform().getOrigin());
    }

    Vector3 GetTotalForce(HRigidBody rigid_body)
    {
    	const btVector3& bt_total_force = rigid_body->getTotalForce();
    	return Vector3(bt_total_force.getX(), bt_total_force.getY(), bt_total_force.getZ());
    }

    Vectormath::Aos::Point3 GetWorldPosition(HRigidBody rigid_body)
    {
        btVector3 position = rigid_body->getWorldTransform().getOrigin();
        return Vectormath::Aos::Point3(position.getX(), position.getY(), position.getZ());
    }

    Vectormath::Aos::Quat GetWorldRotation(HRigidBody rigid_body)
    {
        btQuaternion rotation = rigid_body->getWorldTransform().getRotation();
        return Vectormath::Aos::Quat(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
    }

    void SetDebugRenderer(void* ctx, RenderLine render_line)
    {
        m_DebugDraw.SetRenderLine(ctx, render_line);
    }
}
