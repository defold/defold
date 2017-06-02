#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/transform.h>

#include "physics.h"

namespace dmPhysics
{
    HContext3D NewContext3D(const NewContextParams& params)
    {
        return 0;
    }

    HContext2D NewContext2D(const NewContextParams& params)
    {
        return 0;
    }

    void DeleteContext3D(HContext3D context)
    {
    }

    void DeleteContext2D(HContext2D context)
    {
    }

    dmMessage::HSocket GetSocket3D(HContext3D context)
    {
        return 0;
    }

    dmMessage::HSocket GetSocket2D(HContext2D context)
    {
        return 0;
    }

    HWorld3D NewWorld3D(HContext3D context, const NewWorldParams& params)
    {
        return 0;
    }

    HWorld2D NewWorld2D(HContext2D context, const NewWorldParams& params)
    {
        return 0;
    }

    void DeleteWorld3D(HContext3D context, HWorld3D world)
    {
    }

    void DeleteWorld2D(HContext2D context, HWorld2D world)
    {
    }

    void StepWorld3D(HWorld3D world, const StepWorldContext& context)
    {
    }

    void StepWorld2D(HWorld2D world, const StepWorldContext& context)
    {
    }

    void SetDrawDebug3D(HWorld3D world, bool draw_debug)
    {
    }

    void SetDrawDebug2D(HWorld2D world, bool draw_debug)
    {
    }

    HCollisionShape3D NewSphereShape3D(HContext3D context, float radius)
    {
        return 0;
    }

    HCollisionShape2D NewCircleShape2D(HContext2D context, float radius)
    {
        return 0;
    }

    HCollisionShape3D NewBoxShape3D(HContext3D context, const Vectormath::Aos::Vector3& half_extents)
    {
        return 0;
    }

    HCollisionShape2D NewBoxShape2D(HContext2D context, const Vectormath::Aos::Vector3& half_extents)
    {
        return 0;
    }

    HCollisionShape3D NewCapsuleShape3D(HContext3D context, float radius, float height)
    {
        return 0;
    }

    HCollisionShape3D NewConvexHullShape3D(HContext3D context, const float* vertices, uint32_t vertex_count)
    {
        return 0;
    }

    HCollisionShape2D NewPolygonShape2D(HContext2D context, const float* vertices, uint32_t vertex_count)
    {
        return 0;
    }

    HHullSet2D NewHullSet2D(HContext2D context, const float* vertices, uint32_t vertex_count,
                            const HullDesc* hulls, uint32_t hull_count)
    {
        return 0;
    }

    void DeleteHullSet2D(HHullSet2D hull_set)
    {

    }

    HCollisionShape2D NewGridShape2D(HContext2D context, HHullSet2D hull_set,
                                     const Vectormath::Aos::Point3& position,
                                     uint32_t cell_width, uint32_t cell_height,
                                     uint32_t row_count, uint32_t column_count)
     {
         return 0;
     }

    void SetGridShapeHull(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t row, uint32_t column, uint32_t hull, HullFlags flags)
    {
    }

    void SetCollisionObjectFilter(HCollisionObject2D collision_object,
                                  uint32_t shape, uint32_t child,
                                  uint16_t group, uint16_t mask)
    {
    }

    void DeleteCollisionShape3D(HCollisionShape3D shape)
    {
    }

    void DeleteCollisionShape2D(HCollisionShape2D shape)
    {
    }

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data, HCollisionShape3D* shapes, uint32_t shape_count)
    {
        return 0;
    }

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data,
                                            HCollisionShape3D* shapes,
                                            Vectormath::Aos::Vector3* translations,
                                            Vectormath::Aos::Quat* rotations,
                                            uint32_t shape_count)
    {
        return 0;
    }

    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D* shapes, uint32_t shape_count)
    {
        return 0;
    }

    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data,
                                            HCollisionShape2D* shapes,
                                            Vectormath::Aos::Vector3* translations,
                                            Vectormath::Aos::Quat* rotations,
                                            uint32_t shape_count)
    {
        return 0;
    }

    void DeleteCollisionObject3D(HWorld3D world, HCollisionObject3D collision_object)
    {
    }

    void DeleteCollisionObject2D(HWorld2D world, HCollisionObject2D collision_object)
    {
    }

    uint32_t GetCollisionShapes3D(HCollisionObject3D collision_object, HCollisionShape3D* out_buffer, uint32_t buffer_size)
    {
        return 0;
    }

    uint32_t GetCollisionShapes2D(HCollisionObject2D collision_object, HCollisionShape2D* out_buffer, uint32_t buffer_size)
    {
        return 0;
    }

    void SetCollisionObjectUserData3D(HCollisionObject3D collision_object, void* user_data)
    {
    }

    void SetCollisionObjectUserData2D(HCollisionObject2D collision_object, void* user_data)
    {
    }

    /**
     * Set 3D collision object user data
     *
     * @param collision_object Collision object
     * @return User data
     */
    void* GetCollisionObjectUserData3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    void* GetCollisionObjectUserData2D(HCollisionObject2D collision_object)
    {
        return 0;
    }

    void ApplyForce3D(HContext3D context, HCollisionObject3D collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position)
    {
    }

    void ApplyForce2D(HContext2D context, HCollisionObject2D collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position)
    {
    }

    Vectormath::Aos::Vector3 GetTotalForce3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    Vectormath::Aos::Vector3 GetTotalForce2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    Vectormath::Aos::Point3 GetWorldPosition3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Point3();
    }

    Vectormath::Aos::Point3 GetWorldPosition2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return Vectormath::Aos::Point3();
    }

    Vectormath::Aos::Quat GetWorldRotation3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Quat();
    }

    Vectormath::Aos::Quat GetWorldRotation2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return Vectormath::Aos::Quat();
    }

    Vectormath::Aos::Vector3 GetLinearVelocity3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    Vectormath::Aos::Vector3 GetLinearVelocity2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    Vectormath::Aos::Vector3 GetAngularVelocity3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    Vectormath::Aos::Vector3 GetAngularVelocity2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    bool IsEnabled3D(HCollisionObject3D collision_object)
    {
        return false;
    }

    bool IsEnabled2D(HCollisionObject2D collision_object)
    {
        return false;
    }

    void SetEnabled3D(HWorld3D world, HCollisionObject3D collision_object, bool enabled)
    {
    }

    void SetEnabled2D(HWorld2D world, HCollisionObject2D collision_object, bool enabled)
    {
    }

    bool IsSleeping3D(HCollisionObject3D collision_object)
    {
        return false;
    }

    bool IsSleeping2D(HCollisionObject3D collision_object)
    {
        return false;
    }

    void SetLockedRotation3D(HCollisionObject3D collision_object, bool locked_rotation)
    {
    }

    void SetLockedRotation2D(HCollisionObject2D collision_object, bool locked_rotation)
    {
    }

    float GetLinearDamping3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    float GetLinearDamping2D(HCollisionObject2D collision_object)
    {
        return 0;
    }

    void SetLinearDamping3D(HCollisionObject3D collision_object, float linear_damping)
    {
    }

    void SetLinearDamping2D(HCollisionObject2D collision_object, float linear_damping)
    {
    }

    float GetAngularDamping3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    float GetAngularDamping2D(HCollisionObject2D collision_object)
    {
        return 0;
    }

    void SetAngularDamping3D(HCollisionObject3D collision_object, float angular_damping)
    {
    }

    void SetAngularDamping2D(HCollisionObject2D collision_object, float angular_damping)
    {
    }

    float GetMass3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    float GetMass2D(HCollisionObject2D collision_object)
    {
        return 0;
    }


    void RequestRayCast3D(HWorld3D world, const RayCastRequest& request)
    {
    }

    void RequestRayCast2D(HWorld2D world, const RayCastRequest& request)
    {
    }

    void SetDebugCallbacks3D(HContext3D context, const DebugCallbacks& callbacks)
    {
    }

    void SetDebugCallbacks2D(HContext2D context, const DebugCallbacks& callbacks)
    {
    }

    void ReplaceShape3D(HContext3D context, HCollisionShape3D old_shape, HCollisionShape3D new_shape)
    {
    }

    void ReplaceShape2D(HContext2D context, HCollisionShape2D old_shape, HCollisionShape2D new_shape)
    {
    }

}
