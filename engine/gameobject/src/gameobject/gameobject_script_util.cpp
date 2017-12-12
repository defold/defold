#include "gameobject.h"
#include "gameobject_private.h"
#include <stdint.h>
#include <ddf/ddf.h>
#include <resource/resource.h>
#include "res_script.h"
#include "res_lua.h"
#include "../proto/gameobject/lua_ddf.h"
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

                dmScript::Result sr = dmScript::AddModule(script_context, &module_script->m_LuaModule->m_Source, module_name, module_script, desc.m_NameHash);
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

    Result LuaLoad(dmResource::HFactory factory, dmScript::HContext context, dmLuaDDF::LuaModule* module)
    {
        if (!dmGameObject::RegisterSubModules(factory, context, module) ) {
            dmLogError("Failed to load sub modules to module %s", module->m_Source.m_Filename);
            return dmGameObject::RESULT_COMPONENT_NOT_FOUND;
        }

        lua_State* L = dmScript::GetLuaState(context);
        int ret = dmScript::LuaLoad(L, &module->m_Source);
        if (ret != 0)
            return dmGameObject::RESULT_UNKNOWN_ERROR;

        dmScript::PCall(L, 0, 0);
        return dmGameObject::RESULT_OK;
    }

}


