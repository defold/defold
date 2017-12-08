#include <string.h>
#include <script/script.h>
#include "res_script.h"
#include "res_lua.h"
#include "../proto/gameobject/lua_ddf.h"
#include "gameobject_script.h"
#include "gameobject_script_util.h"

namespace dmGameObject
{
    dmResource::Result ResScriptPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        uint32_t n_modules = lua_module->m_Modules.m_Count;
        for (uint32_t i = 0; i < n_modules; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, lua_module->m_Resources[i]);
        }

        for (uint32_t i = 0; i < lua_module->m_PropertyResources.m_Count; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, lua_module->m_PropertyResources.m_Data[i]);
        }

        *params.m_PreloadData = lua_module;
        return dmResource::RESULT_OK;
    }

    static void UnloadPropertyResources(dmResource::HFactory factory, dmArray<void*>& property_resources)
    {
        for(uint32_t i = 0; i < property_resources.Size(); ++i)
        {
            dmResource::Release(factory, property_resources[i]);
        }
        property_resources.SetSize(0);
        property_resources.SetCapacity(0);
    }

    static dmResource::Result LoadPropertyResources(dmResource::HFactory factory, dmArray<void*>& property_resources, dmLuaDDF::LuaModule* ddf)
    {
        const char**& list = ddf->m_PropertyResources.m_Data;
        uint32_t count = ddf->m_PropertyResources.m_Count;
        assert(property_resources.Size() == 0);
        property_resources.SetCapacity(count);
        for(uint32_t i = 0; i < count; ++i)
        {
            void* resource;
            dmResource::Result res = dmResource::Get(factory, list[i], &resource);
            if(res != dmResource::RESULT_OK)
            {
                dmLogError("Could not load script component property resource '%s'", list[i]);
                UnloadPropertyResources(factory, property_resources);
                return res;
            }
            property_resources.Push(resource);
        }
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResScriptCreate(const dmResource::ResourceCreateParams& params)
    {
        dmLuaDDF::LuaModule* lua_module = (dmLuaDDF::LuaModule*) params.m_PreloadData;

        dmScript::HContext script_context = (dmScript::HContext) params.m_Context;
        if (!RegisterSubModules(params.m_Factory, script_context, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        HScript script = NewScript(script_context, lua_module);
        if (script)
        {
            dmResource::Result res = LoadPropertyResources(params.m_Factory, script->m_PropertyResources, lua_module);
            if(res != dmResource::RESULT_OK)
            {
                DeleteScript(script);
                return res;
            }
            params.m_Resource->m_Resource = (void*) script;
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResScriptDestroy(const dmResource::ResourceDestroyParams& params)
    {
        HScript script = (HScript)params.m_Resource->m_Resource;
        UnloadPropertyResources(params.m_Factory, script->m_PropertyResources);
        dmDDF::FreeMessage(script->m_LuaModule);
        DeleteScript((HScript) script);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResScriptRecreate(const dmResource::ResourceRecreateParams& params)
    {
        HScript script = (HScript) params.m_Resource->m_Resource;

        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmScript::HContext script_context = (dmScript::HContext) params.m_Context;
        if (!RegisterSubModules(params.m_Factory, script_context, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmLuaDDF::LuaModule* old_lua_module = script->m_LuaModule;
        bool ok = ReloadScript(script, lua_module);
        if (ok)
        {
            dmArray<void*> tmp_res;
            dmResource::Result res = LoadPropertyResources(params.m_Factory, tmp_res, lua_module);
            if(res == dmResource::RESULT_OK)
            {
                UnloadPropertyResources(params.m_Factory, script->m_PropertyResources);
                if(tmp_res.Size())
                {
                    script->m_PropertyResources.SetCapacity(tmp_res.Size());
                    script->m_PropertyResources.PushArray(&tmp_res[0], tmp_res.Size());
                }
            }
            dmDDF::FreeMessage(old_lua_module);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }
}
