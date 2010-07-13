#include "gameobject_registration.h"

#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/event.h>
#include <dlib/hash.h>
#include <ddf/ddf.h>
#include "gameobject.h"
#include <physics/physics.h>
#include "resource_creation.h"

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    dmGameObject::CreateResult CreateRigidBody(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* context,
                                               uintptr_t* user_data)
    {
        assert(user_data);

        RigidBodyPrototype* rigid_body_prototype = (RigidBodyPrototype*) resource;
        dmPhysics::HWorld world = (dmPhysics::HWorld) context;

        dmPhysics::HRigidBody rigid_body = dmPhysics::NewRigidBody(world, rigid_body_prototype->m_CollisionShape, instance, rigid_body_prototype->m_Mass, instance);
        *user_data = (uintptr_t) rigid_body;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult InitRigidBody(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* context,
                                            uintptr_t* user_data)
    {
        assert(user_data);
        dmPhysics::HRigidBody rigid_body = (dmPhysics::HRigidBody)*user_data;

        Point3 position = dmGameObject::GetWorldPosition(instance);
        Quat rotation = dmGameObject::GetWorldRotation(instance);

        dmPhysics::SetRigidBodyInitialTransform(rigid_body, position, rotation);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult DestroyRigidBody(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* context,
                                                uintptr_t* user_data)
    {
        assert(user_data);
        dmPhysics::HWorld world = (dmPhysics::HWorld) context;

        dmPhysics::HRigidBody rigid_body = (dmPhysics::HRigidBody) *user_data;
        dmPhysics::DeleteRigidBody(world, rigid_body);
        return dmGameObject::CREATE_RESULT_OK;
    }

    void CollisionCallback(void* user_data_a, void* user_data_b, void* user_data)
    {
        dmGameObject::HInstance instance_a = (dmGameObject::HInstance)user_data_a;
        dmGameObject::HInstance instance_b = (dmGameObject::HInstance)user_data_b;
        const uint32_t data_size = sizeof(dmPhysics::CollisionMessage) + 9;
        char data[data_size];
        dmPhysics::CollisionMessage* ddf = (dmPhysics::CollisionMessage*)&data;
        ddf->m_OtherGameObjectId = (char*)sizeof(dmPhysics::CollisionMessage);
        char* id = &data[sizeof(dmPhysics::CollisionMessage)];
        // Broadcast to A components
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_b));
        dmGameObject::PostNamedEvent(instance_a, 0x0, dmPhysics::CollisionMessage::m_DDFDescriptor->m_Name, dmPhysics::CollisionMessage::m_DDFDescriptor, data);
        // Broadcast to B components
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_a));
        dmGameObject::PostNamedEvent(instance_b, 0x0, dmPhysics::CollisionMessage::m_DDFDescriptor->m_Name, dmPhysics::CollisionMessage::m_DDFDescriptor, data);
    }

    void ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
    {
        dmGameObject::HInstance instance_a = (dmGameObject::HInstance)contact_point.m_UserDataA;
        dmGameObject::HInstance instance_b = (dmGameObject::HInstance)contact_point.m_UserDataB;
        char data[sizeof(dmPhysics::ContactPointMessage) + 9];
        dmPhysics::ContactPointMessage* ddf = (dmPhysics::ContactPointMessage*)&data;
        ddf->m_OtherGameObjectId = (char*)sizeof(dmPhysics::ContactPointMessage);
        char* id = &data[sizeof(dmPhysics::ContactPointMessage)];
        // Broadcast to A components
        ddf->m_Position = contact_point.m_PositionA;
        ddf->m_Normal = contact_point.m_Normal;
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_b));
        dmGameObject::PostNamedEvent(instance_a, 0x0, dmPhysics::ContactPointMessage::m_DDFDescriptor->m_Name, dmPhysics::ContactPointMessage::m_DDFDescriptor, data);
        // Broadcast to B components
        ddf->m_Position = contact_point.m_PositionB;
        ddf->m_Normal = -contact_point.m_Normal;
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_a));
        dmGameObject::PostNamedEvent(instance_b, 0x0, dmPhysics::ContactPointMessage::m_DDFDescriptor->m_Name, dmPhysics::ContactPointMessage::m_DDFDescriptor, data);
    }

    dmGameObject::UpdateResult UpdateRigidBody(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* context)
    {
        dmPhysics::HWorld world = (dmPhysics::HWorld) context;
        dmPhysics::StepWorld(world, update_context->m_DT);
        dmPhysics::ForEachCollision(world, CollisionCallback, 0x0, ContactPointCallback, 0x0);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult OnEventRigidBody(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            const dmGameObject::ScriptEventData* event_data,
            void* context,
            uintptr_t* user_data)
    {
        if (event_data->m_EventHash == dmHashString32("ApplyForceMessage"))
        {
            dmPhysics::ApplyForceMessage* af = (dmPhysics::ApplyForceMessage*) event_data->m_DDFData;
            dmPhysics::HRigidBody rigid_body = (dmPhysics::HRigidBody) *user_data;
            dmPhysics::ApplyForce(rigid_body, af->m_Force, af->m_Position);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::Result RegisterPhysicsComponent(dmResource::HFactory factory,
                                                  dmGameObject::HCollection collection,
                                                  dmPhysics::HWorld physics_world)
    {
        uint32_t type;

        dmGameObject::RegisterDDFType(dmPhysics::ApplyForceMessage::m_DDFDescriptor);
        dmEvent::Register(dmHashString32(dmPhysics::ApplyForceMessage::m_DDFDescriptor->m_Name), sizeof(dmGameObject::ScriptEventData) + sizeof(dmPhysics::ApplyForceMessage));

        dmResource::FactoryResult fact_result = dmResource::GetTypeFromExtension(factory, "rigidbody", &type);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
        {
            dmLogWarning("Unable to get resource type for 'rigidbody' (%d)", fact_result);
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }
        dmGameObject::ComponentType component_type;
        component_type.m_Name = "rigidbody";
        component_type.m_ResourceType = type;
        component_type.m_Context = physics_world;
        component_type.m_CreateFunction = &CreateRigidBody;
        component_type.m_InitFunction = &InitRigidBody;
        component_type.m_DestroyFunction = &DestroyRigidBody;
        component_type.m_UpdateFunction = &UpdateRigidBody;
        component_type.m_OnEventFunction = &OnEventRigidBody;
        component_type.m_InstanceHasUserData = (uint32_t)true;
        dmGameObject::Result res = dmGameObject::RegisterComponentType(collection, component_type);
        return res;
    }
}
