#include <string.h>
#include <script/script.h>
#include "res_script.h"
#include "res_lua.h"
#include "../proto/lua_ddf.h"
#include "gameobject_script.h"

namespace dmGameObject
{

    void ReloadedCallback(void* user_data, dmResource::SResourceDescriptor* resource, const char* name)
    {
        LuaScript* module_script = (LuaScript*) user_data;
        if (module_script->m_NameHash == resource->m_NameHash && dmScript::ModuleLoaded(g_ScriptContext, module_script->m_ModuleHash))
        {
            dmScript::ReloadModule(g_ScriptContext, g_LuaState,
                                   (const char*) module_script->m_LuaModule->m_Script.m_Data,
                                   module_script->m_LuaModule->m_Script.m_Count,
                                   module_script->m_ModuleHash);
        }
    }

    static bool LoadModules(dmResource::HFactory factory, dmLuaDDF::LuaModule* lua_module)
    {
        uint32_t n_modules = lua_module->m_Modules.m_Count;
        for (uint32_t i = 0; i < n_modules; ++i)
        {
            const char* module_resource = lua_module->m_Resources[i];
            const char* module_name = lua_module->m_Modules[i];
            LuaScript* module_script = 0;
            dmResource::Result r = dmResource::Get(factory, module_resource, (void**) (&module_script));
            if (r == dmResource::RESULT_OK)
            {
                dmResource::SResourceDescriptor desc;
                r = dmResource::GetDescriptor(factory, module_resource, &desc);
                assert(r == dmResource::RESULT_OK);
                dmhash_t module_id = desc.m_NameHash;
                if (dmScript::ModuleLoaded(g_ScriptContext, module_id))
                {
                    continue;
                }

                if (!LoadModules(factory, module_script->m_LuaModule))
                {
                    dmResource::Release(factory, module_script);
                    return false;
                }

                dmScript::Result sr = dmScript::AddModule(g_ScriptContext,
                                                          (const char*) module_script->m_LuaModule->m_Script.m_Data,
                                                          module_script->m_LuaModule->m_Script.m_Count,
                                                          module_name, module_script);
                if (sr != dmScript::RESULT_OK)
                {
                    dmResource::Release(factory, module_script);
                    return false;
                }

                // NOTE: Writing to LuaScript is *not* best practice
                // as resource can be shared but I couldn't find a better solution atm
                // Lua-script doesn't know the module-name. Could perhaps manipulate the file-name..
                module_script->m_NameHash = desc.m_NameHash;
                module_script->m_ModuleHash = dmHashString64(module_name);
                dmResource::RegisterResourceReloadedCallback(factory, ReloadedCallback, module_script);
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    dmResource::Result ResScriptCreate(dmResource::HFactory factory,
                                       void* context,
                                       const void* buffer, uint32_t buffer_size,
                                       dmResource::SResourceDescriptor* resource,
                                       const char* filename)
    {
        dmLuaDDF::LuaModule* lua_module = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmLuaDDF::LuaModule>(buffer, buffer_size, &lua_module);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        if (!LoadModules(factory, lua_module))
        {
            dmDDF::FreeMessage(lua_module);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        HScript script = NewScript(lua_module, filename);
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

        dmLuaDDF::LuaModule* old_lua_module = script->m_LuaModule;
        bool ok = ReloadScript(script, lua_module, filename);
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
