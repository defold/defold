// Copyright 2020-2024 The Defold Foundation
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

#include "gameobject.h"
#include "gameobject_private.h"
#include <stdint.h>
#include <ddf/ddf.h>
#include <resource/resource.h>
#include <dmsdk/gameobject/res_lua.h>
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


