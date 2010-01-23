#include "gameobject_registration.h"

#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <ddf/ddf.h>
#include "gameobject.h"
#include <physics/physics.h>
#include "resource_creation.h"

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

        Point3 position = dmGameObject::GetPosition(instance);
        Quat rotation = Quat::identity();
        rotation = Quat::rotationZ(0.4f); // TODO: <--- HAXXOR JUST FOR FUN...

        dmPhysics::HRigidBody rigid_body = dmPhysics::NewRigidBody(world, rigid_body_prototype->m_CollisionShape, instance, rotation, position, rigid_body_prototype->m_Mass);
        *user_data = (uintptr_t) rigid_body;
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

    dmGameObject::Result RegisterPhysicsComponent(dmResource::HFactory factory,
                                                  dmGameObject::HCollection collection,
                                                  dmPhysics::HWorld physics_world)
    {
        uint32_t type;

        dmResource::FactoryResult fact_result = dmResource::GetTypeFromExtension(factory, "rigidbody", &type);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
        {
            dmLogWarning("Unable to get resource type for 'rigidbody' (%d)", fact_result);
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }
        dmGameObject::Result res = dmGameObject::RegisterComponentType(collection, type, physics_world, &CreateRigidBody, &DestroyRigidBody, &UpdateRigidBody, 0, true);
        return res;
    }
}
