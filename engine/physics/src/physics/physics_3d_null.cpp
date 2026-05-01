// Copyright 2020-2026 The Defold Foundation
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

#include <stdint.h>

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

    HCollisionShape3D NewBoxShape3D(HContext3D context, const dmVMath::Vector3& half_extents)
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
                                            dmVMath::Vector3* translations,
                                            dmVMath::Quat* rotations,
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

    void ApplyForce3D(HContext3D context, HCollisionObject3D collision_object, const dmVMath::Vector3& force, const dmVMath::Point3& position)
    {
    }

    dmVMath::Vector3 GetTotalForce3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return dmVMath::Vector3();
    }

    dmVMath::Point3 GetWorldPosition3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return dmVMath::Point3();
    }

    dmVMath::Quat GetWorldRotation3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return dmVMath::Quat();
    }

    dmVMath::Vector3 GetLinearVelocity3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return dmVMath::Vector3();
    }

    dmVMath::Vector3 GetAngularVelocity3D(HContext3D context, HCollisionObject3D collision_object)
    {
        return dmVMath::Vector3();
    }

    void SetLinearVelocity3D(HContext3D context, HCollisionObject3D collision_object, const dmVMath::Vector3& velocity)
    {
    }

    void SetAngularVelocity3D(HContext3D context, HCollisionObject3D collision_object, const dmVMath::Vector3& velocity)
    {
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

    void Wakeup3D(HCollisionObject3D collision_object)
    {
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

    uint16_t GetGroup3D(HCollisionObject3D collision_object)
    {
        return 0;
    }

    void SetGroup3D(HWorld3D world, HCollisionObject3D collision_object, uint16_t groupbit)
    {
    }

    bool GetMaskBit3D(HCollisionObject3D collision_object, uint16_t groupbit)
    {
        return false;
    }

    void SetMaskBit3D(HWorld3D world, HCollisionObject3D collision_object, uint16_t groupbit, bool boolvalue)
    {
    }

    bool RequestRayCast3D(HWorld3D world, const RayCastRequest& request)
    {
        return true;
    }

    void RayCast3D(HWorld3D world, const RayCastRequest& request, dmArray<RayCastResponse>& results)
    {
    }

    void SetGravity3D(HWorld3D world, const dmVMath::Vector3& gravity)
    {
    }

    dmVMath::Vector3 GetGravity3D(HWorld3D world)
    {
        return dmVMath::Vector3(0.0f);
    }

    void SetDebugCallbacks3D(HContext3D context, const DebugCallbacks& callbacks)
    {
    }

    void ReplaceShape3D(HContext3D context, HCollisionShape3D old_shape, HCollisionShape3D new_shape)
    {
    }

    HContext3D GetContext3D(HWorld3D world)
    {
        return 0;
    }


    void ReplaceShape3D(HCollisionObject3D object, HCollisionShape3D old_shape, HCollisionShape3D new_shape)
    {
    }

    HCollisionShape3D GetCollisionShape3D(HCollisionObject3D collision_object, uint32_t index)
    {
        return 0;
    }

    void GetCollisionShapeRadius3D(HCollisionShape3D shape, float* radius)
    {
    }

    void GetCollisionShapeHalfBoxExtents3D(HCollisionShape3D shape, float* xyz)
    {
    }

    void GetCollisionShapeCapsuleRadiusHeight3D(HCollisionShape3D shape, float* radius, float* half_height)
    {
    }

    void SetCollisionShapeRadius3D(HCollisionShape3D shape, float radius)
    {
    }

    void SetCollisionShapeHalfBoxExtents3D(HCollisionShape2D shape, float w, float h, float d)
    {
    }
}
