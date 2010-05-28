#include <stdint.h>
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
        MotionState(const btTransform& initial_pos, void* visual_object, SetObjectState set_object_state, void* set_object_state_context)
        {
            m_Pos1 = initial_pos;
            m_VisualObject = visual_object;
            m_SetObjectState = set_object_state;
            m_SetObjectStateContext = set_object_state_context;
        }

        virtual ~MotionState() {
        }

        virtual void getWorldTransform(btTransform& world_trans) const
        {
            world_trans = m_Pos1;
        }

        virtual void setWorldTransform(const btTransform &worldTrans)
        {
            btQuaternion bt_rot = worldTrans.getRotation();
            btVector3 bt_pos = worldTrans.getOrigin();

            Quat rot = Quat(bt_rot.getX(), bt_rot.getY(), bt_rot.getZ(), bt_rot.getW());
            Point3 pos = Point3(bt_pos.getX(), bt_pos.getY(), bt_pos.getZ());
            m_SetObjectState(m_SetObjectStateContext, m_VisualObject, rot, pos);
        }

    protected:
        SetObjectState  m_SetObjectState;
        void*           m_SetObjectStateContext;
        void*           m_VisualObject;
        btTransform     m_Pos1;
    };

    struct PhysicsWorld
    {
        PhysicsWorld(const Point3& world_min, const Point3& world_max, SetObjectState set_object_state, void* set_object_state_context);
        ~PhysicsWorld();

        btDefaultCollisionConfiguration*        m_CollisionConfiguration;
        btCollisionDispatcher*                  m_Dispatcher;
        btAxisSweep3*                           m_OverlappingPairCache;
        btSequentialImpulseConstraintSolver*    m_Solver;
        btDiscreteDynamicsWorld*                m_DynamicsWorld;
        SetObjectState                          m_SetObjectState;
        void*                                   m_SetObjectStateContext;
    };

    PhysicsWorld::PhysicsWorld(const Point3& world_min, const Point3& world_max, SetObjectState set_object_state, void* set_object_state_context)
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

        m_SetObjectState = set_object_state;
        m_SetObjectStateContext = set_object_state_context;
    }

    PhysicsWorld::~PhysicsWorld()
    {
        delete m_DynamicsWorld;
        delete m_Solver;
        delete m_OverlappingPairCache;
        delete m_Dispatcher;
        delete m_CollisionConfiguration;
    }

    HWorld NewWorld(const Point3& world_min, const Point3& world_max, SetObjectState set_object_state, void* set_object_state_context)
    {
        return new PhysicsWorld(world_min, world_max, set_object_state, set_object_state_context);
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
                            float mass)
    {
        bool is_dynamic = (mass != 0.f);

        btVector3 local_inertia(0,0,0);
        if (is_dynamic)
            shape->calculateLocalInertia(mass, local_inertia);

        btTransform transform;
        transform.setIdentity();
        MotionState* motion_state = new MotionState(transform, visual_object, world->m_SetObjectState, world->m_SetObjectStateContext);
        btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state, shape, local_inertia);
        btRigidBody* body = new btRigidBody(rb_info);
        world->m_DynamicsWorld->addRigidBody(body);
        return body;
    }

    void DeleteRigidBody(HWorld world, HRigidBody rigid_body)
    {
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

    void ApplyForce(HRigidBody rigid_body, Vector3 force, Point3 relative_position)
    {
        btVector3 bt_force(force.getX(), force.getY(), force.getZ());
        btVector3 bt_relative_position(relative_position.getX(), relative_position.getY(), relative_position.getZ());
    	rigid_body->applyForce(bt_force, bt_relative_position);
    }

    Vector3 GetTotalForce(HRigidBody rigid_body)
    {
    	const btVector3& bt_total_force = rigid_body->getTotalForce();
    	return Vector3(bt_total_force.getX(), bt_total_force.getY(), bt_total_force.getZ());
    }

    void SetDebugRenderer(void* ctx, RenderLine render_line)
    {
        m_DebugDraw.SetRenderLine(ctx, render_line);
    }
}
