#ifndef PHYSICS_H
#define PHYSICS_H

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

class btCollisionShape;
class btRigidBody;

namespace dmPhysics
{
    typedef struct PhysicsWorld* HWorld;
    typedef class btCollisionShape* HCollisionShape;
    typedef class btRigidBody* HRigidBody;

    typedef void (*SetObjectState)(void* context, void* visual_object, const Quat& rotation, const Point3& position);

    /**
     * Create a new physics world
     * @param world_min World min (AABB)
     * @param world_max World max (AABB)
     * @param set_object_state Set object state call-back. Used for updating corresponding visual object state
     * @param set_object_state_context User context
     * @return HPhysicsWorld
     */
    HWorld NewWorld(const Point3& world_min, const Point3& world_max, SetObjectState set_object_state, void* set_object_state_context);

    /**
     * Delete a physics world
     * @param world Physics world
     */
    void DeleteWorld(HWorld world);

    /**
     * Simulate physics
     * @param world Physics world
     * @param dt Time step
     */
    void StepWorld(HWorld world, float dt);

    /**
     * Create a new shape
     * @param half_extents Box half extents
     * @return Shape
     */
    HCollisionShape NewBoxShape(const Vectormath::Aos::Vector3& half_extents);

    /**
     * Create a new convex hull shape
     * @param vertices Vertices. x0, y0, z0, x1, ...
     * @param vertex_count Vertex count
     * @return Shape
     */
    HCollisionShape NewConvexHullShape(const float* vertices, uint32_t vertex_count);

    /**
     * Delete a shape
     * @param shape Shape
     */
    void DeleteCollisionShape(HCollisionShape shape);

    /**
     * Create a new rigid body
     * @param world Physics world
     * @param shape Shape
     * @param visual_object Corresponding visual object
     * @param rotation Initial rotation
     * @param position Initial position
     * @param mass Mass. If identical to 0.0f the rigid body is static
     * @return A new rigid body
     */
    HRigidBody NewRigidBody(HWorld world, HCollisionShape shape,
                            void* visual_object,
                            const Quat& rotation, const Point3& position,
                            float mass);

    /**
     * Delete a rigid body
     * @param world Physics world
     * @param rigid_body Rigid body to delete
     */
    void DeleteRigidBody(HWorld world, HRigidBody rigid_body);

    /**
     * Set rigid body user data
     * @param rigid_body Rigid body
     * @param user_data User data
     */
    void SetRigidBodyUserData(HRigidBody rigid_body, void* user_data);

    /**
     * Set rigid body user data
     * @param rigid_body Rigid body
     * @return User data
     */
    void* GetRigidBodyUserData(HRigidBody rigid_body);

    /**
     * Apply a force to the specified rigid body at the specified position.
     * @param rigid_body Rigid body receiving the force.
     * @param force Force to be applied (world space).
     * @param relative_position Position of where the force will be applied, relative to the center of the body (world space).
     */
    void ApplyForce(HRigidBody rigid_body, Vectormath::Aos::Vector3 force, Vectormath::Aos::Vector3 position = Vectormath::Aos::Vector3(0.0f, 0.0f, 0.0f));

    /**
     * Return the total force currently applied to the specified rigid body.
     * @param rigid_body Rigid body receiving the force.
     * @return The total force (world space).
     */
    Vector3 GetTotalForce(HRigidBody rigid_body);
}

#endif // PHYSICS_H
