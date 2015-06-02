#include <string.h>
#include <script/script.h>
#include "res_script.h"
#include "res_lua.h"
#include "../proto/lua_ddf.h"
#include "gameobject_script.h"
#include "gameobject_script_util.h"

namespace dmGameObject
{
    dmResource::Result ResScriptPreload(dmResource::HFactory factory,
                                             dmResource::HPreloadHintInfo hint_info,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             void** preload_data,
                                             const char* filename)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;
            
        uint32_t n_modules = lua_module->m_Modules.m_Count;
        for (uint32_t i = 0; i < n_modules; ++i)
        {
            dmResource::PreloadHint(hint_info, lua_module->m_Resources[i]);
        }
        
        *preload_data = lua_module;    
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResScriptCreate(dmResource::HFactory factory,
                                       void* context,
                                       const void* buffer, uint32_t buffer_size,
                                       void* preload_data,
                                       dmResource::SResourceDescriptor* resource,
                                       const char* filename)
    {
        dmLuaDDF::LuaModule* lua_module = (dmLuaDDF::LuaModule*) preload_data;

        dmScript::HContext script_context = (dmScript::HContext)context;
        lua_State* L = dmScript::GetLuaState(script_context);

        if (!RegisterSubModules(factory, script_context, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        HScript script = NewScript(L, lua_module);
        if (script)
        {
            resource->m_Resource = (void*) script;
            return dmResource::RESULT_OK;
        }
        else
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
    }

    dmResource::Result ResScriptDestroy(dmResource::HFactory factory,
                                        void* context,
                                        dmResource::SResourceDescriptor* resource)
    {
        HScript script = (HScript)resource->m_Resource;
        dmDDF::FreeMessage(script->m_LuaModule);
        DeleteScript((HScript) script);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResScriptRecreate(dmResource::HFactory factory,
                                         void* context,
                                         const void* buffer, uint32_t buffer_size,
                                         dmResource::SResourceDescriptor* resource,
                                         const char* filename)
    {
        HScript script = (HScript) resource->m_Resource;

        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmScript::HContext script_context = (dmScript::HContext)context;
        if (!RegisterSubModules(factory, script_context, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmLuaDDF::LuaModule* old_lua_module = script->m_LuaModule;
        bool ok = ReloadScript(script, lua_module);
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
}
