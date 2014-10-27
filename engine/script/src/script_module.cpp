#include "script.h"

#include "script_private.h"

#include <stdint.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/log.h>

#include <ddf/ddf.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    struct LuaLoadData
    {
        const char* m_Buffer;
        uint32_t m_Size;
    };

    static const char* ReadScript(lua_State *L, void *data, size_t *size)
    {
        LuaLoadData* lua_load_data = (LuaLoadData*)data;
        if (lua_load_data->m_Size == 0)
        {
            return 0x0;
        }
        else
        {
            *size = lua_load_data->m_Size;
            lua_load_data->m_Size = 0;
            return lua_load_data->m_Buffer;
        }
    }

    static bool LoadScript(lua_State* L, const void* buffer, uint32_t buffer_size, const char* filename)
    {
        int top = lua_gettop(L);
        (void) top;

        LuaLoadData data;
        data.m_Buffer = (const char*)buffer;
        data.m_Size = buffer_size;
        int ret = lua_load(L, &ReadScript, &data, filename);
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
    }

    static int LoadModule (lua_State *L) {
        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, SCRIPT_CONTEXT);
        HContext context = (HContext)lua_touserdata(L, -1);
        lua_pop(L, 1);

        const char *name = luaL_checkstring(L, 1);
        dmhash_t name_hash = dmHashString64(name);
        Module* module = context->m_Modules.Get(name_hash);

        if (module == NULL)
        {
            lua_pushfstring(L, "\n\tno file '%s'", name);
            assert(top + 1  == lua_gettop(L));
            return 1;
        }
        if (!LoadScript(L, module->m_Script, module->m_ScriptSize, name))
        {
            luaL_error(L, "error loading module '%s'from file '%s':\n\t%s",
                          lua_tostring(L, 1), name, lua_tostring(L, -1));
        }
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    Result AddModule(HContext context, const char* script, uint32_t script_size, const char* script_name, void* resource, dmhash_t path_hash)
    {
        dmhash_t module_hash = dmHashString64(script_name);

        Module module;
        module.m_Name = strdup(script_name);
        module.m_Script = (char*) malloc(script_size);
        memcpy(module.m_Script, script, script_size);
        module.m_ScriptSize = script_size;
        module.m_Resource = resource;

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

    Result ReloadModule(HContext context, const char* script, uint32_t script_size, dmhash_t path_hash)
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

        free(module->m_Script);

        module->m_Script = (char*) malloc(script_size);
        memcpy(module->m_Script, script, script_size);
        module->m_ScriptSize = script_size;

        if (LoadScript(L, script, script_size, module->m_Name))
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
        free(value->m_Name);
        free(value->m_Script);
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
