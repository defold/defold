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

    dmGameObject::CreateResult CompCollisionObjectNewWorld(void* context, void** world)
    {
        *world = dmPhysics::NewWorld(Vectormath::Aos::Point3(-1000, -1000, -1000), Vectormath::Aos::Point3(1000, 1000, 1000), &GetWorldTransform, &SetWorldTransform);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectDeleteWorld(void* context, void* world)
    {
        dmPhysics::DeleteWorld((dmPhysics::HWorld)world);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollisionObjectCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data)
    {
        assert(user_data);

        CollisionObjectResource* co_resource = (CollisionObjectResource*) resource;
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;
        dmPhysics::CollisionObjectData data;
        data.m_Shape = co_resource->m_ConvexShape->m_Shape;
        data.m_UserData = instance;
        data.m_Type = (dmPhysics::CollisionObjectType)co_resource->m_DDF->m_Type;
        data.m_Mass = co_resource->m_DDF->m_Mass;
        data.m_Friction = co_resource->m_DDF->m_Friction;
        data.m_Restitution = co_resource->m_DDF->m_Restitution;
        data.m_Group = co_resource->m_DDF->m_Group;
        data.m_Mask = co_resource->m_Mask;
        dmPhysics::HCollisionObject collision_object = dmPhysics::NewCollisionObject(physics_world, data);
        if (collision_object != 0x0)
        {
            *user_data = (uintptr_t) collision_object;
            return dmGameObject::CREATE_RESULT_OK;
        }
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompCollisionObjectInit(dmGameObject::HCollection collection,
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

    dmGameObject::CreateResult CompCollisionObjectDestroy(dmGameObject::HCollection collection,
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

    bool CollisionCallback(void* user_data_a, uint16_t group_a, void* user_data_b, uint16_t group_b, void* user_data)
    {
        uint32_t* count = (uint32_t*)user_data;
        if (*count < MAX_COLLISION_COUNT)
        {
            *count += 1;

            dmGameObject::HInstance instance_a = (dmGameObject::HInstance)user_data_a;
            dmGameObject::HInstance instance_b = (dmGameObject::HInstance)user_data_b;
            dmPhysicsDDF::CollisionMessage ddf;
            ddf.m_Group = group_b;
            ddf.m_OtherGameObjectId = dmGameObject::GetIdentifier(instance_b);
            dmGameObject::PostDDFMessageTo(instance_a, 0x0, dmPhysicsDDF::CollisionMessage::m_DDFDescriptor, &ddf);

            // Broadcast to B components
            ddf.m_Group = group_a;
            ddf.m_OtherGameObjectId = dmGameObject::GetIdentifier(instance_a);
            dmGameObject::PostDDFMessageTo(instance_b, 0x0, dmPhysicsDDF::CollisionMessage::m_DDFDescriptor, &ddf);

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

            dmPhysicsDDF::ContactPointMessage ddf;
            float mass_a = dmMath::Select(-contact_point.m_InvMassA, 0.0f, 1.0f / contact_point.m_InvMassA);
            float mass_b = dmMath::Select(-contact_point.m_InvMassB, 0.0f, 1.0f / contact_point.m_InvMassB);

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
            dmGameObject::PostDDFMessageTo(instance_a, 0x0, dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor, &ddf);

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
            ddf.m_LifeTime = contact_point.m_LifeTime;
            dmGameObject::PostDDFMessageTo(instance_b, 0x0, dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor, &ddf);

            return true;
        }
        else
        {
            return false;
        }
    }

    static bool g_CollisionOverflowWarning = false;
    static bool g_ContactOverflowWarning = false;

    dmGameObject::UpdateResult CompCollisionObjectUpdate(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context)
    {
        if (world == 0x0)
            return dmGameObject::UPDATE_RESULT_OK;
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;
        dmPhysics::StepWorld(physics_world, update_context->m_DT);
        uint32_t collision_count = 0;
        uint32_t contact_count = 0;
        dmPhysics::ForEachCollision(physics_world, CollisionCallback, &collision_count, ContactPointCallback, &contact_count);
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
        PhysicsContext* physics_context = (PhysicsContext*)context;
        if (physics_context->m_Debug)
            dmPhysics::DebugRender(physics_world);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollisionObjectOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        if (message_data->m_MessageId == dmHashString64(dmPhysicsDDF::ApplyForceMessage::m_DDFDescriptor->m_ScriptName))
        {
            dmPhysicsDDF::ApplyForceMessage* af = (dmPhysicsDDF::ApplyForceMessage*) message_data->m_Buffer;
            dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject) *user_data;
            dmPhysics::ApplyForce(collision_object, af->m_Force, af->m_Position);
        }
        if (message_data->m_MessageId == dmHashString64(dmPhysicsDDF::VelocityRequest::m_DDFDescriptor->m_ScriptName))
        {
            dmPhysicsDDF::VelocityRequest* request = (dmPhysicsDDF::VelocityRequest*)message_data->m_Buffer;
            if (request->m_ClientId)
            {
                dmGameObject::HInstance client = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(instance), request->m_ClientId);
                dmPhysicsDDF::VelocityResponse response;
                dmPhysics::HCollisionObject collision_object = (dmPhysics::HCollisionObject)*user_data;
                response.m_LinearVelocity = dmPhysics::GetLinearVelocity(collision_object);
                response.m_AngularVelocity = dmPhysics::GetAngularVelocity(collision_object);
                dmGameObject::PostDDFMessageTo(client, 0x0, dmPhysicsDDF::VelocityResponse::m_DDFDescriptor, (char*)&response);
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompCollisionObjectOnReload(dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        CollisionObjectResource* co_resource = (CollisionObjectResource*) resource;
        dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;
        dmPhysics::CollisionObjectData data;
        data.m_Shape = co_resource->m_ConvexShape->m_Shape;
        data.m_UserData = instance;
        data.m_Type = (dmPhysics::CollisionObjectType)co_resource->m_DDF->m_Type;
        data.m_Mass = co_resource->m_DDF->m_Mass;
        data.m_Friction = co_resource->m_DDF->m_Friction;
        data.m_Restitution = co_resource->m_DDF->m_Restitution;
        data.m_Group = co_resource->m_DDF->m_Group;
        data.m_Mask = co_resource->m_Mask;
        dmPhysics::HCollisionObject collision_object = dmPhysics::NewCollisionObject(physics_world, data);
        if (collision_object != 0x0)
        {
            *user_data = (uintptr_t) collision_object;
        }
    }
}
