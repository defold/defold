#include "comp_spawn_point.h"
#include "resources/res_spawn_point.h"

#include <dlib/log.h>
#include <dlib/hash.h>
#include <gameobject/gameobject.h>
#include <vectormath/cpp/vectormath_aos.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmGameObject::CreateResult CompSpawnPointNewWorld(void* context, void** world)
    {
        *world = 0;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpawnPointDeleteWorld(void* context, void* world)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpawnPointCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data)
    {
        SpawnPoint* spawn_point = (SpawnPoint*) resource;
        *user_data = (uintptr_t) spawn_point;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpawnPointDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpawnPointUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpawnPointOnMessage(dmGameObject::HInstance instance,
                                                const dmGameObject::InstanceMessageData* message_data,
                                                void* context,
                                                uintptr_t* user_data)
    {
        if (message_data->m_MessageId == dmHashString64("spawn_object"))
        {
            SpawnPoint* spawn_point = (SpawnPoint*) *user_data;
            if (message_data->m_DDFDescriptor == dmGameSystemDDF::SpawnObject::m_DDFDescriptor)
            {
                dmGameSystemDDF::SpawnObject* spawn_object = (dmGameSystemDDF::SpawnObject*) message_data->m_Buffer;
                dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
                dmGameObject::Spawn(collection, spawn_point->m_SpawnPointDesc->m_Prototype, spawn_object->m_Position, spawn_object->m_Rotation);
            }
            else
            {
                dmLogError("Invalid DDF-type for spawn_object message");
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
