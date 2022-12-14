// Copyright 2020-2022 The Defold Foundation
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

#include "script.h"

#include "script_private.h"

#include <stdint.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/path.h>

#include <ddf/ddf.h>

#include "script/lua_source_ddf.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{

    // Helper function where the decision is made if to load bytecode or source code.
    //
    // Currently the bytecode is only ever built with LuaJIT which means it cannot be loaded
    // with vanilla lua runtime. The LUA_BYTECODE_ENABLE_(32/62) indicates if we can load bytecode,
    // and in reality, if linking happens against LuaJIT.
    static void GetLuaSource(dmLuaDDF::LuaSource *source, const char **buf, uint32_t *size)
    {
        if (source->m_Bytecode.m_Count > 0)
        {
            *buf = (const char*)source->m_Bytecode.m_Data;
            *size = source->m_Bytecode.m_Count;
            return;
        }
        *buf = (const char*)source->m_Script.m_Data;
        *size = source->m_Script.m_Count;
    }

    int LuaLoad(lua_State *L, dmLuaDDF::LuaSource *source)
    {
        const char *buf;
        uint32_t size;
        GetLuaSource(source, &buf, &size);
        return luaL_loadbuffer(L, buf, size, source->m_Filename);
    }

    static bool LuaLoadModule(lua_State *L, const char *buf, uint32_t size, const char *filename)
    {
        int top = lua_gettop(L);
        (void) top;

        int ret = luaL_loadbuffer(L, buf, size, filename);
        if (ret == 0)
        {
            assert(top + 1 == lua_gettop(L));
            return true;
        }
        else
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            assert(top == lua_gettop(L));
            return false;
        }

        return true;
    }

    static int LoadModule(lua_State *L) {
        int top = lua_gettop(L);
        (void) top;

        HContext context = dmScript::GetScriptContext(L);

        const char *name = luaL_checkstring(L, 1);
        dmhash_t name_hash = dmHashString64(name);
        Module* module = context->m_Modules.Get(name_hash);

        if (module == NULL)
        {
            lua_pushfstring(L, "\n\tno file '%s'", name);
            assert(top + 1  == lua_gettop(L));
            return 1;
        }

        if (!LuaLoadModule(L, module->m_Script, module->m_ScriptSize, module->m_Filename))
        {
            luaL_error(L, "error loading module '%s'from file '%s':\n\t%s",
                          lua_tostring(L, 1), name, lua_tostring(L, -1));
        }
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    Result AddModule(HContext context, dmLuaDDF::LuaSource *source, const char *script_name, void* resource, dmhash_t path_hash)
    {
        dmhash_t module_hash = dmHashString64(script_name);


        Module module;
        module.m_Name = strdup(script_name);

        const char *buf;
        uint32_t size;
        GetLuaSource(source, &buf, &size);

        module.m_Script = (char*) malloc(size);
        module.m_ScriptSize = size;
        memcpy(module.m_Script, buf, size);

        module.m_Resource = resource;
        module.m_Filename = strdup(source->m_Filename);

        if (context->m_Modules.Full())
        {
            context->m_Modules.SetCapacity(127, context->m_Modules.Capacity() + 128);
            context->m_PathToModule.SetCapacity(127, context->m_PathToModule.Capacity() + 128);
        }

        context->m_Modules.Put(module_hash, module);
        Module* module_handle = context->m_Modules.Get(module_hash);
        context->m_PathToModule.Put(path_hash, module_handle);

        return RESULT_OK;
    }

    Result ReloadModule(HContext context, dmLuaDDF::LuaSource *source, dmhash_t path_hash)
    {
        lua_State* L = GetLuaState(context);
        int top = lua_gettop(L);
        (void) top;

        Module** module_handle = context->m_PathToModule.Get(path_hash);
        if (module_handle == 0)
        {
            return RESULT_MODULE_NOT_LOADED;
        }
        Module* module = *module_handle;

        const char *buf;
        uint32_t size;
        GetLuaSource(source, &buf, &size);

        module->m_Script = (char*) realloc(module->m_Script, size);
        module->m_ScriptSize = size;
        memcpy(module->m_Script, buf, size);

        if (LuaLoadModule(L, buf, size, module->m_Name))
        {
            lua_pushstring(L, module->m_Name);
            int ret = dmScript::PCall(L, 1, LUA_MULTRET);
            if (ret != 0)
            {
                assert(top == lua_gettop(L));
                return RESULT_LUA_ERROR;
            }
            // As we only run the module for the sake of reloading (updating data), it is safe to remove any module return values here
            lua_pop(L, lua_gettop(L) - top);
        }
        else
        {
            assert(top == lua_gettop(L));
            return RESULT_LUA_ERROR;
        }
        assert(top == lua_gettop(L));
        return RESULT_OK;
    }

    static void FreeModuleCallback(void* context, const uint64_t* key, Module* value)
    {
        if (value->m_Resource != 0) {
            dmResource::Release((dmResource::HFactory)context, value->m_Resource);
        }
        free(value->m_Script);
        free(value->m_Name);
        free(value->m_Filename);
    }

    void ClearModules(HContext context)
    {
        context->m_Modules.Iterate(&FreeModuleCallback, (void*) context->m_ResourceFactory);
        context->m_Modules.Clear();
    }

    bool ModuleLoaded(HContext context, const char* script_name)
    {
        dmhash_t module_hash = dmHashString64(script_name);
        return context->m_Modules.Get(module_hash) != 0;
    }

    bool ModuleLoaded(HContext context, dmhash_t path_hash)
    {
        return context->m_PathToModule.Get(path_hash) != 0;
    }

    void InitializeModule(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;
        lua_getfield(L, LUA_GLOBALSINDEX, "package");
        if (lua_istable(L, -1))
        {
            assert(lua_istable(L, -1));

            // NOTE: We replace package.loaders table
            // only our custom loader
            lua_newtable(L);
            lua_pushcfunction(L, LoadModule);
            lua_rawseti(L, -2, 1);
            lua_setfield(L, -2, "loaders");
            lua_pop(L, 1);
        }
        else
        {
            // Bare-bone lua, no libs opened. We don't created the package table.
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));
    }

}
