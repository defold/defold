#include "comp_collision_object.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <physics/physics.h>

#include "gamesys.h"
#include "../resources/res_collision_object.h"

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    static const uint32_t MAX_COLLISION_COUNT = 64;
    static const uint32_t MAX_CONTACT_COUNT = 128;

    struct Component
    {
        CollisionObjectResource* m_Resource;
        union
        {
            dmPhysics::HCollisionObject3D m_Object3D;
            dmPhysics::HCollisionObject2D m_Object2D;
        };
    };

    void GetWorldTransform(void* user_data, Vectormath::Aos::Point3& position, Vectormath::Aos::Quat& rotation)
    {
        if (!user_data)
            return;
        dmGameObject::HInstance instance = (dmGameObject::HInstance)user_data;
        position = dmGameObject::GetWorldPosition(instance);
        rotation = dmGameObject::GetWorldRotation(instance);
    }

    void SetWorldTransform(void* user_data, const Vectormath::Aos::Point3& position, const Vectormath::Aos::Quat& rotation)
    {
        if (!user_data)
            return;
        dmGameObject::HInstance instance = (dmGameObject::HInstance)user_data;
        dmGameObject::SetPosition(instance, position);
        dmGameObject::SetRotation(instance, rotation);
    }

    dmGameObject::CreateResult CompCollisionObjectNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        dmPhysics::NewWorldParams world_params;
        world_params.m_GetWorldTransformCallback = GetWorldTransform;
        world_params.m_SetWorldTransformCallback = SetWorldTransform;
        if (physics_context->m_3D)
            *params.m_World = dmPhysics::NewWorld3D(physics_context->m_Context3D, world_params);
        else
            *params.m_World = dmPhysics::NewWorld2D(physics_context->m_Context2D, world_params);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        if (physics_context->m_3D)
            dmPhysics::DeleteWorld3D(physics_context->m_Context3D, (dmPhysics::HWorld3D)params.m_World);
        else
            dmPhysics::DeleteWorld2D(physics_context->m_Context2D, (dmPhysics::HWorld2D)params.m_World);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectCreate(const dmGameObject::ComponentCreateParams& params)
    {
        CollisionObjectResource* co_res = (CollisionObjectResource*)params.m_Resource;
        if (co_res == 0x0 || co_res->m_ConvexShape == 0x0 || co_res->m_DDF == 0x0)
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        if ((co_res->m_DDF->m_Mass == 0.0f && co_res->m_DDF->m_Type == dmPhysicsDDF::COLLISION_OBJECT_TYPE_DYNAMIC)
            || (co_res->m_DDF->m_Mass > 0.0f && co_res->m_DDF->m_Type != dmPhysicsDDF::COLLISION_OBJECT_TYPE_DYNAMIC))
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        Component* component = new Component();
        component->m_Resource = (CollisionObjectResource*)params.m_Resource;
        component->m_Object2D = 0;
        *params.m_UserData = (uintptr_t)component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        delete (Component*)*params.m_UserData;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectInit(const dmGameObject::ComponentInitParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        Component* component = (Component*) *params.m_UserData;
        if (component->m_Object2D == 0)
        {
            dmPhysics::CollisionObjectData data;
            data.m_UserData = params.m_Instance;
            data.m_Type = (dmPhysics::CollisionObjectType)component->m_Resource->m_DDF->m_Type;
            data.m_Mass = component->m_Resource->m_DDF->m_Mass;
            data.m_Friction = component->m_Resource->m_DDF->m_Friction;
            data.m_Restitution = component->m_Resource->m_DDF->m_Restitution;
            data.m_Group = component->m_Resource->m_DDF->m_Group;
            data.m_Mask = component->m_Resource->m_Mask;
            if (physics_context->m_3D)
            {
                dmPhysics::HWorld3D physics_world = (dmPhysics::HWorld3D) params.m_World;
                component->m_Object3D = dmPhysics::NewCollisionObject3D(physics_world, data, component->m_Resource->m_ConvexShape->m_Shape3D);
                if (component->m_Object3D != 0x0)
                {
                    return dmGameObject::CREATE_RESULT_OK;
                }
            }
            else
            {
                dmPhysics::HWorld2D physics_world = (dmPhysics::HWorld2D) params.m_World;
                component->m_Object2D = dmPhysics::NewCollisionObject2D(physics_world, data, component->m_Resource->m_ConvexShape->m_Shape2D);
                if (component->m_Object2D != 0x0)
                {
                    return dmGameObject::CREATE_RESULT_OK;
                }
            }
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectFinal(const dmGameObject::ComponentFinalParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        Component* component = (Component*)*params.m_UserData;
        if (physics_context->m_3D)
        {
            if (component->m_Object3D != 0)
            {
                dmPhysics::HWorld3D physics_world = (dmPhysics::HWorld3D) params.m_World;
                dmPhysics::DeleteCollisionObject3D(physics_world, component->m_Object3D);
                component->m_Object3D = 0;
            }
        }
        else
        {
            if (component->m_Object2D != 0)
            {
                dmPhysics::HWorld2D physics_world = (dmPhysics::HWorld2D) params.m_World;
                dmPhysics::DeleteCollisionObject2D(physics_world, component->m_Object2D);
                component->m_Object2D = 0;
            }
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    bool CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
    {
        uint32_t* count = (uint32_t*)user_data;
        if (*count < MAX_COLLISION_COUNT)
        {
            *count += 1;

            dmGameObject::HInstance instance_a = (dmGameObject::HInstance)user_data_a;
            dmGameObject::HInstance instance_b = (dmGameObject::HInstance)user_data_b;
            dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
            dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);
            dmhash_t message_id = dmHashString64(dmPhysicsDDF::CollisionMessage::m_DDFDescriptor->m_Name);
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::CollisionMessage::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::CollisionMessage);
            dmPhysicsDDF::CollisionMessage ddf;
            dmMessage::URL receiver;

            // Broadcast to A components
            ddf.m_Group = group_b;
            ddf.m_OtherGameObjectId = instance_b_id;
            receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(instance_a));
            receiver.m_Path = instance_a_id;
            dmMessage::Result result = dmMessage::Post(0x0, &receiver, message_id, 0, descriptor, &ddf, data_size);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send collision callback to component: %d", result);
            }

            // Broadcast to B components
            ddf.m_Group = group_a;
            ddf.m_OtherGameObjectId = dmGameObject::GetIdentifier(instance_a);
            receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(instance_b));
            receiver.m_Path = instance_b_id;
            result = dmMessage::Post(0x0, &receiver, message_id, 0, descriptor, &ddf, data_size);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send collision callback to component: %d", result);
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
        uint32_t* count = (uint32_t*)user_data;
        if (*count < MAX_CONTACT_COUNT)
        {
            *count += 1;

            dmGameObject::HInstance instance_a = (dmGameObject::HInstance)contact_point.m_UserDataA;
            dmGameObject::HInstance instance_b = (dmGameObject::HInstance)contact_point.m_UserDataB;
            dmhash_t instance_a_id = dmGameObject::GetIdentifier(instance_a);
            dmhash_t instance_b_id = dmGameObject::GetIdentifier(instance_b);

            dmPhysicsDDF::ContactPointMessage ddf;
            float mass_a = dmMath::Select(-contact_point.m_InvMassA, 0.0f, 1.0f / contact_point.m_InvMassA);
            float mass_b = dmMath::Select(-contact_point.m_InvMassB, 0.0f, 1.0f / contact_point.m_InvMassB);

            dmhash_t message_id = dmHashString64(dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor->m_Name);
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::ContactPointMessage);
            dmMessage::URL receiver;

            // Broadcast to A components
            ddf.m_Position = contact_point.m_PositionA;
            ddf.m_Normal = -contact_point.m_Normal;
            ddf.m_RelativeVelocity = -contact_point.m_RelativeVelocity;
            ddf.m_Distance = contact_point.m_Distance;
            ddf.m_AppliedImpulse = contact_point.m_AppliedImpulse;
            ddf.m_Mass = mass_a;
            ddf.m_OtherMass = mass_b;
            ddf.m_OtherGameObjectId = dmGameObject::GetIdentifier(instance_b);
            ddf.m_Group = contact_point.m_GroupB;
            receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(instance_a));
            receiver.m_Path = instance_a_id;
            dmMessage::Result result = dmMessage::Post(0x0, &receiver, message_id, 0, descriptor, &ddf, data_size);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send collision callback to component: %d", result);
            }

            // Broadcast to B components
            ddf.m_Position = contact_point.m_PositionB;
            ddf.m_Normal = contact_point.m_Normal;
            ddf.m_RelativeVelocity = contact_point.m_RelativeVelocity;
            ddf.m_Distance = contact_point.m_Distance;
            ddf.m_AppliedImpulse = contact_point.m_AppliedImpulse;
            ddf.m_Mass = mass_b;
            ddf.m_OtherMass = mass_a;
            ddf.m_OtherGameObjectId = dmGameObject::GetIdentifier(instance_a);
            ddf.m_Group = contact_point.m_GroupA;
            receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(instance_b));
            receiver.m_Path = instance_b_id;
            result = dmMessage::Post(0x0, &receiver, message_id, 0, descriptor, &ddf, data_size);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send collision callback to component: %d", result);
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    static bool g_CollisionOverflowWarning = false;
    static bool g_ContactOverflowWarning = false;

    dmGameObject::UpdateResult CompCollisionObjectUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        if (params.m_World == 0x0)
            return dmGameObject::UPDATE_RESULT_OK;
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        uint32_t collision_count = 0;
        uint32_t contact_count = 0;
        if (physics_context->m_3D)
        {
            dmPhysics::HWorld3D physics_world = (dmPhysics::HWorld3D) params.m_World;
            dmPhysics::SetCollisionCallback3D(physics_world, CollisionCallback, &collision_count);
            dmPhysics::SetContactPointCallback3D(physics_world, ContactPointCallback, &contact_count);
            dmPhysics::StepWorld3D(physics_world, params.m_UpdateContext->m_DT);
        }
        else
        {
            dmPhysics::HWorld2D physics_world = (dmPhysics::HWorld2D) params.m_World;
            dmPhysics::SetCollisionCallback2D(physics_world, CollisionCallback, &collision_count);
            dmPhysics::SetContactPointCallback2D(physics_world, ContactPointCallback, &contact_count);
            dmPhysics::StepWorld2D(physics_world, params.m_UpdateContext->m_DT);
        }
        if (collision_count >= 128)
        {
            if (!g_CollisionOverflowWarning)
            {
                dmLogWarning("Maximum number of collisions (%d) reached, messages have been lost.", MAX_COLLISION_COUNT);
                g_CollisionOverflowWarning = true;
            }
        }
        else
        {
            g_CollisionOverflowWarning = false;
        }
        if (contact_count >= 128)
        {
            if (!g_ContactOverflowWarning)
            {
                dmLogWarning("Maximum number of contacts (%d) reached, messages have been lost.", MAX_CONTACT_COUNT);
                g_ContactOverflowWarning = true;
            }
        }
        else
        {
            g_ContactOverflowWarning = false;
        }
        if (physics_context->m_Debug)
        {
            if (physics_context->m_3D)
                dmPhysics::DrawDebug3D((dmPhysics::HWorld3D)params.m_World);
            else
                dmPhysics::DrawDebug2D((dmPhysics::HWorld2D)params.m_World);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollisionObjectOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        Component* component = (Component*) *params.m_UserData;
        if (params.m_Message->m_Id == dmHashString64(dmPhysicsDDF::ApplyForceMessage::m_DDFDescriptor->m_Name))
        {
            dmPhysicsDDF::ApplyForceMessage* af = (dmPhysicsDDF::ApplyForceMessage*) params.m_Message->m_Data;
            if (physics_context->m_3D)
            {
                dmPhysics::ApplyForce3D(component->m_Object3D, af->m_Force, af->m_Position);
            }
            else
            {
                dmPhysics::ApplyForce2D(component->m_Object2D, af->m_Force, af->m_Position);
            }
        }
        if (params.m_Message->m_Id == dmHashString64(dmPhysicsDDF::VelocityRequest::m_DDFDescriptor->m_Name))
        {
            dmPhysicsDDF::VelocityResponse response;
            if (physics_context->m_3D)
            {
                response.m_LinearVelocity = dmPhysics::GetLinearVelocity3D(component->m_Object3D);
                response.m_AngularVelocity = dmPhysics::GetAngularVelocity3D(component->m_Object3D);
            }
            else
            {
                response.m_LinearVelocity = dmPhysics::GetLinearVelocity2D(component->m_Object2D);
                response.m_AngularVelocity = dmPhysics::GetAngularVelocity2D(component->m_Object2D);
            }
            dmhash_t message_id = dmHashString64(dmPhysicsDDF::VelocityResponse::m_DDFDescriptor->m_Name);
            uintptr_t descriptor = (uintptr_t)dmPhysicsDDF::VelocityResponse::m_DDFDescriptor;
            uint32_t data_size = sizeof(dmPhysicsDDF::VelocityResponse);
            dmMessage::Result result = dmMessage::Post(&params.m_Message->m_Receiver, &params.m_Message->m_Sender, message_id, 0, descriptor, &response, data_size);
            if (result != dmMessage::RESULT_OK)
            {
                dmLogError("Could not send %s to component, result: %d.", dmPhysicsDDF::VelocityResponse::m_DDFDescriptor->m_Name, result);
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompCollisionObjectOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        PhysicsContext* physics_context = (PhysicsContext*)params.m_Context;
        CollisionObjectResource* co_resource = (CollisionObjectResource*) params.m_Resource;
        Component* component = (Component*)*params.m_UserData;
        dmPhysics::CollisionObjectData data;
        data.m_UserData = params.m_Instance;
        data.m_Type = (dmPhysics::CollisionObjectType)co_resource->m_DDF->m_Type;
        data.m_Mass = co_resource->m_DDF->m_Mass;
        data.m_Friction = co_resource->m_DDF->m_Friction;
        data.m_Restitution = co_resource->m_DDF->m_Restitution;
        data.m_Group = co_resource->m_DDF->m_Group;
        data.m_Mask = co_resource->m_Mask;
        bool result = false;
        if (physics_context->m_3D)
        {
            dmPhysics::HWorld3D physics_world = (dmPhysics::HWorld3D) params.m_World;
            dmPhysics::HCollisionObject3D collision_object = dmPhysics::NewCollisionObject3D(physics_world, data, co_resource->m_ConvexShape->m_Shape3D);
            if (collision_object != 0x0)
            {
                dmPhysics::DeleteCollisionObject3D(physics_world, component->m_Object3D);
                component->m_Object3D = collision_object;
                result = true;
            }
        }
        else
        {
            dmPhysics::HWorld2D physics_world = (dmPhysics::HWorld2D) params.m_World;
            dmPhysics::HCollisionObject2D collision_object = dmPhysics::NewCollisionObject2D(physics_world, data, co_resource->m_ConvexShape->m_Shape2D);
            if (collision_object != 0x0)
            {
                dmPhysics::DeleteCollisionObject2D(physics_world, component->m_Object2D);
                component->m_Object2D = collision_object;
                result = true;
            }
        }
        if (!result)
            dmLogError("%s", "Could not recreate collision object component, not reloaded.");
    }
}
