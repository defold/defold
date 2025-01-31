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

#ifndef DM_GAMESYS_COMP_COLLISION_OBJECT_H
#define DM_GAMESYS_COMP_COLLISION_OBJECT_H

#include <gameobject/component.h>
#include <dmsdk/dlib/vmath.h>

// for scripting
#include <stdint.h>
#include <physics/physics.h>
#include <gamesys.h>

#include <gamesys/physics_ddf.h>

namespace dmGameSystem
{
    static const dmhash_t PROP_LINEAR_DAMPING   = dmHashString64("linear_damping");
    static const dmhash_t PROP_ANGULAR_DAMPING  = dmHashString64("angular_damping");
    static const dmhash_t PROP_LINEAR_VELOCITY  = dmHashString64("linear_velocity");
    static const dmhash_t PROP_ANGULAR_VELOCITY = dmHashString64("angular_velocity");
    static const dmhash_t PROP_MASS             = dmHashString64("mass");
    static const dmhash_t PROP_BULLET           = dmHashString64("bullet");

    enum EventMask
    {
        EVENT_MASK_COLLISION = 1 << 0,
        EVENT_MASK_CONTACT   = 1 << 1,
        EVENT_MASK_TRIGGER   = 1 << 2,
    };

    struct ShapeInfo
    {
        union
        {
            float m_BoxDimensions[3];
            float m_CapsuleDiameterHeight[2];
            float m_SphereDiameter;
        };
        dmPhysicsDDF::CollisionShape::Type m_Type;
    };

    void                    CompCollisionIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);
    dmGameObject::HInstance CompCollisionObjectGetInstance(void* user_data);

    void* GetCollisionWorldCallback(CollisionWorld* world);
    void SetCollisionWorldCallback(CollisionWorld* world, void* callback_info);
    void RunCollisionWorldCallback(void* callback_data, const dmDDF::Descriptor* desc, const char* data);

    // For script_physics.cpp
    uint64_t GetLSBGroupHash(CollisionWorld* world, uint16_t mask);
    dmhash_t CompCollisionObjectGetIdentifier(CollisionComponent* component);
    uint16_t CompCollisionGetGroupBitIndex(CollisionWorld* world, uint64_t group_hash);

    // Adapter API
    bool                   IsEnabled(CollisionWorld* world, CollisionComponent* component);
    void                   WakeupCollision(CollisionWorld* world, CollisionComponent* component);
    void                   RayCast(CollisionWorld* world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results);
    void                   SetGravity(CollisionWorld* world, const dmVMath::Vector3& gravity);
    dmVMath::Vector3       GetGravity(CollisionWorld* world);

    // bool                    IsCollision2D(CollisionWorld* world); // TODO: AdapterFamily or AdapterType?
    void                    SetCollisionFlipH(CollisionWorld* world, CollisionComponent* component, bool flip);
    void                    SetCollisionFlipV(CollisionWorld* world, CollisionComponent* component, bool flip);
    void                    WakeupCollision(CollisionWorld* world, CollisionComponent* component);
    dmhash_t                GetCollisionGroup(CollisionWorld* world, CollisionComponent* component);
    bool                    SetCollisionGroup(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash);
    bool                    GetCollisionMaskBit(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash, bool* maskbit);
    bool                    SetCollisionMaskBit(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash, bool boolvalue);
    void                    UpdateMass(CollisionWorld* world, CollisionComponent* component, float mass);
    bool                    GetShapeIndex(CollisionWorld* world, CollisionComponent* component, dmhash_t shape_name_hash, uint32_t* index_out);
    bool                    GetShape(CollisionWorld* world, CollisionComponent* component, uint32_t shape_ix, ShapeInfo* shape_info);
    bool                    SetShape(CollisionWorld* world, CollisionComponent* component, uint32_t shape_ix, ShapeInfo* shape_info);

    // Adapter API joints
    dmPhysics::JointResult CreateJoint(CollisionWorld* world, CollisionComponent* component_a, dmhash_t id, const dmVMath::Point3& apos, CollisionComponent* component_b, const dmVMath::Point3& bpos, dmPhysics::JointType type, const dmPhysics::ConnectJointParams& joint_params);
    dmPhysics::JointResult DestroyJoint(CollisionWorld* world, CollisionComponent* component, dmhash_t id);
    dmPhysics::JointResult GetJointParams(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmPhysics::JointType& joint_type, dmPhysics::ConnectJointParams& joint_params);
    dmPhysics::JointResult GetJointType(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmPhysics::JointType& joint_type);
    dmPhysics::JointResult SetJointParams(CollisionWorld* world, CollisionComponent* component, dmhash_t id, const dmPhysics::ConnectJointParams& joint_params);
    dmPhysics::JointResult GetJointReactionForce(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmVMath::Vector3& force);
    dmPhysics::JointResult GetJointReactionTorque(CollisionWorld* world, CollisionComponent* component, dmhash_t id, float& torque);
}

#endif // DM_GAMESYS_COMP_COLLISION_OBJECT_H
