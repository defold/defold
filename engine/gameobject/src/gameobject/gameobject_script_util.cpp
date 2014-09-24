#include "gameobject.h"
#include "gameobject_private.h"
#include <stdint.h>
#include <ddf/ddf.h>
#include <resource/resource.h>
#include "res_script.h"
#include "res_lua.h"
#include "../proto/lua_ddf.h"
#include "gameobject_script_util.h"

namespace dmGameObject
{
    bool RegisterSubModules(dmResource::HFactory factory, dmScript::HContext script_context, dmLuaDDF::LuaModule* lua_module)
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
                if (dmScript::ModuleLoaded(script_context, desc.m_NameHash))
                {
                    dmResource::Release(factory, module_script);
                    continue;
                }

                if (!RegisterSubModules(factory, script_context, module_script->m_LuaModule))
                {
                    dmResource::Release(factory, module_script);
                    return false;
                }

                dmScript::Result sr = dmScript::AddModule(script_context,
                                                          (const char*) module_script->m_LuaModule->m_Script.m_Data,
                                                          module_script->m_LuaModule->m_Script.m_Count,
                                                          module_name, module_script, desc.m_NameHash);
                if (sr != dmScript::RESULT_OK)
                {
                    dmResource::Release(factory, module_script);
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    static void FreeModule(void* user_context, void* user_data)
    {
        dmResource::HFactory factory = (dmResource::HFactory) user_context;
        LuaScript* lua_script = (LuaScript*) user_data;
        dmResource::Release(factory, lua_script);
    }

    void FreeModules(dmResource::HFactory factory, dmScript::HContext script_context)
    {
        dmScript::IterateModules(script_context, factory, FreeModule);
        dmScript::ClearModules(script_context);
    }

}


