#include "comp_collection_factory.h"
#include "resources/res_collection_factory.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <gameobject/gameobject.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    const char* COLLECTION_FACTORY_MAX_COUNT_KEY = "collectionfactory.max_count";

    struct FactoryWorld
    {
        dmArray<CollectionFactoryComponent>   m_Components;
        dmIndexPool32               m_IndexPool;
        uint32_t                    m_TotalFactoryCount;
    };

    dmGameObject::CreateResult CompCollectionFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        FactoryContext* context = (FactoryContext*)params.m_Context;
        FactoryWorld* fw = new FactoryWorld();
        const uint32_t max_component_count = context->m_MaxFactoryCount;
        fw->m_Components.SetCapacity(max_component_count);
        fw->m_Components.SetSize(max_component_count);
        fw->m_IndexPool.SetCapacity(max_component_count);
        memset(&fw->m_Components[0], 0, sizeof(CollectionFactoryComponent) * max_component_count);
        *params.m_World = fw;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        delete (FactoryWorld*)params.m_World;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryCreate(const dmGameObject::ComponentCreateParams& params)
    {
        FactoryWorld* fw = (FactoryWorld*)params.m_World;
        if (fw->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = fw->m_IndexPool.Pop();
            CollectionFactoryComponent* fc = &fw->m_Components[index];
            fc->m_Resource = (CollectionFactoryResource*) params.m_Resource;
            *params.m_UserData = (uintptr_t) fc;
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Can not create more collection factory components since the buffer is full (%d).", fw->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    static void CleanupAsyncLoading(lua_State* L, CollectionFactoryComponent* component)
    {
        if (component->m_Callback)
        {
            dmScript::Unref(L, LUA_REGISTRYINDEX, component->m_Callback);
        }
        if(component->m_Preloader)
        {
            dmResource::DeletePreloader(component->m_Preloader);
        }
    }

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        FactoryWorld* fw = (FactoryWorld*)params.m_World;
        CollectionFactoryComponent* fc = (CollectionFactoryComponent*)*params.m_UserData;
        uint32_t index = fc - &fw->m_Components[0];
        fc->m_Resource = 0x0;
        fw->m_IndexPool.Push(index);
        CleanupAsyncLoading(dmScript::GetLuaState(((CollectionFactoryContext*)params.m_Context)->m_ScriptContext), fc);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        CollectionFactoryComponent* component = (CollectionFactoryComponent*)*params.m_UserData;
        component->m_AddedToUpdate = 1;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult DoLoad(dmResource::HFactory factory, CollectionFactoryComponent* component)
    {
        dmResource::Result result = dmResource::Get(factory, proxy->m_Resource->m_DDF->m_Collection, (void**)&proxy->m_Collection);
        if (result != dmResource::RESULT_OK)
        {
            dmLogError("The collection %s could not be loaded.", proxy->m_Resource->m_DDF->m_Collection);
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void LoadComplete(CollectionFactoryComponent* proxy)
    {
        if (dmMessage::IsSocketValid(proxy->m_LoadSender.m_Socket))
        {
            dmMessage::Result msg_result = dmMessage::Post(&proxy->m_LoadReceiver, &proxy->m_LoadSender, dmHashString64("proxy_loaded"), 0, 0, 0, 0, 0);
            if (msg_result != dmMessage::RESULT_OK)
            {
                dmLogWarning("proxy_loaded could not be posted: %d", msg_result);
            }
        }
    }


    static bool PreloadCompleteCallback(const dmResource::PreloaderCompleteCallbackParams* params)
    {
        return (DoLoad(params->m_Factory, (CollectionProxyComponent *) params->m_UserData) == dmGameObject::UPDATE_RESULT_OK);
    }

    dmGameObject::UpdateResult CompCollectionFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        FactoryWorld* world = (FactoryWorld*)params.m_World;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            CollectionFactoryComponent& component = world->m_Components[i];
            if (!component.m_AddedToUpdate)
                continue;
            if (component.m_Preloader)
            {
                CollectionFactoryContext* context = (CollectionFactoryContext*)params.m_Context;
                dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Collection);
                dmResource::PreloaderCompleteCallbackParams params;
                params.m_Factory = factory;
                params.m_UserData = proxy;
                dmResource::Result r = dmResource::UpdatePreloader(component.m_Preloader, PreloadCompleteCallback, &params, 10*1000);
                if (r != dmResource::RESULT_PENDING)
                {
                    dmResource::DeletePreloader(proxy->m_Preloader);
                    if(r == dmResource::RESULT_OK)
                    {
                        LoadComplete(proxy);
                    }
                    proxy->m_Preloader = 0;
                }
            }
        }
    }

    bool Create(dmGameObject::HCollection collection, CollectionFactoryComponent* component, dmGameObject::InstancePropertyBuffers *property_buffers,
            const Point3& position, const Quat& rotation, const Vector3& scale,
            dmGameObject::InstanceIdMap *instances)
    {
        dmGameObject::InstanceIdMap instances;
        if (component->m_Resource->m_CollectionFactoryDesc->m_LoadDynamically)
        {
            return false;
        }
        else
        {
            bool success = dmGameObject::SpawnFromCollection(collection, component->m_Resource->m_CollectionDesc, property_buffers,
                                                             position, rotation, scale, &instances);
            return success;
        }
    }
}
