// Copyright 2020-2024 The Defold Foundation
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

#include <gamesys/physics_ddf.h>

template <typename T> class dmArray;

class b2World;
class b2Body;

namespace dmGameSystem
{
    dmGameObject::CreateResult CompCollisionObjectNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollisionObjectDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollisionObjectCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollisionObjectDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollisionObjectFinal(const dmGameObject::ComponentFinalParams& params);

    dmGameObject::CreateResult CompCollisionObjectAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollisionObjectUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompCollisionObjectFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompCollisionObjectPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);

    dmGameObject::UpdateResult CompCollisionObjectOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void CompCollisionObjectOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompCollisionObjectGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompCollisionObjectSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    void CompCollisionIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);

    uint16_t CompCollisionGetGroupBitIndex(void* world, uint64_t group_hash);

    // For script_physics.cpp
    void RayCast(void* world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results);
    uint64_t GetLSBGroupHash(void* world, uint16_t mask);
    dmhash_t CompCollisionObjectGetIdentifier(void* component);

    dmPhysics::JointResult CreateJoint(void* _world, void* _component_a, dmhash_t id, const dmVMath::Point3& apos, void* _component_b, const dmVMath::Point3& bpos, dmPhysics::JointType type, const dmPhysics::ConnectJointParams& joint_params);
    dmPhysics::JointResult DestroyJoint(void* _world, void* _component, dmhash_t id);
    dmPhysics::JointResult GetJointParams(void* _world, void* _component, dmhash_t id, dmPhysics::JointType& joint_type, dmPhysics::ConnectJointParams& joint_params);
    dmPhysics::JointResult GetJointType(void* _world, void* _component, dmhash_t id, dmPhysics::JointType& joint_type);
    dmPhysics::JointResult SetJointParams(void* _world, void* _component, dmhash_t id, const dmPhysics::ConnectJointParams& joint_params);
    dmPhysics::JointResult GetJointReactionForce(void* _world, void* _component, dmhash_t id, dmVMath::Vector3& force);
    dmPhysics::JointResult GetJointReactionTorque(void* _world, void* _component, dmhash_t id, float& torque);

    void SetGravity(void* world, const dmVMath::Vector3& gravity);
    dmVMath::Vector3 GetGravity(void* _world);

    bool IsCollision2D(void* _world);
    void SetCollisionFlipH(void* _component, bool flip);
    void SetCollisionFlipV(void* _component, bool flip);
    void WakeupCollision(void* _world, void* _component);
    dmhash_t GetCollisionGroup(void* _world, void* _component);
    bool SetCollisionGroup(void* _world, void* _component, dmhash_t group_hash);
    bool GetCollisionMaskBit(void* _world, void* _component, dmhash_t group_hash, bool* maskbit);
    bool SetCollisionMaskBit(void* _world, void* _component, dmhash_t group_hash, bool boolvalue);
    void UpdateMass(void* _world, void* _component, float mass);

    void* GetCollisionWorldCallback(void* _world);
    void SetCollisionWorldCallback(void* _world, void* callback_info);
    void RunCollisionWorldCallback(void* callback_data, const dmDDF::Descriptor* desc, const char* data);

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

    bool GetShapeIndex(void* _component, dmhash_t shape_name_hash, uint32_t* index_out);
    bool GetShape(void* _world, void* _component, uint32_t shape_ix, ShapeInfo* shape_info);
    bool SetShape(void* _world, void* _component, uint32_t shape_ix, ShapeInfo* shape_info);


    // For script_box2d.cpp
    b2World* CompCollisionObjectGetBox2DWorld(void* _world);
    b2Body* CompCollisionObjectGetBox2DBody(void* _component);
    dmGameObject::HInstance CompCollisionObjectGetInstance(void* _user_data);
}

#endif // DM_GAMESYS_COMP_COLLISION_OBJECT_H
