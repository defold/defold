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

#include "comp_collision_object.h"
#include "comp_collision_object_private.h"

#include "box2d/comp_collision_object_box2d.h"
#include "bullet3d/comp_collision_object_bullet3d.h"

#include "gamesys.h"

#include <dlib/profile.h>
#include <gameobject/gameobject_ddf.h>
#include <dmsdk/gameobject/script.h>

namespace dmGameSystem
{
    /// Config key to use for tweaking maximum number of collision objects
    const char* PHYSICS_MAX_COLLISION_OBJECTS_KEY   = "physics.max_collision_object_count";
    /// Config key to use for tweaking maximum number of collisions reported
    const char* PHYSICS_MAX_COLLISIONS_KEY          = "physics.max_collisions";
    /// Config key to use for tweaking maximum number of contacts reported
    const char* PHYSICS_MAX_CONTACTS_KEY            = "physics.max_contacts";
    /// Config key for using fixed frame rate for the physics worlds
    const char* PHYSICS_USE_FIXED_TIMESTEP          = "physics.use_fixed_timestep";
    /// Config key for using max updates during a single step
    const char* PHYSICS_MAX_FIXED_TIMESTEPS         = "physics.max_fixed_timesteps";
    /// Config key for Box2D 2.2 velocity iterations
    const char* BOX2D_VELOCITY_ITERATIONS            = "box2d.velocity_iterations";
    /// Config key for Box2D 2.2 position iterations  
    const char* BOX2D_POSITION_ITERATIONS            = "box2d.position_iterations";
    /// Config key for Box2D 3.x sub-step count
    const char* BOX2D_SUB_STEP_COUNT                 = "box2d.sub_step_count";

    dmGameObject::CreateResult CompCollisionObjectNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DNewWorld(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DNewWorld(params);
        }
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompCollisionObjectDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DDeleteWorld(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DDeleteWorld(params);
        }
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompCollisionObjectCreate(const dmGameObject::ComponentCreateParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DCreate(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DCreate(params);
        }
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompCollisionObjectDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DDestroy(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DDestroy(params);
        }
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompCollisionObjectFinal(const dmGameObject::ComponentFinalParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DFinal(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DFinal(params);
        }
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompCollisionObjectAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DAddToUpdate(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DAddToUpdate(params);
        }
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    static void FlushMessages(CollisionWorld* world);

    dmGameObject::UpdateResult CompCollisionObjectUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        CollisionWorld* world = (CollisionWorld*)params.m_World;
        if (!world)
            return dmGameObject::UPDATE_RESULT_OK;

        dmGameObject::UpdateResult r = dmGameObject::UPDATE_RESULT_OK;

        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            r = CompCollisionObjectBox2DUpdate(params, update_result);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            r = CompCollisionObjectBullet3DUpdate(params, update_result);
        }
        else
        {
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        if (world->m_CallbackInfoBatched)
            FlushMessages(world);
        return r;
    }

    dmGameObject::UpdateResult CompCollisionObjectFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DFixedUpdate(params, update_result);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DFixedUpdate(params, update_result);
        }
        return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::UpdateResult CompCollisionObjectPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DPostUpdate(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DPostUpdate(params);
        }
        return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::UpdateResult CompCollisionObjectOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DOnMessage(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DOnMessage(params);
        }
        return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
    }

    void* CompCollisionObjectGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        return (void*) params.m_UserData;
    }

    void CompCollisionObjectOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            CompCollisionObjectBox2DOnReload(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            CompCollisionObjectBullet3DOnReload(params);
        }
    }

    dmGameObject::PropertyResult CompCollisionObjectGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DGetProperty(params, out_value);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DGetProperty(params, out_value);
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult CompCollisionObjectSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*) params.m_Context;
        if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BOX2D)
        {
            return CompCollisionObjectBox2DSetProperty(params);
        }
        else if (physics_context->m_PhysicsType == PHYSICS_ENGINE_BULLET3D)
        {
            return CompCollisionObjectBullet3DSetProperty(params);
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    // Looks into world->m_Groups index for the speficied group_hash. It returns its position as
    // bit index (a uint16_t with n-th bit set). If the hash is not found and we're in readonly mode
    // it will return 0. If readonly is false though, it assigns the hash to the first empty bit slot.
    // If there are no positions are left, it returns 0.
    uint16_t GetGroupBitIndex(CollisionWorld* world, uint64_t group_hash, bool readonly)
    {
        if (group_hash != 0)
        {
            for (uint32_t i = 0; i < 16; ++i)
            {
                if (world->m_Groups[i] != 0)
                {
                    if (world->m_Groups[i] == group_hash)
                    {
                        return 1 << i;
                    }
                }
                else if (readonly)
                {
                    return 0;
                } else
                {
                    world->m_Groups[i] = group_hash;
                    return 1 << i;
                }
            }

            // When we get here, there are no more group bits available
            dmLogWarning("The collision group '%s' could not be used since the maximum group count has been reached (16).", dmHashReverseSafe64(group_hash));
        }
        return 0;
    }

    static bool CompCollisionIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        CollisionWorld* world = (CollisionWorld*)pit->m_Node->m_ComponentWorld;
        CollisionComponent* component = (CollisionComponent*)pit->m_Node->m_Component;

        uint64_t index = pit->m_Next++;

        uint32_t num_bool_properties = 1;
        if (index < num_bool_properties)
        {
            if (index == 0)
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN;
                pit->m_Property.m_Value.m_Bool = world->m_AdapterFunctions->m_IsEnabled(world, component);
                pit->m_Property.m_NameHash = dmHashString64("enabled");
            }
            return true;
        }
        index -= num_bool_properties;

        return false;
    }

    bool GetShapeIndexShared(CollisionWorld* world, CollisionComponent* _component, dmhash_t shape_name_hash, uint32_t* index_out)
    {
        CollisionComponent* component = (CollisionComponent*) _component;
        uint32_t shape_count = component->m_Resource->m_DDF->m_EmbeddedCollisionShape.m_Shapes.m_Count;
        for (int i = 0; i < shape_count; ++i)
        {
            if (component->m_Resource->m_DDF->m_EmbeddedCollisionShape.m_Shapes[i].m_IdHash == shape_name_hash)
            {
                *index_out = i;
                return true;
            }
        }
        return false;
    }

    // We use this to determine if a physics object is still alive, by determinig if the game object is still alive
    dmGameObject::HInstance CompCollisionObjectGetInstance(void* _user_data)
    {
        CollisionComponent* component = (CollisionComponent*) _user_data; // See SetCollisionObjectData and dmPhysics::NewCollisionObject2D
        return component->m_Instance;
    }

    void CompCollisionIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = 0;
        pit->m_FnIterateNext = CompCollisionIterPropertiesGetNext;
    }

    uint16_t CompCollisionGetGroupBitIndex(CollisionWorld* world, uint64_t group_hash)
    {
        return GetGroupBitIndex(world, group_hash, false);
    }

    dmScript::LuaCallbackInfo* GetCollisionWorldCallback(CollisionWorld* world)
    {
        return world->m_CallbackInfo;
    }

    void SetCollisionWorldCallback(CollisionWorld* world, dmScript::LuaCallbackInfo* callback_info, bool use_batching)
    {
        world->m_CallbackInfo = callback_info;
        world->m_CallbackInfoBatched = use_batching;
    }

    // Converts a collision mask bitfield to the respective group hash. Takes into account only the least significant bit.
    uint64_t GetLSBGroupHash(CollisionWorld* world, uint16_t mask)
    {
        if (mask > 0)
        {
            uint32_t index = 0;
            while ((mask & 1) == 0)
            {
                mask >>= 1;
                ++index;
            }
            return world->m_Groups[index];
        }
        return 0;
    }

    dmhash_t CompCollisionObjectGetIdentifier(CollisionComponent* component)
    {
        return dmGameObject::GetIdentifier(component->m_Instance);
    }

    static void StoreMessage(CollisionWorld* world, const dmDDF::Descriptor* desc, const char* data)
    {
        DM_PROFILE("StoreMessage");

        // We want to avoid serializing the message, for performance
        uint32_t msg_size = 0;
        if (desc->m_NameHash == dmPhysicsDDF::CollisionEvent::m_DDFDescriptor->m_NameHash)
            msg_size = sizeof(dmPhysicsDDF::CollisionEvent);
        else if (desc->m_NameHash == dmPhysicsDDF::ContactPointEvent::m_DDFDescriptor->m_NameHash)
            msg_size = sizeof(dmPhysicsDDF::ContactPointEvent);
        else if (desc->m_NameHash == dmPhysicsDDF::TriggerEvent::m_DDFDescriptor->m_NameHash)
            msg_size = sizeof(dmPhysicsDDF::TriggerEvent);
        else if (desc->m_NameHash == dmPhysicsDDF::RayCastResponse::m_DDFDescriptor->m_NameHash)
            msg_size = sizeof(dmPhysicsDDF::RayCastResponse);
        else if (desc->m_NameHash == dmPhysicsDDF::RayCastMissed::m_DDFDescriptor->m_NameHash)
            msg_size = sizeof(dmPhysicsDDF::RayCastMissed);

        if (world->m_MessageData.Remaining() < msg_size)
        {
            world->m_MessageData.OffsetCapacity( 2 * 1024 );
        }

        if (world->m_MessageInfos.Full())
        {
            world->m_MessageInfos.OffsetCapacity(64);
        }

        PhysicsMessage msg;
        msg.m_Descriptor = desc;
        msg.m_Offset     = world->m_MessageData.Size();
        msg.m_Size       = msg_size;

        world->m_MessageData.PushArray((uint8_t*)data, msg_size);
        world->m_MessageInfos.Push(msg);
    }

    static void FlushMessages(CollisionWorld* world)
    {
        if (world->m_MessageInfos.Empty())
            return;
        RunBatchedEventCallback(world->m_CallbackInfo, world->m_MessageInfos.Size(), world->m_MessageInfos.Begin(), world->m_MessageData.Begin());
        world->m_MessageInfos.SetSize(0);
        world->m_MessageData.SetSize(0);
    }

    static void RunPhysicsCallback(CollisionWorld* world, const dmDDF::Descriptor* desc, const char* data)
    {
        if (world->m_CallbackInfoBatched)
        {
            StoreMessage(world, desc, data);
        }
        else
        {
            RunCollisionWorldCallback(world->m_CallbackInfo, desc, data); // script_physics.cpp
        }
    }

    // Returns true if either of the components supports the flag(s)
    static bool SupportsEvent(CollisionComponent* component, uint8_t event)
    {
        return (component->m_EventMask & event) == event;
    }

    template <class DDFMessage>
    static void BroadCast(DDFMessage* ddf, dmGameObject::HInstance instance, dmhash_t instance_id, uint16_t component_index)
    {
        dmhash_t message_id = DDFMessage::m_DDFDescriptor->m_NameHash;
        uintptr_t descriptor = (uintptr_t)DDFMessage::m_DDFDescriptor;
        uint32_t data_size = sizeof(DDFMessage);
        dmMessage::URL sender;
        dmMessage::ResetURL(&sender);
        dmMessage::URL receiver;
        dmMessage::ResetURL(&receiver);
        receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(instance));
        receiver.m_Path = instance_id;
        // sender is the same as receiver, but with the specific collision object as fragment
        sender = receiver;
        dmGameObject::Result r = dmGameObject::GetComponentId(instance, component_index, &sender.m_Fragment);
        if (r != dmGameObject::RESULT_OK)
        {
            dmLogError("Could not retrieve sender component when reporting %s: %d", DDFMessage::m_DDFDescriptor->m_Name, r);
        }
        dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, ddf, data_size, 0);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogError("Could not send %s to component: %d", DDFMessage::m_DDFDescriptor->m_Name, result);
        }
    }

    void RayCastCallback(const dmPhysics::RayCastResponse& response, const dmPhysics::RayCastRequest& request, void* user_data)
    {
        dmGameObject::Result message_result = dmGameObject::RESULT_OK;
        CollisionWorld* world = (CollisionWorld*)user_data;
        if (response.m_Hit)
        {
            dmPhysicsDDF::RayCastResponse response_ddf;
            CollisionComponent* component = (CollisionComponent*)response.m_CollisionObjectUserData;

            response_ddf.m_Fraction = response.m_Fraction;
            response_ddf.m_Id = dmGameObject::GetIdentifier(component->m_Instance);
            response_ddf.m_Group = GetLSBGroupHash(world, response.m_CollisionObjectGroup);
            response_ddf.m_Position = response.m_Position;
            response_ddf.m_Normal = response.m_Normal;
            response_ddf.m_RequestId = request.m_UserId & 0xff;

            if (world->m_CallbackInfo != 0x0)
            {
                RunPhysicsCallback(world, dmPhysicsDDF::RayCastResponse::m_DDFDescriptor, (const char*)&response_ddf);
            }
            else
            {
                message_result = dmGameObject::PostDDF(&response_ddf, 0x0, (dmMessage::URL*)request.m_UserData, 0x0, false);
            }
        }
        else
        {
            dmPhysicsDDF::RayCastMissed missed_ddf;
            missed_ddf.m_RequestId = request.m_UserId & 0xff;
            if (world->m_CallbackInfo != 0x0)
            {
                RunPhysicsCallback(world, dmPhysicsDDF::RayCastMissed::m_DDFDescriptor, (const char*)&missed_ddf);
            }
            else
            {
                message_result = dmGameObject::PostDDF(&missed_ddf, 0x0, (dmMessage::URL*)request.m_UserData, 0x0, false);
            }
        }
        free(request.m_UserData);
        if (message_result != dmGameObject::RESULT_OK)
        {
            dmLogError("Error when sending ray cast response: %d", message_result);
        }
    }

    void TriggerExitedCallback(const dmPhysics::TriggerExit& trigger_exit, void* user_data)
    {
        CollisionWorld* world = (CollisionWorld*)user_data;
        CollisionComponent* component_a = (CollisionComponent*)trigger_exit.m_UserDataA;
        CollisionComponent* component_b = (CollisionComponent*)trigger_exit.m_UserDataB;

        bool event_supported_a = SupportsEvent(component_a, EVENT_MASK_TRIGGER);
        bool event_supported_b = SupportsEvent(component_b, EVENT_MASK_TRIGGER);
        if (!event_supported_a && !event_supported_b)
            return; // We early out because neither supported this event

        dmGameObject::HInstance instance_a = component_a->m_Instance;
        dmGameObject::HInstance instance_b = component_b->m_Instance;
        dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
        dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);

        uint64_t group_hash_a = GetLSBGroupHash(world, trigger_exit.m_GroupA);
        uint64_t group_hash_b = GetLSBGroupHash(world, trigger_exit.m_GroupB);

        if (world->m_CallbackInfo != 0x0)
        {
            dmPhysicsDDF::TriggerEvent ddf;
            ddf.m_Enter = 0;

            dmPhysicsDDF::Trigger& a = ddf.m_A;
            a.m_Group       = group_hash_a;
            a.m_Id          = instance_a_id;

            dmPhysicsDDF::Trigger& b = ddf.m_B;
            b.m_Group       = group_hash_b;
            b.m_Id          = instance_b_id;

            RunPhysicsCallback(world, dmPhysicsDDF::TriggerEvent::m_DDFDescriptor, (const char*)&ddf);
            return;
        }

        dmPhysicsDDF::TriggerResponse ddf;
        ddf.m_Enter = 0;

        // Broadcast to A components
        if (event_supported_a)
        {
            ddf.m_OtherId = instance_b_id;
            ddf.m_Group = group_hash_b;
            ddf.m_OwnGroup = group_hash_a;
            ddf.m_OtherGroup = group_hash_b;
            BroadCast(&ddf, instance_a, instance_a_id, component_a->m_ComponentIndex);
        }

        // Broadcast to B components
        if (event_supported_b)
        {
            ddf.m_OtherId = instance_a_id;
            ddf.m_Group = group_hash_a;
            ddf.m_OwnGroup = group_hash_b;
            ddf.m_OtherGroup = group_hash_a;
            BroadCast(&ddf, instance_b, instance_b_id, component_b->m_ComponentIndex);
        }
    }

    void TriggerEnteredCallback(const dmPhysics::TriggerEnter& trigger_enter, void* user_data)
    {
        CollisionWorld* world = (CollisionWorld*)user_data;
        CollisionComponent* component_a = (CollisionComponent*)trigger_enter.m_UserDataA;
        CollisionComponent* component_b = (CollisionComponent*)trigger_enter.m_UserDataB;

        bool event_supported_a = SupportsEvent(component_a, EVENT_MASK_TRIGGER);
        bool event_supported_b = SupportsEvent(component_b, EVENT_MASK_TRIGGER);
        if (!event_supported_a && !event_supported_b)
            return; // We early out because neither supported this event

        dmGameObject::HInstance instance_a = component_a->m_Instance;
        dmGameObject::HInstance instance_b = component_b->m_Instance;
        dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
        dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);

        uint64_t group_hash_a = GetLSBGroupHash(world, trigger_enter.m_GroupA);
        uint64_t group_hash_b = GetLSBGroupHash(world, trigger_enter.m_GroupB);

        if (world->m_CallbackInfo != 0x0)
        {
            dmPhysicsDDF::TriggerEvent ddf;
            ddf.m_Enter = 1;

            dmPhysicsDDF::Trigger& a = ddf.m_A;
            a.m_Group       = group_hash_a;
            a.m_Id          = instance_a_id;

            dmPhysicsDDF::Trigger& b = ddf.m_B;
            b.m_Group       = group_hash_b;
            b.m_Id          = instance_b_id;

            RunPhysicsCallback(world, dmPhysicsDDF::TriggerEvent::m_DDFDescriptor, (const char*)&ddf);
            return;
        }

        dmPhysicsDDF::TriggerResponse ddf;
        ddf.m_Enter = 1;

        // Broadcast to A components
        if (event_supported_a)
        {
            ddf.m_OtherId = instance_b_id;
            ddf.m_Group = group_hash_b;
            ddf.m_OwnGroup = group_hash_a;
            ddf.m_OtherGroup = group_hash_b;
            BroadCast(&ddf, instance_a, instance_a_id, component_a->m_ComponentIndex);
        }

        // Broadcast to B components
        if (event_supported_b)
        {
            ddf.m_OtherId = instance_a_id;
            ddf.m_Group = group_hash_a;
            ddf.m_OwnGroup = group_hash_b;
            ddf.m_OtherGroup = group_hash_a;
            BroadCast(&ddf, instance_b, instance_b_id, component_b->m_ComponentIndex);
        }
    }

    bool CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
    {
        CollisionUserData* cud = (CollisionUserData*)user_data;
        if (cud->m_Count < cud->m_Context->m_MaxCollisionCount)
        {
            CollisionWorld* world = cud->m_World;
            CollisionComponent* component_a = (CollisionComponent*)user_data_a;
            CollisionComponent* component_b = (CollisionComponent*)user_data_b;

            bool event_supported_a = SupportsEvent(component_a, EVENT_MASK_COLLISION);
            bool event_supported_b = SupportsEvent(component_b, EVENT_MASK_COLLISION);
            if (!event_supported_a && !event_supported_b)
                return true; // We early out because neither supported this event

            cud->m_Count += 1;

            dmGameObject::HInstance instance_a = component_a->m_Instance;
            dmGameObject::HInstance instance_b = component_b->m_Instance;
            dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
            dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);
            uint64_t group_hash_a = GetLSBGroupHash(world, group_a);
            uint64_t group_hash_b = GetLSBGroupHash(world, group_b);

            if (world->m_CallbackInfo != 0x0)
            {
                dmPhysicsDDF::CollisionEvent ddf;

                dmPhysicsDDF::Collision& a = ddf.m_A;
                a.m_Group =     group_hash_a;
                a.m_Id =        instance_a_id;
                a.m_Position =  dmGameObject::GetWorldPosition(instance_a);

                dmPhysicsDDF::Collision& b = ddf.m_B;
                b.m_Group =     group_hash_b;
                b.m_Id =        instance_b_id;
                b.m_Position =  dmGameObject::GetWorldPosition(instance_b);

                RunPhysicsCallback(world, dmPhysicsDDF::CollisionEvent::m_DDFDescriptor, (const char*)&ddf);
                return true;
            }

            // Broadcast to A components
            if (event_supported_a)
            {
                dmPhysicsDDF::CollisionResponse ddf;
                ddf.m_OwnGroup = group_hash_a;
                ddf.m_OtherGroup = group_hash_b;
                ddf.m_Group = group_hash_b;
                ddf.m_OtherId = instance_b_id;
                ddf.m_OtherPosition = dmGameObject::GetWorldPosition(instance_b);
                BroadCast(&ddf, instance_a, instance_a_id, component_a->m_ComponentIndex);
            }

            // Broadcast to B components
            if (event_supported_b)
            {
                dmPhysicsDDF::CollisionResponse ddf;
                ddf.m_OwnGroup = group_hash_b;
                ddf.m_OtherGroup = group_hash_a;
                ddf.m_Group = group_hash_a;
                ddf.m_OtherId = instance_a_id;
                ddf.m_OtherPosition = dmGameObject::GetWorldPosition(instance_a);
                BroadCast(&ddf, instance_b, instance_b_id, component_b->m_ComponentIndex);
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    bool ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
    {
        CollisionUserData* cud = (CollisionUserData*) user_data;
        if (cud->m_Count < cud->m_Context->m_MaxContactPointCount)
        {
            CollisionWorld* world           = cud->m_World;
            CollisionComponent* component_a = (CollisionComponent*)contact_point.m_UserDataA;
            CollisionComponent* component_b = (CollisionComponent*)contact_point.m_UserDataB;

            bool event_supported_a = SupportsEvent(component_a, EVENT_MASK_CONTACT);
            bool event_supported_b = SupportsEvent(component_b, EVENT_MASK_CONTACT);
            if (!event_supported_a && !event_supported_b)
                return true; // We early out because neither supported this event

            cud->m_Count += 1;

            dmGameObject::HInstance instance_a = component_a->m_Instance;
            dmGameObject::HInstance instance_b = component_b->m_Instance;
            dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
            dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);
            float mass_a = dmMath::Select(-contact_point.m_MassA, 0.0f, contact_point.m_MassA);
            float mass_b = dmMath::Select(-contact_point.m_MassB, 0.0f, contact_point.m_MassB);
            uint64_t group_hash_a = GetLSBGroupHash(world, contact_point.m_GroupA);
            uint64_t group_hash_b = GetLSBGroupHash(world, contact_point.m_GroupB);

            if (world->m_CallbackInfo != 0x0)
            {
                dmPhysicsDDF::ContactPointEvent ddf;
                ddf.m_AppliedImpulse = contact_point.m_AppliedImpulse;
                ddf.m_Distance = contact_point.m_Distance;

                dmPhysicsDDF::ContactPoint& a = ddf.m_A;
                a.m_Group               = group_hash_a;
                a.m_Id                  = instance_a_id;
                a.m_Position            = contact_point.m_PositionA;
                a.m_InstancePosition    = dmGameObject::GetWorldPosition(instance_a);
                a.m_Mass                = mass_a;
                a.m_RelativeVelocity    = -contact_point.m_RelativeVelocity;
                a.m_Normal              = -contact_point.m_Normal;

                dmPhysicsDDF::ContactPoint& b = ddf.m_B;
                b.m_Group               = group_hash_b;
                b.m_Id                  = instance_b_id;
                b.m_Position            = contact_point.m_PositionB;
                b.m_InstancePosition    = dmGameObject::GetWorldPosition(instance_b);
                b.m_Mass                = mass_b;
                b.m_RelativeVelocity    = contact_point.m_RelativeVelocity;
                b.m_Normal              = contact_point.m_Normal;

                RunPhysicsCallback(world, dmPhysicsDDF::ContactPointEvent::m_DDFDescriptor, (const char*)&ddf);
                return true;
            }

            // Broadcast to A components
            if (event_supported_a)
            {
                dmPhysicsDDF::ContactPointResponse ddf;
                ddf.m_Position = contact_point.m_PositionA;
                ddf.m_Normal = -contact_point.m_Normal;
                ddf.m_RelativeVelocity = -contact_point.m_RelativeVelocity;
                ddf.m_Distance = contact_point.m_Distance;
                ddf.m_AppliedImpulse = contact_point.m_AppliedImpulse;
                ddf.m_Mass = mass_a;
                ddf.m_OtherMass = mass_b;
                ddf.m_OtherId = instance_b_id;
                ddf.m_OtherPosition = dmGameObject::GetWorldPosition(instance_b);
                ddf.m_Group = group_hash_b;
                ddf.m_OwnGroup = group_hash_a;
                ddf.m_OtherGroup = group_hash_b;
                ddf.m_LifeTime = 0;
                BroadCast(&ddf, instance_a, instance_a_id, component_a->m_ComponentIndex);
            }

            // Broadcast to B components
            if (event_supported_b)
            {
                dmPhysicsDDF::ContactPointResponse ddf;
                ddf.m_Position = contact_point.m_PositionB;
                ddf.m_Normal = contact_point.m_Normal;
                ddf.m_RelativeVelocity = contact_point.m_RelativeVelocity;
                ddf.m_Distance = contact_point.m_Distance;
                ddf.m_AppliedImpulse = contact_point.m_AppliedImpulse;
                ddf.m_Mass = mass_b;
                ddf.m_OtherMass = mass_a;
                ddf.m_OtherId = instance_a_id;
                ddf.m_OtherPosition = dmGameObject::GetWorldPosition(instance_a);
                ddf.m_Group = group_hash_a;
                ddf.m_OwnGroup = group_hash_b;
                ddf.m_OtherGroup = group_hash_a;
                ddf.m_LifeTime = 0;
                BroadCast(&ddf, instance_b, instance_b_id, component_b->m_ComponentIndex);
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    ///////////////////////////////////////////
    // Adapter functions
    ///////////////////////////////////////////
    void WakeupCollision(CollisionWorld* world, CollisionComponent* component)
    {
        if (!world->m_AdapterFunctions->m_WakeupCollision)
        {
            return;
        }
        world->m_AdapterFunctions->m_WakeupCollision(world, component);
    }

    void RayCast(CollisionWorld* world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results)
    {
        if (!world->m_AdapterFunctions->m_RayCast)
        {
            return;
        }
        world->m_AdapterFunctions->m_RayCast(world, request, results);
    }

    dmPhysics::JointResult CreateJoint(CollisionWorld* world, CollisionComponent* component_a, dmhash_t id, const dmVMath::Point3& apos, CollisionComponent* component_b, const dmVMath::Point3& bpos, dmPhysics::JointType type, const dmPhysics::ConnectJointParams& joint_params)
    {
        if (!world->m_AdapterFunctions->m_CreateJoint)
        {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }
        return world->m_AdapterFunctions->m_CreateJoint(world, component_a, id, apos, component_b, bpos, type, joint_params);
    }

    dmPhysics::JointResult DestroyJoint(CollisionWorld* world, CollisionComponent* component, dmhash_t id)
    {
        if (!world->m_AdapterFunctions->m_DestroyJoint)
        {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }
        return world->m_AdapterFunctions->m_DestroyJoint(world, component, id);
    }

    dmPhysics::JointResult GetJointParams(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmPhysics::JointType& joint_type, dmPhysics::ConnectJointParams& joint_params)
    {
        if (!world->m_AdapterFunctions->m_GetJointParams)
        {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }
        return world->m_AdapterFunctions->m_GetJointParams(world, component, id, joint_type, joint_params);
    }

    dmPhysics::JointResult GetJointType(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmPhysics::JointType& joint_type)
    {
        if (!world->m_AdapterFunctions->m_GetJointType)
        {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }
        return world->m_AdapterFunctions->m_GetJointType(world, component, id, joint_type);
    }

    dmPhysics::JointResult SetJointParams(CollisionWorld* world, CollisionComponent* component, dmhash_t id, const dmPhysics::ConnectJointParams& joint_params)
    {
        if (!world->m_AdapterFunctions->m_SetJointParams)
        {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }
        return world->m_AdapterFunctions->m_SetJointParams(world, component, id, joint_params);
    }

    dmPhysics::JointResult GetJointReactionForce(CollisionWorld* world, CollisionComponent* component, dmhash_t id, dmVMath::Vector3& force)
    {
        if (!world->m_AdapterFunctions->m_GetJointReactionForce)
        {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }
        return world->m_AdapterFunctions->m_GetJointReactionForce(world, component, id, force);
    }

    dmPhysics::JointResult GetJointReactionTorque(CollisionWorld* world, CollisionComponent* component, dmhash_t id, float& torque)
    {
        if (!world->m_AdapterFunctions->m_GetJointReactionTorque)
        {
            return dmPhysics::RESULT_NOT_SUPPORTED;
        }
        return world->m_AdapterFunctions->m_GetJointReactionTorque(world, component, id, torque);
    }

    void SetGravity(CollisionWorld* world, const dmVMath::Vector3& gravity)
    {
        if (!world->m_AdapterFunctions->m_SetGravity)
        {
            return;
        }
        world->m_AdapterFunctions->m_SetGravity(world, gravity);
    }

    dmVMath::Vector3 GetGravity(CollisionWorld* world)
    {
        if (!world->m_AdapterFunctions->m_GetGravity)
        {
            return dmVMath::Vector3(0.0f, 0.0f, 0.0f);
        }
        return world->m_AdapterFunctions->m_GetGravity(world);
    }

    void SetCollisionFlipH(CollisionWorld* world, CollisionComponent* component, bool flip)
    {
        if (!world->m_AdapterFunctions->m_SetCollisionFlipH)
        {
            return;
        }
        world->m_AdapterFunctions->m_SetCollisionFlipH(world, component, flip);
    }

    void SetCollisionFlipV(CollisionWorld* world, CollisionComponent* component, bool flip)
    {
        if (!world->m_AdapterFunctions->m_SetCollisionFlipV)
        {
            return;
        }
        world->m_AdapterFunctions->m_SetCollisionFlipV(world, component, flip);
    }

    dmhash_t GetCollisionGroup(CollisionWorld* world, CollisionComponent* component)
    {
        if (!world->m_AdapterFunctions->m_GetCollisionGroup)
        {
            return 0;
        }
        return world->m_AdapterFunctions->m_GetCollisionGroup(world, component);
    }

    bool SetCollisionGroup(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash)
    {
        if (!world->m_AdapterFunctions->m_SetCollisionGroup)
        {
            return false;
        }
        return world->m_AdapterFunctions->m_SetCollisionGroup(world, component, group_hash);
    }

    bool GetCollisionMaskBit(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash, bool* maskbit)
    {
        if (!world->m_AdapterFunctions->m_GetCollisionMaskBit)
        {
            return false;
        }
        return world->m_AdapterFunctions->m_GetCollisionMaskBit(world, component, group_hash, maskbit);
    }

    bool SetCollisionMaskBit(CollisionWorld* world, CollisionComponent* component, dmhash_t group_hash, bool boolvalue)
    {
        if (!world->m_AdapterFunctions->m_SetCollisionMaskBit)
        {
            return false;
        }
        return world->m_AdapterFunctions->m_SetCollisionMaskBit(world, component, group_hash, boolvalue);
    }

    void UpdateMass(CollisionWorld* world, CollisionComponent* component, float mass)
    {
        if (!world->m_AdapterFunctions)
        {
            return;
        }
        world->m_AdapterFunctions->m_UpdateMass(world, component, mass);
    }

    bool GetShapeIndex(CollisionWorld* world, CollisionComponent* component, dmhash_t shape_name_hash, uint32_t* index_out)
    {
        if (!world->m_AdapterFunctions->m_GetShapeIndex)
        {
            return false;
        }
        return world->m_AdapterFunctions->m_GetShapeIndex(world, component, shape_name_hash, index_out);
    }

    bool GetShape(CollisionWorld* world, CollisionComponent* component, uint32_t shape_ix, ShapeInfo* shape_info)
    {
        if (!world->m_AdapterFunctions->m_GetShape)
        {
            return false;
        }
        return world->m_AdapterFunctions->m_GetShape(world, component, shape_ix, shape_info);
    }

    bool SetShape(CollisionWorld* world, CollisionComponent* component, uint32_t shape_ix, ShapeInfo* shape_info)
    {
        if (!world->m_AdapterFunctions->m_SetShape)
        {
            return false;
        }
        return world->m_AdapterFunctions->m_SetShape(world, component, shape_ix, shape_info);
    }

    PhysicsEngineType GetPhysicsEngineType(CollisionWorld* world)
    {
        if (!world->m_AdapterFunctions->m_GetPhysicsEngineType)
        {
            return PHYSICS_ENGINE_NONE;
        }
        return world->m_AdapterFunctions->m_GetPhysicsEngineType(world);
    }
}
