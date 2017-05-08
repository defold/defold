#ifndef PHYSICS_H
#define PHYSICS_H

#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/transform.h>

#include <dmsdk/physics/physics.h>

namespace dmPhysics
{
    extern const char* PHYSICS_SOCKET_NAME;



    /// Empty cell value, see SetGridShapeHull
    const uint32_t GRIDSHAPE_EMPTY_CELL = 0xffffffff;

    /**
     * Create a new physics 3D context.
     *
     * @param params Parameters describing the context
     * @return the new context
     */
    HContext NewContext(const NewContextParams& params);


    /**
     * Destroy a 3D context.
     *
     * @param context Context to delete
     */
    void DeleteContext(HContext context);

    /**
     * Retrieve the message socket from a 3D context.
     * @return socket
     */
    dmMessage::HSocket GetSocket(HContext context);


    /**
     * Create a new 3D physics world
     *
     * @return 3D world
     */
    HWorld NewWorld(HContext context, const NewWorldParams& params);

    /**
     * Delete a 3D physics world
     *
     * @param world Physics world
     */
    void DeleteWorld(HContext context, HWorld world);


    /**
     * Simulate 3D physics
     *
     * @param world Physics world
     * @param context Function parameter struct
     */
    void StepWorld(HWorld world, const StepWorldContext& context);


    /**
     * Enable/disable debug-draw
     * @param world Physics world
     * @param draw_debug enable/disable
     */
    void SetDrawDebug(HWorld world, bool draw_debug);

    /**
     * Create a new 3D sphere shape.
     *
     * @param context Physics context
     * @param radius Sphere radius
     * @return Shape
     */
    HCollisionShape NewSphereShape(HContext context, float radius);

    /**
     * Create a new 3D box shape
     *
     * @param context Physics context
     * @param half_extents Box half extents
     * @return Shape
     */
    HCollisionShape NewBoxShape(HContext context, const Vectormath::Aos::Vector3& half_extents);


    /**
     * Create a new 3D capsule shape
     * @param context Physics context
     * @param radius Radius of top and bottom half-spheres of the capsule
     * @param height Height of the capsule; the distance between the two half-spheres
     * @return Shape
     */
    HCollisionShape NewCapsuleShape(HContext context, float radius, float height);

    /**
     * Create a new 3D convex hull shape
     *
     * @param context Physics context
     * @param vertices Vertices. x0, y0, z0, x1, ...
     * @param vertex_count Vertex count
     * @return Shape
     */
    HCollisionShape NewConvexHullShape(HContext context, const float* vertices, uint32_t vertex_count);

    /**
     * Create a new hull set. A hull set is a set of hulls
     * that can be shared by several grid-shapes
     * @param context Physics context
     * @param vertices vertices [x0, y0, x1, ...]
     * @param vertex_count vertex count
     * @param hulls hulls
     * @param hull_count hull count
     * @return new hull set
     */
    HHullSet2D NewHullSet2D(HContext context, const float* vertices, uint32_t vertex_count,
                            const HullDesc* hulls, uint32_t hull_count);

    /**
     * Delete a hull set
     * @param hull_set hull-set
     */
    void DeleteHullSet2D(HHullSet2D hull_set);

    /**
     * Create a new grid-shape
     * @param hull_set hull set to use. The handle to hull-set is
     * stored in grid-shape and the handle must be valid throughout the
     * life-time of the grid-shape
     * @param context Physics context
     * @param position local offset. when zero the boundary AABB is symmetric around local origin
     * @param cell_width cell width
     * @param cell_height cell height
     * @param cells_per_row
     * @param cells_per_column
     * @return new grid-snape
     */
    HCollisionShape NewGridShape2D(HContext context, HHullSet2D hull_set,
                                     const Vectormath::Aos::Point3& position,
                                     uint32_t cell_width, uint32_t cell_height,
                                     uint32_t row_count, uint32_t column_count);

    /**
     * Set hull for cell in grid-shape
     * @param collision_object collision object
     * @param shape_index index of the collision shape
     * @param row row
     * @param column column
     * @param hull hull index. Use GRIDSHAPE_EMPTY_CELL to clear the cell.
     */
    void SetGridShapeHull(HCollisionObject collision_object, uint32_t shape_index, uint32_t row, uint32_t column, uint32_t hull, HullFlags flags);

    /**
     * Set group and mask for collision object
     * @param collision_object collsion object
     * @param shape shape index.
     * @param child sub-shape index
     * @param group group to set
     * @param mask mask to set
     */
    void SetCollisionObjectFilter(HCollisionObject collision_object,
                                  uint32_t shape, uint32_t child,
                                  uint16_t group, uint16_t mask);

    /**
     * Delete a 3D shape
     *
     * @param shape Shape
     */
    void DeleteCollisionShape(HCollisionShape shape);


    /**
     * Create a new 3D collision object
     *
     * @note If the world has a registered callback to retrieve the world transform, it will be called to initialize the collision object.
     *
     * @param world Physics world
     * @param data @see CollisionObjectData
     * @param shapes pointer to a c-array of shapes
     * @param translations pointer to a c-array of per-shape translation. Can be null.
     * @param rotation pointer to a c-array of per-shape rotation. Can be null.
     * @param shape_count number of shapes in the array
     * @return A new collision object
     */
    HCollisionObject NewCollisionObject(HWorld world, const CollisionObjectData& data, HCollisionShape* shapes, uint32_t shape_count);

    /**
     * Create a new 3D collision object with per shape transform
     *
     * @note If the world has a registered callback to retrieve the world transform, it will be called to initialize the collision object.
     *
     * @param world Physics world
     * @param data @see CollisionObjectData
     * @param shapes pointer to a c-array of shapes
     * @param translations pointer to a c-array of per-shape translation
     * @param rotation pointer to a c-array of per-shape rotation
     * @param shape_count number of shapes in the array
     * @return A new collision object
     */

    HCollisionObject NewCollisionObjectTransform(HWorld world, const CollisionObjectData& data,
                                                HCollisionShape* shapes,
                                                Vectormath::Aos::Vector3* translations,
                                                Vectormath::Aos::Quat* rotations,
                                                uint32_t shape_count);

    /**
     * Delete a 3D collision object
     *
     * @param world Physics world
     * @param collision_object Collision object to delete
     */
    void DeleteCollisionObject(HWorld world, HCollisionObject collision_object);

    /**
     * Retrieve the shapes of a 3D collision object.
     *
     * @param collision_object Collision object
     * @param out_buffer buffer into which the shapes will be written
     * @param buffer_size the maximum size of the buffer
     * @return The number of shapes returned
     */
    uint32_t GetCollisionShapes(HCollisionObject collision_object, HCollisionShape* out_buffer, uint32_t buffer_size);

    /**
     * Set 3D collision object user data
     *
     * @param collision_object Collision object
     * @param user_data User data
     */
    void SetCollisionObjectUserData(HCollisionObject collision_object, void* user_data);

    /**
     * Set 3D collision object user data
     *
     * @param collision_object Collision object
     * @return User data
     */
    void* GetCollisionObjectUserData(HCollisionObject collision_object);

    /**
     * Apply a force to the specified 3D collision object at the specified position
     *
     * @param context Physics context
     * @param collision_object Collision object receiving the force, must be of type COLLISION_OBJECT_TYPE_DYNAMIC
     * @param force Force to be applied (world space)
     * @param position Position of where the force will be applied (world space)
     */
    void ApplyForce(HContext context, HCollisionObject collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position);

    /**
     * Return the total force currently applied to the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Which collision object to inspect. For objects with another type than COLLISION_OBJECT_TYPE_DYNAMIC, the force will always be of zero size.
     * @return The total force (world space).
     */
    Vectormath::Aos::Vector3 GetTotalForce(HContext context, HCollisionObject collision_object);

    /**
     * Return the world position of the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Collision object handle
     * @return The world space position
     */
    Vectormath::Aos::Point3 GetWorldPosition(HContext context, HCollisionObject collision_object);

    /**
     * Return the world rotation of the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Collision object handle
     * @return The world space rotation
     */
    Vectormath::Aos::Quat GetWorldRotation(HContext context, HCollisionObject collision_object);

    /**
     * Return the linear velocity of the 3D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @return The linear velocity.
     */
    Vectormath::Aos::Vector3 GetLinearVelocity(HContext context, HCollisionObject collision_object);

    /**
     * Return the linear velocity of the 3D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @return The angular velocity. The direction of the vector coincides with the axis of rotation, the magnitude is the angle of rotation.
     */
    Vectormath::Aos::Vector3 GetAngularVelocity(HContext context, HCollisionObject collision_object);

    /**
     * Return whether the 3D collision object is enabled or not.
     *
     * @param collision_object Collision object
     * @return true if the collision object is enabled
     */
    bool IsEnabled(HCollisionObject collision_object);

    /**
     * Set whether the 3D collision object is enabled or not.
     *
     * If a disabled dynamic collision object is enabled, it will have its internal state reset and assume the transform
     * from the transform callback before being being enabled.
     *
     * @param world World of the collision object
     * @param collision_object Collision object to enable/disable
     * @param enabled true if the object should be enabled, false otherwise
     */
    void SetEnabled(HWorld world, HCollisionObject collision_object, bool enabled);

    /**
     * Return whether the 3D collision object is sleeping or not.
     *
     * @param collision_object Collision object
     * @return true if the collision object is sleeping
     */
    bool IsSleeping(HCollisionObject collision_object);

    /**
     * Set whether the 3D collision object has locked rotation or not, which means that the angular velocity will always be 0.
     *
     * @param collision_object Collision object to lock
     * @param locked_rotation true to lock the rotation
     */
    void SetLockedRotation(HCollisionObject collision_object, bool locked_rotation);

    /**
     * Return the linear damping of the 3D collision object, which decreases the linear velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @return the linear damping
     */
    float GetLinearDamping(HCollisionObject collision_object);

    /**
     * Set the linear damping of the 3D collision object, which decreases the linear velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @param linear_damping Damping in the interval [0, 1]
     */
    void SetLinearDamping(HCollisionObject collision_object, float linear_damping);

    /**
     * Return the angular damping of the 3D collision object, which decreases the angular velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @return the linear damping
     */
    float GetAngularDamping(HCollisionObject collision_object);

    /**
     * Set the angular damping of the 3D collision object, which decreases the angular velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @param angular_damping Damping in the interval [0, 1]
     */
    void SetAngularDamping(HCollisionObject collision_object, float angular_damping);

    /**
     * Return the mass of the 3D collision object.
     *
     * @param collision_object Collision object
     * @return the total mass
     */
    float GetMass(HCollisionObject collision_object);

    /**
     * Request a ray cast that will be performed the next time the 3D world is updated
     *
     * @param world Physics world in which to perform the ray cast
     * @param request Struct containing data for the query
     */
    void RequestRayCast(HWorld world, const RayCastRequest& request);

    /**
     * Set functions to use when drawing debug data.
     *
     * @param context Context for which to set callbacks
     * @param callbacks New callback functions
     */
    void SetDebugCallbacks(HContext context, const DebugCallbacks& callbacks);

    /**
     * Replace a shape with another shape for all 3D collision objects connected to that shape.
     *
     * @param context Context in which to replace shapes
     * @param old_shape The shape to disconnect
     * @param new_shape The shape to connect
     */
    void ReplaceShape(HContext context, HCollisionShape old_shape, HCollisionShape new_shape);



}

#endif // PHYSICS_H
