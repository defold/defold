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

    const uint32_t MAX_SPAWN_REQUEST_COUNT = 16;

    struct SpawnPointComponent
    {
        Vectormath::Aos::Point3 m_RequestedPositions[MAX_SPAWN_REQUEST_COUNT];
        Vectormath::Aos::Quat   m_RequestedRotations[MAX_SPAWN_REQUEST_COUNT];
        SpawnPointResource*     m_Resource;
        uint32_t                m_SpawnRequests;

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
            spc->m_SpawnRequests = 0;
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

    dmGameObject::UpdateResult CompSpawnPointPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params)
    {
        SpawnPointWorld* spw = (SpawnPointWorld*)params.m_World;
        const char* id_base = "spawn";
        char id[64];
        for (uint32_t i = 0; i < spw->m_Components.Size(); ++i)
        {
            SpawnPointComponent* spc = &spw->m_Components[i];
            if (spc->m_Resource != 0x0 && spc->m_SpawnRequests > 0)
            {
                for (uint32_t i = 0; i < spc->m_SpawnRequests; ++i)
                {
                    DM_SNPRINTF(id, 64, "%s%d", id_base, spw->m_TotalSpawnCount);
                    dmGameObject::Spawn(params.m_Collection, spc->m_Resource->m_SpawnPointDesc->m_Prototype, id, spc->m_RequestedPositions[i], spc->m_RequestedRotations[i]);
                    ++spw->m_TotalSpawnCount;
                }
                spc->m_SpawnRequests = 0;
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpawnPointOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::Spawn::m_DDFDescriptor)
        {
            SpawnPointComponent* spc = (SpawnPointComponent*) *params.m_UserData;
            if ((dmDDF::Descriptor*)params.m_Message->m_Descriptor == dmGameSystemDDF::Spawn::m_DDFDescriptor)
            {
                if (spc->m_SpawnRequests < MAX_SPAWN_REQUEST_COUNT)
                {
                    uint32_t index = spc->m_SpawnRequests;
                    dmGameSystemDDF::Spawn* spawn_object = (dmGameSystemDDF::Spawn*) params.m_Message->m_Data;
                    spc->m_RequestedPositions[index] = spawn_object->m_Position;
                    spc->m_RequestedRotations[index] = spawn_object->m_Rotation;
                    ++spc->m_SpawnRequests;
                }
                else
                {
                    dmLogError("The maximum number of spawn requests have been received (%d), spawn request ignored.", MAX_SPAWN_REQUEST_COUNT);
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
