#include "comp_collection_factory.h"
#include "resources/res_collection_factory.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <gameobject/gameobject.h>
#include <gameobject/gameobject_ddf.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    const char* COLLECTION_FACTORY_MAX_COUNT_KEY = "collectionfactory.max_count";

    static void CleanupAsyncLoading(lua_State*, CollectionFactoryComponent*);
    static bool PreloadCompleteCallback(const dmResource::PreloaderCompleteCallbackParams*);
    static void LoadComplete(const dmGameObject::ComponentsUpdateParams&, CollectionFactoryComponent*, const dmResource::Result);
    static dmResource::Result LoadCollectionResources(dmResource::HFactory, CollectionFactoryComponent*);
    static void UnloadCollectionResources(dmResource::HFactory, CollectionFactoryComponent*);

    struct FactoryWorld
    {
        dmArray<CollectionFactoryComponent>   m_Components;
        dmIndexPool32               m_IndexPool;
        uint32_t                    m_TotalFactoryCount;
    };

    void CollectionFactoryComponent::Init()
    {
        memset(this, 0x0, sizeof(CollectionFactoryComponent));
        this->m_PreloaderCallbackRef = LUA_NOREF;
        this->m_PreloaderSelfRef = LUA_NOREF;
        this->m_PreloaderURLRef = LUA_NOREF;
    }

    dmGameObject::CreateResult CompCollectionFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        CollectionFactoryContext* context = (CollectionFactoryContext*)params.m_Context;
        FactoryWorld* fw = new FactoryWorld();
        const uint32_t max_component_count = context->m_MaxCollectionFactoryCount;
        fw->m_Components.SetCapacity(max_component_count);
        fw->m_Components.SetSize(max_component_count);
        fw->m_IndexPool.SetCapacity(max_component_count);
        for(uint32_t i = 0; i < max_component_count; ++i)
        {
            fw->m_Components[i].Init();
        }
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
        CollectionFactoryComponent* component;
        if (fw->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = fw->m_IndexPool.Pop();
            component = &fw->m_Components[index];
            component->m_Resource = (CollectionFactoryResource*) params.m_Resource;
            *params.m_UserData = (uintptr_t) component;
        }
        else
        {
            dmLogError("Can not create more collection factory components since the buffer is full (%d).", fw->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        FactoryWorld* fw = (FactoryWorld*)params.m_World;
        CollectionFactoryComponent* fc = (CollectionFactoryComponent*)*params.m_UserData;
        CleanupAsyncLoading(dmScript::GetLuaState(((CollectionFactoryContext*)params.m_Context)->m_ScriptContext), fc);
        uint32_t index = fc - &fw->m_Components[0];
        fc->m_Resource = 0x0;
        fc->m_AddedToUpdate = 0;
        fw->m_IndexPool.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        CollectionFactoryComponent* component = (CollectionFactoryComponent*)*params.m_UserData;
        component->m_AddedToUpdate = 1;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompCollectionFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        FactoryWorld* world = (FactoryWorld*)params.m_World;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            CollectionFactoryComponent* component = &world->m_Components[i];
            if (!component->m_AddedToUpdate)
                continue;
            if (component->m_Loading)
            {
                dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Collection);
                dmResource::Result r = dmResource::RESULT_OK;
                if(component->m_Preloader)
                {
                    dmResource::PreloaderCompleteCallbackParams preloader_params;
                    preloader_params.m_Factory = factory;
                    preloader_params.m_UserData = component;
                    r = dmResource::UpdatePreloader(component->m_Preloader, PreloadCompleteCallback, &preloader_params, 10*1000);
                }
                if (r != dmResource::RESULT_PENDING)
                {
                    LoadComplete(params, component, r);
                }
            }
        }

        return result;
    }

    bool CompCollectionFactoryLoad(dmGameObject::HCollection collection, CollectionFactoryComponent* component)
    {
        if(!component->m_Resource->m_LoadDynamically)
        {
            // set as loading without preloader so complete callback is invoked as should be by design.
            component->m_Loading = 1;
            return true;
        }
        if(component->m_Loading)
        {
            dmLogError("Trying to load factory prototype resources when already loading.");
            return false;
        }
        if(!component->m_Resource->m_CollectionResources.Empty())
        {
            // If loaded, complete callback is invoked.
            component->m_Loading = 1;
            return true;
        }

        dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) component->m_Resource->m_CollectionDesc;
        dmArray<const char*> names;
        names.SetCapacity(collection_desc->m_Instances.m_Count);
        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            if (instance_desc.m_Prototype == 0x0)
                continue;
            names.Push(instance_desc.m_Prototype);
        }
        component->m_Preloader = dmResource::NewPreloader(dmGameObject::GetFactory(collection), names);
        if(!component->m_Preloader)
        {
            return false;
        }
        component->m_Loading = 1;
        return true;
    }

    bool CompCollectionFactoryUnload(dmGameObject::HCollection collection, CollectionFactoryComponent* component)
    {
        if(!component->m_Resource->m_LoadDynamically)
        {
            return true;
        }
        if(component->m_Loading)
        {
            dmLogError("Trying to unload factory prototype resources while loading.");
            return false;
        }
        UnloadCollectionResources(dmGameObject::GetFactory(collection), component);
        return true;
    }

    CompCollectionFactoryStatus CompCollectionFactoryGetStatus(CollectionFactoryComponent* component)
    {
        if(component->m_Loading)
        {
            return COMP_COLLECTION_FACTORY_STATUS_LOADING;
        }
        if(component->m_Resource->m_CollectionResources.Empty())
        {
            return COMP_COLLECTION_FACTORY_STATUS_UNLOADED;
        }
        return COMP_COLLECTION_FACTORY_STATUS_LOADED;
    }

    static void UnloadCollectionResources(dmResource::HFactory factory, CollectionFactoryComponent* component)
    {
        dmArray<void*>& r = component->m_Resource->m_CollectionResources;
        for(uint32_t i = 0; i < r.Size(); ++i)
        {
            dmResource::Release( factory, r[i]);
        }
        r.SetSize(0);
    }

    static dmResource::Result LoadCollectionResources(dmResource::HFactory factory, CollectionFactoryComponent* component)
    {
        UnloadCollectionResources(factory, component);
        dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) component->m_Resource->m_CollectionDesc;
        dmArray<void*>& collection_resources = component->m_Resource->m_CollectionResources;

        collection_resources.SetCapacity(collection_desc->m_Instances.m_Count);
        dmResource::Result result = dmResource::RESULT_OK;
        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            if (instance_desc.m_Prototype == 0x0)
                continue;
            void* resource;
            result = dmResource::Get(factory, instance_desc.m_Prototype, &resource);
            if(result != dmResource::RESULT_OK)
            {
                UnloadCollectionResources(factory, component);
                break;
            }
            collection_resources.Push(resource);
        }
        return result;
    }

    static void CleanupAsyncLoading(lua_State* L, CollectionFactoryComponent* component)
    {
        component->m_Loading = 0;
        if (component->m_PreloaderCallbackRef != LUA_NOREF)
        {
            dmScript::Unref(L, LUA_REGISTRYINDEX, component->m_PreloaderCallbackRef);
            dmScript::Unref(L, LUA_REGISTRYINDEX, component->m_PreloaderSelfRef);
            dmScript::Unref(L, LUA_REGISTRYINDEX, component->m_PreloaderURLRef);
            component->m_PreloaderCallbackRef = LUA_NOREF;
            component->m_PreloaderSelfRef = LUA_NOREF;
            component->m_PreloaderURLRef = LUA_NOREF;
        }
        if(component->m_Preloader)
        {
            dmResource::DeletePreloader(component->m_Preloader);
            component->m_Preloader = 0x0;
        }
    }

    static bool PreloadCompleteCallback(const dmResource::PreloaderCompleteCallbackParams* params)
    {
        CollectionFactoryComponent* component = (CollectionFactoryComponent *) params->m_UserData;
        if(component->m_Resource->m_LoadDynamically)
        {
            if(LoadCollectionResources(params->m_Factory, component) != dmResource::RESULT_OK)
                return false;
        }
        return true;
    }

    static void LoadComplete(const dmGameObject::ComponentsUpdateParams& params, CollectionFactoryComponent* component, const dmResource::Result result)
    {
        component->m_Loading = 0;
        lua_State* L = dmScript::GetLuaState(((CollectionFactoryContext*)params.m_Context)->m_ScriptContext);
        int top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, component->m_PreloaderCallbackRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, component->m_PreloaderSelfRef);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);
        if (!dmScript::IsInstanceValid(L))
        {
            lua_pop(L, 2);
            dmLogError("Could not run collectionfactory.load complete callback because the instance has been deleted.");
            CleanupAsyncLoading(L, component);
            assert(top == lua_gettop(L));
            return;
        }
        if (component->m_PreloaderCallbackRef == LUA_NOREF)
        {
            lua_pop(L, 2);
            dmLogError("No callback set");
            CleanupAsyncLoading(L, component);
            assert(top == lua_gettop(L));
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, component->m_PreloaderURLRef);
        lua_pushboolean(L, result == dmResource::RESULT_OK ? 1 : 0);
        dmScript::PCall(L, 3, 0);
        CleanupAsyncLoading(L, component);
        assert(top == lua_gettop(L));
    }

}

