#include "physics.h"
#include "btBulletDynamicsCommon.h"

namespace Physics
{
    struct PhysicsWorld
    {
        PhysicsWorld(const Point3& world_min, const Point3& world_max);
        ~PhysicsWorld();

        btDefaultCollisionConfiguration*     m_CollisionConfiguration;
        btCollisionDispatcher*               m_Dispatcher;
        btAxisSweep3*                        m_OverlappingPairCache;
        btSequentialImpulseConstraintSolver* m_Solver;
        btDiscreteDynamicsWorld*             m_DynamicsWorld;
    };

    PhysicsWorld::PhysicsWorld(const Point3& world_min, const Point3& world_max)
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
    }

    PhysicsWorld::~PhysicsWorld()
    {
        delete m_DynamicsWorld;
        delete m_Solver;
        delete m_OverlappingPairCache;
        delete m_Dispatcher;
        delete m_CollisionConfiguration;
    }

    HWorld NewWorld(const Point3& world_min, const Point3& world_max)
    {
        return new PhysicsWorld(world_min, world_max);
    }

    void DeleteWorld(HWorld world)
    {
        delete world;
    }

    void StepWorld(HWorld world, float dt)
    {
        // TODO: Max substeps = 1 for now...
        world->m_DynamicsWorld->stepSimulation(dt, 1);
    }

    HCollisionShape NewBoxShape(HWorld world, const Point3& half_extents)
    {
        return new btBoxShape(btVector3(half_extents.getX(), half_extents.getY(), half_extents.getZ()));
    }

    void DeleteShape(HWorld world, HCollisionShape shape)
    {
        delete shape;
    }

    HRigidBody NewRigidBody(HWorld world, HCollisionShape shape,
                            const Quat& rotation, const Point3& position,
                            float mass)
    {
        btQuaternion bt_rotation(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
        btVector3 bt_position(position.getX(), position.getY(), position.getZ());
        btTransform transform(bt_rotation, bt_position);

        bool is_dynamic = (mass != 0.f);

        btVector3 local_inertia(0,0,0);
        if (is_dynamic)
            shape->calculateLocalInertia(mass, local_inertia);

        btDefaultMotionState* motion_state = new btDefaultMotionState(transform);
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

#include <stdio.h>
    Matrix4 GetTransform(HWorld world, HRigidBody rigid_body)
    {
        // TODO: This is ugly slow etc... PLEASE FIX...

        const btMotionState* motion_state = rigid_body->getMotionState();

        btTransform world_trans;
        motion_state->getWorldTransform(world_trans);
        Matrix3 rotation;
        for (int i = 0; i < 3; ++i)
        {
            btVector3 c = world_trans.getBasis().getColumn(i);
            rotation.setCol(i, Vector3(c.getX(), c.getY(), c.getZ()));
        }

        Vector3 translation = Vector3(world_trans.getOrigin().getX(),
                                world_trans.getOrigin().getY(),
                                world_trans.getOrigin().getZ());

        return Matrix4(rotation, translation);
    }

}
