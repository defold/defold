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

        *params.m_PreloadData = lua_module;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResScriptCreate(const dmResource::ResourceCreateParams& params)
    {
        dmLuaDDF::LuaModule* lua_module = (dmLuaDDF::LuaModule*) params.m_PreloadData;

        dmScript::HContext script_context = (dmScript::HContext) params.m_Context;
        lua_State* L = dmScript::GetLuaState(script_context);

        if (!RegisterSubModules(params.m_Factory, script_context, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        HScript script = NewScript(L, lua_module, params.m_BufferSize);
        if (script)
        {
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
        bool ok = ReloadScript(script, lua_module, params.m_BufferSize);
        if (ok)
        {
            dmDDF::FreeMessage(old_lua_module);
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResScriptGetInfo(dmResource::ResourceGetInfoParams& params)
    {
        params.m_DataSize = dmGameObject::GetScriptResourceSize((HScript)params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;

    }

}
