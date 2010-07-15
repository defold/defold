#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdint.h>

#include <vectormath/cpp/vectormath_aos.h>

class btCollisionShape;
class btRigidBody;

namespace dmPhysics
{
    enum RigidBodyType
    {
        RIGID_BODY_TYPE_DYNAMIC,
        RIGID_BODY_TYPE_KINEMATIC,
        RIGID_BODY_TYPE_STATIC,
        RIGID_BODY_TYPE_TRIGGER,
        RIGID_BODY_TYPE_COUNT
    };

    typedef struct PhysicsWorld* HWorld;
    typedef class btCollisionShape* HCollisionShape;
    typedef class btRigidBody* HRigidBody;

    struct ContactPoint
    {
        Vectormath::Aos::Point3 m_PositionA;
        Vectormath::Aos::Point3 m_PositionB;
        // Always A->B
        Vectormath::Aos::Vector3 m_Normal;
        float m_Distance;
        void* m_UserDataA;
        void* m_UserDataB;
    };

    typedef void (*GetWorldTransformCallback)(void* visual_object, void* callback_context, Vectormath::Aos::Point3& position, Vectormath::Aos::Quat& rotation);
    typedef void (*SetWorldTransformCallback)(void* visual_object, void* callback_context, const Vectormath::Aos::Point3& position, const Vectormath::Aos::Quat& rotation);

    typedef void (*CollisionCallback)(void* user_data_a, void* user_data_b, void* user_data);

    typedef void (*ContactPointCallback)(const ContactPoint& contact_point, void* user_data);

    /**
     * Create a new physics world
     * @param world_min World min (AABB)
     * @param world_max World max (AABB)
     * @param set_object_state Set object state call-back. Used for updating corresponding visual object state
     * @param set_object_state_context User context
     * @return HPhysicsWorld
     */
    HWorld NewWorld(const Vectormath::Aos::Point3& world_min, const Vectormath::Aos::Point3& world_max, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform, void* callback_context);

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
     * Call callback functions for each collision and contact
     * @param world Physics world
     * @param collision_callback Collision callback function, called once per collision pair
     * @param collision_callback_user_data User data passed to collision_callback
     * @param contact_point_callback Contact point callback function, called once per contact point
     * @param contact_point_callback_user_data User data passed to contact_point_callback
     */
    void ForEachCollision(HWorld world,
            CollisionCallback collision_callback,
            void* collision_callback_user_data,
            ContactPointCallback contact_point_callback,
            void* contact_point_callback_user_data);

    /**
     * Draws the world using the callback function registered through SetDebugRenderer.
     */
    void DebugRender(HWorld world);

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
     * @param is_kinematic If the rigid body should be flagged as kinematic or not. This is only possible when mass == 0.
     * @param user_data User data
     * @return A new rigid body
     */
    HRigidBody NewRigidBody(HWorld world, HCollisionShape shape,
                            void* visual_object,
                            float mass,
                            RigidBodyType rigid_body_type,
                            void* user_data);

    /**
     * Delete a rigid body
     * @param world Physics world
     * @param rigid_body Rigid body to delete
     */
    void DeleteRigidBody(HWorld world, HRigidBody rigid_body);

    /**
     * Set rigid body initial transform
     * @param rigid_body Rigid body
     * @param position Initial position
     * @param orientation Initial orientation
     */
    void SetRigidBodyInitialTransform(HRigidBody rigid_body, Vectormath::Aos::Point3 position, Vectormath::Aos::Quat orientation);

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
     * @param position Position of where the force will be applied (world space).
     */
    void ApplyForce(HRigidBody rigid_body, Vectormath::Aos::Vector3 force, Vectormath::Aos::Point3 position);

    /**
     * Return the total force currently applied to the specified rigid body.
     * @param rigid_body Rigid body receiving the force.
     * @return The total force (world space).
     */
    Vectormath::Aos::Vector3 GetTotalForce(HRigidBody rigid_body);

    /**
     * Return the world position of the specified rigid body.
     * @param rigid_body Rigid body handle
     * @return The world space position
     */
    Vectormath::Aos::Point3 GetWorldPosition(HRigidBody rigid_body);

    /**
     * Return the world rotation of the specified rigid body.
     * @param rigid_body Rigid body handle
     * @return The world space rotation
     */
    Vectormath::Aos::Quat GetWorldRotation(HRigidBody rigid_body);

    typedef void (*RenderLine)(void* ctx, Vectormath::Aos::Point3 p0, Vectormath::Aos::Point3 p1, Vectormath::Aos::Vector4 color);
    /**
     * Registers a callback function used to render lines when .
     * @param ctx Context that will be supplied to the RenderLine callback.
     * @param render_line Callback used to render lines.
     */
    void SetDebugRenderer(void* ctx, RenderLine render_line);
}

#endif // PHYSICS_H
