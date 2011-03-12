#include "comp_collection_spawn_point.h"
#include "resources/res_collection_spawn_point.h"

#include <string.h>

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>

#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    struct CollectionSpawnPointComponent
    {
        CollectionSpawnPointResource*   m_Resource;
        dmGameObject::HCollection       m_Collection;
        uint32_t                        m_Initialized : 1;
        uint32_t                        m_Active : 1;
        uint32_t                        m_Unload : 1;
    };

    struct CollectionSpawnPointWorld
    {
        dmArray<CollectionSpawnPointComponent>  m_Components;
        dmIndexPool32                           m_IndexPool;
    };

    dmGameObject::CreateResult CompCollectionSpawnPointNewWorld(void* context, void** world)
    {
        CollectionSpawnPointWorld* cspw = new CollectionSpawnPointWorld();
        // TODO: tweak count from project-file
        const uint32_t component_count = 8;
        cspw->m_Components.SetCapacity(component_count);
        cspw->m_Components.SetSize(component_count);
        memset(&cspw->m_Components[0], 0, sizeof(CollectionSpawnPointComponent) * component_count);
        cspw->m_IndexPool.SetCapacity(component_count);
        *world = cspw;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionSpawnPointDeleteWorld(void* context, void* world)
    {
        CollectionSpawnPointWorld* cspw = (CollectionSpawnPointWorld*)world;
        for (uint32_t i = 0; i < cspw->m_Components.Size(); ++i)
        {
            dmGameObject::HCollection collection = cspw->m_Components[i].m_Collection;
            if (collection != 0)
            {
                if (cspw->m_Components[i].m_Initialized)
                    dmGameObject::Final(collection);
                dmResource::Release((dmResource::HFactory)context, collection);
            }
        }
        delete cspw;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionSpawnPointCreate(dmGameObject::HCollection collection,
                                               dmGameObject::HInstance instance,
                                               void* resource,
                                               void* world,
                                               void* context,
                                               uintptr_t* user_data)
    {
        CollectionSpawnPointWorld* cspw = (CollectionSpawnPointWorld*)world;
        if (cspw->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = cspw->m_IndexPool.Pop();
            CollectionSpawnPointComponent* cspc = &cspw->m_Components[index];
            memset(cspc, 0, sizeof(CollectionSpawnPointComponent));
            cspc->m_Resource = (CollectionSpawnPointResource*) resource;
            *user_data = (uintptr_t) cspc;
            // TODO: This is done to ensure the focus is acquired before any scripts etc to be the last to receive it.. not pretty.
            dmGameObject::AcquireInputFocus(dmGameObject::GetCollection(instance), instance);
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Collection spawn point could not be created since the buffer is full (%ud).", cspw->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompCollectionSpawnPointDestroy(dmGameObject::HCollection collection,
                                                dmGameObject::HInstance instance,
                                                void* world,
                                                void* context,
                                                uintptr_t* user_data)
    {
        CollectionSpawnPointComponent* cspc = (CollectionSpawnPointComponent*)*user_data;
        if (cspc->m_Collection != 0)
        {
            if (cspc->m_Initialized)
                dmGameObject::Final(cspc->m_Collection);
            dmResource::Release((dmResource::HFactory)context, cspc->m_Collection);
        }
        CollectionSpawnPointWorld* cspw = (CollectionSpawnPointWorld*)world;
        uint32_t index = cspc - &cspw->m_Components[0];
        cspw->m_IndexPool.Push(index);
        memset(cspc, 0, sizeof(CollectionSpawnPointComponent));
        // TODO: See above
        dmGameObject::ReleaseInputFocus(dmGameObject::GetCollection(instance), instance);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollectionSpawnPointUpdate(dmGameObject::HCollection collection,
                                               const dmGameObject::UpdateContext* update_context,
                                               void* world,
                                               void* context)
    {
        CollectionSpawnPointWorld* cspw = (CollectionSpawnPointWorld*)world;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < cspw->m_Components.Size(); ++i)
        {
            CollectionSpawnPointComponent* cspc = &cspw->m_Components[i];
            if (cspc->m_Collection != 0 && cspc->m_Active)
            {
                if (!dmGameObject::Update(&cspc->m_Collection, update_context, 1))
                    result = dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        return result;
    }

    dmGameObject::UpdateResult CompCollectionSpawnPointPostUpdate(dmGameObject::HCollection collection,
                                               void* world,
                                               void* context)
    {
        CollectionSpawnPointWorld* cspw = (CollectionSpawnPointWorld*)world;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < cspw->m_Components.Size(); ++i)
        {
            CollectionSpawnPointComponent* cspc = &cspw->m_Components[i];
            if (cspc->m_Collection != 0)
            {
                if (cspc->m_Active)
                {
                    if (!dmGameObject::PostUpdate(&cspc->m_Collection, 1))
                        result = dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                }
                if (cspc->m_Unload)
                {
                    dmResource::Release((dmResource::HFactory)context, cspc->m_Collection);
                    cspc->m_Collection = 0;
                    cspc->m_Unload = 0;
                }
            }
        }
        return result;
    }

    dmGameObject::UpdateResult CompCollectionSpawnPointOnMessage(dmGameObject::HInstance instance,
                                                const dmGameObject::InstanceMessageData* message_data,
                                                void* context,
                                                uintptr_t* user_data)
    {
        CollectionSpawnPointComponent* cspc = (CollectionSpawnPointComponent*) *user_data;
        if (message_data->m_MessageId == dmHashString64("load"))
        {
            if (cspc->m_Collection == 0)
            {
                cspc->m_Unload = 0;
                // TODO: asynchronous loading
                dmResource::FactoryResult result = dmResource::Get((dmResource::HFactory)context, cspc->m_Resource->m_DDF->m_Collection, (void**)&cspc->m_Collection);
                if (result != dmResource::FACTORY_RESULT_OK)
                {
                    dmLogError("The collection %s could not be loaded.", cspc->m_Resource->m_DDF->m_Collection);
                    return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
                }
            }
            else
            {
                dmLogWarning("The collection %s could not loaded since it was already.", cspc->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("unload"))
        {
            if (cspc->m_Collection != 0)
            {
                cspc->m_Unload = 1;
            }
            else
            {
                dmLogWarning("The collection %s could not be unloaded since it was never loaded.", cspc->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("init"))
        {
            if (cspc->m_Initialized == 0)
            {
                dmGameObject::Init(cspc->m_Collection);
                cspc->m_Initialized = 1;
            }
            else
            {
                dmLogWarning("The collection %s could not be initialized since it has been already.", cspc->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("final"))
        {
            if (cspc->m_Initialized == 1)
            {
                dmGameObject::Final(cspc->m_Collection);
                cspc->m_Initialized = 0;
            }
            else
            {
                dmLogWarning("The collection %s could not be finalized since it was never initialized.", cspc->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("activate"))
        {
            if (cspc->m_Active == 0)
            {
                cspc->m_Active = 1;
            }
            else
            {
                dmLogWarning("The collection %s could not be activated since it is already active.", cspc->m_Resource->m_DDF->m_Collection);
            }
        }
        else if (message_data->m_MessageId == dmHashString64("deactivate"))
        {
            if (cspc->m_Active == 1)
            {
                cspc->m_Active = 0;
            }
            else
            {
                dmLogWarning("The collection %s could not be deactivated since it is not active.", cspc->m_Resource->m_DDF->m_Collection);
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::InputResult CompCollectionSpawnPointOnInput(dmGameObject::HInstance instance,
            const dmGameObject::InputAction* input_action,
            void* context,
            uintptr_t* user_data)
    {
        CollectionSpawnPointComponent* cspc = (CollectionSpawnPointComponent*) *user_data;
        if (cspc->m_Active && !cspc->m_Unload)
            dmGameObject::DispatchInput(&cspc->m_Collection, 1, (dmGameObject::InputAction*)input_action, 1);
        return dmGameObject::INPUT_RESULT_IGNORED;
    }
}
