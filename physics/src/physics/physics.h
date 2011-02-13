#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdint.h>
#include <dlib/hash.h>
#include <vectormath/cpp/vectormath_aos.h>

namespace dmPhysics
{
    /// Collision object types defining how such an object will be moved.
    enum CollisionObjectType
    {
        COLLISION_OBJECT_TYPE_DYNAMIC,
        COLLISION_OBJECT_TYPE_KINEMATIC,
        COLLISION_OBJECT_TYPE_STATIC,
        COLLISION_OBJECT_TYPE_TRIGGER,
        COLLISION_OBJECT_TYPE_COUNT
    };

    /// 3D context handle.
    typedef struct Context3D* HContext3D;
    /// 3D world handle.
    typedef struct World3D* HWorld3D;
    /// 3D collision shape handle.
    typedef void* HCollisionShape3D;
    /// 3D collision object handle.
    typedef void* HCollisionObject3D;

    /// 2D context handle.
    typedef struct Context2D* HContext2D;
    /// 2D world handle.
    typedef struct World2D* HWorld2D;
    /// 2D collision shape handle.
    typedef void* HCollisionShape2D;
    /// 2D collision object handle.
    typedef void* HCollisionObject2D;

    /**
     * Callback used to propagate the world transform of an external object into the physics simulation.
     *
     * @param user_data User data pointing to the external object
     * @param position Position output parameter
     * @param rotation Rotation output parameter
     */
    typedef void (*GetWorldTransformCallback)(void* user_data, Vectormath::Aos::Point3& position, Vectormath::Aos::Quat& rotation);
    /**
     * Callback used to propagate the world transform from the physics simulation to an external object.
     *
     * @param user_data User data poiting to the external object
     * @param position Position that the external object will obtain
     * @param rotation Rotation that the external object will obtain
     */
    typedef void (*SetWorldTransformCallback)(void* user_data, const Vectormath::Aos::Point3& position, const Vectormath::Aos::Quat& rotation);

    /**
     * Callback used to signal collisions.
     *
     * @param user_data_a User data pointing to the external object of the first colliding object
     * @param group_a Collision group of the first colliding object
     * @param user_data_b User data pointing to the external object of the second colliding object
     * @param group_b Collision group of the second colliding object
     * @param user_data User data supplied to the collision query
     * @return If the collision signaling should be continued or not
     */
    typedef bool (*CollisionCallback)(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data);

    /**
     * Structure containing data about contact points
     */
    struct ContactPoint
    {
        /// Position of the first object
        Vectormath::Aos::Point3 m_PositionA;
        /// Position of the second object
        Vectormath::Aos::Point3 m_PositionB;
        /// Normal of the contact, pointing from A->B
        Vectormath::Aos::Vector3 m_Normal;
        /// Relative velocity between the objects
        Vectormath::Aos::Vector3 m_RelativeVelocity;
        /// User data of the first object
        void* m_UserDataA;
        /// User data of the second object
        void* m_UserDataB;
        /// Penetration of the objects at the contact
        float m_Distance;
        /// The impulse that resulted from the contact
        float m_AppliedImpulse;
        /// Inverse mass of the first object
        float m_InvMassA;
        /// Inverse mass of the second object
        float m_InvMassB;
        /// Collision group of the first object
        uint16_t m_GroupA;
        /// Collision group of the second object
        uint16_t m_GroupB;
    };

    /**
     * Callback used to signal contacts.
     *
     * @param contact_point Contact data
     * @param user_data User data supplied to the collision query
     * @return If the contact signaling should be continued or not
     */
    typedef bool (*ContactPointCallback)(const ContactPoint& contact_point, void* user_data);

    /**
     * Parameters to use when creating a context.
     */
    struct NewContextParams
    {
        NewContextParams();

        /// Number of 3D worlds the context supports
        uint32_t m_WorldCount;
    };

    /**
     * Create a new physics 3D context.
     *
     * @param params Parameters describing the context
     * @return the new context
     */
    HContext3D NewContext3D(const NewContextParams& params);

    /**
     * Create a new physics 2D context.
     *
     * @param params Parameters describing the context
     * @return the new context
     */
    HContext2D NewContext2D(const NewContextParams& params);

    /**
     * Destroy a 3D context.
     *
     * @param context Context to delete
     */
    void DeleteContext3D(HContext3D context);

    /**
     * Destroy a 2D context.
     *
     * @param context Context to delete
     */
    void DeleteContext2D(HContext2D context);

    /**
     * Parameters to use when creating a world.
     */
    struct NewWorldParams
    {
        NewWorldParams();

        /// World gravity
        Vectormath::Aos::Vector3 m_Gravity;
        /// world_min World min (AABB)
        Vectormath::Aos::Point3 m_WorldMin;
        /// world_max World max (AABB)
        Vectormath::Aos::Point3 m_WorldMax;
        /// param get_world_transform Callback for copying the transform from the corresponding user data to the collision object
        GetWorldTransformCallback m_GetWorldTransformCallback;
        /// param set_world_transform Callback for copying the transform from the collision object to the corresponding user data
        SetWorldTransformCallback m_SetWorldTransformCallback;
        /// callback to use when reporting collisions
        CollisionCallback m_CollisionCallback;
        /// user data to pass along to the collision callback function
        void* m_CollisionCallbackUserData;
        /// callback to use when reporting contact points
        ContactPointCallback m_ContactPointCallback;
        /// user data to pass along to the contact point callback function
        void* m_ContactPointCallbackUserData;
    };

    /**
     * Create a new 3D physics world
     *
     * @return 3D world
     */
    HWorld3D NewWorld3D(HContext3D context, const NewWorldParams& params);

    /**
     * Create a new 2D physics world
     *
     * @param world_min World min (AABB)
     * @param world_max World max (AABB)
     * @param get_world_transform Callback for copying the transform from the corresponding user data to the collision object
     * @param set_world_transform Callback for copying the transform from the collision object to the corresponding user data
     * @return 2D world
     */
    HWorld2D NewWorld2D(HContext2D context, const NewWorldParams& params);

    /**
     * Delete a 3D physics world
     *
     * @param world Physics world
     */
    void DeleteWorld3D(HContext3D context, HWorld3D world);

    /**
     * Delete a 2D physics world
     *
     * @param world Physics world
     */
    void DeleteWorld2D(HContext2D context, HWorld2D world);

    /**
     * Simulate 3D physics
     *
     * @param world Physics world
     * @param dt Time step in seconds
     */
    void StepWorld3D(HWorld3D world, float dt);

    /**
     * Simulate 2D physics
     *
     * @param world Physics world
     * @param dt Time step in seconds
     */
    void StepWorld2D(HWorld2D world, float dt);

    /**
     * Call callback functions for each collision in a 3D physics world.
     *
     * @param world Physics world
     * @param collision_callback Collision callback function, called once per collision pair
     * @param collision_callback_user_data User data passed to collision_callback
     */
    void SetCollisionCallback3D(HWorld3D world, CollisionCallback collision_callback, void* collision_callback_user_data);

    /**
     * Set callback functions for each collision in a 2D physics world.
     *
     * @param world Physics world
     * @param collision_callback Collision callback function, called once per collision pair
     * @param collision_callback_user_data User data passed to collision_callback
     */
    void SetCollisionCallback2D(HWorld2D world, CollisionCallback collision_callback, void* collision_callback_user_data);

    /**
     * Call callback functions for each contact in a 3D physics world.
     *
     * @param world Physics world
     * @param contact_point_callback Contact point callback function, called once per contact point
     * @param contact_point_callback_user_data User data passed to contact_point_callback
     */
    void SetContactPointCallback3D(HWorld3D world, ContactPointCallback contact_point_callback, void* contact_point_callback_user_data);

    /**
     * Set callback functions for each contact in a 2D physics world.
     *
     * @param world Physics world
     * @param contact_point_callback Contact point callback function, called once per contact point
     * @param contact_point_callback_user_data User data passed to contact_point_callback
     */
    void SetContactPointCallback2D(HWorld2D world, ContactPointCallback contact_point_callback, void* contact_point_callback_user_data);

    /**
     * Draws the 3D world using the callback function registered through SetDebugCallbacks.
     *
     * @param world Physics world
     */
    void DrawDebug3D(HWorld3D world);

    /**
     * Draws the 2D world using the callback function registered through SetDebugCallbacks.
     *
     * @param world Physics world
     */
    void DrawDebug2D(HWorld2D world);

    /**
     * Create a new 3D sphere shape.
     *
     * @param radius Sphere radius
     * @return Shape
     */
    HCollisionShape3D NewSphereShape3D(float radius);

    /**
     * Create a new 2D circle shape
     *
     * @param radius Circle radius
     * @return Shape
     */
    HCollisionShape2D NewCircleShape2D(float radius);

    /**
     * Create a new 3D box shape
     *
     * @param half_extents Box half extents
     * @return Shape
     */
    HCollisionShape3D NewBoxShape3D(const Vectormath::Aos::Vector3& half_extents);

    /**
     * Create a new 2D box shape
     *
     * @param half_extents Box half extents
     * @return Shape
     */
    HCollisionShape2D NewBoxShape2D(const Vectormath::Aos::Vector3& half_extents);

    /**
     * Create a new 3D capsule shape
     * @param radius Radius of top and bottom half-spheres of the capsule
     * @param height Height of the capsule; the distance between the two half-spheres
     * @return Shape
     */
    HCollisionShape3D NewCapsuleShape3D(float radius, float height);

    /**
     * Create a new 3D convex hull shape
     *
     * @param vertices Vertices. x0, y0, z0, x1, ...
     * @param vertex_count Vertex count
     * @return Shape
     */
    HCollisionShape3D NewConvexHullShape3D(const float* vertices, uint32_t vertex_count);

    /**
     * Create a new 2D polygon shape
     *
     * @param vertices Vertices. x0, y0, x1, ...
     * @param vertex_count Vertex count
     * @return Shape
     */
    HCollisionShape2D NewPolygonShape2D(const float* vertices, uint32_t vertex_count);

    /**
     * Delete a 3D shape
     *
     * @param shape Shape
     */
    void DeleteCollisionShape3D(HCollisionShape3D shape);

    /**
     * Delete a 2D shape
     *
     * @param shape Shape
     */
    void DeleteCollisionShape2D(HCollisionShape2D shape);

    /**
     * Data for collision object construction.
     */
    struct CollisionObjectData
    {
        CollisionObjectData();

        /// User data
        void* m_UserData;
        /// Type of collision object, default is COLLISION_OBJECT_TYPE_DYNAMIC
        CollisionObjectType m_Type;
        /// Mass, must be positive for COLLISION_OBJECT_TYPE_DYNAMIC and zero for all other types, default is 1
        float m_Mass;
        /// Friction, should be positive for best result, default is 0.5
        float m_Friction;
        /// Restitution, should be 0 for best results, default is 0
        float m_Restitution;
        /// Collision filter group. Two objects a and b are tested for collision if a.group & b.mask != 0 && a.mask & b.group != 0. Default is 1
        uint16_t m_Group;
        /// Collision filter mask @see m_Group. Default is 1.
        uint16_t m_Mask;
    };

    /**
     * Create a new 3D collision object
     *
     * @note If the world has a registered callback to retrieve the world transform, it will be called to initialize the collision object.
     *
     * @param world Physics world
     * @param data @see CollisionObjectData
     * @return A new collision object
     */
    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data, HCollisionShape3D shape);

    /**
     * Create a new 2D collision object
     *
     * @note If the world has a registered callback to retrieve the world transform, it will be called to initialize the collision object.
     *
     * @param world Physics world
     * @param data @see CollisionObjectData
     * @return A new collision object
     */
    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D shape);

    /**
     * Delete a 3D collision object
     *
     * @param world Physics world
     * @param collision_object Collision object to delete
     */
    void DeleteCollisionObject3D(HWorld3D world, HCollisionObject3D collision_object);

    /**
     * Delete a 2D collision object
     *
     * @param world Physics world
     * @param collision_object Collision object to delete
     */
    void DeleteCollisionObject2D(HWorld2D world, HCollisionObject2D collision_object);

    /**
     * Retrieve the shape of a 3D collision object.
     *
     * @param collision_object Collision object
     * @return Collision shape
     */
    HCollisionShape3D GetCollisionShape3D(HCollisionObject3D collision_object);

    /**
     * Retrieve the shape of a 2D collision object.
     *
     * @param collision_object Collision object
     * @return Collision shape
     */
    HCollisionShape2D GetCollisionShape2D(HCollisionObject2D collision_object);

    /**
     * Set 3D collision object user data
     *
     * @param collision_object Collision object
     * @param user_data User data
     */
    void SetCollisionObjectUserData3D(HCollisionObject3D collision_object, void* user_data);

    /**
     * Set 2D collision object user data
     *
     * @param collision_object Collision object
     * @param user_data User data
     */
    void SetCollisionObjectUserData2D(HCollisionObject2D collision_object, void* user_data);

    /**
     * Set 3D collision object user data
     *
     * @param collision_object Collision object
     * @return User data
     */
    void* GetCollisionObjectUserData3D(HCollisionObject3D collision_object);

    /**
     * Set 2D collision object user data
     *
     * @param collision_object Collision object
     * @return User data
     */
    void* GetCollisionObjectUserData2D(HCollisionObject2D collision_object);

    /**
     * Apply a force to the specified 3D collision object at the specified position
     *
     * @param collision_object Collision object receiving the force, must be of type COLLISION_OBJECT_TYPE_DYNAMIC
     * @param force Force to be applied (world space)
     * @param position Position of where the force will be applied (world space)
     */
    void ApplyForce3D(HCollisionObject3D collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position);

    /**
     * Apply a force to the specified 2D collision object at the specified position
     *
     * @param collision_object Collision object receiving the force, must be of type COLLISION_OBJECT_TYPE_DYNAMIC
     * @param force Force to be applied (world space)
     * @param position Position of where the force will be applied (world space)
     */
    void ApplyForce2D(HCollisionObject2D collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position);

    /**
     * Return the total force currently applied to the specified 3D collision object.
     *
     * @param collision_object Which collision object to inspect. For objects with another type than COLLISION_OBJECT_TYPE_DYNAMIC, the force will always be of zero size.
     * @return The total force (world space).
     */
    Vectormath::Aos::Vector3 GetTotalForce3D(HCollisionObject3D collision_object);

    /**
     * Return the total force currently applied to the specified 2D collision object.
     *
     * @param collision_object Which collision object to inspect. For objects with another type than COLLISION_OBJECT_TYPE_DYNAMIC, the force will always be of zero size.
     * @return The total force (world space).
     */
    Vectormath::Aos::Vector3 GetTotalForce2D(HCollisionObject2D collision_object);

    /**
     * Return the world position of the specified 3D collision object.
     *
     * @param collision_object Collision object handle
     * @return The world space position
     */
    Vectormath::Aos::Point3 GetWorldPosition3D(HCollisionObject3D collision_object);

    /**
     * Return the world position of the specified 2D collision object.
     *
     * @param collision_object Collision object handle
     * @return The world space position
     */
    Vectormath::Aos::Point3 GetWorldPosition2D(HCollisionObject2D collision_object);

    /**
     * Return the world rotation of the specified 3D collision object.
     *
     * @param collision_object Collision object handle
     * @return The world space rotation
     */
    Vectormath::Aos::Quat GetWorldRotation3D(HCollisionObject3D collision_object);

    /**
     * Return the world rotation of the specified 2D collision object.
     *
     * @param collision_object Collision object handle
     * @return The world space rotation
     */
    Vectormath::Aos::Quat GetWorldRotation2D(HCollisionObject2D collision_object);

    /**
     * Return the linear velocity of the 3D collision object.
     *
     * @param collision_object
     * @return The linear velocity.
     */
    Vectormath::Aos::Vector3 GetLinearVelocity3D(HCollisionObject3D collision_object);

    /**
     * Return the linear velocity of the 2D collision object.
     *
     * @param collision_object
     * @return The linear velocity.
     */
    Vectormath::Aos::Vector3 GetLinearVelocity2D(HCollisionObject2D collision_object);

    /**
     * Return the linear velocity of the 3D collision object.
     *
     * @param collision_object
     * @return The angular velocity. The direction of the vector coincides with the axis of rotation, the magnitude is the angle of rotation.
     */
    Vectormath::Aos::Vector3 GetAngularVelocity3D(HCollisionObject3D collision_object);

    /**
     * Return the linear velocity of the 2D collision object.
     *
     * @param collision_object
     * @return The angular velocity. The direction of the vector coincides with the axis of rotation, the magnitude is the angle of rotation.
     */
    Vectormath::Aos::Vector3 GetAngularVelocity2D(HCollisionObject2D collision_object);

    struct RayCastRequest;
    struct RayCastResponse;

    /**
     * Callback used to report ray cast response.
     * @param response Information about the result of the ray cast
     * @param request The request that the callback originated from
     */
    typedef void (*RayCastCallback)(const RayCastResponse& response, const RayCastRequest& request);

    /**
     * Container of data for ray cast queries.
     */
    struct RayCastRequest
    {
        RayCastRequest();

        /// Start of ray
        Vectormath::Aos::Point3 m_From;
        /// End of ray, exclusive since the ray is valid in [m_From, m_To)
        Vectormath::Aos::Point3 m_To;
        /// User supplied id to identify this query when the response is handled
        dmhash_t m_UserId;
        /// Bit field to filter out collision objects of the corresponding groups
        uint16_t m_Mask;
        /// All collision objects with this user data will be ignored in the ray cast
        void* m_IgnoredUserData;
        /// User supplied data that will be passed to the response callback
        void* m_UserData;
        /// Response callback function that will be called once the ray cast has been performed
        RayCastCallback m_Callback;
    };

    /**
     * Container of data for ray cast results.
     */
    struct RayCastResponse
    {
        RayCastResponse();

        /// Fraction between ray start and end at which the hit occured. The valid interval is [start, end), so 1.0f is considered outside the range
        float m_Fraction;
        /// Position at which the ray hit the surface
        Vectormath::Aos::Point3 m_Position;
        /// Normal of the surface at the position the ray hit the surface
        Vectormath::Aos::Vector3 m_Normal;
        /// User specified data for the object that the ray hit
        void* m_CollisionObjectUserData;
        /// Group of the object the ray hit
        uint16_t m_CollisionObjectGroup;
        /// If the ray hit something or not. If this is false, all other fields are invalid
        uint16_t m_Hit : 1;
    };

    /**
     * Request a ray cast that will be performed the next time the 3D world is updated
     *
     * @param world Physics world in which to perform the ray cast
     * @param request Struct containing data for the query
     */
    void RequestRayCast3D(HWorld3D world, const RayCastRequest& request);

    /**
     * Request a ray cast that will be performed the next time the 2D world is updated
     *
     * @param world Physics world in which to perform the ray cast
     * @param request Struct containing data for the query
     */
    void RequestRayCast2D(HWorld2D world, const RayCastRequest& request);

    /**
     * Callbacks used to draw the world for debugging purposes.
     */
    struct DebugCallbacks
    {
        DebugCallbacks();

        /**
         * Callback to draw multiple lines.
         *
         * @param points Array of points to draw. For n points, n/2 lines will be drawn between points <0,1>, <2,3>, etc.
         * @param point_count Number of points to draw, minimum is 2
         * @param color Color of the lines
         * @param user_data User data as supplied when registering the drawing callbacks
         */
        void (*m_DrawLines)(Vectormath::Aos::Point3* points, uint32_t point_count, Vectormath::Aos::Vector4 color, void* user_data);
        /**
         * Callback to draw multiple triangles.
         *
         * @param points Array of points to draw. For n points, n/3 triangles will be drawn between points <0,1,2>, <3,4,5>, etc.
         * @param point_count Number of points to draw, minimum is 3
         * @param color Color of the lines
         * @param user_data User data as supplied when registering the drawing callbacks
         */
        void (*m_DrawTriangles)(Vectormath::Aos::Point3* points, uint32_t point_count, Vectormath::Aos::Vector4 color, void* user_data);
        /// User data to be supplied to the callbacks
        void* m_UserData;
    };

    /**
     * Set functions to use when drawing debug data.
     *
     * @param context Context for which to set callbacks
     * @param callbacks New callback functions
     */
    void SetDebugCallbacks3D(HContext3D context, const DebugCallbacks& callbacks);

    /**
     * Set functions to use when drawing debug data.
     *
     * @param context Context for which to set callbacks
     * @param callbacks New callback functions
     */
    void SetDebugCallbacks2D(HContext2D context, const DebugCallbacks& callbacks);

    /**
     * Replace a shape with another shape for all 3D collision objects connected to that shape.
     *
     * @param context Context in which to replace shapes
     * @param old_shape The shape to disconnect
     * @param new_shape The shape to connect
     */
    void ReplaceShape3D(HContext3D context, HCollisionShape3D old_shape, HCollisionShape3D new_shape);

    /**
     * Replace a shape with another shape for all 2D collision objects connected to that shape.
     *
     * @param context Context in which to replace shapes
     * @param old_shape The shape to disconnect
     * @param new_shape The shape to connect
     */
    void ReplaceShape2D(HContext2D context, HCollisionShape2D old_shape, HCollisionShape2D new_shape);
}

#endif // PHYSICS_H
