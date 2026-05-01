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

#include "comp_factory.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <gameobject/gameobject.h>
#include <gameobject/gameobject_props.h>
#include <dmsdk/dlib/vmath.h>
#include "../resources/res_factory.h"

#include "../gamesys.h"
#include "../gamesys_private.h"

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Factory, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);

namespace dmGameSystem
{
    using namespace dmVMath;

    const char* FACTORY_MAX_COUNT_KEY = "factory.max_count";

    static const dmhash_t FACTORY_PROP_PROTOTYPE = dmHashString64("prototype");

    static void CleanupAsyncLoading(lua_State*, FactoryComponent*);
    static bool PreloadCompleteCallback(const dmResource::PreloaderCompleteCallbackParams*);
    static void LoadComplete(const dmGameObject::ComponentsUpdateParams&, FactoryComponent*, const dmResource::Result);

    struct FactoryComponent
    {
        void Init();

        FactoryResource*        m_Resource;
        FactoryResource*        m_CustomResource;
        dmResource::HPreloader  m_Preloader;
        int                     m_PreloaderCallbackRef;
        int                     m_PreloaderSelfRef;
        int                     m_PreloaderURLRef;
        uint32_t                m_Loading : 1;
        uint32_t                m_AddedToUpdate : 1;
        uint32_t                : 30;
    };

    void FactoryComponent::Init()
    {
        memset(this, 0x0, sizeof(FactoryComponent));
        this->m_PreloaderCallbackRef = LUA_NOREF;
        this->m_PreloaderSelfRef = LUA_NOREF;
        this->m_PreloaderURLRef = LUA_NOREF;
    }

    struct FactoryWorld
    {
        dmResource::HFactory        m_Factory;
        dmArray<FactoryComponent>   m_Components;
        dmIndexPool32               m_IndexPool;
        uint32_t                    m_TotalFactoryCount;
    };

    inline dmGameObject::Result DoSpawn(HFactoryWorld world, HFactoryComponent component, dmGameObject::HCollection collection,
                                        dmhash_t id, const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale,
                                        dmGameObject::HPropertyContainer properties, dmGameObject::HInstance* out_instance)
    {
        uint32_t index = dmGameObject::AcquireInstanceIndex(collection);
        if (index == dmGameObject::INVALID_INSTANCE_POOL_INDEX)
        {
            dmLogWarning("Gameobject buffer is full. See `collection.max_instances` in game.project.");
            return dmGameObject::RESULT_OUT_OF_RESOURCES;
        }

        if (!id)
        {
            id = dmGameObject::CreateInstanceId();
        }

        dmGameObject::HPrototype prototype = CompFactoryGetPrototype(world, component);
        if (prototype == 0x0)
        {
            dmLogError("Unable to find the prototype specified in the factory component.");
            return dmGameObject::RESULT_RESOURCE_ERROR;
        }

        const char* path = CompFactoryGetPrototypePath(world, component);

        dmGameObject::Result result = dmGameObject::Spawn(collection, prototype, path, id, properties, position, rotation, scale, out_instance);
        if (result != dmGameObject::RESULT_OK)
        {
            dmLogError("Could not spawn an instance of prototype %s.", path);
            return result;
        }

        if (*out_instance != 0x0)
        {
            dmGameObject::AssignInstanceIndex(index, *out_instance);
        }
        else
        {
            dmGameObject::ReleaseInstanceIndex(index, collection);
        }
        return result;
    }

    dmGameObject::CreateResult CompFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        FactoryContext* context = (FactoryContext*)params.m_Context;
        FactoryWorld* world = new FactoryWorld();
        world->m_Factory = context->m_Factory;

        uint32_t max_component_count = dmMath::Min(params.m_MaxComponentInstances, context->m_MaxFactoryCount);
        world->m_Components.SetCapacity(max_component_count);
        world->m_Components.SetSize(max_component_count);
        world->m_IndexPool.SetCapacity(max_component_count);
        for(uint32_t i = 0; i < max_component_count; ++i)
        {
            world->m_Components[i].Init();
        }
        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompFactoryDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        delete (FactoryWorld*)params.m_World;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompFactoryCreate(const dmGameObject::ComponentCreateParams& params)
    {
        FactoryWorld* fw = (FactoryWorld*)params.m_World;
        FactoryComponent* component;
        if (fw->m_IndexPool.Remaining() > 0)
        {
            uint32_t index = fw->m_IndexPool.Pop();
            component= &fw->m_Components[index];
            component->m_Resource = (FactoryResource*) params.m_Resource;
            component->m_CustomResource = 0;
            *params.m_UserData = (uintptr_t) component;
        }
        else
        {
            ShowFullBufferError("Factory", FACTORY_MAX_COUNT_KEY, fw->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    void* CompFactoryGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        return (void*)params.m_UserData;
    }

    dmGameObject::CreateResult CompFactoryDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        FactoryWorld* world = (FactoryWorld*)params.m_World;
        FactoryComponent* component = (FactoryComponent*)*params.m_UserData;
        CleanupAsyncLoading(dmScript::GetLuaState(((FactoryContext*)params.m_Context)->m_ScriptContext), component);
        uint32_t index = component - &world->m_Components[0];
        component->m_Resource = 0x0;
        if (component->m_CustomResource)
            dmGameSystem::ResFactoryDestroyResource(world->m_Factory, component->m_CustomResource);
        component->m_AddedToUpdate = 0;
        world->m_IndexPool.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        FactoryComponent* component = (FactoryComponent*)*params.m_UserData;
        component->m_AddedToUpdate = 1;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        FactoryWorld* world = (FactoryWorld*)params.m_World;
        dmGameObject::UpdateResult result = dmGameObject::UPDATE_RESULT_OK;
        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            FactoryComponent* component = &world->m_Components[i];
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
        DM_PROPERTY_ADD_U32(rmtp_Factory, world->m_IndexPool.Size());
        return result;
    }

    dmGameObject::UpdateResult CompFactoryOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::Create::m_DDFDescriptor)
        {
            HFactoryWorld world = (HFactoryWorld)params.m_World;
            dmGameObject::HInstance instance = params.m_Instance;
            dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
            dmMessage::Message* message = params.m_Message;

            dmGameSystemDDF::Create* create = (dmGameSystemDDF::Create*) params.m_Message->m_Data;
            uint32_t msg_size = sizeof(dmGameSystemDDF::Create);
            uint32_t property_buffer_size = message->m_DataSize - msg_size;
            dmGameObject::HPropertyContainer properties = 0;
            if (property_buffer_size > 0)
            {
                uint8_t* property_buffer = (uint8_t*)(((uintptr_t)create) + msg_size);

                properties = dmGameObject::PropertyContainerAllocateWithSize(property_buffer_size);
                PropertyContainerDeserialize(property_buffer, property_buffer_size, properties);
            }
            FactoryComponent* component = (FactoryComponent*) *params.m_UserData;

            // m_Scale is legacy, so use it if Scale3 is all zeroes
            Vector3 scale;
            if (create->m_Scale3.getX() == 0 && create->m_Scale3.getY() == 0 && create->m_Scale3.getZ() == 0)
            {
                scale = Vector3(create->m_Scale, create->m_Scale, create->m_Scale);
            }
            else
            {
                scale = create->m_Scale3;
            }

            dmGameObject::HInstance spawned_instance;
            dmGameObject::Result result = DoSpawn(world, component, collection, create->m_Id, create->m_Position, create->m_Rotation, scale, properties, &spawned_instance);

            if (properties)
            {
                dmGameObject::PropertyContainerDestroy(properties);
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static FactoryResource* CompFactoryGetResourceInternal(HFactoryComponent component)
    {
        return component->m_CustomResource ? component->m_CustomResource : component->m_Resource;
    }

    FactoryResource* CompFactoryGetResource(HFactoryWorld world, HFactoryComponent component)
    {
        (void)world;
        return CompFactoryGetResourceInternal(component);
    }

    static dmGameObject::HPrototype GetPrototype(dmResource::HFactory factory, HFactoryComponent component)
    {
        FactoryResource* resource = CompFactoryGetResourceInternal(component);
        if(!resource->m_Prototype)
        {
            if(dmResource::Get(factory, resource->m_PrototypePath, (void**)&resource->m_Prototype) != dmResource::RESULT_OK)
            {
                dmLogError("Failed to get factory prototype resource: %s", resource->m_PrototypePath);
                return 0;
            }
        }
        return resource->m_Prototype;
    }

    static void ResetCallbacks(FactoryComponent* component)
    {
        component->m_PreloaderCallbackRef = LUA_NOREF;
        component->m_PreloaderSelfRef = LUA_NOREF;
        component->m_PreloaderURLRef = LUA_NOREF;
    }

    bool CompFactoryLoad(HFactoryWorld world, HFactoryComponent component, int callback_ref, int self_ref, int url_ref)
    {
        component->m_PreloaderCallbackRef = callback_ref;
        component->m_PreloaderSelfRef = self_ref;
        component->m_PreloaderURLRef = url_ref;

        FactoryResource* resource = CompFactoryGetResource(world, component);
        if(!resource->m_LoadDynamically)
        {
            // set as loading without preloader so complete callback is invoked as should be by design.
            component->m_Loading = 1;
            return true;
        }
        if(component->m_Loading)
        {
            dmLogError("Trying to load factory prototype resource when already loading.");
            ResetCallbacks(component);
            return false;
        }
        if(resource->m_Prototype)
        {
            // If loaded, complete callback is invoked.
            component->m_Loading = 1;
            return true;
        }

        component->m_Preloader = dmResource::NewPreloader(world->m_Factory, resource->m_PrototypePath);
        if(!component->m_Preloader)
        {
            ResetCallbacks(component);
            return false;
        }
        component->m_Loading = 1;
        return true;
    }

    bool CompFactoryUnload(HFactoryWorld world, HFactoryComponent component)
    {
        FactoryResource* resource = CompFactoryGetResource(world, component);
        if(!resource->m_LoadDynamically)
        {
            return true;
        }
        if(component->m_Loading)
        {
            dmLogError("Trying to unload factory prototype resource while loading.");
            return false;
        }
        if(resource->m_Prototype)
        {
            dmResource::Release(world->m_Factory, resource->m_Prototype);
            resource->m_Prototype = 0;
        }
        return true;
    }

    // scripting
    dmResource::HFactory CompFactoryGetResourceFactory(FactoryWorld* world)
    {
        return world->m_Factory;
    }

    dmGameObject::HPrototype CompFactoryGetPrototype(HFactoryWorld world, HFactoryComponent component)
    {
        return GetPrototype(world->m_Factory, component);
    }

    const char* CompFactoryGetPrototypePath(HFactoryWorld world, HFactoryComponent component)
    {
        return CompFactoryGetResource(world, component)->m_PrototypePath;
    }

    CompFactoryStatus CompFactoryGetStatus(HFactoryWorld world, HFactoryComponent component)
    {
        if(component->m_Loading)
        {
            return COMP_FACTORY_STATUS_LOADING;
        }
        if(CompFactoryGetResource(world, component)->m_Prototype == 0x0)
        {
            return COMP_FACTORY_STATUS_UNLOADED;
        }
        return COMP_FACTORY_STATUS_LOADED;
    }

    bool CompFactoryIsLoading(HFactoryWorld world, HFactoryComponent component)
    {
        return component->m_Loading;
    }

    bool CompFactoryIsDynamicPrototype(HFactoryWorld world, HFactoryComponent component)
    {
         return component->m_Resource->m_DynamicPrototype;
    }

    FactoryResource* CompFactoryGetDefaultResource(HFactoryWorld world, HFactoryComponent component)
    {
        return component->m_Resource;
    }

    FactoryResource* CompFactoryGetCustomResource(HFactoryWorld world, HFactoryComponent component)
    {
        return component->m_CustomResource;
    }

    void CompFactorySetResource(HFactoryWorld world, HFactoryComponent component, FactoryResource* resource)
    {
        component->m_CustomResource = resource;
    }
    // end scripting

    dmGameObject::PropertyResult CompFactoryGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        FactoryComponent* component = (FactoryComponent*)*params.m_UserData;

        dmhash_t get_property = params.m_PropertyId;

        if (get_property == FACTORY_PROP_PROTOTYPE)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(dmHashString64(CompFactoryGetResourceInternal(component)->m_PrototypePath));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    static void CleanupAsyncLoading(lua_State* L, HFactoryComponent component)
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
        HFactoryComponent component = (HFactoryComponent) params->m_UserData;
        return GetPrototype(params->m_Factory, component) != 0;
    }

    static void LoadComplete(const dmGameObject::ComponentsUpdateParams& params, HFactoryComponent component, const dmResource::Result result)
    {
        component->m_Loading = 0;
        lua_State* L = dmScript::GetLuaState(((FactoryContext*)params.m_Context)->m_ScriptContext);
        int top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, component->m_PreloaderCallbackRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, component->m_PreloaderSelfRef);
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);
        if (!dmScript::IsInstanceValid(L))
        {
            lua_pop(L, 2);
            dmLogError("Could not run factory.load complete callback because the instance has been deleted.");
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

    dmGameObject::Result CompFactorySpawn(HFactoryWorld world, HFactoryComponent component, dmGameObject::HCollection collection, dmhash_t id,
                                                const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale,
                                                dmGameObject::HPropertyContainer properties, dmGameObject::HInstance* out_instance)
    {
        return DoSpawn(world, component, collection, id, position, rotation, scale, properties, out_instance);
    }

}
