#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdint.h>

#include <vectormath/cpp/vectormath_aos.h>

class btCollisionShape;
class btCollisionObject;

namespace dmPhysics
{
    enum CollisionObjectType
    {
        COLLISION_OBJECT_TYPE_DYNAMIC,
        COLLISION_OBJECT_TYPE_KINEMATIC,
        COLLISION_OBJECT_TYPE_STATIC,
        COLLISION_OBJECT_TYPE_TRIGGER,
        COLLISION_OBJECT_TYPE_COUNT
    };

    typedef struct PhysicsWorld* HWorld;
    typedef class btCollisionShape* HCollisionShape;
    typedef class btCollisionObject* HCollisionObject;

    typedef void (*GetWorldTransformCallback)(void* user_data, Vectormath::Aos::Point3& position, Vectormath::Aos::Quat& rotation);
    typedef void (*SetWorldTransformCallback)(void* user_data, const Vectormath::Aos::Point3& position, const Vectormath::Aos::Quat& rotation);

    typedef void (*CollisionCallback)(void* user_data_collider, void* user_data_collidee, void* user_data);

    struct ContactPoint
    {
        Vectormath::Aos::Point3 m_PositionA;
        Vectormath::Aos::Point3 m_PositionB;
        /// Always A->B
        Vectormath::Aos::Vector3 m_Normal;
        float m_Distance;
        void* m_UserDataA;
        void* m_UserDataB;
    };

    typedef void (*ContactPointCallback)(const ContactPoint& contact_point, void* user_data);

    /**
     * Create a new physics world
     * @param world_min World min (AABB)
     * @param world_max World max (AABB)
     * @param get_world_transform Callback for copying the transform from the corresponding user data to the collision object
     * @param set_world_transform Callback for copying the transform from the collision object to the corresponding user data
     * @return HPhysicsWorld
     */
    HWorld NewWorld(const Vectormath::Aos::Point3& world_min, const Vectormath::Aos::Point3& world_max, GetWorldTransformCallback get_world_transform, SetWorldTransformCallback set_world_transform);

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
     * Create a new collision object
     * @param world Physics world
     * @param shape Shape
     * @param rotation Initial rotation
     * @param position Initial position
     * @param mass Mass, must be positive for COLLISION_OBJECT_TYPE_DYNAMIC and zero for all other types
     * @param collision_object_type Type of collision object
     * @param user_data User data
     * @return A new collision object
     */
    HCollisionObject NewCollisionObject(HWorld world, HCollisionShape shape,
                            float mass,
                            CollisionObjectType collision_object_type,
                            void* user_data);

    /**
     * Delete a collision object
     * @param world Physics world
     * @param collision_object Collision object to delete
     */
    void DeleteCollisionObject(HWorld world, HCollisionObject collision_object);

    /**
     * Set collision object initial transform
     * @param collision_object Collision object
     * @param position Initial position
     * @param orientation Initial orientation
     */
    void SetCollisionObjectInitialTransform(HCollisionObject collision_object, Vectormath::Aos::Point3 position, Vectormath::Aos::Quat orientation);

    /**
     * Set collision object user data
     * @param collision_object Collision object
     * @param user_data User data
     */
    void SetCollisionObjectUserData(HCollisionObject collision_object, void* user_data);

    /**
     * Set collision object user data
     * @param collision_object Collision object
     * @return User data
     */
    void* GetCollisionObjectUserData(HCollisionObject collision_object);

    /**
     * Apply a force to the specified collision object at the specified position
     * @param collision_object Collision object receiving the force, must be of type COLLISION_OBJECT_TYPE_DYNAMIC
     * @param force Force to be applied (world space)
     * @param position Position of where the force will be applied (world space)
     */
    void ApplyForce(HCollisionObject collision_object, Vectormath::Aos::Vector3 force, Vectormath::Aos::Point3 position);

    /**
     * Return the total force currently applied to the specified collision object.
     * @param collision_object Which collision object to inspect. For objects with another type than COLLISION_OBJECT_TYPE_DYNAMIC, the force will always be of zero size.
     * @return The total force (world space).
     */
    Vectormath::Aos::Vector3 GetTotalForce(HCollisionObject collision_object);

    /**
     * Return the world position of the specified collision object.
     * @param collision_object Collision object handle
     * @return The world space position
     */
    Vectormath::Aos::Point3 GetWorldPosition(HCollisionObject collision_object);

    /**
     * Return the world rotation of the specified collision object.
     * @param collision_object Collision object handle
     * @return The world space rotation
     */
    Vectormath::Aos::Quat GetWorldRotation(HCollisionObject collision_object);

    typedef void (*RayCastResponseCallback)(bool hit, float hit_fraction, uint32_t user_id, void* user_data);

    struct RayCastRequest
    {
        Vectormath::Aos::Point3 m_From;
        Vectormath::Aos::Point3 m_To;
        uint32_t m_UserId;
        void* m_UserData;
        RayCastResponseCallback m_ResponseCallback;
    };

    void RequestRayCast(HWorld world, const RayCastRequest& request);

    typedef void (*RenderLine)(void* ctx, Vectormath::Aos::Point3 p0, Vectormath::Aos::Point3 p1, Vectormath::Aos::Vector4 color);
    /**
     * Registers a callback function used to render lines when .
     * @param ctx Context that will be supplied to the RenderLine callback.
     * @param render_line Callback used to render lines.
     */
    void SetDebugRenderer(void* ctx, RenderLine render_line);
}

#endif // PHYSICS_H
