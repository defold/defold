#include <dlib/dstrings.h>
#include "res_lua.h"
#include "gameobject_script.h"

namespace dmGameObject
{
    dmResource::Result ResLuaCreate(const dmResource::ResourceCreateParams& params)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        LuaScript* lua_script = new LuaScript(lua_module);
        params.m_Resource->m_Resource = lua_script;
        params.m_Resource->m_ResourceSize = sizeof(LuaScript) + params.m_BufferSize - lua_script->m_LuaModule->m_Source.m_Script.m_Count;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLuaDestroy(const dmResource::ResourceDestroyParams& params)
    {
        LuaScript* script = (LuaScript*) params.m_Resource->m_Resource;
        dmDDF::FreeMessage(script->m_LuaModule);
        delete script;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLuaRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(params.m_Buffer, params.m_BufferSize, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        ModuleContext* module_context = (ModuleContext*) params.m_Context;
        uint32_t context_count = module_context->m_ScriptContexts.Size();
        for (uint32_t i = 0; i < context_count; ++i) {
            dmScript::HContext script_context = module_context->m_ScriptContexts[i];
            dmScript::ReloadModule(script_context, &lua_module->m_Source, params.m_Resource->m_NameHash);
        }
        LuaScript* lua_script = (LuaScript*) params.m_Resource->m_Resource;
        params.m_Resource->m_ResourceSize = sizeof(LuaScript) + params.m_BufferSize - lua_script->m_LuaModule->m_Source.m_Script.m_Count;
        dmDDF::FreeMessage(lua_script->m_LuaModule);
        lua_script->m_LuaModule = lua_module;
        return dmResource::RESULT_OK;
    }
}
