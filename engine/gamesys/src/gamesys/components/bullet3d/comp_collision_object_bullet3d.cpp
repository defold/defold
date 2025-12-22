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

#include "../comp_collision_object.h"
#include "../comp_collision_object_private.h"
#include "comp_collision_object_bullet3d.h"

#include <dlib/dlib.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/static_assert.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/gameobject/script.h>

#include <physics/physics.h>
#include <gameobject/gameobject.h>
#include <gameobject/gameobject_ddf.h>
#include <gamesys/gamesys_ddf.h>

#include "gamesys.h"
#include "gamesys_private.h" // ShowFullBufferError

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_CollisionObjectBullet3D, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);

namespace dmGameSystem
{
    static PhysicsAdapterFunctionTable* g_PhysicsAdapter = 0x0;
    static int g_NumPhysicsTransformsUpdated = 0;
    static bool g_CollisionOverflowWarning   = false;
    static bool g_ContactOverflowWarning     = false;

    static void InstallBullet3DPhysicsAdapter();
    static void GetWorldTransform(void* user_data, dmTransform::Transform& world_transform);
    static void SetWorldTransform(void* user_data, const dmVMath::Point3& position, const dmVMath::Quat& rotation);

    struct CollisionComponentBullet3D;
    struct CollisionWorldBullet3D
    {
        CollisionWorld                       m_BaseWorld;
        dmPhysics::HWorld3D                  m_World3D;
        uint8_t                              m_ComponentTypeIndex;
        uint8_t                              m_FirstUpdate : 1;
        dmArray<CollisionComponentBullet3D*> m_Components;
    };

    struct CollisionComponentBullet3D
    {
        CollisionComponent            m_BaseComponent;
        dmPhysics::HCollisionObject3D m_Object3D;
        dmPhysics::HCollisionShape3D* m_ShapeBuffer;
    };

    struct DispatchContext
    {
        PhysicsContextBullet3D* m_PhysicsContext;
        dmGameObject::HRegister m_Register;
        uint32_t                m_ComponentTypeIndex;
        bool                    m_Success;
    };

    dmGameObject::CreateResult CompCollisionObjectBullet3DNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        DM_STATIC_ASSERT(sizeof(ShapeInfo::m_BoxDimensions) <= sizeof(dmVMath::Vector3), Invalid_Struct_Size);

        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*) params.m_Context;
        if (params.m_MaxComponentInstances == 0 || physics_context->m_Context == 0x0)
        {
            *params.m_World = 0x0;
            return dmGameObject::CREATE_RESULT_OK;
        }

        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, physics_context->m_BaseContext.m_MaxCollisionObjectCount);
        dmPhysics::NewWorldParams world_params;
        world_params.m_GetWorldTransformCallback = GetWorldTransform;
        world_params.m_SetWorldTransformCallback = SetWorldTransform;
        world_params.m_MaxCollisionObjectsCount  = comp_count;

        dmPhysics::HWorld3D physics_world = dmPhysics::NewWorld3D(physics_context->m_Context, world_params);
        if (physics_world == 0x0)
        {
            *params.m_World = 0x0;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        InstallBullet3DPhysicsAdapter();

        CollisionWorldBullet3D* world = new CollisionWorldBullet3D();
        memset(world, 0, sizeof(CollisionWorldBullet3D));

        world->m_BaseWorld.m_AdapterFunctions = g_PhysicsAdapter;
        world->m_World3D                      = physics_world;
        world->m_ComponentTypeIndex           = params.m_ComponentIndex;
        world->m_FirstUpdate                  = 1;
        world->m_Components.SetCapacity(comp_count);

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectBullet3DDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*) params.m_Context;
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*) params.m_World;

        if (world == 0x0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        dmPhysics::DeleteWorld3D(physics_context->m_Context, world->m_World3D);

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void SetCollisionObjectData(CollisionWorldBullet3D* world, CollisionComponentBullet3D* component, CollisionObjectResourceBullet3D* resource, dmPhysicsDDF::CollisionObjectDesc* ddf, bool enabled, dmPhysics::CollisionObjectData &out_data)
    {
        out_data.m_UserData = component;
        out_data.m_Type = (dmPhysics::CollisionObjectType)ddf->m_Type;
        out_data.m_Mass = ddf->m_Mass;
        out_data.m_Friction = ddf->m_Friction;
        out_data.m_Restitution = ddf->m_Restitution;
        out_data.m_Group = GetGroupBitIndex(&world->m_BaseWorld, resource->m_BaseResource.m_Group, false);
        out_data.m_Mask = 0;
        out_data.m_LinearDamping = ddf->m_LinearDamping;
        out_data.m_AngularDamping = ddf->m_AngularDamping;
        out_data.m_LockedRotation = ddf->m_LockedRotation;
        out_data.m_LockedRotation = ddf->m_LockedRotation;
        out_data.m_Bullet = ddf->m_Bullet;
        out_data.m_Enabled = enabled;
        for (uint32_t i = 0; i < 16 && resource->m_BaseResource.m_Mask[i] != 0; ++i)
        {
            out_data.m_Mask |= GetGroupBitIndex(&world->m_BaseWorld, resource->m_BaseResource.m_Mask[i], false);
        }
    }

    static bool CreateCollisionObject(PhysicsContextBullet3D* physics_context, CollisionWorldBullet3D* world, dmGameObject::HInstance instance, CollisionComponentBullet3D* component, bool enabled)
    {
        if (world == 0x0)
        {
            return false;
        }

        CollisionObjectResourceBullet3D* resource = (CollisionObjectResourceBullet3D*) component->m_BaseComponent.m_Resource;
        CollisionObjectResource* resource_base = (CollisionObjectResource*) resource;

        dmPhysicsDDF::CollisionObjectDesc* ddf = resource_base->m_DDF;
        dmPhysics::CollisionObjectData data;
        SetCollisionObjectData(world, component, resource, ddf, enabled, data);
        component->m_BaseComponent.m_Mask = data.m_Mask;

        component->m_BaseComponent.m_EventMask  = ddf->m_EventCollision?EVENT_MASK_COLLISION:0;
        component->m_BaseComponent.m_EventMask |= ddf->m_EventContact?EVENT_MASK_CONTACT:0;
        component->m_BaseComponent.m_EventMask |= ddf->m_EventTrigger?EVENT_MASK_TRIGGER:0;

        dmPhysics::HWorld3D physics_world = world->m_World3D;
        dmPhysics::HCollisionObject3D collision_object =
                dmPhysics::NewCollisionObject3D(physics_world, data,
                                                resource->m_Shapes3D,
                                                resource_base->m_ShapeTranslation,
                                                resource_base->m_ShapeRotation,
                                                resource_base->m_ShapeCount);

        if (collision_object != 0x0)
        {
            if (component->m_Object3D != 0x0)
                dmPhysics::DeleteCollisionObject3D(physics_world, component->m_Object3D);
            component->m_Object3D = collision_object;
        }
        else
        {
            return false;
        }
        return true;
    }

    dmGameObject::CreateResult CompCollisionObjectBullet3DCreate(const dmGameObject::ComponentCreateParams& params)
    {
        CollisionObjectResource* co_res = (CollisionObjectResource*)params.m_Resource;
        if (co_res == 0x0 || co_res->m_DDF == 0x0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        if ((co_res->m_DDF->m_Mass == 0.0f && co_res->m_DDF->m_Type == dmPhysicsDDF::COLLISION_OBJECT_TYPE_DYNAMIC) ||
            (co_res->m_DDF->m_Mass > 0.0f && co_res->m_DDF->m_Type != dmPhysicsDDF::COLLISION_OBJECT_TYPE_DYNAMIC))
        {
            dmLogError("Invalid mass %f for shape type %d", co_res->m_DDF->m_Mass, co_res->m_DDF->m_Type);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*)params.m_Context;
        CollisionComponentBullet3D* component = new CollisionComponentBullet3D();
        component->m_Object3D = 0;
        component->m_ShapeBuffer = 0;

        CollisionComponent* component_base = &component->m_BaseComponent;
        component_base->m_Resource         = (CollisionObjectResource*) params.m_Resource;
        component_base->m_Instance         = params.m_Instance;
        component_base->m_ComponentIndex   = params.m_ComponentIndex;
        component_base->m_AddedToUpdate    = false;
        component_base->m_StartAsEnabled   = true;

        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*) params.m_World;
        if (!CreateCollisionObject(physics_context, world, params.m_Instance, component, false))
        {
            delete component;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        *params.m_UserData = (uintptr_t)component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectBullet3DDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*) *params.m_UserData;
        CollisionWorldBullet3D* world         = (CollisionWorldBullet3D*) params.m_World;

        delete[] component->m_ShapeBuffer;

        if (component->m_Object3D != 0)
        {
            dmPhysics::HWorld3D physics_world = world->m_World3D;
            dmPhysics::DeleteCollisionObject3D(physics_world, component->m_Object3D);
            component->m_Object3D = 0;
        }

        uint32_t num_components = world->m_Components.Size();
        for (uint32_t i = 0; i < num_components; ++i)
        {
            CollisionComponentBullet3D* c = world->m_Components[i];
            if (c == component)
            {
                world->m_Components.EraseSwap(i);
                break;
            }
        }

        delete component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectBullet3DFinal(const dmGameObject::ComponentFinalParams& params)
    {
        CollisionComponent* component = (CollisionComponent*)*params.m_UserData;
        component->m_AddedToUpdate = false;
        component->m_StartAsEnabled = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DispatchCallback(dmMessage::Message* message, void* user_ptr)
    {
        DispatchContext* context = (DispatchContext*)user_ptr;

        if (message->m_Descriptor == 0)
            return;

        dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
        if (descriptor == dmPhysicsDDF::RequestRayCast::m_DDFDescriptor)
        {
            dmPhysicsDDF::RequestRayCast* ddf = (dmPhysicsDDF::RequestRayCast*)message->m_Data;

            dmhash_t coll_name_hash = dmMessage::GetSocketNameHash(message->m_Sender.m_Socket);

            // Target collection which can be different than we are updating for.
            dmGameObject::HCollection collection = dmGameObject::GetCollectionByHash(context->m_Register, coll_name_hash);
            if (!collection) // if the collection has been removed
                return;

            // NOTE! The collision world for the target collection is looked up using this worlds component index
            //       which is assumed to be the same as in the target collection.
            uint32_t component_type_index = context->m_ComponentTypeIndex;
            CollisionWorldBullet3D* world = (CollisionWorldBullet3D*) dmGameObject::GetWorld(collection, component_type_index);

            // Give that the assumption above holds, this assert will hold too.
            assert(world->m_ComponentTypeIndex == component_type_index);

            dmMessage::URL* receiver = (dmMessage::URL*)malloc(sizeof(dmMessage::URL));
            memcpy(receiver, &message->m_Sender, sizeof(*receiver));

            dmPhysics::RayCastRequest request;
            request.m_From = ddf->m_From;
            request.m_To = ddf->m_To;
            request.m_Mask = ddf->m_Mask;
            request.m_UserId = (ddf->m_RequestId & 0xff);
            request.m_UserData = (void*)receiver;
            request.m_IgnoredUserData = 0;
            if (!dmPhysics::RequestRayCast3D(world->m_World3D, request))
            {
                free(receiver);
            }
        }
    }

    // This function will dispatch on the (global) physics socket, and will potentially handle messages belonging to other collections
    // than the current one being updated.
    //
    // TODO: Make a nicer solution for this, perhaps a per-collection physics socket
    static bool CompCollisionObjectDispatchPhysicsMessages(PhysicsContextBullet3D* physics_context, CollisionWorldBullet3D* world, dmGameObject::HCollection collection)
    {
        DispatchContext dispatch_context;
        dispatch_context.m_PhysicsContext = physics_context;
        dispatch_context.m_Success = true;
        dispatch_context.m_ComponentTypeIndex = world->m_ComponentTypeIndex;
        dispatch_context.m_Register = dmGameObject::GetRegister(collection);

        dmMessage::HSocket physics_socket;
        physics_socket = dmPhysics::GetSocket3D(physics_context->m_Context);

        dmMessage::Dispatch(physics_socket, DispatchCallback, (void*)&dispatch_context);
        return dispatch_context.m_Success;
    }

    static void Step(CollisionWorldBullet3D* world, PhysicsContextBullet3D* physics_context, dmGameObject::HCollection collection, const dmPhysics::StepWorldContext* step_ctx)
    {
        CollisionUserData* collision_user_data = (CollisionUserData*)step_ctx->m_CollisionUserData;
        CollisionUserData* contact_user_data = (CollisionUserData*)step_ctx->m_ContactPointUserData;

        g_NumPhysicsTransformsUpdated = 0;

        if (!CompCollisionObjectDispatchPhysicsMessages(physics_context, world, collection))
        {
            dmLogWarning("Failed to dispatch physics messages");
        }

        DM_PROFILE("StepWorld3D");
        dmPhysics::StepWorld3D(world->m_World3D, *step_ctx);

        if (collision_user_data->m_Count >= physics_context->m_BaseContext.m_MaxCollisionCount && physics_context->m_BaseContext.m_MaxCollisionCount > 0)
        {
            if (!g_CollisionOverflowWarning)
            {
                dmLogWarning("Maximum number of collisions (%d) reached, messages have been lost. Tweak \"%s\" in the game.project file.", physics_context->m_BaseContext.m_MaxCollisionCount, PHYSICS_MAX_COLLISIONS_KEY);
                g_CollisionOverflowWarning = true;
            }
        }
        else
        {
            g_CollisionOverflowWarning = false;
        }
        if (contact_user_data->m_Count >= physics_context->m_BaseContext.m_MaxContactPointCount && physics_context->m_BaseContext.m_MaxContactPointCount > 0)
        {
            if (!g_ContactOverflowWarning)
            {
                dmLogWarning("Maximum number of contacts (%d) reached, messages have been lost. Tweak \"%s\" in the game.project file.", physics_context->m_BaseContext.m_MaxContactPointCount, PHYSICS_MAX_CONTACTS_KEY);
                g_ContactOverflowWarning = true;
            }
        }
        else
        {
            g_ContactOverflowWarning = false;
        }

        dmMessage::HSocket socket = dmGameObject::GetMessageSocket(collection);
        dmGameObject::DispatchMessages(collection, &socket, 1);

        if (g_NumPhysicsTransformsUpdated > 0)
        {
            dmGameObject::UpdateTransforms(collection);
        }
    }

    static dmGameObject::UpdateResult CompCollisionObjectUpdateInternal(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        if (params.m_World == 0x0)
            return dmGameObject::UPDATE_RESULT_OK;

        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*)params.m_Context;
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)params.m_World;

        CollisionUserData collision_user_data;
        collision_user_data.m_World = &world->m_BaseWorld;
        collision_user_data.m_Context = &physics_context->m_BaseContext;
        collision_user_data.m_Count = 0;
        CollisionUserData contact_user_data;
        contact_user_data.m_World = &world->m_BaseWorld;
        contact_user_data.m_Context = &physics_context->m_BaseContext;
        contact_user_data.m_Count = 0;

        dmPhysics::StepWorldContext step_world_context;
        step_world_context.m_CollisionCallback = CollisionCallback;
        step_world_context.m_CollisionUserData = &collision_user_data;
        step_world_context.m_ContactPointCallback = ContactPointCallback;
        step_world_context.m_ContactPointUserData = &contact_user_data;
        step_world_context.m_TriggerEnteredCallback = TriggerEnteredCallback;
        step_world_context.m_TriggerEnteredUserData = world;
        step_world_context.m_TriggerExitedCallback = TriggerExitedCallback;
        step_world_context.m_TriggerExitedUserData = world;
        step_world_context.m_RayCastCallback = RayCastCallback;
        step_world_context.m_RayCastUserData = world;
        step_world_context.m_FixedTimeStep = physics_context->m_BaseContext.m_UseFixedTimestep;
        step_world_context.m_MaxFixedTimeSteps = physics_context->m_BaseContext.m_MaxFixedTimesteps;

        step_world_context.m_DT = params.m_UpdateContext->m_DT;

        Step(world, physics_context, params.m_Collection, &step_world_context);

        // We only want to call this once per frame
        dmPhysics::SetDrawDebug3D(world->m_World3D, physics_context->m_BaseContext.m_Debug);

        DM_PROPERTY_ADD_U32(rmtp_CollisionObjectBullet3D, world->m_Components.Size());
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollisionObjectBullet3DUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {

        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        if (physics_context->m_UseFixedTimestep)
            return dmGameObject::UPDATE_RESULT_OK; // Let the fixed update handle this

        return CompCollisionObjectUpdateInternal(params, update_result);
    }

    dmGameObject::UpdateResult CompCollisionObjectBullet3DFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        if (!physics_context->m_UseFixedTimestep)
            return dmGameObject::UPDATE_RESULT_OK; // Let the dynamic update handle this

        return CompCollisionObjectUpdateInternal(params, update_result);
    }

    dmGameObject::UpdateResult CompCollisionObjectBullet3DPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params)
    {
        if (params.m_World == 0x0)
            return dmGameObject::UPDATE_RESULT_OK;

        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*)params.m_Context;
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)params.m_World;

        // Dispatch also in post-messages since messages might have been posting from script components, or init
        // functions in factories, and they should not linger around to next frame (which might not come around)
        if (!CompCollisionObjectDispatchPhysicsMessages(physics_context, world, params.m_Collection))
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void GetWorldTransform(void* user_data, dmTransform::Transform& world_transform)
    {
        if (!user_data)
            return;
        CollisionComponent* component = (CollisionComponent*)user_data;
        dmGameObject::HInstance instance = component->m_Instance;
        world_transform = dmGameObject::GetWorldTransform(instance);
    }

    static void SetWorldTransform(void* user_data, const dmVMath::Point3& position, const dmVMath::Quat& rotation)
    {
        if (!user_data)
            return;
        CollisionComponent* component = (CollisionComponent*)user_data;
        dmGameObject::HInstance instance = component->m_Instance;
        dmGameObject::SetPosition(instance, position);
        dmGameObject::SetRotation(instance, rotation);
        ++g_NumPhysicsTransformsUpdated;
    }

    dmGameObject::CreateResult CompCollisionObjectBullet3DAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)params.m_World;
        if (world == 0x0)
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        if (world->m_Components.Full())
        {
            ShowFullBufferError("Collision object", PHYSICS_MAX_COLLISION_OBJECTS_KEY, world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*)*params.m_UserData;
        assert(!component->m_BaseComponent.m_AddedToUpdate);

        dmPhysics::SetEnabled3D(world->m_World3D, component->m_Object3D, component->m_BaseComponent.m_StartAsEnabled);
        component->m_BaseComponent.m_AddedToUpdate = true;

        world->m_Components.Push(component);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollisionObjectBullet3DOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*)params.m_Context;
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*) *params.m_UserData;
        CollisionComponent* component_base = (CollisionComponent*) *params.m_UserData;

        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash
                || params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            bool enable = false;
            if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
            {
                enable = true;
            }
            CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)params.m_World;

            if (component_base->m_AddedToUpdate)
            {
                dmPhysics::SetEnabled3D(world->m_World3D, component->m_Object3D, enable);
            }
            else
            {
                // Deferred controlling the enabled state. Objects are force disabled until
                // they are added to update.
                component_base->m_StartAsEnabled = enable;
            }
        }
        else if (params.m_Message->m_Id == dmPhysicsDDF::ApplyForce::m_DDFDescriptor->m_NameHash)
        {
            dmPhysicsDDF::ApplyForce* af = (dmPhysicsDDF::ApplyForce*) params.m_Message->m_Data;
            dmPhysics::ApplyForce3D(physics_context->m_Context, component->m_Object3D, af->m_Force, af->m_Position);
        }
        else if (params.m_Message->m_Id == dmPhysicsDDF::RequestVelocity::m_DDFDescriptor->m_NameHash)
        {
            dmPhysicsDDF::VelocityResponse response;
            response.m_LinearVelocity = dmPhysics::GetLinearVelocity3D(physics_context->m_Context, component->m_Object3D);
            response.m_AngularVelocity = dmPhysics::GetAngularVelocity3D(physics_context->m_Context, component->m_Object3D);
            dmhash_t message_id = dmPhysicsDDF::VelocityResponse::m_DDFDescriptor->m_NameHash;
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::VelocityResponse::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::VelocityResponse);
            dmMessage::Result result = dmMessage::Post(&params.m_Message->m_Receiver, &params.m_Message->m_Sender, message_id, 0, descriptor, &response, data_size, 0);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send %s to component, result: %d.", dmPhysicsDDF::VelocityResponse::m_DDFDescriptor->m_Name, result);
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        else if (params.m_Message->m_Id == dmPhysicsDDF::SetGridShapeHull::m_DDFDescriptor->m_NameHash)
        {
            dmLogError("Grid shape hulls can only be set for 2D physics.");
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        else if(params.m_Message->m_Id == dmPhysicsDDF::EnableGridShapeLayer::m_DDFDescriptor->m_NameHash)
        {
            dmLogError("Grid shape layers can only be enabled for 2D physics.");
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompCollisionObjectBullet3DOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*)params.m_Context;
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)params.m_World;
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*)*params.m_UserData;
        component->m_BaseComponent.m_Resource = (CollisionObjectResource*)params.m_Resource;
        component->m_BaseComponent.m_AddedToUpdate = false;
        component->m_BaseComponent.m_StartAsEnabled = true;
        if (!CreateCollisionObject(physics_context, world, params.m_Instance, component, true))
        {
            dmLogError("%s", "Could not recreate collision object component, not reloaded.");
        }
    }

    dmGameObject::PropertyResult CompCollisionObjectBullet3DGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*)*params.m_UserData;
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*)params.m_Context;

        if (params.m_PropertyId == PROP_LINEAR_VELOCITY)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetLinearVelocity3D(physics_context->m_Context, component->m_Object3D));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANGULAR_VELOCITY)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetAngularVelocity3D(physics_context->m_Context, component->m_Object3D));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_MASS)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetMass3D(component->m_Object3D));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_BULLET)
        {
            dmLogWarning("'bullet' property not supported in 3d physics mode");
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }
        else if (params.m_PropertyId == PROP_LINEAR_DAMPING)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetLinearDamping3D(component->m_Object3D));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANGULAR_DAMPING)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmPhysics::GetAngularDamping3D(component->m_Object3D));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult CompCollisionObjectBullet3DSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*)*params.m_UserData;
        PhysicsContextBullet3D* physics_context = (PhysicsContextBullet3D*)params.m_Context;

        if (params.m_PropertyId == PROP_LINEAR_VELOCITY)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR3)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetLinearVelocity3D(physics_context->m_Context, component->m_Object3D, dmVMath::Vector3(params.m_Value.m_V4[0], params.m_Value.m_V4[1], params.m_Value.m_V4[2]));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANGULAR_VELOCITY)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR3)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetAngularVelocity3D(physics_context->m_Context, component->m_Object3D, dmVMath::Vector3(params.m_Value.m_V4[0], params.m_Value.m_V4[1], params.m_Value.m_V4[2]));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_BULLET)
        {
            dmLogWarning("'bullet' property not supported in 3d physics mode");
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }
        else if (params.m_PropertyId == PROP_LINEAR_DAMPING) {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetLinearDamping3D(component->m_Object3D, params.m_Value.m_Number);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ANGULAR_DAMPING)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            dmPhysics::SetAngularDamping3D(component->m_Object3D, params.m_Value.m_Number);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_MASS)
        {
            return dmGameObject::PROPERTY_RESULT_READ_ONLY;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    // Adapter functions
    static bool IsEnabledBullet3D(CollisionWorld* world, CollisionComponent* component)
    {
        CollisionComponentBullet3D* _component = (CollisionComponentBullet3D*) component;
        return dmPhysics::IsEnabled3D(_component->m_Object3D);
    }

    static void WakeupCollisionBullet3D(CollisionWorld* _world, CollisionComponent* _component)
    {
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*) _component;
        dmPhysics::Wakeup3D(component->m_Object3D);
    }

    static void RayCastBullet3D(CollisionWorld* _world, const dmPhysics::RayCastRequest& request, dmArray<dmPhysics::RayCastResponse>& results)
    {
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)_world;
        dmPhysics::RayCast3D(world->m_World3D, request, results);
    }

    static void SetGravityBullet3D(CollisionWorld* _world, const dmVMath::Vector3& gravity)
    {
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)_world;
        dmPhysics::SetGravity3D(world->m_World3D, gravity);
    }

    static dmVMath::Vector3 GetGravityBullet3D(CollisionWorld* _world)
    {
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)_world;
        return dmPhysics::GetGravity3D(world->m_World3D);
    }

    static dmhash_t GetCollisionGroupBullet3D(CollisionWorld* _world, CollisionComponent* _component)
    {
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*)_component;
        uint16_t groupbit = dmPhysics::GetGroup3D(component->m_Object3D);
        return GetLSBGroupHash(_world, groupbit);
    }

    // returns false if no such collision group has been registered
    static bool SetCollisionGroupBullet3D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t group_hash)
    {
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)_world;
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*)_component;

        uint16_t groupbit = GetGroupBitIndex(&world->m_BaseWorld, group_hash, true);
        if (!groupbit)
        {
            return false; // error. No such group.
        }

        dmPhysics::SetGroup3D(world->m_World3D, component->m_Object3D, groupbit);
        return true; // all good
    }

    // Updates 'maskbit' with the mask value. Returns false if no such collision group has been registered.
    static bool GetCollisionMaskBitBullet3D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t group_hash, bool* maskbit)
    {
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)_world;
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*)_component;

        uint16_t groupbit = GetGroupBitIndex(&world->m_BaseWorld, group_hash, true);
        if (!groupbit) {
            return false;
        }

        *maskbit = dmPhysics::GetMaskBit3D(component->m_Object3D, groupbit);
        return true;
    }

    // returns false if no such collision group has been registered
    static bool SetCollisionMaskBitBullet3D(CollisionWorld* _world, CollisionComponent* _component, dmhash_t group_hash, bool boolvalue)
    {
        CollisionWorldBullet3D* world = (CollisionWorldBullet3D*)_world;
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*)_component;

        uint16_t groupbit = GetGroupBitIndex(&world->m_BaseWorld, group_hash, true);
        if (!groupbit)
        {
            return false;
        }

        dmPhysics::SetMaskBit3D(world->m_World3D, component->m_Object3D, groupbit, boolvalue);
        return true;
    }

    static inline dmPhysics::HCollisionShape3D GetShape3D(CollisionComponentBullet3D* component, uint32_t shape_index)
    {
        if (component->m_ShapeBuffer)
            return component->m_ShapeBuffer[shape_index];
        return dmPhysics::GetCollisionShape3D(component->m_Object3D, shape_index);
    }

    static bool GetShapeBullet3D(CollisionWorld* _world, CollisionComponent* _component, uint32_t shape_ix, ShapeInfo* shape_info)
    {
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*) _component;
        uint32_t shape_count = _component->m_Resource->m_ShapeCount;

        if (shape_ix >= shape_count)
        {
            return false;
        }

        shape_info->m_Type = _component->m_Resource->m_ShapeTypes[shape_ix];

        dmPhysics::HCollisionShape3D shape3d = GetShape3D(component, shape_ix);

        switch(shape_info->m_Type)
        {
            case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
            {
                float sphere_radius;
                dmPhysics::GetCollisionShapeRadius3D(shape3d, &sphere_radius);
                shape_info->m_SphereDiameter = sphere_radius * 2.0f;
            } break;
            case dmPhysicsDDF::CollisionShape::TYPE_BOX:
            {
                float half_extents[3];
                dmPhysics::GetCollisionShapeHalfBoxExtents3D(shape3d, half_extents);
                shape_info->m_BoxDimensions[0] = half_extents[0] * 2.0f;
                shape_info->m_BoxDimensions[1] = half_extents[1] * 2.0f;
                shape_info->m_BoxDimensions[2] = half_extents[2] * 2.0f;
            } break;
            case dmPhysicsDDF::CollisionShape::TYPE_CAPSULE:
            {
                float radius, half_height;
                dmPhysics::GetCollisionShapeCapsuleRadiusHeight3D(shape3d, &radius, &half_height);
                shape_info->m_CapsuleDiameterHeight[0] = radius * 2.0f;
                shape_info->m_CapsuleDiameterHeight[1] = half_height * 2.0f;
            } break;
            default: assert(0);
        }
        return true;
    }

    static void ReplaceAndDeleteShape3D(dmPhysics::HContext3D context,
        CollisionComponentBullet3D* component,
        dmPhysics::HCollisionShape3D old_shape,
        dmPhysics::HCollisionShape3D new_shape,
        uint32_t shape_index)
    {
        uint32_t shape_count = component->m_BaseComponent.m_Resource->m_ShapeCount;

        // The direction table is needed for component that has a box shape,
        // because it is the only shape that currently needs to alter the shape hierarchy
        // of a collision object.
        if (!component->m_ShapeBuffer)
        {
            component->m_ShapeBuffer = new dmPhysics::HCollisionShape3D[shape_count];
            uint32_t res = dmPhysics::GetCollisionShapes3D(component->m_Object3D, component->m_ShapeBuffer, shape_count);
            assert(res == shape_count);
        }

        dmPhysics::ReplaceShape3D(context, old_shape, new_shape);
        dmPhysics::ReplaceShape3D(component->m_Object3D, old_shape, new_shape);
        dmPhysics::DeleteCollisionShape3D(old_shape);

        component->m_ShapeBuffer[shape_index] = new_shape;
    }

    static bool SetShapeBullet3D(CollisionWorld* _world, CollisionComponent* _component, uint32_t shape_ix, ShapeInfo* shape_info)
    {
        CollisionWorldBullet3D* world         = (CollisionWorldBullet3D*)_world;
        CollisionComponentBullet3D* component = (CollisionComponentBullet3D*) _component;
        uint32_t shape_count                  = _component->m_Resource->m_ShapeCount;

        if (shape_ix >= shape_count)
        {
            return false;
        }

        dmPhysics::HCollisionShape3D shape3d = GetShape3D(component, shape_ix);

        switch(shape_info->m_Type)
        {
            case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
            {
                dmPhysics::SetCollisionShapeRadius3D(shape3d, shape_info->m_SphereDiameter * 0.5f);
            } break;
            case dmPhysicsDDF::CollisionShape::TYPE_BOX:
            {
                dmPhysics::HCollisionShape3D new_shape = dmPhysics::NewBoxShape3D(dmPhysics::GetContext3D(world->m_World3D),
                    dmVMath::Vector3(shape_info->m_BoxDimensions[0] * 0.5f,
                                     shape_info->m_BoxDimensions[1] * 0.5f,
                                     shape_info->m_BoxDimensions[2] * 0.5f));

                ReplaceAndDeleteShape3D(dmPhysics::GetContext3D(world->m_World3D), component, shape3d, new_shape, shape_ix);
            } break;
            case dmPhysicsDDF::CollisionShape::TYPE_CAPSULE:
            {
                dmPhysics::HCollisionShape3D new_shape = dmPhysics::NewCapsuleShape3D(dmPhysics::GetContext3D(world->m_World3D),
                    shape_info->m_CapsuleDiameterHeight[0] * 0.5f,
                    shape_info->m_CapsuleDiameterHeight[1]);

                ReplaceAndDeleteShape3D(dmPhysics::GetContext3D(world->m_World3D), component, shape3d, new_shape, shape_ix);
            } break;
            default: assert(0);
        }

        return true;
    }

    static PhysicsEngineType GetPhysicsEngineTypeBullet3D(CollisionWorld* _world)
    {
        return PhysicsEngineType::PHYSICS_ENGINE_BULLET3D;
    }

    static void InstallBullet3DPhysicsAdapter()
    {
        if (g_PhysicsAdapter)
        {
            return;
        }

        g_PhysicsAdapter = new PhysicsAdapterFunctionTable();
        g_PhysicsAdapter->m_IsEnabled              = IsEnabledBullet3D;
        g_PhysicsAdapter->m_WakeupCollision        = WakeupCollisionBullet3D;
        g_PhysicsAdapter->m_RayCast                = RayCastBullet3D;
        g_PhysicsAdapter->m_SetGravity             = SetGravityBullet3D;
        g_PhysicsAdapter->m_GetGravity             = GetGravityBullet3D;
        g_PhysicsAdapter->m_GetCollisionGroup      = GetCollisionGroupBullet3D;
        g_PhysicsAdapter->m_SetCollisionGroup      = SetCollisionGroupBullet3D;
        g_PhysicsAdapter->m_GetCollisionMaskBit    = GetCollisionMaskBitBullet3D;
        g_PhysicsAdapter->m_SetCollisionMaskBit    = SetCollisionMaskBitBullet3D;
        g_PhysicsAdapter->m_GetShapeIndex          = GetShapeIndexShared;
        g_PhysicsAdapter->m_GetShape               = GetShapeBullet3D;
        g_PhysicsAdapter->m_SetShape               = SetShapeBullet3D;
        g_PhysicsAdapter->m_GetPhysicsEngineType   = GetPhysicsEngineTypeBullet3D;

        // Unimplemented functions:
        //g_PhysicsAdapter->m_SetCollisionFlipH      = SetCollisionFlipHBullet3D;
        //g_PhysicsAdapter->m_SetCollisionFlipV      = SetCollisionFlipVBullet3D;
        //g_PhysicsAdapter->m_UpdateMass             = UpdateMassBullet3D;
        //g_PhysicsAdapter->m_CreateJoint            = CreateJointBullet3D;
        //g_PhysicsAdapter->m_DestroyJoint           = DestroyJointBullet3D;
        //g_PhysicsAdapter->m_GetJointParams         = GetJointParamsBullet3D;
        //g_PhysicsAdapter->m_GetJointType           = GetJointTypeBullet3D;
        //g_PhysicsAdapter->m_SetJointParams         = SetJointParamsBullet3D;
        //g_PhysicsAdapter->m_GetJointReactionForce  = GetJointReactionForceBullet3D;
        //g_PhysicsAdapter->m_GetJointReactionTorque = GetJointReactionTorqueBullet3D;
    }
}
