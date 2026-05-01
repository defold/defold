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
    dmGameObject::CreateResult   CompCollisionObjectNewWorld(const dmGameObject::ComponentNewWorldParams& params);
    dmGameObject::CreateResult   CompCollisionObjectDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);
    dmGameObject::CreateResult   CompCollisionObjectCreate(const dmGameObject::ComponentCreateParams& params);
    dmGameObject::CreateResult   CompCollisionObjectDestroy(const dmGameObject::ComponentDestroyParams& params);
    dmGameObject::CreateResult   CompCollisionObjectFinal(const dmGameObject::ComponentFinalParams& params);
    dmGameObject::CreateResult   CompCollisionObjectAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);
    dmGameObject::UpdateResult   CompCollisionObjectUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);
    dmGameObject::UpdateResult   CompCollisionObjectFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);
    dmGameObject::UpdateResult   CompCollisionObjectPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);
    dmGameObject::UpdateResult   CompCollisionObjectOnMessage(const dmGameObject::ComponentOnMessageParams& params);
    void*                        CompCollisionObjectGetComponent(const dmGameObject::ComponentGetParams& params);
    void                         CompCollisionObjectOnReload(const dmGameObject::ComponentOnReloadParams& params);
    dmGameObject::PropertyResult CompCollisionObjectGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);
    dmGameObject::PropertyResult CompCollisionObjectSetProperty(const dmGameObject::ComponentSetPropertyParams& params);
    void                         CompCollisionIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);
    dmGameObject::HInstance      CompCollisionObjectGetInstance(void* user_data);

    dmScript::LuaCallbackInfo* GetCollisionWorldCallback(CollisionWorld* world);
    void SetCollisionWorldCallback(CollisionWorld* world, dmScript::LuaCallbackInfo* callback_info, bool use_batching);

    // script_physics.cpp
    void RunCollisionWorldCallback(dmScript::LuaCallbackInfo* callback, const dmDDF::Descriptor* desc, const char* data);
    void RunBatchedEventCallback(dmScript::LuaCallbackInfo* callback, uint32_t count, PhysicsMessage* infos, const uint8_t* data);

    // For script_physics.cpp
    uint64_t GetLSBGroupHash(CollisionWorld* world, uint16_t mask);
    dmhash_t CompCollisionObjectGetIdentifier(CollisionComponent* component);
    uint16_t CompCollisionGetGroupBitIndex(CollisionWorld* world, uint64_t group_hash);

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

    // Adapter API
    bool                   IsEnabled(CollisionWorld* world, CollisionComponent* component);
    void                   WakeupCollision(CollisionWorld* world, CollisionComponent* component);
    void                   RayCast(CollisionWorld* world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results);
    void                   SetGravity(CollisionWorld* world, const dmVMath::Vector3& gravity);
    dmVMath::Vector3       GetGravity(CollisionWorld* world);
    void                   SetCollisionFlipH(CollisionWorld* world, CollisionComponent* component, bool flip);
    void                   SetCollisionFlipV(CollisionWorld* world, CollisionComponent* component, bool flip);
    void                   WakeupCollision(CollisionWorld* world, CollisionComponent* component);
    dmhash_t               GetCollisionGroup(CollisionWorld* world, CollisionComponent* component);
    bool                   SetCollisionGroup(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash);
    bool                   GetCollisionMaskBit(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash, bool* maskbit);
    bool                   SetCollisionMaskBit(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash, bool boolvalue);
    void                   UpdateMass(CollisionWorld* world, CollisionComponent* component, float mass);
    bool                   GetShapeIndex(CollisionWorld* world, CollisionComponent* component, dmhash_t shape_name_hash, uint32_t* index_out);
    bool                   GetShape(CollisionWorld* world, CollisionComponent* component, uint32_t shape_ix, ShapeInfo* shape_info);
    bool                   SetShape(CollisionWorld* world, CollisionComponent* component, uint32_t shape_ix, ShapeInfo* shape_info);
    PhysicsEngineType      GetPhysicsEngineType(CollisionWorld* world);

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
