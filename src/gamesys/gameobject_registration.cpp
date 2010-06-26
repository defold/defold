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

        dmPhysics::HRigidBody rigid_body = dmPhysics::NewRigidBody(world, rigid_body_prototype->m_CollisionShape, instance, rigid_body_prototype->m_Mass);
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

    void UpdateRigidBody(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* context)
    {
        dmPhysics::HWorld world = (dmPhysics::HWorld) context;
        dmPhysics::StepWorld(world, update_context->m_DT);
    }

    void OnEventRigidBody(dmGameObject::HCollection collection,
    		dmGameObject::HInstance instance,
			const dmGameObject::ScriptEventData* event_data,
			void* context,
			uintptr_t* user_data)
    {
        if (event_data->m_EventHash == dmHashString32("ApplyForce"))
        {
        	dmPhysics::ApplyForceMessage* af = (dmPhysics::ApplyForceMessage*) event_data->m_DDFData;
            dmPhysics::HRigidBody rigid_body = (dmPhysics::HRigidBody) *user_data;
            dmPhysics::ApplyForce(rigid_body, af->m_Force, af->m_RelativePosition);
        }
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
