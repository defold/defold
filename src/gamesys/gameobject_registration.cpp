#include "gameobject_registration.h"

#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/message.h>
#include <dlib/hash.h>
#include <ddf/ddf.h>
#include <gameobject/gameobject.h>
#include <physics/physics.h>
#include "resource_creation.h"
#include <render/model_ddf.h>
#include <render/model/model.h>
#include <render/render.h>

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
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

    dmGameObject::CreateResult GOCollisionObjectNewWorld(void* context, void** world)
    {
        *world = dmPhysics::NewWorld(Vectormath::Aos::Point3(-1000, -1000, -1000), Vectormath::Aos::Point3(1000, 1000, 1000), &GetWorldTransform, &SetWorldTransform);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult GOCollisionObjectDeleteWorld(void* context, void* world)
    {
        dmPhysics::DeleteWorld((dmPhysics::HWorld)world);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CreateCollisionObject(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data)
    {
        assert(user_data);

        CollisionObjectPrototype* collision_object_prototype = (CollisionObjectPrototype*) resource;
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;

        dmPhysics::HCollisionObject collision_object = dmPhysics::NewCollisionObject(physics_world, collision_object_prototype->m_CollisionShape, collision_object_prototype->m_Mass, collision_object_prototype->m_Type, collision_object_prototype->m_Group, collision_object_prototype->m_Mask, instance);
        *user_data = (uintptr_t) collision_object;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult InitCollisionObject(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* context,
                                            uintptr_t* user_data)
    {
        assert(user_data);
        dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject)*user_data;

        Point3 position = dmGameObject::GetWorldPosition(instance);
        Quat rotation = dmGameObject::GetWorldRotation(instance);

        dmPhysics::SetCollisionObjectInitialTransform(collision_object, position, rotation);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult DestroyCollisionObject(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data)
    {
        assert(user_data);
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;

        dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject) *user_data;
        dmPhysics::DeleteCollisionObject(physics_world, collision_object);
        return dmGameObject::CREATE_RESULT_OK;
    }

    void CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
    {
        dmGameObject::HInstance instance_a = (dmGameObject::HInstance)user_data_a;
        dmGameObject::HInstance instance_b = (dmGameObject::HInstance)user_data_b;
        const uint32_t data_size = sizeof(dmPhysicsDDF::CollisionMessage) + 9;
        char data[data_size];
        dmPhysicsDDF::CollisionMessage* ddf = (dmPhysicsDDF::CollisionMessage*)&data;
        ddf->m_OtherGameObjectId = (char*)sizeof(dmPhysicsDDF::CollisionMessage);
        char* id = &data[sizeof(dmPhysicsDDF::CollisionMessage)];
        // Broadcast to A components
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_b));
        ddf->m_Group = group_b;
        dmGameObject::PostNamedEvent(instance_a, 0x0, dmPhysicsDDF::CollisionMessage::m_DDFDescriptor->m_Name, dmPhysicsDDF::CollisionMessage::m_DDFDescriptor, data);
        // Broadcast to B components
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_a));
        ddf->m_Group = group_a;
        dmGameObject::PostNamedEvent(instance_b, 0x0, dmPhysicsDDF::CollisionMessage::m_DDFDescriptor->m_Name, dmPhysicsDDF::CollisionMessage::m_DDFDescriptor, data);
    }

    void ContactPointCallback(const dmPhysics::ContactPoint& contact_point, void* user_data)
    {
        dmGameObject::HInstance instance_a = (dmGameObject::HInstance)contact_point.m_UserDataA;
        dmGameObject::HInstance instance_b = (dmGameObject::HInstance)contact_point.m_UserDataB;
        char data[sizeof(dmPhysicsDDF::ContactPointMessage) + 9];
        dmPhysicsDDF::ContactPointMessage* ddf = (dmPhysicsDDF::ContactPointMessage*)&data;
        ddf->m_OtherGameObjectId = (char*)sizeof(dmPhysicsDDF::ContactPointMessage);
        char* id = &data[sizeof(dmPhysicsDDF::ContactPointMessage)];
        // Broadcast to A components
        ddf->m_Position = contact_point.m_PositionA;
        ddf->m_Normal = -contact_point.m_Normal;
        ddf->m_Distance = contact_point.m_Distance;
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_b));
        ddf->m_Group = contact_point.m_GroupB;
        dmGameObject::PostNamedEvent(instance_a, 0x0, dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor->m_Name, dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor, data);
        // Broadcast to B components
        ddf->m_Position = contact_point.m_PositionB;
        ddf->m_Normal = contact_point.m_Normal;
        ddf->m_Distance = contact_point.m_Distance;
        DM_SNPRINTF(id, 9, "%X", dmGameObject::GetIdentifier(instance_a));
        ddf->m_Group = contact_point.m_GroupA;
        dmGameObject::PostNamedEvent(instance_b, 0x0, dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor->m_Name, dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor, data);
    }

    dmGameObject::UpdateResult UpdateCollisionObject(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context)
    {
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;
        dmPhysics::StepWorld(physics_world, update_context->m_DT);
        dmPhysics::ForEachCollision(physics_world, CollisionCallback, 0x0, ContactPointCallback, 0x0);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult OnEventCollisionObject(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            const dmGameObject::ScriptEventData* event_data,
            void* context,
            uintptr_t* user_data)
    {
        if (event_data->m_EventHash == dmHashString32("ApplyForceMessage"))
        {
            dmPhysicsDDF::ApplyForceMessage* af = (dmPhysicsDDF::ApplyForceMessage*) event_data->m_DDFData;
            dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject) *user_data;
            dmPhysics::ApplyForce(collision_object, af->m_Force, af->m_Position);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::Result RegisterPhysicsComponent(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist)
    {
        uint32_t type;

        dmGameObject::RegisterDDFType(dmPhysicsDDF::ApplyForceMessage::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmPhysicsDDF::CollisionMessage::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor);

        dmResource::FactoryResult fact_result = dmResource::GetTypeFromExtension(factory, "collisionobject", &type);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
        {
            dmLogWarning("Unable to get resource type for 'collisionobject' (%d)", fact_result);
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }
        dmGameObject::ComponentType component_type;
        component_type.m_Name = "collisionobject";
        component_type.m_ResourceType = type;
        component_type.m_Context = 0x0;
        component_type.m_NewWorldFunction = &GOCollisionObjectNewWorld;
        component_type.m_DeleteWorldFunction = &GOCollisionObjectDeleteWorld;
        component_type.m_CreateFunction = &CreateCollisionObject;
        component_type.m_InitFunction = &InitCollisionObject;
        component_type.m_DestroyFunction = &DestroyCollisionObject;
        component_type.m_UpdateFunction = &UpdateCollisionObject;
        component_type.m_OnEventFunction = &OnEventCollisionObject;
        component_type.m_InstanceHasUserData = (uint32_t)true;
        dmGameObject::Result res = dmGameObject::RegisterComponentType(regist, component_type);
        return res;
    }

    void SetObjectModel(void* context, void* gameobject, Vectormath::Aos::Quat* rotation,
            Vectormath::Aos::Point3* position)
    {
        if (!gameobject) return;
        dmGameObject::HInstance go = (dmGameObject::HInstance) gameobject;
        *position = dmGameObject::GetWorldPosition(go);
        *rotation = dmGameObject::GetWorldRotation(go);
    }

    dmGameObject::CreateResult GoModelComponentNewWorld(void* context, void** world)
    {
        *world = dmRender::NewRenderWorld(1000, 10, SetObjectModel);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult GoModelComponentDeleteWorld(void* context, void* world)
    {
        dmRender::DeleteRenderWorld((dmRender::HRenderWorld)world);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CreateModelComponent(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* resource,
                                            void* world,
                                            void* context,
                                            uintptr_t* user_data)
    {

        dmModel::HModel prototype = (dmModel::HModel)resource;
        dmRender::HRenderWorld render_world = (dmRender::HRenderWorld)world;


        dmRender::HRenderObject ro = NewRenderObjectInstance(render_world, (void*)prototype, (void*)instance, dmRender::RENDEROBJECT_TYPE_MODEL);
        *user_data = (uintptr_t) ro;

        return dmGameObject::CREATE_RESULT_OK;

    }

    dmGameObject::CreateResult DestroyModelComponent(dmGameObject::HCollection collection,
                                             dmGameObject::HInstance instance,
                                             void* world,
                                             void* context,
                                             uintptr_t* user_data)
    {
        dmRender::HRenderObject ro = (dmRender::HRenderObject)*user_data;
        dmRender::HRenderWorld render_world = (dmRender::HRenderWorld)world;

        dmRender::DeleteRenderObject(render_world, ro);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult UpdateModelComponent(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context)
    {
        dmRender::HRenderWorld render_world = (dmRender::HRenderWorld)world;
        dmRender::Update(render_world, update_context->m_DT);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult OnEventModelComponent(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            const dmGameObject::ScriptEventData* event_data,
            void* context,
            uintptr_t* user_data)
    {
        dmRender::HRenderWorld world = (dmRender::HRenderWorld)context;
        (void)world;

        if (event_data->m_EventHash == dmHashString32("SetRenderColor"))
        {
            dmRender::HRenderObject ro = (dmRender::HRenderObject)*user_data;
            dmRender::SetRenderColor* ddf = (dmRender::SetRenderColor*)event_data->m_DDFData;
            dmRender::ColorType color_type = (dmRender::ColorType)ddf->m_ColorType;
            dmRender::SetColor(ro, ddf->m_Color, color_type);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::Result RegisterModelComponent(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist)
    {
        uint32_t type;

        dmResource::FactoryResult fact_result = dmResource::GetTypeFromExtension(factory, "modelc", &type);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
        {
            dmLogWarning("Unable to get resource type for 'model' (%d)", fact_result);
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }
        dmGameObject::ComponentType component_type;
        component_type.m_Name = "modelc";
        component_type.m_ResourceType = type;
        component_type.m_Context = 0x0;
        component_type.m_NewWorldFunction = dmGameSystem::GoModelComponentNewWorld;
        component_type.m_DeleteWorldFunction = dmGameSystem::GoModelComponentDeleteWorld;
        component_type.m_CreateFunction = dmGameSystem::CreateModelComponent;
        component_type.m_InitFunction = 0x0;
        component_type.m_DestroyFunction = dmGameSystem::DestroyModelComponent;
        component_type.m_UpdateFunction = dmGameSystem::UpdateModelComponent;
        component_type.m_OnEventFunction = dmGameSystem::OnEventModelComponent;
        component_type.m_InstanceHasUserData = true;

        dmGameObject::Result res = dmGameObject::RegisterComponentType(regist, component_type);
        return res;
    }

}
