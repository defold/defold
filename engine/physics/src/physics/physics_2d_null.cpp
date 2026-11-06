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
    HContext2D NewContext2D(const NewContextParams& params)
    {
        return 0;
    }

    void DeleteContext2D(HContext2D context)
    {
    }

    dmMessage::HSocket GetSocket2D(HContext2D context)
    {
        return 0;
    }

    HWorld2D NewWorld2D(HContext2D context, const NewWorldParams& params)
    {
        return 0;
    }

    void DeleteWorld2D(HContext2D context, HWorld2D world)
    {
    }

    void StepWorld2D(HWorld2D world, const StepWorldContext& context)
    {
    }

    void SetDrawDebug2D(HWorld2D world, bool draw_debug)
    {
    }

    HCollisionShape2D NewCircleShape2D(HContext2D context, float radius)
    {
        return 0;
    }

    HCollisionShape2D NewBoxShape2D(HContext2D context, const dmVMath::Vector3& half_extents)
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
                                     const dmVMath::Point3& position,
                                     uint32_t cell_width, uint32_t cell_height,
                                     uint32_t row_count, uint32_t column_count)
     {
         return 0;
     }

    void ClearGridShapeHulls(HCollisionObject2D collision_object)
    {
    }

    void CreateGridCellShape(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t child)
    {
    }

    bool SetGridShapeHull(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t row, uint32_t column, uint32_t hull, HullFlags flags)
    {
        return false;
    }

    bool SetGridShapeEnable(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t enable)
    {
        return false;
    }

    void SetCollisionObjectFilter(HCollisionObject2D collision_object,
                                  uint32_t shape, uint32_t child,
                                  uint16_t group, uint16_t mask)
    {
    }

    void DeleteCollisionShape2D(HCollisionShape2D shape)
    {
    }

    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D* shapes, uint32_t shape_count)
    {
        return 0;
    }

    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data,
                                            HCollisionShape2D* shapes,
                                            dmVMath::Vector3* translations,
                                            dmVMath::Quat* rotations,
                                            uint32_t shape_count)
    {
        return 0;
    }

    void DeleteCollisionObject2D(HWorld2D world, HCollisionObject2D collision_object)
    {
    }

    uint32_t GetCollisionShapes2D(HCollisionObject2D collision_object, HCollisionShape2D* out_buffer, uint32_t buffer_size)
    {
        return 0;
    }

    void SetCollisionObjectUserData2D(HCollisionObject2D collision_object, void* user_data)
    {
    }

    void* GetCollisionObjectUserData2D(HCollisionObject2D collision_object)
    {
        return 0;
    }

    void ApplyForce2D(HContext2D context, HCollisionObject2D collision_object, const dmVMath::Vector3& force, const dmVMath::Point3& position)
    {
    }

    dmVMath::Vector3 GetTotalForce2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return dmVMath::Vector3();
    }

    dmVMath::Point3 GetWorldPosition2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return dmVMath::Point3();
    }

    dmVMath::Quat GetWorldRotation2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return dmVMath::Quat();
    }

    dmVMath::Vector3 GetLinearVelocity2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return dmVMath::Vector3();
    }

    dmVMath::Vector3 GetAngularVelocity2D(HContext2D context, HCollisionObject2D collision_object)
    {
        return dmVMath::Vector3();
    }

    void SetLinearVelocity2D(HContext2D context, HCollisionObject2D collision_object, const dmVMath::Vector3& velocity)
    {
    }

    void SetAngularVelocity2D(HContext2D context, HCollisionObject2D collision_object, const dmVMath::Vector3& velocity)
    {
    }

    bool IsEnabled2D(HCollisionObject2D collision_object)
    {
        return false;
    }

    void SetEnabled2D(HWorld2D world, HCollisionObject2D collision_object, bool enabled)
    {
    }

    bool IsSleeping2D(HCollisionObject3D collision_object)
    {
        return false;
    }

    void Wakeup2D(HCollisionObject2D collision_object)
    {
    }

    void SetLockedRotation2D(HCollisionObject2D collision_object, bool locked_rotation)
    {
    }

    float GetLinearDamping2D(HCollisionObject2D collision_object)
    {
        return 0;
    }

    void SetLinearDamping2D(HCollisionObject2D collision_object, float linear_damping)
    {
    }

    float GetAngularDamping2D(HCollisionObject2D collision_object)
    {
        return 0;
    }

    void SetAngularDamping2D(HCollisionObject2D collision_object, float angular_damping)
    {
    }

    float GetMass2D(HCollisionObject2D collision_object)
    {
        return 0;
    }

    bool IsBullet2D(HCollisionObject2D collision_object)
    {
        return false;
    }

    void SetBullet2D(HCollisionObject2D collision_object, bool value)
    {
    }

    uint16_t GetGroup2D(HWorld2D world, HCollisionObject2D collision_object)
    {
        return 0;
    }

    void SetGroup2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit)
    {
    }

    bool GetMaskBit2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit)
    {
        return false;
    }

    void SetMaskBit2D(HWorld2D world, HCollisionObject2D collision_object, uint16_t groupbit, bool boolvalue)
    {
    }

    bool UpdateMass2D(HWorld2D world, HCollisionObject2D collision_object, float mass)
    {
        return false;
    }

    bool RequestRayCast2D(HWorld2D world, const RayCastRequest& request)
    {
        return true;
    }

    void RayCast2D(HWorld2D world, const RayCastRequest& request, dmArray<RayCastResponse>& results)
    {
    }

    void SetGravity2D(HWorld2D world, const dmVMath::Vector3& gravity)
    {
    }

    dmVMath::Vector3 GetGravity2D(HWorld2D world)
    {
        return dmVMath::Vector3(0.0f);
    }

    void SetDebugCallbacks2D(HContext2D context, const DebugCallbacks& callbacks)
    {
    }

    void ReplaceShape2D(HContext2D context, HCollisionShape2D old_shape, HCollisionShape2D new_shape)
    {
    }

    HJoint CreateJoint2D(HWorld2D world, HCollisionObject2D obj_a, const dmVMath::Point3& pos_a, HCollisionObject2D obj_b, const dmVMath::Point3& pos_b, dmPhysics::JointType type, const ConnectJointParams& params)
    {
        return (dmPhysics::HJoint)0x1;
    }

    bool GetJointParams2D(HWorld2D world, HJoint joint, dmPhysics::JointType type, ConnectJointParams& params)
    {
        return true;
    }

    bool SetJointParams2D(HWorld2D world, HJoint joint, dmPhysics::JointType type, const ConnectJointParams& params)
    {
        return true;
    }

    void DeleteJoint2D(HWorld2D world, HJoint joint)
    {
    }

    bool GetJointReactionForce2D(HWorld2D world, HJoint _joint, dmVMath::Vector3& force, float inv_dt)
    {
        return true;
    }

    bool GetJointReactionTorque2D(HWorld2D world, HJoint _joint, float& torque, float inv_dt)
    {
        return true;
    }

    void FlipH2D(HWorld2D world, HCollisionObject2D collision_object)
    {
    }

    void FlipV2D(HWorld2D world, HCollisionObject2D collision_object)
    {
    }

    bool IsWorldLocked(HWorld2D world)
    {
        return false;
    }

    void GetCollisionShapeRadius2D(HWorld2D world, HCollisionShape2D shape, float* radius)
    {
    }

    void GetCollisionShapeBoxDimensions2D(HWorld2D world, HCollisionShape2D shape, dmVMath::Quat rotation, float& w, float& h)
    {
    }

    HCollisionShape2D GetCollisionShape2D(HWorld2D world, HCollisionObject2D collision_object, uint32_t shape_index)
    {
        return 0;
    }

    void SetCollisionShapeRadius2D(HWorld2D world, HCollisionShape2D shape, float radius)
    {
    }

    void SetCollisionShapeBoxDimensions2D(HWorld2D world, HCollisionShape2D shape, dmVMath::Quat rotation, float w, float h)
    {
    }

    void SynchronizeObject2D(HWorld2D world, HCollisionObject2D collision_object)
    {
    }

    void* GetWorldContext2D(HWorld2D world)
    {
        return 0;
    }

    void* GetCollisionObjectContext2D(HCollisionObject2D collision_object)
    {
        return 0;
    }

}
