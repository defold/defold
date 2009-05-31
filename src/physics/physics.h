#ifndef PHYSICS_H
#define PHYSICS_H

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

class btCollisionShape;
class btRigidBody;

namespace Physics
{
    typedef struct PhysicsWorld* HWorld;
    typedef class btCollisionShape* HCollisionShape;
    typedef class btRigidBody* HRigidBody;

    struct WorldConfiguration
    {
        WorldConfiguration()
        {

        }
    };

    /**
     * Create a new physics world
     * @return HPhysicsWorld
     */
    HWorld NewWorld(const Point3& world_min, const Point3& world_max);

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
     * @param world Physics world
     * @param half_extents Box half extents
     * @return Shape
     */
    HCollisionShape NewBoxShape(HWorld world, const Point3& half_extents);

    /**
     * Delete a shape
     * @param world Physics world
     * @param shape Shape
     */
    void DeleteShape(HWorld world, HCollisionShape shape);

    /**
     * Create a new rigid body
     * @param world Physics world
     * @param shape Shape
     * @param rotation Initial rotation
     * @param position Initial position
     * @param mass Mass. If identical to 0.0f the rigid body is static
     * @return A new rigid body
     */
    HRigidBody NewRigidBody(HWorld world, HCollisionShape shape,
                            const Quat& rotation, const Point3& position,
                            float mass);

    /**
     * Delete a rigid body
     * @param world Physics world
     * @param rigid_body Rigid body to delete
     */
    void DeleteRigidBody(HWorld world, HRigidBody rigid_body);

    /**
     * TODO: How to handle synchronize position, orient. with physics world eff...
     * We will probably remove this function later...
     * Get rigid body transform.
     * @param world Physics world
     * @param rigid_body Rigid body
     * @return Transform
     */
    Matrix4 GetTransform(HWorld world, HRigidBody rigid_body);
}

#endif // PHYSICS_H
