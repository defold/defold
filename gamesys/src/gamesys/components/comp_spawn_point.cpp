#include "comp_spawn_point.h"
#include "resources/res_spawn_point.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <gameobject/gameobject.h>
#include <vectormath/cpp/vectormath_aos.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    struct SpawnPointComponent
    {
        SpawnPointResource*     m_Resource;
    };

    struct SpawnPointWorld
    {
        dmArray<SpawnPointComponent>    m_Components;
        dmIndexPool32                   m_IndexPool;
        uint32_t                        m_TotalSpawnCount;
    };

    dmGameObject::CreateResult CompSpawnPointNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpawnPointWorld* spw = new SpawnPointWorld();
        const uint32_t max_component_count = 64;
        spw->m_Components.SetCapacity(max_component_count); // TODO: Tweakable!
        spw->m_Components.SetSize(max_component_count);
        spw->m_IndexPool.SetCapacity(max_component_count);
        memset(&spw->m_Components[0], 0, sizeof(SpawnPointComponent) * max_component_count);
        *params.m_World = spw;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpawnPointDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        delete (SpawnPointWorld*)params.m_World;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpawnPointCreate(const dmGameObject::ComponentCreateParams& params)
    {
        SpawnPointWorld* spw = (SpawnPointWorld*)params.m_World;
        if (spw->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = spw->m_IndexPool.Pop();
            SpawnPointComponent* spc = &spw->m_Components[index];
            spc->m_Resource = (SpawnPointResource*) params.m_Resource;
            *params.m_UserData = (uintptr_t) spc;

            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Can not create more spawn point components since the buffer is full (%d).", spw->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompSpawnPointDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpawnPointWorld* spw = (SpawnPointWorld*)params.m_World;
        SpawnPointComponent* spc = (SpawnPointComponent*)*params.m_UserData;
        uint32_t index = spc - &spw->m_Components[0];
        spc->m_Resource = 0x0;
        spw->m_IndexPool.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpawnPointOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::Spawn::m_DDFDescriptor)
        {
            dmGameObject::HInstance instance = params.m_Instance;
            dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
            dmMessage::Message* message = params.m_Message;
            if ((dmDDF::Descriptor*)params.m_Message->m_Descriptor == dmGameSystemDDF::Spawn::m_DDFDescriptor)
            {
                // redispatch messages on the component socket to the frame socket
                dmMessage::URL receiver = message->m_Receiver;
                if (receiver.m_Socket == dmGameObject::GetMessageSocket(collection))
                {
                    receiver.m_Socket = dmGameObject::GetFrameMessageSocket(collection);
                    dmMessage::Post(&message->m_Sender, &receiver, message->m_Id, message->m_UserData, message->m_Descriptor, (const void*)message->m_Data, message->m_DataSize);
                }
                else
                {
                    dmGameSystemDDF::Spawn* spawn_object = (dmGameSystemDDF::Spawn*) params.m_Message->m_Data;
                    uint32_t msg_size = sizeof(dmGameSystemDDF::Spawn);
                    uint32_t table_buffer_size = message->m_DataSize - msg_size;
                    uint8_t* table_buffer = 0x0;
                    if (table_buffer_size > 0)
                    {
                        table_buffer = (uint8_t*)(((uintptr_t)spawn_object) + msg_size);
                    }
                    SpawnPointComponent* spc = (SpawnPointComponent*) *params.m_UserData;
                    dmhash_t id = spawn_object->m_Id;
                    if (id == 0)
                    {
                        id = dmGameObject::GenerateUniqueInstanceId(collection);
                    }
                    dmGameObject::Spawn(collection, spc->m_Resource->m_SpawnPointDesc->m_Prototype, id, table_buffer, spawn_object->m_Position, spawn_object->m_Rotation);
                }
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
