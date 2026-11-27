// Copyright 2020-2026 The Defold Foundation
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

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/path.h>

#include <ddf/ddf.h>
#include <lz4/lz4.h>

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

    static int DumpModuleCode(lua_State* L, const void* data, size_t size, void* context)
    {
        dmArray<uint8_t>* buffer = (dmArray<uint8_t>*)context;
        uint32_t count = (uint32_t)size;
        uint32_t total = buffer->Size() + count;
        if (total > buffer->Capacity())
            buffer->SetCapacity(dmMath::Max(total, buffer->Capacity() * 2));
        buffer->PushArray((const uint8_t*)data, count);
        return 0;
    }

    static int LoadDumpedModuleChunk(lua_State* L, int code_index, int size_index)
    {
        size_t size;
        const char* code = lua_tolstring(L, code_index, &size);
        uint32_t uncompressed_size = (uint32_t)lua_tointeger(L, size_index);
        dmArray<char> buffer;
        if (uncompressed_size)
        {
            buffer.SetCapacity(uncompressed_size);
            int decoded = LZ4_decompress_safe(code, buffer.Begin(), (int)size, (int)uncompressed_size);
            if (decoded != (int)uncompressed_size)
            {
                lua_pushliteral(L, "Failed to decompress Lua module code");
                return LUA_ERRSYNTAX;
            }
            code = buffer.Begin();
            size = uncompressed_size;
        }
        return luaL_loadbuffer(L, code, size, "=(module factory)");
    }

    static int GetPreparedModuleChunk(lua_State* L)
    {
        lua_pushvalue(L, lua_upvalueindex(2));
        if (!lua_isnil(L, -1))
        {
            // Release the first-use function before its body can install a module environment.
            lua_pushnil(L);
            lua_replace(L, lua_upvalueindex(2));
            return 1;
        }
        lua_pop(L, 1);
        if (LoadDumpedModuleChunk(L, lua_upvalueindex(1), lua_upvalueindex(3)) != 0)
            return lua_error(L);
        return 1;
    }

    struct ModuleFactoryParams
    {
        const char* m_Code;
        uint32_t m_Size;
        uint32_t m_UncompressedSize;
    };

    static int CreateModuleFactory(lua_State* L)
    {
        ModuleFactoryParams* params = (ModuleFactoryParams*)lua_touserdata(L, 1);
        lua_pushlstring(L, params->m_Code, params->m_Size);
        lua_pushvalue(L, 2);
        lua_pushinteger(L, params->m_UncompressedSize);
        lua_pushcclosure(L, GetPreparedModuleChunk, 3);
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_setfenv(L, -2);
        return 1;
    }

    static int PrepareModuleFactory(lua_State* L)
    {
        int top = lua_gettop(L);
        lua_pushcfunction(L, CreateModuleFactory);
        lua_pushvalue(L, -2);
        dmArray<uint8_t> code, compressed;
        int status = lua_dump(L, DumpModuleCode, &code);
        if (status != 0)
        {
            lua_settop(L, top - 1);
            lua_pushliteral(L, "Failed to prepare Lua module code");
            return status;
        }
        lua_pop(L, 1);
        int capacity = code.Size() <= LZ4_MAX_INPUT_SIZE ? LZ4_compressBound((int)code.Size()) : 0;
        int compressed_size = 0;
        if (capacity)
        {
            compressed.SetCapacity(capacity);
            compressed_size = LZ4_compress_default((const char*)code.Begin(), (char*)compressed.Begin(), (int)code.Size(), capacity);
        }
        bool use_compressed = compressed_size > 0 && (uint32_t)compressed_size < code.Size();
        ModuleFactoryParams params;
        params.m_Code = (const char*)(use_compressed ? compressed.Begin() : code.Begin());
        params.m_Size = use_compressed ? compressed_size : code.Size();
        params.m_UncompressedSize = use_compressed ? code.Size() : 0;
        lua_pushlightuserdata(L, &params);
        lua_pushvalue(L, top);
        // Keep native temporary buffers outside the protected call so Lua allocation errors cannot skip their destruction.
        status = lua_pcall(L, 2, 1, 0);
        lua_remove(L, -2);
        return status;
    }

    static bool LuaLoadModule(lua_State* L, const char* buf, uint32_t size, const char* filename, int* chunk_ref)
    {
        int top = lua_gettop(L);
        (void) top;

        int ret = luaL_loadbuffer(L, buf, size, filename);
        if (ret == 0 && chunk_ref)
            ret = PrepareModuleFactory(L);
        if (chunk_ref)
        {
            Unref(L, LUA_REGISTRYINDEX, *chunk_ref);
            lua_pushvalue(L, -1);
            *chunk_ref = Ref(L, LUA_REGISTRYINDEX);
            if (ret == 0)
            {
                // Reload executes a separate function so the cached first-use function stays untouched.
                lua_getupvalue(L, -1, 1);
                lua_getupvalue(L, -2, 3);
                ret = LoadDumpedModuleChunk(L, -2, -1);
                lua_remove(L, -2);
                lua_remove(L, -2);
                lua_remove(L, -2);
            }
        }
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

        if (module->m_ChunkRef != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, module->m_ChunkRef);
            if (!lua_isfunction(L, -1))
                return lua_error(L);
            lua_call(L, 0, 1);
            // Each execution starts in the requiring thread's global environment.
            lua_pushvalue(L, LUA_GLOBALSINDEX);
            lua_setfenv(L, -2);
        }
        else if (!LuaLoadModule(L, module->m_Script, module->m_ScriptSize, module->m_Filename, 0))
        {
            luaL_error(L, "error loading module '%s'from file '%s':\n\t%s",
                          lua_tostring(L, 1), name, lua_tostring(L, -1));
        }
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static void FreeModuleCallback(HContext context, const uint64_t* key, Module* value);

    Result AddModule(HContext context, dmLuaDDF::LuaSource *source, const char *script_name, void* resource, dmhash_t path_hash)
    {
        return AddModule(context, source, script_name, resource, path_hash, 0);
    }

    Result AddModule(HContext context, dmLuaDDF::LuaSource* source, const char* script_name, void* resource, dmhash_t path_hash, ModuleLoader loader)
    {
        dmhash_t module_hash = dmHashString64(script_name);
        Module* existing_module = context->m_Modules.Get(module_hash);
        if (existing_module != 0 && strcmp(existing_module->m_Name, script_name) != 0)
        {
            dmLogError("Lua module name hash collision between '%s' and '%s'", existing_module->m_Name, script_name);
            return RESULT_MODULE_NAME_HASH_COLLISION;
        }

        int chunk_ref = LUA_NOREF;
        if (loader)
        {
            // Mark the path while dependencies are prepared, so recursive registration can skip it.
            bool new_path = context->m_PathToModule.Get(path_hash) == 0;
            if (new_path)
            {
                if (context->m_PathToModule.Full())
                    context->m_PathToModule.SetCapacity(127, context->m_PathToModule.Capacity() + 128);
                context->m_PathToModule.Put(path_hash, module_hash);
            }

            lua_State* L = GetLuaState(context);
            int top = lua_gettop(L);
            (void)top;
            int status = loader(context, L, resource);
            if (status == 0)
                status = PrepareModuleFactory(L);
            assert(top + 1 == lua_gettop(L));
            if (status != 0 && status != LUA_ERRSYNTAX)
            {
                dmLogError("Error preparing module '%s': %s", script_name, lua_tostring(L, -1));
                lua_pop(L, 1);
                if (new_path)
                    context->m_PathToModule.Erase(path_hash);
                return RESULT_LUA_ERROR;
            }
            chunk_ref = Ref(L, LUA_REGISTRYINDEX);
        }

        Module module;
        module.m_Name = strdup(script_name);

        module.m_ChunkRef = chunk_ref;
        module.m_Script = 0;
        module.m_ScriptSize = 0;
        if (loader == 0)
        {
            const char* buf;
            uint32_t size;
            GetLuaSource(source, &buf, &size);
            module.m_Script = (char*)malloc(size);
            module.m_ScriptSize = size;
            memcpy(module.m_Script, buf, size);
        }

        module.m_Resource = resource;
        module.m_Filename = loader == 0 ? strdup(source->m_Filename) : 0;

        if (context->m_Modules.Full())
        {
            context->m_Modules.SetCapacity(127, context->m_Modules.Capacity() + 128);
        }
        if (context->m_PathToModule.Full())
        {
            context->m_PathToModule.SetCapacity(127, context->m_PathToModule.Capacity() + 128);
        }

        existing_module = context->m_Modules.Get(module_hash);
        if (existing_module)
            FreeModuleCallback(context, &module_hash, existing_module);
        context->m_Modules.Put(module_hash, module);
        context->m_PathToModule.Put(path_hash, module_hash);

        return RESULT_OK;
    }

    Result ReloadModule(HContext context, dmLuaDDF::LuaSource *source, dmhash_t path_hash)
    {
        lua_State* L = GetLuaState(context);
        int top = lua_gettop(L);
        (void) top;

        dmhash_t* module_hash_ptr = context->m_PathToModule.Get(path_hash);
        if (module_hash_ptr == 0)
        {
            return RESULT_MODULE_NOT_LOADED;
        }
        dmhash_t module_hash = *module_hash_ptr;
        Module* module = context->m_Modules.Get(module_hash);
        if (module == 0)
        {
            return RESULT_MODULE_NOT_LOADED;
        }

        const char *buf;
        uint32_t size;
        GetLuaSource(source, &buf, &size);

        if (module->m_ChunkRef == LUA_NOREF)
        {
            module->m_ScriptSize = size;
            module->m_Script = (char*)realloc(module->m_Script, size);
            memcpy(module->m_Script, buf, size);
        }

        int chunk_ref = module->m_ChunkRef;
        bool loaded = LuaLoadModule(L, buf, size, module->m_Name, chunk_ref == LUA_NOREF ? 0 : &chunk_ref);
        module = context->m_Modules.Get(module_hash);
        module->m_ChunkRef = chunk_ref;
        if (loaded)
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

    static void FreeModuleCallback(HContext context, const uint64_t* key, Module* value)
    {
        if (value->m_ChunkRef != LUA_NOREF)
            Unref(GetLuaState(context), LUA_REGISTRYINDEX, value->m_ChunkRef);
        if (value->m_Resource != 0) {
            dmResource::Release(context->m_ResourceFactory, value->m_Resource);
        }
        free(value->m_Script);
        free(value->m_Name);
        free(value->m_Filename);
    }

    void ClearModules(HContext context)
    {
        context->m_Modules.Iterate(&FreeModuleCallback, context);
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
