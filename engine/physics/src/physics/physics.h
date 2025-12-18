// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_PHYSICS_H
#define DM_PHYSICS_H

#include <stdint.h>
#include <dmsdk/physics/physics.h>
#include <dmsdk/dlib/vmath.h>

#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/transform.h>

template <typename T> class dmArray;

namespace dmPhysics
{
    extern const char* PHYSICS_SOCKET_NAME;

    /// Collision object types defining how such an object will be moved.
    enum CollisionObjectType
    {
        COLLISION_OBJECT_TYPE_DYNAMIC,
        COLLISION_OBJECT_TYPE_KINEMATIC,
        COLLISION_OBJECT_TYPE_STATIC,
        COLLISION_OBJECT_TYPE_TRIGGER,
        COLLISION_OBJECT_TYPE_COUNT
    };

    enum JointType
    {
        JOINT_TYPE_SPRING,
        JOINT_TYPE_FIXED,
        JOINT_TYPE_HINGE,
        JOINT_TYPE_SLIDER,
        JOINT_TYPE_WELD,
        JOINT_TYPE_WHEEL,
        JOINT_TYPE_COUNT,

    };

    // Matches PhysicsResultString in script_physics.cpp
    enum JointResult
    {
        RESULT_OK = 0,
        RESULT_NOT_SUPPORTED = 1,
        RESULT_ID_EXISTS = 2,
        RESULT_ID_NOT_FOUND = 3,
        RESULT_NOT_CONNECTED = 4,
        RESULT_PHYSICS_WORLD_LOCKED = 5,
        RESULT_UNKNOWN_ERROR = 6,
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
    /// 2D collision object handle.
    typedef void* HCollisionObject2D;

    /// Generic Joint handle
    typedef void* HJoint;

    /// Empty cell value, see SetGridShapeHull
    const uint32_t GRIDSHAPE_EMPTY_CELL = 0xffffffff;

    /// Boundary values for allowed scaling
    static const float MIN_SCALE = 0.01f;
    static const float MAX_SCALE = 1.0f;

    /**
     * HullDesc structure
     */
    struct HullDesc
    {
        /// First vertex in hull
        uint16_t m_Index;
        /// Vertex count
        uint16_t m_Count;
    };

    struct HullFlags
    {
        HullFlags()
        : m_FlipHorizontal(0)
        , m_FlipVertical(0)
        , m_Rotate90(0)
        , m_Padding(0)
        {
        }

        uint8_t m_FlipHorizontal : 1;
        uint8_t m_FlipVertical : 1;
        uint8_t m_Rotate90 : 1;
        uint8_t m_Padding : 5;
    };

    /**
     * Callback used to propagate the world transform of an external object into the physics simulation.
     *
     * @param user_data User data pointing to the external object
     * @param world_transform World transform output parameter
     */
    typedef void (*GetWorldTransformCallback)(void* user_data, dmTransform::Transform& world_transform);
    /**
     * Callback used to propagate the world transform from the physics simulation to an external object.
     *
     * @param user_data User data pointing to the external object
     * @param position Position that the external object will obtain
     * @param rotation Rotation that the external object will obtain
     */
    typedef void (*SetWorldTransformCallback)(void* user_data, const dmVMath::Point3& position, const dmVMath::Quat& rotation);

    /**
     * Callback used to signal collisions.
     *
     * @param user_data_a User data pointing to the external object of the first colliding object
     * @param group_a Collision group of the first colliding object
     * @param user_data_b User data pointing to the external object of the second colliding object
     * @param group_b Collision group of the second colliding object
     * @param user_data User data supplied to StepWorldContext::m_CollisionUserData
     * @return If the collision signaling should be continued or not
     */
    typedef bool (*CollisionCallback)(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data);

    /**
     * Structure containing data about contact points
     */
    struct ContactPoint
    {
        /// Position of the first object
        dmVMath::Point3 m_PositionA;
        /// Position of the second object
        dmVMath::Point3 m_PositionB;
        /// Normal of the contact, pointing from A->B
        dmVMath::Vector3 m_Normal;
        /// Relative velocity between the objects
        dmVMath::Vector3 m_RelativeVelocity;
        /// User data of the first object
        void* m_UserDataA;
        /// User data of the second object
        void* m_UserDataB;
        /// Penetration of the objects at the contact
        float m_Distance;
        /// The impulse that resulted from the contact
        float m_AppliedImpulse;
        /// Mass of the first object
        float m_MassA;
        /// Mass of the second object
        float m_MassB;
        /// Collision group of the first object
        uint16_t m_GroupA;
        /// Collision group of the second object
        uint16_t m_GroupB;
    };

    /**
     * Callback used to signal contacts.
     *
     * @param contact_point Contact data
     * @param user_data User data supplied to StepWorldContext::m_ContactPointUserData
     * @return If the contact signaling should be continued or not
     */
    typedef bool (*ContactPointCallback)(const ContactPoint& contact_point, void* user_data);

    struct TriggerEnter
    {
        /// User data of the first object
        void* m_UserDataA;
        /// User data of the second object
        void* m_UserDataB;
        /// Collision group of the first object
        uint16_t m_GroupA;
        /// Collision group of the second object
        uint16_t m_GroupB;
    };
    typedef void (*TriggerEnteredCallback)(const TriggerEnter& trigger_enter, void* user_data);

    struct TriggerExit
    {
        /// User data of the first object
        void* m_UserDataA;
        /// User data of the second object
        void* m_UserDataB;
        /// Collision group of the first object
        uint16_t m_GroupA;
        /// Collision group of the second object
        uint16_t m_GroupB;
    };
    typedef void (*TriggerExitedCallback)(const TriggerExit& trigger_exit, void* user_data);

    struct RayCastRequest;
    struct RayCastResponse;

    /**
     * Callback used to report ray cast response.
     * @param response Information about the result of the ray cast
     * @param request The request that the callback originated from
     * @param user_data The user data as supplied to the StepWorldContext::m_RayCastUserData
     * @return 0 if no more callbacks are needed, non zero otherwise
     */
    typedef void (*RayCastCallback)(const RayCastResponse& response, const RayCastRequest& request, void* user_data);

    /**
     * Parameters to use when creating a context.
     */
    struct NewContextParams
    {
        NewContextParams();

        /// Gravity of the worlds created in this context
        dmVMath::Vector3 m_Gravity;
        /// Number of 3D worlds the context supports
        uint32_t m_WorldCount;
        /// How the physics worlds are scaled in relation to the game world
        float m_Scale;
        /// Contacts with impulses below this limit will not be reported through callbacks
        float m_ContactImpulseLimit;
        /// Contacts with penetration depths below this limit will not be considered inside a trigger
        float m_TriggerEnterLimit;
        /// Min velocity over which colliding objects will still bounce
        float m_VelocityThreshold;
        /// Maximum number of ray casts per frame when using 2D physics
        uint32_t m_RayCastLimit2D;
        /// Maximum number of ray casts per frame when using 3D physics
        uint32_t m_RayCastLimit3D;
        /// Maximum number of overlapping triggers
        uint32_t m_TriggerOverlapCapacity;
        /// If true, the collision objects will retrieve the position of its game object
        uint8_t m_AllowDynamicTransforms:1;
        uint8_t :7;
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
     * Retrieve the message socket from a 3D context.
     * @return socket
     */
    dmMessage::HSocket GetSocket3D(HContext3D context);

    /**
     * Retrieve the message socket from a 2D context.
     * @return socket
     */
    dmMessage::HSocket GetSocket2D(HContext2D context);

    /**
     * Parameters to use when creating a world.
     */
    struct NewWorldParams
    {
        NewWorldParams();

        /// world_min World min (AABB)
        dmVMath::Point3 m_WorldMin;
        /// world_max World max (AABB)
        dmVMath::Point3 m_WorldMax;
        /// param get_world_transform Callback for copying the transform from the corresponding user data to the collision object
        GetWorldTransformCallback m_GetWorldTransformCallback;
        /// param set_world_transform Callback for copying the transform from the collision object to the corresponding user data
        SetWorldTransformCallback m_SetWorldTransformCallback;
        /// max number of collision objects
        uint32_t m_MaxCollisionObjectsCount;
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

    HContext3D GetContext3D(HWorld3D world);

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

    void* GetWorldContext2D(HWorld2D world);
    void* GetCollisionObjectContext2D(HCollisionObject2D collision_object);

    /**
     * Arguments passed to the StepWorld* functions.
     */
    struct StepWorldContext
    {
        StepWorldContext();

        /// Time step
        float                   m_DT;
        bool                    m_FixedTimeStep;
        uint32_t                m_MaxFixedTimeSteps;
        uint32_t                m_Box2DVelocityIterations;
        uint32_t                m_Box2DPositionIterations;
        uint32_t                m_Box2DSubStepCount;
        /// Collision callback function
        CollisionCallback       m_CollisionCallback;
        /// Collision callback user data
        void*                   m_CollisionUserData;
        /// Contact point callback
        ContactPointCallback    m_ContactPointCallback;
        /// Contact point callback user data
        void*                   m_ContactPointUserData;
        /// Ray cast callback
        RayCastCallback         m_RayCastCallback;
        /// Ray cast callback user data
        void*                   m_RayCastUserData;
        /// Trigger entered callback
        TriggerEnteredCallback  m_TriggerEnteredCallback;
        /// Trigger entered callback user data
        void*                   m_TriggerEnteredUserData;
        /// Trigger exited callback
        TriggerExitedCallback   m_TriggerExitedCallback;
        /// Trigger exited callback
        void*                   m_TriggerExitedUserData;
    };

    /**
     * Simulate 3D physics
     *
     * @param world Physics world
     * @param context Function parameter struct
     */
    void StepWorld3D(HWorld3D world, const StepWorldContext& context);

    /**
     * Simulate 2D physics
     *
     * @param world Physics world
     * @param context Function parameter struct
     */
    void StepWorld2D(HWorld2D world, const StepWorldContext& context);

    /**
     * Enable/disable debug-draw
     * @param world Physics world
     * @param draw_debug enable/disable
     */
    void SetDrawDebug3D(HWorld3D world, bool draw_debug);

    /**
     * Enable/disable debug-draw
     * @param world Physics world
     * @param draw_debug enable/disable
     */
    void SetDrawDebug2D(HWorld2D world, bool draw_debug);

    /**
     * Create a new 3D sphere shape.
     *
     * @param context Physics context
     * @param radius Sphere radius
     * @return Shape
     */
    HCollisionShape3D NewSphereShape3D(HContext3D context, float radius);

    /**
     * Create a new 2D circle shape
     *
     * @param context Physics context
     * @param radius Circle radius
     * @return Shape
     */
    HCollisionShape2D NewCircleShape2D(HContext2D context, float radius);

    /**
     * Create a new 3D box shape
     *
     * @param context Physics context
     * @param half_extents Box half extents
     * @return Shape
     */
    HCollisionShape3D NewBoxShape3D(HContext3D context, const dmVMath::Vector3& half_extents);

    /**
     * Create a new 2D box shape
     *
     * @param context Physics context
     * @param half_extents Box half extents
     * @return Shape
     */
    HCollisionShape2D NewBoxShape2D(HContext2D context, const dmVMath::Vector3& half_extents);

    /**
     * Create a new 3D capsule shape
     * @param context Physics context
     * @param radius Radius of top and bottom half-spheres of the capsule
     * @param height Height of the capsule; the distance between the two half-spheres
     * @return Shape
     */
    HCollisionShape3D NewCapsuleShape3D(HContext3D context, float radius, float height);

    /**
     * Create a new 3D convex hull shape
     *
     * @param context Physics context
     * @param vertices Vertices. x0, y0, z0, x1, ...
     * @param vertex_count Vertex count
     * @return Shape
     */
    HCollisionShape3D NewConvexHullShape3D(HContext3D context, const float* vertices, uint32_t vertex_count);

    /**
     * Create a new 2D polygon shape
     *
     * @param context Physics context
     * @param vertices Vertices. x0, y0, x1, ...
     * @param vertex_count Vertex count
     * @return Shape
     */
    HCollisionShape2D NewPolygonShape2D(HContext2D context, const float* vertices, uint32_t vertex_count);

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
    HHullSet2D NewHullSet2D(HContext2D context, const float* vertices, uint32_t vertex_count,
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
    HCollisionShape2D NewGridShape2D(HContext2D context, HHullSet2D hull_set,
                                     const dmVMath::Point3& position,
                                     uint32_t cell_width, uint32_t cell_height,
                                     uint32_t row_count, uint32_t column_count);

    void ClearGridShapeHulls(HCollisionObject2D collision_object);

    /**
     * Set hull for cell in grid-shape
     * @param collision_object collision object
     * @param shape_index index of the collision shape
     * @param row row
     * @param column column
     * @param hull hull index. Use GRIDSHAPE_EMPTY_CELL to clear the cell.
     * @return true if successful
     */
    bool SetGridShapeHull(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t row, uint32_t column, uint32_t hull, HullFlags flags);

    /**
     * Enable or disable a grid shape (layer)
     * @param shape_index index of the collision shape
     * @param enable true if the layer should be enabled
     * @return true if successful
     */
    bool SetGridShapeEnable(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t enable);

    /**
     * Set group and mask for collision object
     * @param collision_object collsion object
     * @param shape shape index.
     * @param child sub-shape index
     * @param group group to set
     * @param mask mask to set
     */
    void SetCollisionObjectFilter(HCollisionObject2D collision_object,
                                  uint32_t shape, uint32_t child,
                                  uint16_t group, uint16_t mask);

    /**
     * Create a grid shape for a layer and a cell index denoted by shape_index and child
     * @param collision_object collision object
     * @param shape_index shape index
     * @param child sub-shape index
     */
    void CreateGridCellShape(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t child);

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
        /// Linear damping, 1 is full damping and 0 is no damping
        float m_LinearDamping;
        /// Angular damping, 1 is full damping and 0 is no damping
        float m_AngularDamping;
        /// Collision filter group. Two objects a and b are tested for collision if a.group & b.mask != 0 && a.mask & b.group != 0. Default is 1
        uint16_t m_Group;
        /// Collision filter mask @see m_Group. Default is 1.
        uint16_t m_Mask;
        /// Locked rotation keeps the angular velocity at 0
        uint16_t m_LockedRotation : 1;
        /// Bullet determings if this behaves like a bullet (fast moving object) or not
        uint16_t m_Bullet : 1;
        /// Whether the object is enabled from the start or not, default is 1
        uint16_t m_Enabled : 1;
        uint16_t :14;
    };

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
    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data, HCollisionShape3D* shapes, uint32_t shape_count);

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

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data,
                                            HCollisionShape3D* shapes,
                                            dmVMath::Vector3* translations,
                                            dmVMath::Quat* rotations,
                                            uint32_t shape_count);

    /**
     * Create a new 2D collision object
     *
     * @note If the world has a registered callback to retrieve the world transform, it will be called to initialize the collision object.
     *
     * @param world Physics world
     * @param data @see CollisionObjectData
     * @param shapes pointer to a c-array of shapes
     * @param shape_count number of shapes in the array
     * @return A new collision object
     */
    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D* shapes, uint32_t shape_count);

    /**
     * Create a new 2D collision object with per shape transform
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
    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data,
                                            HCollisionShape2D* shapes,
                                            dmVMath::Vector3* translations,
                                            dmVMath::Quat* rotations,
                                            uint32_t shape_count);

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
     * Retrieve the shapes of a 3D collision object.
     *
     * @param collision_object Collision object
     * @param out_buffer buffer into which the shapes will be written
     * @param buffer_size the maximum size of the buffer
     * @return The number of shapes returned
     */
    uint32_t GetCollisionShapes3D(HCollisionObject3D collision_object, HCollisionShape3D* out_buffer, uint32_t buffer_size);

    /**
     * Retrieve the shapes of a 2D collision object.
     *
     * @param collision_object Collision object
     * @param out_buffer buffer into which the shapes will be written
     * @param buffer_size the maximum size of the buffer
     * @return The number of shapes returned
     */
    uint32_t GetCollisionShapes2D(HCollisionObject2D collision_object, HCollisionShape2D* out_buffer, uint32_t buffer_size);

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
     * @param context Physics context
     * @param collision_object Collision object receiving the force, must be of type COLLISION_OBJECT_TYPE_DYNAMIC
     * @param force Force to be applied (world space)
     * @param position Position of where the force will be applied (world space)
     */
    void ApplyForce3D(HContext3D context, HCollisionObject3D collision_object, const dmVMath::Vector3& force, const dmVMath::Point3& position);

    /**
     * Apply a force to the specified 2D collision object at the specified position
     *
     * @param context Physics context
     * @param collision_object Collision object receiving the force, must be of type COLLISION_OBJECT_TYPE_DYNAMIC
     * @param force Force to be applied (world space)
     * @param position Position of where the force will be applied (world space)
     */
    void ApplyForce2D(HContext2D context, HCollisionObject2D collision_object, const dmVMath::Vector3& force, const dmVMath::Point3& position);

    /**
     * Return the total force currently applied to the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Which collision object to inspect. For objects with another type than COLLISION_OBJECT_TYPE_DYNAMIC, the force will always be of zero size.
     * @return The total force (world space).
     */
    dmVMath::Vector3 GetTotalForce3D(HContext3D context, HCollisionObject3D collision_object);

    /**
     * Return the total force currently applied to the specified 2D collision object.
     *
     * @param context Physics context
     * @param collision_object Which collision object to inspect. For objects with another type than COLLISION_OBJECT_TYPE_DYNAMIC, the force will always be of zero size.
     * @return The total force (world space).
     */
    dmVMath::Vector3 GetTotalForce2D(HContext2D context, HCollisionObject2D collision_object);

    /**
     * Return the world position of the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Collision object handle
     * @return The world space position
     */
    dmVMath::Point3 GetWorldPosition3D(HContext3D context, HCollisionObject3D collision_object);

    /**
     * Return the world position of the specified 2D collision object.
     *
     * @param context Physics context
     * @param collision_object Collision object handle
     * @return The world space position
     */
    dmVMath::Point3 GetWorldPosition2D(HContext2D context, HCollisionObject2D collision_object);

    /**
     * Return the world rotation of the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Collision object handle
     * @return The world space rotation
     */
    dmVMath::Quat GetWorldRotation3D(HContext3D context, HCollisionObject3D collision_object);

    /**
     * Return the world rotation of the specified 2D collision object.
     *
     * @param context Physics context
     * @param collision_object Collision object handle
     * @return The world space rotation
     */
    dmVMath::Quat GetWorldRotation2D(HContext2D context, HCollisionObject2D collision_object);

    /**
     * Return the linear velocity of the 3D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @return The linear velocity.
     */
    dmVMath::Vector3 GetLinearVelocity3D(HContext3D context, HCollisionObject3D collision_object);

    /**
     * Return the linear velocity of the 2D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @return The linear velocity.
     */
    dmVMath::Vector3 GetLinearVelocity2D(HContext2D context, HCollisionObject2D collision_object);

    /**
     * Set the linear velocity of the 3D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @param linear_velocity the linear velocity
     */
    void SetLinearVelocity3D(HContext3D context, HCollisionObject3D collision_object, const dmVMath::Vector3& linear_velocity);

    /**
     * Set the linear velocity of the 2D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @param linear_velocity the linear velocity
     */
    void SetLinearVelocity2D(HContext2D context, HCollisionObject2D collision_object, const dmVMath::Vector3& linear_velocity);

    /**
     * Return the linear velocity of the 3D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @return The angular velocity. The direction of the vector coincides with the axis of rotation, the magnitude is the angle of rotation.
     */
    dmVMath::Vector3 GetAngularVelocity3D(HContext3D context, HCollisionObject3D collision_object);

    /**
     * Return the linear velocity of the 2D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @return The angular velocity. The direction of the vector coincides with the axis of rotation, the magnitude is the angle of rotation.
     */
    dmVMath::Vector3 GetAngularVelocity2D(HContext2D context, HCollisionObject2D collision_object);

    /**
     * Set the angular velocity of the 3D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @param angular_velocity the angular velocity
     */
    void SetAngularVelocity3D(HContext3D context, HCollisionObject3D collision_object, const dmVMath::Vector3& angular_velocity);

    /**
     * Set the angular velocity of the 2D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @param velocity the angular velocity (uses the Z component.)
     */
    void SetAngularVelocity2D(HContext2D context, HCollisionObject2D collision_object, const dmVMath::Vector3& angular_velocity);

    /**
     * Return whether the 3D collision object is enabled or not.
     *
     * @param collision_object Collision object
     * @return true if the collision object is enabled
     */
    bool IsEnabled3D(HCollisionObject3D collision_object);

    /**
     * Return whether the 2D collision object is enabled or not.
     *
     * @param collision_object Collision object
     * @return true if the collision object is enabled
     */
    bool IsEnabled2D(HCollisionObject2D collision_object);

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
    void SetEnabled3D(HWorld3D world, HCollisionObject3D collision_object, bool enabled);

    /**
     * Set whether the 2D collision object is enabled or not.
     *
     * If a disabled dynamic collision object is enabled, it will have its internal state reset and assume the transform
     * from the transform callback before being being enabled.
     *
     * @param world World of the collision object
     * @param collision_object Collision object to enable/disable
     * @param enabled true if the object should be enabled, false otherwise
     */
    void SetEnabled2D(HWorld2D world, HCollisionObject2D collision_object, bool enabled);

    /**
     * Return whether the 3D collision object is sleeping or not.
     *
     * @param collision_object Collision object
     * @return true if the collision object is sleeping
     */
    bool IsSleeping3D(HCollisionObject3D collision_object);

    /**
     * Return whether the 2D collision object is sleeping or not.
     *
     * @param collision_object Collision object
     * @return true if the collision object is sleeping
     */
    bool IsSleeping2D(HCollisionObject3D collision_object);

    /**
     * Activate 3D collision object
     *
     * @param collision_object
     */
    void Wakeup3D(HCollisionObject3D collision_object);

    /**
     * Wake up collision object
     *
     * @param collision_object
     */
    void Wakeup2D(HCollisionObject2D collision_object);

    /**
     * Set whether the 3D collision object has locked rotation or not, which means that the angular velocity will always be 0.
     *
     * @param collision_object Collision object to lock
     * @param locked_rotation true to lock the rotation
     */
    void SetLockedRotation3D(HCollisionObject3D collision_object, bool locked_rotation);

    /**
     * Set whether the 2D collision object has locked rotation or not, which means that the angular velocity will always be 0.
     *
     * @param collision_object Collision object to lock
     * @param locked_rotation true to lock the rotation
     */
    void SetLockedRotation2D(HCollisionObject2D collision_object, bool locked_rotation);

    /**
     * Return the linear damping of the 3D collision object, which decreases the linear velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @return the linear damping
     */
    float GetLinearDamping3D(HCollisionObject3D collision_object);

    /**
     * Return the linear damping of the 2D collision object, which decreases the linear velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @return the linear damping
     */
    float GetLinearDamping2D(HCollisionObject2D collision_object);

    /**
     * Set the linear damping of the 3D collision object, which decreases the linear velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @param linear_damping Damping in the interval [0, 1]
     */
    void SetLinearDamping3D(HCollisionObject3D collision_object, float linear_damping);

    /**
     * Set the linear damping of the 2D collision object, which decreases the linear velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @param linear_damping Damping in the interval [0, 1]
     */
    void SetLinearDamping2D(HCollisionObject2D collision_object, float linear_damping);

    /**
     * Return the angular damping of the 3D collision object, which decreases the angular velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @return the linear damping
     */
    float GetAngularDamping3D(HCollisionObject3D collision_object);

    /**
     * Return the angular damping of the 2D collision object, which decreases the angular velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @return the linear damping
     */
    float GetAngularDamping2D(HCollisionObject2D collision_object);

    /**
     * Set the angular damping of the 3D collision object, which decreases the angular velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @param angular_damping Damping in the interval [0, 1]
     */
    void SetAngularDamping3D(HCollisionObject3D collision_object, float angular_damping);

    /**
     * Set the angular damping of the 2D collision object, which decreases the angular velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @param angular_damping Damping in the interval [0, 1]
     */
    void SetAngularDamping2D(HCollisionObject2D collision_object, float angular_damping);

    /**
     * Return the mass of the 3D collision object.
     *
     * @param collision_object Collision object
     * @return the total mass
     */
    float GetMass3D(HCollisionObject3D collision_object);

    /**
     * Return the mass of the 2D collision object.
     *
     * @param collision_object Collision object
     * @return the total mass
     */
    float GetMass2D(HCollisionObject2D collision_object);

    /**
     * Return whether this 2D collision object is a bullet or not. Technically a _bullet_
     * is a fast moving collision object requiring special handling by the underlying
     * physics engine.
     *
     * @param collision_object The affected collision object
     * @return true if is indeed a bullet
     */
    bool IsBullet2D(HCollisionObject2D collision_object);

    /**
     * Makes this collision object a bullet or not.
     *
     * @param collision_object The affected collision object
     * @param value true to make this a bullet, false otherwise
     */
    void SetBullet2D(HCollisionObject2D collision_object, bool value);

    uint16_t GetGroup2D(HWorld2D world, HCollisionObject2D collision_object);
    void SetGroup2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit);
    bool GetMaskBit2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit);
    void SetMaskBit2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit, bool boolvalue);
    bool UpdateMass2D(HWorld2D world, HCollisionObject2D collision_object, float mass);

    uint16_t GetGroup3D(HCollisionObject3D collision_object);
    void SetGroup3D(HWorld3D world, HCollisionObject3D collision_object, uint16_t groupbit);
    bool GetMaskBit3D(HCollisionObject3D collision_object, uint16_t groupbit);
    void SetMaskBit3D(HWorld3D world, HCollisionObject3D collision_object, uint16_t groupbit, bool boolvalue);

    /**
     * Container of data for ray cast queries.
     */
    struct RayCastRequest
    {
        RayCastRequest();

        /// Start of ray
        dmVMath::Point3 m_From;
        /// End of ray, exclusive since the ray is valid in [m_From, m_To)
        dmVMath::Point3 m_To;
        /// All collision objects with this user data will be ignored in the ray cast
        void* m_IgnoredUserData;
        /// User supplied data that will be passed to the response callback
        void* m_UserData;
        /// Bit field to filter out collision objects of the corresponding groups
        uint16_t m_Mask;

        uint16_t m_ReturnAllResults:1;
        uint16_t :15;

        /// User supplied id to identify this query when the response is handled
        uint32_t m_UserId;
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
        dmVMath::Point3 m_Position;
        /// Normal of the surface at the position the ray hit the surface
        dmVMath::Vector3 m_Normal;
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
     * @return result True if successful
     */
    bool RequestRayCast3D(HWorld3D world, const RayCastRequest& request);

    /**
     * Request a ray cast that will be performed the next time the 2D world is updated
     *
     * @param world Physics world in which to perform the ray cast
     * @param request Struct containing data for the query
     * @return result True if successful
     */
    bool RequestRayCast2D(HWorld2D world, const RayCastRequest& request);

    /**
     * Request a synchronous ray cast
     *
     * @param world Physics world in which to perform the ray cast
     * @param request Struct containing data for the query
     * @param results Array receiving the raycast hits. Array is sorted with lowest hit time first.
     * @note The result array may grow during the call
     */
    void RayCast3D(HWorld3D world, const RayCastRequest& request, dmArray<RayCastResponse>& results);

    /**
     * Request a synchronous ray cast
     *
     * @param world Physics world in which to perform the ray cast
     * @param request Struct containing data for the query
     * @param results Array receiving the raycast hits. Array is sorted with lowest hit time first.
     * @note The result array may grow during the call
     */
    void RayCast2D(HWorld2D world, const RayCastRequest& request, dmArray<RayCastResponse>& results);

    /**
     * Set the gravity for a 2D physics world.
     *
     * @param world Physics world for which to set the gravity
     * @param gravity Gravity vector (z component will be ignored).
     */
    void SetGravity2D(HWorld2D world, const dmVMath::Vector3& gravity);

    /**
     * Set the gravity for a 3D physics world.
     *
     * @param world Physics world for which to set the gravity
     * @param gravity Gravity vector.
     */
    void SetGravity3D(HWorld3D world, const dmVMath::Vector3& gravity);

    /**
     * Get the gravity for 2D physics world.
     *
     * @param world Physics world for which to get the gravity
     */
    dmVMath::Vector3 GetGravity2D(HWorld2D world);

    /**
     * Get the gravity for 3D physics world.
     *
     * @param world Physics world for which to get the gravity
     */
    dmVMath::Vector3 GetGravity3D(HWorld3D world);


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
        void (*m_DrawLines)(dmVMath::Point3* points, uint32_t point_count, dmVMath::Vector4 color, void* user_data);
        /**
         * Callback to draw multiple triangles.
         *
         * @param points Array of points to draw. For n points, n/3 triangles will be drawn between points <0,1,2>, <3,4,5>, etc.
         * @param point_count Number of points to draw, minimum is 3
         * @param color Color of the lines
         * @param user_data User data as supplied when registering the drawing callbacks
         */
        void (*m_DrawTriangles)(dmVMath::Point3* points, uint32_t point_count, dmVMath::Vector4 color, void* user_data);
        /// User data to be supplied to the callbacks
        void* m_UserData;
        /// Alpha to use for everything rendered
        float m_Alpha;
        /// Scale from game world to physics world
        float m_Scale;
        /// Scale from physics world to game world
        float m_InvScale;
        /// Scale to use for rendered debug graphics (transforms, arrows, etc)
        float m_DebugScale;
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

    struct ConnectJointParams
    {
        bool m_CollideConnected;

        union {
            struct
            {
                float m_Length;
                float m_FrequencyHz;
                float m_DampingRatio;
            } m_SpringJointParams;

            struct
            {
                float m_MaxLength;
            } m_FixedJointParams;

            struct
            {
                float m_JointAngle; // read only
                float m_JointSpeed; // read only
                float m_ReferenceAngle;
                float m_LowerAngle;
                float m_UpperAngle;
                float m_MaxMotorTorque;
                float m_MotorSpeed;
                bool m_EnableLimit;
                bool m_EnableMotor;
            } m_HingeJointParams;

            struct
            {
                float m_JointTranslation; // read only
                float m_JointSpeed; // read only
                float m_LocalAxisA[3];
                float m_ReferenceAngle;
                bool m_EnableLimit;
                float m_LowerTranslation;
                float m_UpperTranslation;
                bool m_EnableMotor;
                float m_MaxMotorForce;
                float m_MotorSpeed;
            } m_SliderJointParams;

            struct
            {
                float m_ReferenceAngle;
                float m_FrequencyHz;
                float m_DampingRatio;
            } m_WeldJointParams;

            struct
            {
                float m_JointTranslation; // read only
                float m_JointSpeed; // read only
                float m_LocalAxisA[3];
                float m_MaxMotorTorque;
                float m_MotorSpeed;
                bool  m_EnableMotor;
                float m_FrequencyHz;
                float m_DampingRatio;
            } m_WheelJointParams;
        };

        ConnectJointParams()
        {
        }

        ConnectJointParams(JointType type)
        {
            m_CollideConnected = false;
            switch (type)
            {
                case JOINT_TYPE_SPRING:
                    m_SpringJointParams.m_Length = 1.0f;
                    m_SpringJointParams.m_FrequencyHz = 0.0f;
                    m_SpringJointParams.m_DampingRatio = 0.0f;
                    break;

                case JOINT_TYPE_FIXED:
                    m_FixedJointParams.m_MaxLength = 0.0f;
                    break;
                case JOINT_TYPE_HINGE:
                    m_HingeJointParams.m_ReferenceAngle = 0.0f;
                    m_HingeJointParams.m_LowerAngle = 0.0f;
                    m_HingeJointParams.m_UpperAngle = 0.0f;
                    m_HingeJointParams.m_MaxMotorTorque = 0.0f;
                    m_HingeJointParams.m_MotorSpeed = 0.0f;
                    m_HingeJointParams.m_EnableLimit = false;
                    m_HingeJointParams.m_EnableMotor = false;
                    break;
                case JOINT_TYPE_SLIDER:
                    m_SliderJointParams.m_LocalAxisA[0] = 1.0f;
                    m_SliderJointParams.m_LocalAxisA[1] = 0.0f;
                    m_SliderJointParams.m_LocalAxisA[2] = 0.0f;
                    m_SliderJointParams.m_ReferenceAngle = 0.0f;
                    m_SliderJointParams.m_EnableLimit = false;
                    m_SliderJointParams.m_LowerTranslation = 0.0f;
                    m_SliderJointParams.m_UpperTranslation = 0.0f;
                    m_SliderJointParams.m_EnableMotor = false;
                    m_SliderJointParams.m_MaxMotorForce = 0.0f;
                    m_SliderJointParams.m_MotorSpeed = 0.0f;
                    break;
                case JOINT_TYPE_WELD:
                    m_WeldJointParams.m_ReferenceAngle = 0.0f;
                    m_WeldJointParams.m_FrequencyHz = 0.0f;
                    m_WeldJointParams.m_DampingRatio = 0.0f;
                    break;
                case JOINT_TYPE_WHEEL:
                    m_WheelJointParams.m_LocalAxisA[0] = 1.0f;
                    m_WheelJointParams.m_LocalAxisA[1] = 0.0f;
                    m_WheelJointParams.m_LocalAxisA[2] = 0.0f;
                    m_WheelJointParams.m_MaxMotorTorque = 0.0f;
                    m_WheelJointParams.m_MotorSpeed = 0.0f;
                    m_WheelJointParams.m_EnableMotor = false;
                    m_WheelJointParams.m_FrequencyHz = 0.0f;
                    m_WheelJointParams.m_DampingRatio = 0.0f;
                    break;
                default:
                    break;
            }
        }
    };

    HJoint CreateJoint2D(HWorld2D world, HCollisionObject2D obj_a, const dmVMath::Point3& pos_a, HCollisionObject2D obj_b, const dmVMath::Point3& pos_b, dmPhysics::JointType type, const ConnectJointParams& params);
    bool GetJointParams2D(HWorld2D world, HJoint joint, dmPhysics::JointType type, ConnectJointParams& params);
    bool SetJointParams2D(HWorld2D world, HJoint joint, dmPhysics::JointType type, const ConnectJointParams& params);
    void DeleteJoint2D(HWorld2D world, HJoint joint);
    bool GetJointReactionForce2D(HWorld2D world, HJoint joint, dmVMath::Vector3& force, float inv_dt);
    bool GetJointReactionTorque2D(HWorld2D world, HJoint joint, float& torque, float inv_dt);
    void FlipH2D(HWorld2D world, HCollisionObject2D collision_object);
    void FlipV2D(HWorld2D world, HCollisionObject2D collision_object);
    bool IsWorldLocked(HWorld2D world);

    void              ReplaceShape3D(HCollisionObject3D object, HCollisionShape3D old_shape, HCollisionShape3D new_shape);
    HCollisionShape3D GetCollisionShape3D(HCollisionObject3D collision_object, uint32_t index);
    void              GetCollisionShapeRadius3D(HCollisionShape3D shape, float* radius);
    void              GetCollisionShapeHalfBoxExtents3D(HCollisionShape3D shape, float* xyz);
    void              GetCollisionShapeCapsuleRadiusHeight3D(HCollisionShape3D shape, float* radius, float* half_height);
    void              SetCollisionShapeRadius3D(HCollisionShape3D shape, float radius);
    void              SetCollisionShapeHalfBoxExtents3D(HCollisionShape2D shape, float w, float h, float d);

    void              GetCollisionShapeRadius2D(HWorld2D world, HCollisionShape2D shape, float* radius);
    void              GetCollisionShapeBoxDimensions2D(HWorld2D world, HCollisionShape2D shape, dmVMath::Quat rotation, float& w, float& h);
    HCollisionShape2D GetCollisionShape2D(HWorld2D world, HCollisionObject2D collision_object, uint32_t shape_index);
    void              SetCollisionShapeRadius2D(HWorld2D world, HCollisionShape2D shape, float radius);
    void              SetCollisionShapeBoxDimensions2D(HWorld2D world, HCollisionShape2D shape, dmVMath::Quat rotation, float w, float h);
    void              SynchronizeObject2D(HWorld2D world, HCollisionObject2D collision_object);
}

#endif // DM_PHYSICS_H
