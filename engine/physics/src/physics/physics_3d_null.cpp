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

    void DeleteContext3D(HContext3D context)
    {
    }

    dmMessage::HSocket GetSocket3D(HContext3D context)
    {
        return 0;
    }

    HWorld3D NewWorld3D(HContext3D context, const NewWorldParams& params)
    {
        return 0;
    }

    void DeleteWorld3D(HContext3D context, HWorld3D world)
    {
    }

    void StepWorld3D(HWorld3D world, const StepWorldContext& context)
    {
    }

    void SetDrawDebug3D(HWorld3D world, bool draw_debug)
    {
    }

    HCollisionShape3D NewSphereShape3D(HContext3D context, float radius)
    {
        return 0;
    }

    HCollisionShape3D NewBoxShape3D(HContext3D context, const Vectormath::Aos::Vector3& half_extents)
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

    void DeleteCollisionShape3D(HCollisionShape3D shape)
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

    void DeleteCollisionObject3D(HWorld3D world, HCollisionObject3D collision_object)
    {
    }

    uint32_t GetCollisionShapes3D(HCollisionObject3D collision_object, HCollisionShape3D* out_buffer, uint32_t buffer_size)
    {
        return 0;
    }

    void SetCollisionObjectUserData3D(HCollisionObject3D collision_object, void* user_data)
    {
    }

    void* GetCollisionObjectUserData3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    void ApplyForce3D(HContext3D context, HCollisionObject3D collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position)
    {
    }

    Vectormath::Aos::Vector3 GetTotalForce3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    Vectormath::Aos::Point3 GetWorldPosition3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Point3();
    }

    Vectormath::Aos::Quat GetWorldRotation3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Quat();
    }

    Vectormath::Aos::Vector3 GetLinearVelocity3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    Vectormath::Aos::Vector3 GetAngularVelocity3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return Vectormath::Aos::Vector3();
    }

    bool IsEnabled3D(HCollisionObject3D collision_object)
    {
        return false;
    }

    void SetEnabled3D(HWorld3D world, HCollisionObject3D collision_object, bool enabled)
    {
    }

    bool IsSleeping3D(HCollisionObject3D collision_object)
    {
        return false;
    }

    void SetLockedRotation3D(HCollisionObject3D collision_object, bool locked_rotation)
    {
    }

    float GetLinearDamping3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    void SetLinearDamping3D(HCollisionObject3D collision_object, float linear_damping)
    {
    }

    float GetAngularDamping3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    void SetAngularDamping3D(HCollisionObject3D collision_object, float angular_damping)
    {
    }

    float GetMass3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    void RequestRayCast3D(HWorld3D world, const RayCastRequest& request)
    {
    }

    void RayCast3D(HWorld3D world, const RayCastRequest& request, RayCastResponse& response)
    {
    }

    void SetDebugCallbacks3D(HContext3D context, const DebugCallbacks& callbacks)
    {
    }

    void ReplaceShape3D(HContext3D context, HCollisionShape3D old_shape, HCollisionShape3D new_shape)
    {
    }
}
