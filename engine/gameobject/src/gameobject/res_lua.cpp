#include <dlib/dstrings.h>
#include "res_lua.h"
#include "gameobject_script.h"

namespace dmGameObject
{
    dmResource::Result ResLuaCreate(dmResource::HFactory factory,
                                    void* context,
                                    const void* buffer, uint32_t buffer_size,
                                    void* preload_data,
                                    dmResource::SResourceDescriptor* resource,
                                    const char* filename)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        resource->m_Resource = (void*) new LuaScript(lua_module);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLuaDestroy(dmResource::HFactory factory,
                                     void* context,
                                     dmResource::SResourceDescriptor* resource)
    {
        LuaScript* script = (LuaScript*) resource->m_Resource;
        dmDDF::FreeMessage(script->m_LuaModule);
        delete script;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLuaRecreate(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        ModuleContext* module_context = (ModuleContext*)context;
        uint32_t context_count = module_context->m_ScriptContexts.Size();
        for (uint32_t i = 0; i < context_count; ++i) {
            dmScript::HContext script_context = module_context->m_ScriptContexts[i];
            dmScript::ReloadModule(script_context, &lua_module->m_Source, resource->m_NameHash);
        }
        LuaScript* lua_script = (LuaScript*) resource->m_Resource;
        dmDDF::FreeMessage(lua_script->m_LuaModule);
        lua_script->m_LuaModule = lua_module;
        return dmResource::RESULT_OK;
    }
}
