// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "comp_collection_factory.h"
#include "resources/res_collection_factory.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <gameobject/gameobject.h>
#include <gameobject/gameobject_ddf.h>
#include <dmsdk/dlib/vmath.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_CollectionFactory, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);

namespace dmGameSystem
{
    const char* COLLECTION_FACTORY_MAX_COUNT_KEY = "collectionfactory.max_count";

    static const dmhash_t COLLECTION_FACTORY_PROP_PROTOTYPE = dmHashString64("prototype");

    static void CleanupAsyncLoading(lua_State*, CollectionFactoryComponent*);
    static bool PreloadCompleteCallback(const dmResource::PreloaderCompleteCallbackParams*);
    static void LoadComplete(const dmGameObject::ComponentsUpdateParams&, CollectionFactoryComponent*, const dmResource::Result);
    static dmResource::Result LoadCollectionResources(dmResource::HFactory, CollectionFactoryComponent*);
    static void UnloadCollectionResources(dmResource::HFactory, CollectionFactoryComponent*);
    static CollectionFactoryResource* GetResource(CollectionFactoryComponent* component);

    struct CollectionFactoryComponent
    {
        void Init();

        CollectionFactoryResource*  m_Resource;         // set from the Editor
        CollectionFactoryResource*  m_CustomResource;   // set from script as an override
        dmResource::HPreloader      m_Preloader;
        int                         m_PreloaderCallbackRef;
        int                         m_PreloaderSelfRef;
        int                         m_PreloaderURLRef;
        uint8_t                     m_Loading : 1;
        uint8_t                     m_AddedToUpdate : 1;
    };

    struct CollectionFactoryWorld
    {
        dmArray<CollectionFactoryComponent> m_Components;
        dmIndexPool32                       m_IndexPool;
        dmResource::HFactory                m_Factory;
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
        CollectionFactoryWorld* world = new CollectionFactoryWorld();
        uint32_t max_component_count = dmMath::Min(params.m_MaxComponentInstances, context->m_MaxCollectionFactoryCount);
        world->m_Components.SetCapacity(max_component_count);
        world->m_Components.SetSize(max_component_count);
        world->m_IndexPool.SetCapacity(max_component_count);
        world->m_Factory = context->m_Factory;
        for(uint32_t i = 0; i < max_component_count; ++i)
        {
            world->m_Components[i].Init();
        }
        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        delete (CollectionFactoryWorld*)params.m_World;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryCreate(const dmGameObject::ComponentCreateParams& params)
    {
        CollectionFactoryWorld* fw = (CollectionFactoryWorld*)params.m_World;
        CollectionFactoryComponent* component;
        if (fw->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = fw->m_IndexPool.Pop();
            component = &fw->m_Components[index];
            component->m_Resource = (CollectionFactoryResource*) params.m_Resource;
            component->m_CustomResource = 0;
            *params.m_UserData = (uintptr_t) component;
        }
        else
        {
            ShowFullBufferError("Collection factory", COLLECTION_FACTORY_MAX_COUNT_KEY, fw->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompCollectionFactoryDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        CollectionFactoryWorld* fw = (CollectionFactoryWorld*)params.m_World;
        CollectionFactoryComponent* fc = (CollectionFactoryComponent*)*params.m_UserData;
        CleanupAsyncLoading(dmScript::GetLuaState(((CollectionFactoryContext*)params.m_Context)->m_ScriptContext), fc);
        uint32_t index = fc - &fw->m_Components[0];
        fc->m_Resource = 0x0;
        if (fc->m_CustomResource)
            dmGameSystem::ResCollectionFactoryDestroyResource(fw->m_Factory, fc->m_CustomResource);
        fc->m_CustomResource = 0;
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

    dmGameObject::UpdateResult CompCollectionFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        CollectionFactoryWorld* world = (CollectionFactoryWorld*)params.m_World;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            CollectionFactoryComponent* component = &world->m_Components[i];
            if (!component->m_AddedToUpdate)
                continue;
            if (component->m_Loading)
            {
                dmResource::Result r = dmResource::RESULT_OK;
                if(component->m_Preloader)
                {
                    dmResource::PreloaderCompleteCallbackParams preloader_params;
                    preloader_params.m_Factory = world->m_Factory;
                    preloader_params.m_UserData = component;
                    r = dmResource::UpdatePreloader(component->m_Preloader, PreloadCompleteCallback, &preloader_params, 10*1000);
                }
                if (r != dmResource::RESULT_PENDING)
                {
                    LoadComplete(params, component, r);
                }
            }
        }
        DM_PROPERTY_ADD_U32(rmtp_CollectionFactory, world->m_IndexPool.Size());
        return result;
    }

    bool CompCollectionFactoryLoad(CollectionFactoryWorld* world, CollectionFactoryComponent* component,
                                    int callback_ref, int self_ref, int url_ref)
    {
        component->m_PreloaderCallbackRef = callback_ref;
        component->m_PreloaderSelfRef = self_ref;
        component->m_PreloaderURLRef = url_ref;

        // Here we assume that we've successfully chosen a resource to load
        CollectionFactoryResource* resource = GetResource(component);

        if(!resource->m_LoadDynamically)
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
        if(!resource->m_CollectionResources.Empty())
        {
            // If loaded, complete callback is invoked.
            component->m_Loading = 1;
            return true;
        }

        // We assume that the resource has loaded it's collection description up front

        dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) resource->m_CollectionDesc;

        // No need to process this collection further if there's nothing to load
        if (collection_desc->m_Instances.m_Count == 0)
        {
            component->m_Loading = 1;
            return true;
        }

        dmArray<const char*> names;
        names.SetCapacity(collection_desc->m_Instances.m_Count);
        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            if (instance_desc.m_Prototype == 0x0)
                continue;
            names.Push(instance_desc.m_Prototype);
        }
        component->m_Preloader = dmResource::NewPreloader(world->m_Factory, names);
        if(!component->m_Preloader)
        {
            return false;
        }
        component->m_Loading = 1;
        return true;
    }

    bool CompCollectionFactoryUnload(CollectionFactoryWorld* world, CollectionFactoryComponent* component)
    {
        if(!GetResource(component)->m_LoadDynamically)
        {
            return true;
        }
        if(component->m_Loading)
        {
            dmLogError("Trying to unload factory prototype resources while loading.");
            return false;
        }
        UnloadCollectionResources(world->m_Factory, component);
        return true;
    }

    CompCollectionFactoryStatus CompCollectionFactoryGetStatus(CollectionFactoryComponent* component)
    {
        if(component->m_Loading)
        {
            return COMP_COLLECTION_FACTORY_STATUS_LOADING;
        }
        CollectionFactoryResource* resource = GetResource(component);
        if(resource->m_CollectionResources.Empty())
        {
            return COMP_COLLECTION_FACTORY_STATUS_UNLOADED;
        }
        return COMP_COLLECTION_FACTORY_STATUS_LOADED;
    }

    static void UnloadCollectionResources(dmResource::HFactory factory, CollectionFactoryComponent* component)
    {
        dmArray<void*>& r = GetResource(component)->m_CollectionResources;
        for(uint32_t i = 0; i < r.Size(); ++i)
        {
            dmResource::Release( factory, r[i]);
        }
        r.SetSize(0);
    }

    static dmResource::Result LoadCollectionResources(dmResource::HFactory factory, CollectionFactoryComponent* component)
    {
        UnloadCollectionResources(factory, component);
        dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) GetResource(component)->m_CollectionDesc;
        dmArray<void*>& collection_resources = GetResource(component)->m_CollectionResources;

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

    static inline CollectionFactoryResource* GetResource(CollectionFactoryComponent* component)
    {
        return component->m_CustomResource ? component->m_CustomResource : component->m_Resource;
    }

    CollectionFactoryResource* CompCollectionFactoryGetResource(CollectionFactoryComponent* component)
    {
        return GetResource(component);
    }

    CollectionFactoryResource* CompCollectionFactoryGetDefaultResource(CollectionFactoryComponent* component)
    {
        return component->m_Resource;
    }

    CollectionFactoryResource* CompCollectionFactoryGetCustomResource(CollectionFactoryComponent* component)
    {
        return component->m_CustomResource;
    }

    bool CompCollectionFactoryIsLoading(CollectionFactoryComponent* component)
    {
        return component->m_Loading;
    }

    bool CompCollectionFactoryIsDynamicPrototype(CollectionFactoryComponent* component)
    {
        return component->m_Resource->m_DynamicPrototype;
    }

    dmResource::HFactory CompCollectionFactoryGetResourceFactory(CollectionFactoryWorld* world)
    {
        return world->m_Factory;
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
        if(GetResource(component)->m_LoadDynamically)
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

    dmGameObject::PropertyResult CompCollectionFactoryGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        CollectionFactoryComponent* component = (CollectionFactoryComponent*)*params.m_UserData;

        dmhash_t get_property = params.m_PropertyId;

        if (get_property == COLLECTION_FACTORY_PROP_PROTOTYPE)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(GetResource(component)->m_PrototypePathHash);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    void CompCollectionFactorySetResource(CollectionFactoryComponent* component, CollectionFactoryResource* resource)
    {
        component->m_CustomResource = resource;
    }

    dmGameObject::Result CompCollectionFactorySpawn(HCollectionFactoryWorld world, HCollectionFactoryComponent component, dmGameObject::HCollection collection,
                                                const char* id_prefix,
                                                const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale,
                                                dmGameObject::InstancePropertyContainers* properties, dmGameObject::InstanceIdMap* out_instances)
    {
        return dmGameObject::SpawnFromCollection(collection, CompCollectionFactoryGetResource(component)->m_CollectionDesc, id_prefix,
                                                         properties, position, rotation, scale, out_instances);
    }
}

