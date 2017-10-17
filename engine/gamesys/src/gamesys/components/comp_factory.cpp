#include "comp_factory.h"

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

    const char* FACTORY_MAX_COUNT_KEY = "factory.max_count";

    static void CleanupAsyncLoading(lua_State*, FactoryComponent*);
    static bool PreloadCompleteCallback(const dmResource::PreloaderCompleteCallbackParams*);
    static void LoadComplete(const dmGameObject::ComponentsUpdateParams&, FactoryComponent*, const dmResource::Result);

    struct FactoryWorld
    {
        dmArray<FactoryComponent>   m_Components;
        dmIndexPool32               m_IndexPool;
        uint32_t                    m_TotalFactoryCount;
    };

    void FactoryComponent::Init()
    {
        memset(this, 0x0, sizeof(FactoryComponent));
        this->m_PreloaderCallbackRef = LUA_NOREF;
        this->m_PreloaderSelfRef = LUA_NOREF;
        this->m_PreloaderURLRef = LUA_NOREF;
    }

    dmGameObject::CreateResult CompFactoryNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        FactoryContext* context = (FactoryContext*)params.m_Context;
        FactoryWorld* fw = new FactoryWorld();
        const uint32_t max_component_count = context->m_MaxFactoryCount;
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
            *params.m_UserData = (uintptr_t) component;
        }
        else
        {
            dmLogError("Can not create more factory components since the buffer is full (%d).", fw->m_Components.Size());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompFactoryDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        FactoryWorld* fw = (FactoryWorld*)params.m_World;
        FactoryComponent* fc = (FactoryComponent*)*params.m_UserData;
        CleanupAsyncLoading(dmScript::GetLuaState(((FactoryContext*)params.m_Context)->m_ScriptContext), fc);
        uint32_t index = fc - &fw->m_Components[0];
        fc->m_Resource = 0x0;
        fc->m_AddedToUpdate = 0;
        fw->m_IndexPool.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompFactoryAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        FactoryComponent* component = (FactoryComponent*)*params.m_UserData;
        component->m_AddedToUpdate = 1;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompFactoryUpdate(const dmGameObject::ComponentsUpdateParams& params)
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

        return result;
    }

    dmGameObject::UpdateResult CompFactoryOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        if (params.m_Message->m_Descriptor == (uintptr_t)dmGameSystemDDF::Create::m_DDFDescriptor)
        {
            dmGameObject::HInstance instance = params.m_Instance;
            dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
            dmMessage::Message* message = params.m_Message;

            dmGameSystemDDF::Create* create = (dmGameSystemDDF::Create*) params.m_Message->m_Data;
            uint32_t msg_size = sizeof(dmGameSystemDDF::Create);
            uint32_t property_buffer_size = message->m_DataSize - msg_size;
            uint8_t* property_buffer = 0x0;
            if (property_buffer_size > 0)
            {
                property_buffer = (uint8_t*)(((uintptr_t)create) + msg_size);
            }
            FactoryComponent* fc = (FactoryComponent*) *params.m_UserData;

            uint32_t index = create->m_Index;
            dmhash_t id = create->m_Id;

            if (id == 0)
            {
                if (index == dmGameObject::INVALID_INSTANCE_POOL_INDEX)
                {
                    index = dmGameObject::AcquireInstanceIndex(collection);
                }

                if (index == dmGameObject::INVALID_INSTANCE_POOL_INDEX)
                {
                    dmLogError("Can not create gameobject since the buffer is full.");
                    return dmGameObject::UPDATE_RESULT_OK;
                }

                id = dmGameObject::ConstructInstanceId(index);
            }

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
            dmGameObject::HPrototype prototype = CompFactoryGetPrototype(collection, fc);
            dmGameObject::HInstance spawned_instance =  dmGameObject::Spawn(collection, prototype, fc->m_Resource->m_FactoryDesc->m_Prototype, id, property_buffer, property_buffer_size,
                create->m_Position, create->m_Rotation, scale);
            if (index != dmGameObject::INVALID_INSTANCE_POOL_INDEX)
            {
                if (spawned_instance != 0x0)
                {
                    dmGameObject::AssignInstanceIndex(index, spawned_instance);
                }
                else
                {
                    dmGameObject::ReleaseInstanceIndex(index, collection);
                }
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmGameObject::HPrototype GetPrototype(dmResource::HFactory factory, FactoryComponent* component)
    {
        if(!component->m_Resource->m_Prototype)
        {
            if(dmResource::Get(factory, component->m_Resource->m_FactoryDesc->m_Prototype, (void**)&component->m_Resource->m_Prototype) != dmResource::RESULT_OK)
            {
                dmLogError("Failed to get factory prototype resource: %s", component->m_Resource->m_FactoryDesc->m_Prototype);
                return 0;
            }
        }
        return component->m_Resource->m_Prototype;
    }

    dmGameObject::HPrototype CompFactoryGetPrototype(dmGameObject::HCollection collection, FactoryComponent* component)
    {
        return GetPrototype(dmGameObject::GetFactory(collection), component);
    }

    bool CompFactoryLoad(dmGameObject::HCollection collection, FactoryComponent* component)
    {
        if(!component->m_Resource->m_FactoryDesc->m_LoadDynamically)
        {
            // set as loading without preloader so complete callback is invoked as should be by design.
            component->m_Loading = 1;
            return true;
        }
        if(component->m_Loading)
        {
            dmLogError("Trying to load factory prototype resource when already loading.");
            return false;
        }
        if(component->m_Resource->m_Prototype)
        {
            // If loaded, complete callback is invoked.
            component->m_Loading = 1;
            return true;
        }

        component->m_Preloader = dmResource::NewPreloader(dmGameObject::GetFactory(collection), component->m_Resource->m_FactoryDesc->m_Prototype);
        if(!component->m_Preloader)
        {
            return false;
        }
        component->m_Loading = 1;
        return true;
    }

    bool CompFactoryUnload(dmGameObject::HCollection collection, FactoryComponent* component)
    {
        if(!component->m_Resource->m_FactoryDesc->m_LoadDynamically)
        {
            return true;
        }
        if(component->m_Loading)
        {
            dmLogError("Trying to unload factory prototype resource while loading.");
            return false;
        }
        if(component->m_Resource->m_Prototype)
        {
            dmResource::Release(dmGameObject::GetFactory(collection), component->m_Resource->m_Prototype);
            component->m_Resource->m_Prototype = 0;
        }
        return true;
    }

    CompFactoryStatus CompFactoryGetStatus(FactoryComponent* component)
    {
        if(component->m_Loading)
        {
            return COMP_FACTORY_STATUS_LOADING;
        }
        if(component->m_Resource->m_Prototype == 0x0)
        {
            return COMP_FACTORY_STATUS_UNLOADED;
        }
        return COMP_FACTORY_STATUS_LOADED;
    }

    static void CleanupAsyncLoading(lua_State* L, FactoryComponent* component)
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
        FactoryComponent* component = (FactoryComponent *) params->m_UserData;
        return GetPrototype(params->m_Factory, component) != 0;
    }

    static void LoadComplete(const dmGameObject::ComponentsUpdateParams& params, FactoryComponent* component, const dmResource::Result result)
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


}
