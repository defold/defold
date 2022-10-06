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

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/pprint.h>
#include <dlib/profile.h>

#include "script_private.h"
#include "script_hash.h"
#include "script_msg.h"
#include "script_vmath.h"
#include "script_sys.h"
#include "script_module.h"
#include "script_image.h"
#include "script_json.h"
#include "script_http.h"
#include "script_zlib.h"
#include "script_html5.h"
#include "script_luasocket.h"
#include "script_bitop.h"
#include "script_timer.h"
#include "script_extensions.h"

extern "C"
{
#include <lua/lualib.h>
}

DM_PROPERTY_GROUP(rmtp_Script, "");

namespace dmScript
{
    /*# Built-ins API documentation
     *
     * Built-in scripting functions.
     *
     * @document
     * @name Built-ins
     * @namespace builtins
     */

    static const char INSTANCE_NAME[] = "__dm_script_instance__";
    static const uint32_t INSTANCE_NAME_HASH = dmHashBuffer32(INSTANCE_NAME, sizeof(INSTANCE_NAME) - 1);

    static const char SCRIPT_CONTEXT[] = "__script_context";
    static uint32_t SCRIPT_CONTEXT_HASH = 0;

    const char META_TABLE_RESOLVE_PATH[]             = "__resolve_path";
    const char META_TABLE_GET_URL[]                  = "__get_url";
    const char META_TABLE_GET_USER_DATA[]            = "__get_user_data";
    const char META_TABLE_IS_VALID[]                 = "__is_valid";
    const char META_GET_INSTANCE_CONTEXT_TABLE_REF[] = "__get_instance_context_table_ref";

    const char SCRIPT_METATABLE_TYPE_HASH_KEY_NAME[] = "__dmengine_type";
    static const uint32_t SCRIPT_METATABLE_TYPE_HASH_KEY = dmHashBufferNoReverse32(SCRIPT_METATABLE_TYPE_HASH_KEY_NAME, sizeof(SCRIPT_METATABLE_TYPE_HASH_KEY_NAME) - 1);

    // A debug value for profiling lua references
    int g_LuaReferenceCount = 0;

    HContext NewContext(dmConfigFile::HConfig config_file, dmResource::HFactory factory, bool enable_extensions)
    {
        Context* context = new Context();
        context->m_Modules.SetCapacity(127, 256);
        context->m_PathToModule.SetCapacity(127, 256);
        context->m_HashInstances.SetCapacity(443, 256);
        context->m_ScriptExtensions.SetCapacity(8);
        context->m_ConfigFile = config_file;
        context->m_ResourceFactory = factory;
        context->m_LuaState = lua_open();
        context->m_ContextTableRef = LUA_NOREF;
        context->m_EnableExtensions = enable_extensions;
        return context;
    }

    void DeleteContext(HContext context)
    {
        ClearModules(context);
        lua_close(context->m_LuaState);
        delete context;
    }

    int LuaPrint(lua_State* L);
    int LuaPPrint(lua_State* L);

#define RANDOM_SEED "__random_seed"

    static int Lua_Math_Random (lua_State *L)
    {
        // More or less from lmathlib.c
        DM_LUA_STACK_CHECK(L, 1);

        lua_getglobal(L, RANDOM_SEED);
        uint32_t* seed = (uint32_t*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        // NOTE: + 1 changed from original lua implementation
        // Otherwise upper + 1 when dmMath::Rand() returns DM_RAND_MAX
        // However no proof for correctness
        lua_Number r = (lua_Number)dmMath::Rand(seed) / (lua_Number)(DM_RAND_MAX + 1);
        switch (lua_gettop(L)) {
            case 0: {
                lua_pushnumber(L, r);
                break;
            }
            case 1: {
                int u = luaL_checkint(L, 1);
                luaL_argcheck(L, 1<=u, 1, "interval is empty");
                lua_pushnumber(L, floor(r*u)+1);  /* int between 1 and `u' */
                break;
            }
            case 2: {
                int l = luaL_checkint(L, 1);
                int u = luaL_checkint(L, 2);
                luaL_argcheck(L, l<=u, 2, "interval is empty");
                lua_pushnumber(L, floor(r*(u-l+1))+l);  /* int between `l' and `u' */
                break;
            }
            default:
                return DM_LUA_ERROR("wrong number of arguments");
        }
        return 1;
    }

    static int Lua_Math_Randomseed (lua_State *L)
    {
        // More or less from lmathlib.c
        DM_LUA_STACK_CHECK(L, 0);
        lua_getglobal(L, RANDOM_SEED);
        uint32_t* seed = (uint32_t*) lua_touserdata(L, -1);
        *seed = luaL_checkint(L, 1);
        // discard first value to avoid repeated values
        dmMath::Rand(seed);
        lua_pop(L, 1);
        return 0;
    }

    void Initialize(HContext context)
    {
        lua_State* L = context->m_LuaState;
        DM_LUA_STACK_CHECK(L, 0);

        luaL_openlibs(L);

        // To support 'math.mod' even though it's been deprecated in 5.1 and removed in 5.2
        lua_getglobal(L, "math");
        lua_getfield(L, -1, "fmod");
        lua_setfield(L, -2, "mod");
        lua_pop(L, 1);

        InitializeHash(L);
        InitializeMsg(L);
        InitializeVmath(L);
        InitializeSys(L);
        InitializeModule(L);
        InitializeImage(L);
        InitializeJson(L);
        InitializeZlib(L);
        InitializeHtml5(L);
        InitializeLuasocket(L);
        InitializeBitop(L);

        lua_register(L, "print", LuaPrint);
        lua_register(L, "pprint", LuaPPrint);

        lua_getglobal(L, "math");
        if (!lua_isnil(L, -1)) {
            uint32_t *seed = (uint32_t*) malloc(sizeof(uint32_t));
            *seed = 0;
            lua_pushlightuserdata(L, seed);
            lua_setglobal(L, RANDOM_SEED);
            // discard first value to avoid repeated values
            dmMath::Rand(seed);

            lua_pushcfunction(L, Lua_Math_Random);
            lua_setfield(L, -2, "random");

            lua_pushcfunction(L, Lua_Math_Randomseed);
            lua_setfield(L, -2, "randomseed");
        } else {
            dmLogWarning("math library not loaded")
        }

        lua_pop(L, 1);

        lua_pushlightuserdata(L, (void*)context);
        SCRIPT_CONTEXT_HASH = dmScript::SetGlobal(L, SCRIPT_CONTEXT);

        lua_pushlightuserdata(L, (void*)L);
        lua_setglobal(L, SCRIPT_MAIN_THREAD);

        lua_newtable(L);
        context->m_ContextTableRef = Ref(L, LUA_REGISTRYINDEX);

        InitializeHttp(context);
        InitializeTimer(context);
        if (context->m_EnableExtensions)
        {
            InitializeExtensions(context);
        }

        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->Initialize != 0x0)
            {
                (*l)->Initialize(context);
            }
        }
    }

    void RegisterScriptExtension(HContext context, HScriptExtension script_extension)
    {
        if (context->m_ScriptExtensions.Full())
        {
            context->m_ScriptExtensions.SetCapacity(context->m_ScriptExtensions.Capacity() + 8);
        }
        context->m_ScriptExtensions.Push(script_extension);
    }

    void Update(HContext context)
    {
        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->Update != 0x0)
            {
                (*l)->Update(context);
            }
        }
    }

    void Finalize(HContext context)
    {
        lua_State* L = context->m_LuaState;

        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->Finalize != 0x0)
            {
                (*l)->Finalize(context);
            }
        }

        lua_getglobal(L, RANDOM_SEED);
        uint32_t* seed = (uint32_t*) lua_touserdata(L, -1);
        free(seed);
        lua_pop(L, 1);

        Unref(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
    }

    lua_State* GetLuaState(HContext context)
    {
        if (context != 0x0) {
            return context->m_LuaState;
        }
        return 0x0;
    }

    // From ldblib.c (getthread)
    static lua_State* GetLuaThread(lua_State *L, int *arg)
    {
        if (lua_isthread(L, 1)) {
            *arg = 1;
            return lua_tothread(L, 1);
        }
        else {
            *arg = 0;
            return L;
        }
    }

    // From https://zeux.io/2010/11/07/lua-callstack-with-c-debugger/
    // and also
    // https://github.com/defold/defold/blob/dev/engine/lua/src/lua/ldblib.c#L321
    void GetLuaTraceback(lua_State* L, const char* infostring, void (*cbk)(lua_State* L, lua_Debug* entry, void* ctx), void* ctx)
    {
        const int LEVELS1 = 12;  // size of the first part of the stack
        const int LEVELS2 = 10;  // size of the second part of the stack

        int firstpart = 1;
        int arg = 0;
        lua_State* L1 = GetLuaThread(L, &arg);

        lua_Debug entry;
        int level = 0;

        if (lua_isnumber(L, arg+2)) {
            level = (int)lua_tointeger(L, arg+2);
            lua_pop(L, 1); // problematic?!
        }
        else {
            level = (L == L1) ? 1 : 0;  // level 0 may be this own function
        }

        if (lua_gettop(L) != arg && !lua_isstring(L, arg+1)) return;  // message is not a string

        while (lua_getstack(L1, level++, &entry))
        {
            if (level > LEVELS1 && firstpart) {
                // no more than `LEVELS2' more levels?
                if (!lua_getstack(L1, level+LEVELS2, &entry)) {
                    level--;  // keep going
                } else {
                    lua_pushliteral(L, "\n\t...");  // too many levels
                    while (lua_getstack(L1, level+LEVELS2, &entry))  // find last levels
                        level++;
                }
                firstpart = 0;
                continue;
            }
            int status = lua_getinfo(L1, infostring, &entry);
            if (!status)
                continue;

            cbk(L1, &entry, ctx);
        }
    }

    // From ldblib.c function db_errorfb()
    uint32_t WriteLuaTracebackEntry(lua_Debug* entry, char* buffer, uint32_t buffer_size)
    {
        int32_t nwritten = 0;
        if (*entry->namewhat != '\0')
        {
            nwritten = dmSnPrintf(buffer, buffer_size, "  %s:%d: in function %s\n", entry->short_src, entry->currentline, entry->name);
        }
        else if (*entry->what == 'm')
        {
            nwritten = dmSnPrintf(buffer, buffer_size, "  %s:%d: in main chunk\n", entry->short_src, entry->currentline);
        }
        else if (*entry->what == 'C' || *entry->what == 't')
        {
            nwritten = dmSnPrintf(buffer, buffer_size, "  %s:%d: ?\n", entry->short_src, entry->currentline);
        }
        else
        {
            nwritten = dmSnPrintf(buffer, buffer_size, "  %s:%d: in function <%s:%d>\n", entry->short_src, entry->currentline, entry->short_src, entry->linedefined);
        }

        // Truncated, so we'll just make sure we've not written anything
        if (nwritten < 0)
            nwritten = 0;

        return (uint32_t)nwritten;
    }


    dmConfigFile::HConfig GetConfigFile(HContext context)
    {
        if (context != 0x0)
        {
            return context->m_ConfigFile;
        }
        return 0x0;
    }

    int LuaPrint(lua_State* L)
    {
        int n = lua_gettop(L);
        lua_getglobal(L, "tostring");
        char buffer[dmLog::MAX_STRING_SIZE];
        buffer[0] = 0;
        for (int i = 1; i <= n; ++i)
        {
            const char *s;
            lua_pushvalue(L, -1);
            lua_pushvalue(L, i);
            lua_call(L, 1, 1);
            s = lua_tostring(L, -1);
            if (s == 0x0)
                return luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
            if (i > 1)
                dmStrlCat(buffer, "\t", sizeof(buffer));
            dmStrlCat(buffer, s, sizeof(buffer));
            lua_pop(L, 1);
        }
        dmLogUserDebug("%s", buffer);
        lua_pop(L, 1);
        assert(n == lua_gettop(L));
        return 0;
    }

    static const char* PushValueAsString(lua_State* L, int index)
    {
        lua_pushvalue(L, index);
        // [-1] value
        lua_getglobal(L, "tostring");
        // [-2] value
        // [-1] tostring()
        lua_insert(L, -2);
        // [-2] tostring()
        // [-1] value
        lua_call(L, 1, 1);
        // [-1] result
        const char* result = lua_tostring(L, -1);
        if (result == 0x0)
        {
            lua_pop(L, 1);
        }
        return result;
    }

    static int DoLuaPPrintTable(lua_State* L, int index, dmPPrint::Printer* printer, dmHashTable<uintptr_t, bool>& printed_tables) {
        DM_LUA_STACK_CHECK(L, 0);

        const void* table_data = (const void*)lua_topointer(L, index);

        if (printed_tables.Get((uintptr_t)table_data) != 0x0)
        {
            printer->Printf("{ ... } --[[%p]]", table_data);
            return 0;
        }

        if (printed_tables.Capacity() == printed_tables.Size())
        {
            uint32_t new_capacity = printed_tables.Capacity() + 10;
            printed_tables.SetCapacity((new_capacity * 2) / 3, new_capacity * 2);
        }
        printed_tables.Put((uintptr_t)table_data, true);

        lua_pushvalue(L, index);
        lua_pushnil(L);
        // [-2] table
        // [-1] key

        if(lua_next(L, -2) == 0)
        {
            // [-1] table
            printer->Printf("{ } --[[%p]]", table_data);
            lua_pop(L, 1);
            return 0;
        }

        // [-3] table
        // [-2] key
        // [-1] value
        printer->Printf("{ --[[%p]]", table_data);
        printer->Indent(2);

        bool is_first = true;
        do
        {
            printer->Printf("%s\n", is_first ? "" : ",");
            int value_type = lua_type(L, -1);

            const char* key_string = PushValueAsString(L, -2);
            if (key_string == 0x0)
            {
                return luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
            }
            // [-4] table
            // [-3] key
            // [-2] value
            // [-1] key name

            printer->Printf("%s = ", key_string);
            lua_pop(L, 1);
            // [-3] table
            // [-2] key
            // [-1] value

            if (value_type == LUA_TTABLE)
            {
                DoLuaPPrintTable(L, -1, printer, printed_tables);
            }
            else if (value_type == LUA_TSTRING)
            {
                printer->Printf("\"%s\"", lua_tostring(L, -1));
            }
            else
            {
                const char* value_string = PushValueAsString(L, -1);
                if (value_string == 0x0)
                {
                    return luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
                }
                // [-4] table
                // [-3] key
                // [-2] value
                // [-1] value name

                printer->Printf("%s", value_string);
                lua_pop(L, 1);
                // [-3] table
                // [-2] key
                // [-1] value
            }

            lua_pop(L, 1);
            // [-2] table
            // [-1] key
            is_first = false;
        } while (lua_next(L, -2) != 0);

        // [-1] table

        printer->Indent(-2);
        printer->Printf("\n");
        printer->Printf("}");

        printed_tables.Erase((uintptr_t)table_data);

        lua_pop(L, 1);
        return 0;
    }

    /*# pretty printing
     * Pretty printing of Lua values. This function prints Lua values
     * in a manner similar to +print()+, but will also recurse into tables
     * and pretty print them. There is a limit to how deep the function
     * will recurse.
     *
     * @name pprint
     * @param v [type:any] value to print
     * @examples
     *
     * Pretty printing a Lua table with a nested table:
     *
     * ```lua
     * local t2 = { 1, 2, 3, 4 }
     * local t = { key = "value", key2 = 1234, key3 = t2 }
     * pprint(t)
     * ```
     *
     * Resulting in the following output (note that the key order in non array
     * Lua tables is undefined):
     *
     * ```
     * {
     *   key3 = {
     *     1 = 1,
     *     2 = 2,
     *     3 = 3,
     *     4 = 4,
     *   }
     *   key2 = 1234,
     *   key = value,
     * }
     * ```
     */
    int LuaPPrint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int n = lua_gettop(L);

        char buf[dmLog::MAX_STRING_SIZE];
        dmPPrint::Printer printer(buf, sizeof(buf));
        dmHashTable<uintptr_t, bool> printed_tables;
        for (int s = 1; s <= n; ++s)
        {
            printed_tables.Clear();
            if (lua_type(L, s) == LUA_TTABLE)
            {
                if (s == 1)
                {
                    printer.Printf("\n");
                }
                DoLuaPPrintTable(L, s, &printer, printed_tables);
                printer.Printf("%s", (n > s) ? ",\n" : "");
            }
            else
            {
                const char* value_str = PushValueAsString(L, s);
                if (value_str == 0x0)
                {
                    return luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
                }
                printer.Printf("%s%s", value_str, (n > s) ? ",\n" : "");
                lua_pop(L, 1);
            }
        }

        dmLogUserDebug("%s", buf);
        return 0;
    }

    HContext GetScriptContext(lua_State* L)
    {
        GetGlobal(L, SCRIPT_CONTEXT_HASH);
        HContext context = (HContext)lua_touserdata(L, -1);
        lua_pop(L, 1);
        return context;
    }

    uint32_t SetGlobal(lua_State* L, const char* name)
    {
        size_t name_length = strlen(name);
        uint32_t name_hash = dmHashBuffer32(name, name_length);
        // [-1] instance

        lua_pushlstring(L, name, name_length);
        // [-1] name
        // [-2] instance

        lua_pushvalue(L, -2);
        // [-1] instance
        // [-2] name
        // [-3] instance

        lua_settable(L, LUA_GLOBALSINDEX);
        // [-1] instance

        lua_pushinteger(L, (lua_Integer)name_hash);
        // [-1] name_hash
        // [-2] instance

        lua_insert(L, -2);
        // [-1] instance
        // [-2] name_hash

        lua_settable(L, LUA_GLOBALSINDEX);

        return name_hash;
    }

    void GetGlobal(lua_State*L, uint32_t name_hash)
    {
        lua_pushinteger(L, (lua_Integer)name_hash);
        // [-1] name_hash

        lua_gettable(L, LUA_GLOBALSINDEX);
        // [-1] instance
    }

    void GetInstance(lua_State* L)
    {
        lua_pushinteger(L, (lua_Integer)INSTANCE_NAME_HASH);
        // [-1] name_hash

        lua_gettable(L, LUA_GLOBALSINDEX);
        // [-1] instance
    }

    void SetInstance(lua_State* L)
    {
        // [-1] instance

        lua_pushinteger(L, (lua_Integer)INSTANCE_NAME_HASH);
        // [-1] name_hash
        // [-2] instance

        lua_insert(L, -2);
        // [-1] instance
        // [-2] name_hash

        lua_settable(L, LUA_GLOBALSINDEX);
    }

    bool IsInstanceValid(lua_State* L)
    {
        return IsValidInstance(L);
    }

    lua_State* GetMainThread(lua_State* L)
    {
        lua_getglobal(L, SCRIPT_MAIN_THREAD);
        lua_State* main_thread = (lua_State*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        return main_thread;
    }

    uint32_t SetUserType(lua_State* L, int meta_table_index, const char* name)
    {
        DM_LUA_STACK_CHECK(L, 0);

        uint32_t type_hash = dmHashBuffer32(name, strlen(name));

        lua_pushvalue(L, meta_table_index);
        // [-1] meta table

        lua_pushinteger(L, SCRIPT_METATABLE_TYPE_HASH_KEY);
        // [-1] SCRIPT_METATABLE_TYPE_HASH_KEY
        // [-2] meta table

        lua_pushinteger(L, (lua_Integer)type_hash);
        // [-1] type_hash
        // [-2] SCRIPT_METATABLE_TYPE_HASH_KEY
        // [-3] meta table

        lua_settable(L, -3);
        // [-1] meta table

        lua_pop(L, 1);

        return type_hash;
    }

    uint32_t RegisterUserType(lua_State* L, const char* name, const luaL_reg methods[], const luaL_reg meta[]) {
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, name, methods);   // create methods table, add it to the globals
        int methods_idx = lua_gettop(L);
        luaL_newmetatable(L, name);                         // create metatable for ScriptInstance, add it to the Lua registry

        uint32_t type_hash = SetUserType(L, -1, name);

        int metatable_idx = lua_gettop(L);
        luaL_register(L, 0, meta);                   // fill metatable

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, methods_idx);                       // dup methods table
        lua_settable(L, metatable_idx);
        lua_pop(L, 2);

        return type_hash;
    }

    uint32_t GetUserType(lua_State* L, int user_data_index)
    {
        DM_LUA_STACK_CHECK(L, 0);
        lua_pushvalue(L, user_data_index);
        // [-1] user data
        lua_Integer type_hash = 0;
        if (lua_type(L, -1) == LUA_TUSERDATA)
        {
            if (lua_getmetatable(L, -1))
            {
                // [-1] meta table
                // [-2] user data

                lua_pushinteger(L, SCRIPT_METATABLE_TYPE_HASH_KEY);
                // [-1] SCRIPT_METATABLE_TYPE_HASH_KEY
                // [-2] meta table
                // [-3] user data

                lua_rawget(L, -2);
                // [-1] type hash
                // [-2] meta table
                // [-3] user data

                type_hash = lua_tointeger(L, -1);
                lua_pop(L, 2);
                // [-1] user data
            }
        }
        lua_pop(L, 1);
        return (uint32_t)type_hash;
    }

    void* ToUserType(lua_State* L, int user_data_index, uint32_t type_hash)
    {
        if (GetUserType(L, user_data_index) == type_hash)
        {
            return lua_touserdata(L, user_data_index);
        }
        return 0;
    }

    void* CheckUserType(lua_State* L, int user_data_index, uint32_t type_hash, const char* error_message)
    {
        void* result = ToUserType(L, user_data_index, type_hash);
        if (result == 0)
        {
            if (error_message == 0x0) {
                const char* type = (const char*)dmHashReverse32(type_hash, 0);
                luaL_typerror(L, user_data_index, type);
            }
            else {
                luaL_error(L, "%s", error_message);
            }
        }
        return result;
    }

    static bool GetMetaFunction(lua_State* L, int index, const char* meta_table_key, size_t meta_table_key_length) {
        if (lua_getmetatable(L, index)) {
            lua_pushlstring(L, meta_table_key, meta_table_key_length);
            lua_rawget(L, -2);
            lua_remove(L, -2);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                return false;
            } else {
                return true;
            }
        }
        return false;
    }

    bool ResolvePath(lua_State* L, const char* path, uint32_t path_size, dmhash_t& out_hash) {
        DM_LUA_STACK_CHECK(L, 0);
        GetInstance(L);
        if (GetMetaFunction(L, -1, META_TABLE_RESOLVE_PATH, sizeof(META_TABLE_RESOLVE_PATH) - 1)) {
            lua_pushvalue(L, -2);
            lua_pushlstring(L, path, path_size);
            lua_call(L, 2, 1);
            out_hash = CheckHash(L, -1);
            lua_pop(L, 2);
            return true;
        }
        lua_pop(L, 1);
        return false;
    }

    bool GetURL(lua_State* L, dmMessage::URL& out_url) {
        DM_LUA_STACK_CHECK(L, 0);
        GetInstance(L);

        int res = luaL_callmeta(L, -1, META_TABLE_GET_URL);
        if (res != 1)
        {
            lua_pop(L, 1);
            return false;
        }
        // We either get the URL or not, if the return value is
        // non-null it must be an URL or we have done bad coding
        // on our end and did not follow the contract of META_TABLE_GET_URL
        dmMessage::URL* url = (dmMessage::URL*)lua_touserdata(L, -1);
        if (url)
        {
            out_url = *url;
            lua_pop(L, 2);
            return true;
        }

        // If the URL is null, we call CheckURL to trigger a proper
        // lua type error.
        (void)CheckURL(L, -1);
        return false;
    }

    bool GetUserData(lua_State* L, uintptr_t* out_user_data, uint32_t user_type_hash) {
        DM_LUA_STACK_CHECK(L, 0);

        GetInstance(L);
        // [-1] instance

        if (lua_type(L, -1) != LUA_TUSERDATA)
        {
            lua_pop(L, 1);
            return false;
        }

        if (!lua_getmetatable(L, -1))
        {
            lua_pop(L, 1);
            return false;
        }
        // [-1] meta table
        // [-2] instance

        lua_pushinteger(L, SCRIPT_METATABLE_TYPE_HASH_KEY);
        // [-1] SCRIPT_METATABLE_TYPE_HASH_KEY
        // [-2] meta table
        // [-3] instance

        lua_rawget(L, -2);
        // [-1] type hash
        // [-2] meta table
        // [-3] instance

        if (lua_tointeger(L, -1) != user_type_hash)
        {
            lua_pop(L, 3);
            return false;
        }

        lua_pop(L, 1);
        // [-1] meta table
        // [-2] instance

        lua_pushlstring(L, META_TABLE_GET_USER_DATA, sizeof(META_TABLE_GET_USER_DATA) - 1);
        // [-1] META_TABLE_GET_USER_DATA
        // [-2] meta table
        // [-3] instance

        lua_rawget(L, -2);
        // [-1] get_user_data method
        // [-2] meta table
        // [-3] instance

        if (lua_isnil(L, -1)) {
            lua_pop(L, 3);
            return false;
        }

        lua_pushvalue(L, -3);
        // [-1] instance
        // [-2] get_user_data
        // [-3] meta table
        // [-4] instance

        lua_call(L, 1, 1);
        // [-1] user data
        // [-2] meta table
        // [-3] instance

        *out_user_data = (uintptr_t)lua_touserdata(L, -1);
        lua_pop(L, 3);
        return true;
    }

    bool IsValidInstance(lua_State* L) {
        int top = lua_gettop(L);
        (void)top;
        GetInstance(L);
        if (GetMetaFunction(L, -1, META_TABLE_IS_VALID, sizeof(META_TABLE_IS_VALID) - 1)) {
            lua_pushvalue(L, -2);
            lua_call(L, 1, 1);
            assert(top + 2 == lua_gettop(L));
            bool result = lua_toboolean(L, -1);
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return result;
        }
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return false;
    }

    void SetContextValue(HContext context)
    {
        assert(context != 0x0);
        lua_State* L = context->m_LuaState;

        DM_LUA_STACK_CHECK(L, -2);

        lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-3] key
        // [-2] value
        // [-1] context table

        assert(lua_type(L, -1) == LUA_TTABLE);

        lua_insert(L, -3);
        // [-3] context table
        // [-2] key
        // [-1] value

        lua_settable(L, -3);
        // [-1] context table

        lua_pop(L, 1);
    }

    void GetContextValue(HContext context)
    {
        assert(context != 0x0);
        lua_State* L = context->m_LuaState;

        DM_LUA_STACK_CHECK(L, 0);

        lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-2] key
        // [-1] context table

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 2);
            lua_pushnil(L);
            // [-1] LUA_NIL
            return;
        }

        lua_insert(L, -2);
        // [-2] context table
        // [-1] key
        lua_gettable(L, -2);
        // [-2] context table
        // [-1] value

        lua_remove(L, -2);
        // [-1] value
    }

    static void GetInstanceContextTable(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        GetInstance(L);
        // [-1] instance

        if (!GetMetaFunction(L, -1, META_GET_INSTANCE_CONTEXT_TABLE_REF, sizeof(META_GET_INSTANCE_CONTEXT_TABLE_REF) - 1))
        {
            lua_pop(L, 1);
            lua_pushnil(L);
            return;
        }
        // [-2] instance
        // [-1] META_GET_INSTANCE_CONTEXT_TABLE_REF()

        lua_insert(L, -2);
        // [-2] META_GET_INSTANCE_CONTEXT_TABLE_REF()
        // [-1] instance

        lua_call(L, 1, 1);
        // [-1] instance context table ref or LUA_NOREF
        assert(lua_type(L, -1) == LUA_TNUMBER);

        int context_table_ref = lua_tonumber(L, -1);
        lua_pop(L, 1);

        if (context_table_ref == LUA_NOREF)
        {
            lua_pushnil(L);
            // [-1] LUA_NIL
            return;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, context_table_ref);
        // [-1] instance context table
    }

    uintptr_t GetInstanceId(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        GetInstance(L);
        int instance_type = lua_type(L, -1);
        // We assume that all users of SetInstance puts some form of user data/light user data, it is an assumption that works for now
        uintptr_t id = (instance_type == LUA_TLIGHTUSERDATA || instance_type == LUA_TUSERDATA) ? (uintptr_t)lua_touserdata(L, -1) : 0;
        lua_pop(L, 1);
        return id;
    }

    struct ScriptWorld
    {
        HContext m_Context;
        int      m_WorldContextTableRef;
    };

    HContext GetScriptWorldContext(HScriptWorld script_world)
    {
        return script_world == 0x0 ? 0x0 : script_world->m_Context;
    }

    void SetScriptWorldContextValue(HScriptWorld script_world)
    {
        lua_State* L = script_world->m_Context->m_LuaState;
        lua_rawgeti(L, LUA_REGISTRYINDEX, script_world->m_WorldContextTableRef);
        // [-3] key
        // [-2] value
        // [-1] context table

        lua_insert(L, -3);
        // [-3] context table
        // [-2] key
        // [-1] value

        lua_settable(L, -3);
        // [-1] context table

        lua_pop(L, 1);
    }

    void GetScriptWorldContextValue(HScriptWorld script_world)
    {
        lua_State* L = script_world->m_Context->m_LuaState;
        lua_rawgeti(L, LUA_REGISTRYINDEX, script_world->m_WorldContextTableRef);
        // [-2] key
        // [-1] context table

        lua_insert(L, -2);
        // [-2] context table
        // [-1] key

        lua_gettable(L, -2);
        // [-2] context table
        // [-1] value

        lua_insert(L, -2);
        // [-2] value
        // [-1] context table

        lua_pop(L, 1);
        // [-1] value
    }

    HScriptWorld NewScriptWorld(HContext context)
    {
        HScriptWorld script_world = (ScriptWorld*)malloc(sizeof(ScriptWorld));
        assert(script_world != 0x0);
        script_world->m_Context = context;
        lua_State* L = script_world->m_Context->m_LuaState;
        lua_newtable(L);
        script_world->m_WorldContextTableRef = Ref(L, LUA_REGISTRYINDEX);
        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->NewScriptWorld != 0x0)
            {
                (*l)->NewScriptWorld(script_world);
            }
        }
        return script_world;
    }

    void DeleteScriptWorld(HScriptWorld script_world)
    {
        assert(script_world != 0x0);
        HContext context = GetScriptWorldContext(script_world);
        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->DeleteScriptWorld != 0x0)
            {
                (*l)->DeleteScriptWorld(script_world);
            }
        }
        lua_State* L = script_world->m_Context->m_LuaState;
        Unref(L, LUA_REGISTRYINDEX, script_world->m_WorldContextTableRef);

        free(script_world);
    }

    void UpdateScriptWorld(HScriptWorld script_world, float dt)
    {
        if (script_world == 0x0)
        {
            return;
        }
        HContext context = GetScriptWorldContext(script_world);
        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->UpdateScriptWorld != 0x0)
            {
                (*l)->UpdateScriptWorld(script_world, dt);
            }
        }
    }

    void FixedUpdateScriptWorld(HScriptWorld script_world, float dt)
    {
        if (script_world == 0x0)
        {
            return;
        }
        HContext context = GetScriptWorldContext(script_world);
        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->FixedUpdateScriptWorld != 0x0)
            {
                (*l)->FixedUpdateScriptWorld(script_world, dt);
            }
        }
    }

    void InitializeInstance(HScriptWorld script_world)
    {
        if (script_world == 0x0)
        {
            return;
        }
        HContext context = GetScriptWorldContext(script_world);
        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->InitializeScriptInstance != 0x0)
            {
                (*l)->InitializeScriptInstance(script_world);
            }
        }
    }

    void FinalizeInstance(HScriptWorld script_world)
    {
        if (script_world == 0x0)
        {
            return;
        }
        HContext context = GetScriptWorldContext(script_world);
        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->FinalizeScriptInstance != 0x0)
            {
                (*l)->FinalizeScriptInstance(script_world);
            }
        }
    }

    bool SetInstanceContextValue(lua_State* L)
    {
        // [-2] key
        // [-1] value

        DM_LUA_STACK_CHECK(L, -2);

        GetInstanceContextTable(L);
        // [-3] key
        // [-2] value
        // [-1] instance context table or LUA_NIL

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 3);
            return false;
        }
        // [-3] key
        // [-2] value
        // [-1] instance context table

        lua_insert(L, -3);
        // [-3] instance context table
        // [-2] key
        // [-1] value

        lua_settable(L, -3);
        // [-1] instance context table

        lua_pop(L, 1);
        return true;
    }

    void GetInstanceContextValue(lua_State* L)
    {
        // [-1] key
        DM_LUA_STACK_CHECK(L, 0);

        GetInstanceContextTable(L);
        // [-2] key
        // [-1] instance context table or LUA_NIL

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 2);

            lua_pushnil(L);
            // [-1] LUA_NIL
            return;
        }
        // [-2] key
        // [-1] instance context table

        lua_insert(L, -2);
        // [-2] instance context table
        // [-1] key

        lua_gettable(L, -2);
        // [-2] instance context table
        // [-1] value

        lua_insert(L, -2);
        // [-2] value
        // [-1] instance context table

        lua_pop(L, 1);
        // [-1] value
    }

    int RefInInstance(lua_State* L)
    {
        // [-1] value
        DM_LUA_STACK_CHECK(L, -1);

        GetInstanceContextTable(L);
        // [-2] value
        // [-1] instance context table or LUA_NIL

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            // [-2] value
            // [-1] LUA_NIL

            lua_pop(L, 2);
            return LUA_NOREF;
        }
        // [-2] value
        // [-1] instance context table

        lua_insert(L, -2);
        // [-2] instance context table
        // [-1] value

        int instance_ref = luaL_ref(L, -2);
        // [-1] instance context table

        lua_pop(L, 1);

        return instance_ref;
    }

    void UnrefInInstance(lua_State* L, int ref)
    {
        DM_LUA_STACK_CHECK(L, 0);

        GetInstanceContextTable(L);
        // [-1] instance context table or LUA_NIL

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            // [-1] LUA_NIL

            lua_pop(L, 1);
            return;
        }
        // [-1] instance context table

        luaL_unref(L, -1, ref);
        // [-1] instance context table

        lua_pop(L, 1);
    }

    void ResolveInInstance(lua_State* L, int ref)
    {
        DM_LUA_STACK_CHECK(L, 1);

        GetInstanceContextTable(L);
        // [-1] instance context table or LUA_NIL

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 1);

            lua_pushnil(L);
            // [-1] LUA_NIL
            return;
        }
        // [-1] instance context table

        lua_rawgeti(L, -1, ref);
        // [-2] instance context table
        // [-1] value

        lua_insert(L, -2);
        // [-2] value
        // [-1] instance context table

        lua_pop(L, 1);
        // [-1] value
    }


    struct LuaCallstackCtx
    {
        bool     m_First;
        char*    m_Buffer;
        uint32_t m_BufferSize;
    };

    static void GetLuaStackTraceCbk(lua_State* L, lua_Debug* entry, void* _ctx)
    {
        LuaCallstackCtx* ctx = (LuaCallstackCtx*)_ctx;

        if (ctx->m_First)
        {
            int32_t nwritten = dmSnPrintf(ctx->m_Buffer, ctx->m_BufferSize, "stack traceback:\n");
            if (nwritten < 0)
                nwritten = 0;
            ctx->m_Buffer += nwritten;
            ctx->m_BufferSize -= nwritten;
            ctx->m_First = false;
        }

        uint32_t nwritten = dmScript::WriteLuaTracebackEntry(entry, ctx->m_Buffer, ctx->m_BufferSize);
        ctx->m_Buffer += nwritten;
        ctx->m_BufferSize -= nwritten;
    }

    static int BacktraceErrorHandler(lua_State *m_state) {
        if (!lua_isstring(m_state, 1))
            return 1;

        lua_createtable(m_state, 0, 2);
        lua_pushvalue(m_state, 1);
        lua_setfield(m_state, -2, "error");

        char traceback[1024];
        LuaCallstackCtx ctx;
        ctx.m_First = true;
        ctx.m_Buffer = traceback;
        ctx.m_BufferSize = sizeof(traceback);
        dmScript::GetLuaTraceback(m_state, "Sln", GetLuaStackTraceCbk, &ctx);
        lua_pushstring(m_state, traceback);
        lua_setfield(m_state, -2, "traceback");

        return 1;
    }

    static int PCallInternal(lua_State* L, int nargs, int nresult, int in_error_handler) {
        lua_pushcfunction(L, BacktraceErrorHandler);
        int err_index = lua_gettop(L) - nargs - 1;
        lua_insert(L, err_index);
        int result = lua_pcall(L, nargs, nresult, err_index);
        lua_remove(L, err_index);
        if (result == LUA_ERRMEM) {
            lua_pop(L, 1);  // Pop BacktraceErrorHandler since it will not be called on OOM
            dmLogError("Lua memory allocation error.");
        } else if (result != 0) {
            // extract the individual fields for printing and passing
            lua_getfield(L, -1, "error");
            lua_getfield(L, -2, "traceback");
            // if handling error that happened during the error handling, print it and clean up and exit
            if (in_error_handler) {
                dmLogError("In error handler: %s%s", lua_tostring(L, -2), lua_tostring(L, -1));
                lua_pop(L, 3);
                return result;
            }
            // print before calling the error handler
            dmLogError("%s\n%s", lua_tostring(L, -2), lua_tostring(L, -1));
            lua_getfield(L, LUA_GLOBALSINDEX, "debug");
            if (lua_istable(L, -1)) {
                lua_pushstring(L, SCRIPT_ERROR_HANDLER_VAR);
                lua_rawget(L, -2);
                if (lua_isfunction(L, -1)) {
                    lua_pushlstring(L, "lua", 3); // 1st arg: source = 'lua'
                    lua_pushvalue(L, -5);     // 2nd arg: error
                    lua_pushvalue(L, -5);     // 3rd arg: traceback
                    PCallInternal(L, 3, 0, 1);
                } else {
                    if (!lua_isnil(L, -1)) {
                        dmLogError("Registered error handler is not a function");
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 4); // debug value, traceback, error, table
        }
        return result;
    }

    int PCall(lua_State* L, int nargs, int nresult) {
        return PCallInternal(L, nargs, nresult, 0);
    }

    int Ref(lua_State* L, int table)
    {
        ++g_LuaReferenceCount;
        return luaL_ref(L, table);
    }

    void Unref(lua_State* L, int table, int reference)
    {
        if (reference == LUA_NOREF)
        {
            return;
        }
        if (g_LuaReferenceCount <= 0)
        {
            dmLogError("Unbalanced number of Lua refs - possibly double calls to dmScript::Unref");
        }
        --g_LuaReferenceCount;
        luaL_unref(L, table, reference);
    }

    int GetLuaRefCount()
    {
        return g_LuaReferenceCount;
    }

    void ClearLuaRefCount()
    {
        g_LuaReferenceCount = 0;
    }

    uint32_t GetLuaGCCount(lua_State* L)
    {
        return (uint32_t)lua_gc(L, LUA_GCCOUNT, 0);
    }

    LuaStackCheck::LuaStackCheck(lua_State* L, int diff, const char* filename, int linenumber) : m_L(L), m_Filename(filename), m_Linenumber(linenumber), m_Top(lua_gettop(L)), m_Diff(diff)
    {
        if (!(m_Diff >= -m_Top)) {
            dmLogError("%s:%d: LuaStackCheck: m_Diff >= -m_Top == false (m_Diff: %d, m_Top: %d)", m_Filename, m_Linenumber, m_Diff, m_Top);
        }
        assert(m_Diff >= -m_Top);
    }

    int LuaStackCheck::Error(const char* fmt, ... )
    {
        Verify(0);
        va_list argp;
        va_start(argp, fmt);
        luaL_where(m_L, 1);
        lua_pushvfstring(m_L, fmt, argp);
        va_end(argp);
        lua_concat(m_L, 2);
        m_Diff = -0x800000;
        return lua_error(m_L);
    }

    void LuaStackCheck::Verify(int diff)
    {
        int32_t expected = m_Top + diff;
        int32_t actual = lua_gettop(m_L);
        if (expected != actual)
        {
            dmLogError("%s:%d: LuaStackCheck: Unbalanced Lua stack, expected (%d), actual (%d)", m_Filename, m_Linenumber, expected, actual);
            assert(expected == actual);
        }
    }

    LuaStackCheck::~LuaStackCheck()
    {
        if (m_Diff != -0x800000) {
            Verify(m_Diff);
        }
    }

    struct LuaCallbackInfo
    {
        LuaCallbackInfo() : m_L(0), m_ContextTableRef(LUA_NOREF), m_Callback(LUA_NOREF), m_Self(LUA_NOREF) {}
        lua_State* m_L;
        int        m_ContextTableRef;
        int        m_CallbackInfoRef;
        int        m_Callback;
        int        m_Self;
    };

    // static void printStack(lua_State* L)
    // {
    //     int top = lua_gettop(L);
    //     int bottom = 1;
    //     lua_getglobal(L, "tostring");
    //     for(int i = top; i >= bottom; i--)
    //     {
    //         lua_pushvalue(L, -1);
    //         lua_pushvalue(L, i);
    //         lua_pcall(L, 1, 1, 0);
    //         const char *str = lua_tostring(L, -1);
    //         if (str) {
    //             printf("%2d: %s\n", i, str);
    //         }else{
    //             printf("%2d: %s\n", i, luaL_typename(L, i));
    //         }
    //         lua_pop(L, 1);
    //     }
    //     lua_pop(L, 1);
    // }

    LuaCallbackInfo* CreateCallback(lua_State* L, int callback_stack_index)
    {
        luaL_checktype(L, callback_stack_index, LUA_TFUNCTION);

        DM_LUA_STACK_CHECK(L, 0);

        GetInstance(L);
        // [-1] instance

        if (!GetMetaFunction(L, -1, META_GET_INSTANCE_CONTEXT_TABLE_REF, sizeof(META_GET_INSTANCE_CONTEXT_TABLE_REF) - 1)) {
            lua_pop(L, 1);
            return 0x0;
        }
        // [-2] instance
        // [-1] META_GET_INSTANCE_CONTEXT_TABLE_REF()

        lua_pushvalue(L, -2);
        // [-3] instance
        // [-2] META_GET_INSTANCE_CONTEXT_TABLE_REF()
        // [-1] instance

        lua_call(L, 1, 1);
        // [-2] instance
        // [-1] instance context table ref
        assert(lua_type(L, -1) == LUA_TNUMBER);

        int context_table_ref = lua_tonumber(L, -1);
        lua_pop(L, 2);

        lua_pushvalue(L, callback_stack_index);
        // [-1] callback

        lua_rawgeti(L, LUA_REGISTRYINDEX, context_table_ref);
        // [-2] callback
        // [-1] context table
        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 2);
            return 0x0;
        }

        lua_insert(L, -2);
        // [-2] context table
        // [-1] callback

        LuaCallbackInfo* cbk = (LuaCallbackInfo*)lua_newuserdata(L, sizeof(LuaCallbackInfo));
        // [-3] context table
        // [-2] callback
        // [-1] LuaCallbackInfo

        cbk->m_L = GetMainThread(L);
        cbk->m_ContextTableRef = context_table_ref;

        // For the callback ref (that can actually outlive the script instance)
        // we want to add to the lua debug count
        cbk->m_CallbackInfoRef = dmScript::Ref(L, LUA_REGISTRYINDEX);
        // [-2] context table
        // [-1] callback

        // We do not use dmScript::Unref for refs in the context local table as we don't
        // want to count those refs the ref debug count shown in the profiler

        cbk->m_Callback = luaL_ref(L, -2);
        // [-1] context table

        GetInstance(L);
        // [-1] context table
        // [-2] instance

        cbk->m_Self = luaL_ref(L, -2);
        // [-1] context table

        lua_pop(L, 1);

        return cbk;
     }

    bool IsCallbackValid(LuaCallbackInfo* cbk)
    {
        if (cbk == NULL ||
            cbk->m_L == NULL ||
            cbk->m_ContextTableRef == LUA_NOREF ||
            cbk->m_CallbackInfoRef == LUA_NOREF ||
            cbk->m_Callback == LUA_NOREF ||
            cbk->m_Self == LUA_NOREF) {
            return false;
        }
        return true;
    }

    lua_State* GetCallbackLuaContext(LuaCallbackInfo* cbk)
    {
        return cbk ? cbk->m_L : 0;
    }

    void DestroyCallback(LuaCallbackInfo* cbk)
    {
        lua_State* L = cbk->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        if(cbk->m_ContextTableRef != LUA_NOREF)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_ContextTableRef);
            if (lua_type(L, -1) == LUA_TTABLE)
            {
                // We do not use dmScript::Unref for refs in the context local table as we don't
                // want to count those refs the ref debug count shown in the profiler
                luaL_unref(L, -1, cbk->m_Self);
                luaL_unref(L, -1, cbk->m_Callback);

                // For the callback (that can actually outlive the script instance)
                // we want to add to the lua debug count
                dmScript::Unref(L, LUA_REGISTRYINDEX, cbk->m_CallbackInfoRef);
            }
            cbk->m_Self = LUA_NOREF;
            cbk->m_Callback = LUA_NOREF;
            cbk->m_CallbackInfoRef = LUA_NOREF;
            cbk->m_ContextTableRef = LUA_NOREF;

            lua_pop(L, 1);
            return;
        }
        else
        {
            dmLogWarning("Failed to unregister callback (it was not registered)");
        }
    }

    bool SetupCallback(LuaCallbackInfo* cbk)
    {
        lua_State* L = cbk->m_L;
        int top = lua_gettop(L);
        if(cbk->m_CallbackInfoRef == LUA_NOREF)
        {
            dmLogWarning("Failed to invoke callback (it was not registered)");
            assert(top == lua_gettop(L));
            return false;
        }

        GetInstance(L);
        // [-1] old instance

        lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_ContextTableRef);
        // [-2] old instance
        // [-1] context table

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return false;
        }

        const int context_table_stack_index = lua_gettop(L);

        lua_rawgeti(L, context_table_stack_index, cbk->m_Callback);
        // [-3] old instance
        // [-2] context table
        // [-1] callback

        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            lua_pop(L, 3);
            assert(top == lua_gettop(L));
            return false;
        }

        lua_rawgeti(L, context_table_stack_index, cbk->m_Self); // Setup self (the script instance)
        // [-4] old instance
        // [-3] context table
        // [-2] callback
        // [-1] self

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 4);
            assert(top == lua_gettop(L));
            return false;
        }

        lua_pushvalue(L, -1);
        // [-5] old instance
        // [-4] context table
        // [-3] callback
        // [-2] self
        // [-1] self

        SetInstance(L);
        // [-4] old instance
        // [-3] context table
        // [-2] callback
        // [-1] self

        if (!IsInstanceValid(L))
        {
            lua_pop(L, 3);
            // [-1] old instance

            SetInstance(L);
            assert(top == lua_gettop(L));
            return false;
        }

        assert((top + 4) == lua_gettop(L));
        return true;
    }

    void TeardownCallback(LuaCallbackInfo* cbk)
    {
        lua_State* L = cbk->m_L;
        // [-2] old instance
        // [-1] context table
        lua_pop(L, 1);

        SetInstance(L);
    }

    bool InvokeCallback(LuaCallbackInfo* cbk, LuaCallbackUserFn fn, void* user_context)
    {
        lua_State* L = cbk->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        if (!SetupCallback(cbk)) {
            return false;
        }
        // [-4] old instance
        // [-3] context table
        // [-2] callback
        // [-1] self

        int user_args_start = lua_gettop(L);

        if (fn)
            fn(L, user_context);

        int user_args_end = lua_gettop(L);

        int number_of_arguments = 1 + user_args_end - user_args_start; // instance + number of arguments that the user pushed

        int ret;
        {
            char buffer[128];
            const char* profiler_string = GetProfilerString(L, -(number_of_arguments + 1), "?", "on_timer", 0, buffer, sizeof(buffer)); // TODO: Why "on_timer" ???
            DM_PROFILE_DYN(profiler_string, 0);

            ret = PCall(L, number_of_arguments, 0);
        }

        // [-2] old instance
        // [-1] context table
        TeardownCallback(cbk);

        return ret != 0 ? false : true;
    }

    /** Information about a function, in which file and at what line it is defined
     * Use with GetLuaFunctionRefInfo
     */
    struct LuaFunctionInfo
    {
        const char* m_FileName;
        const char* m_OptionalName;
        int m_LineNumber;
    };

    /**
     * Get information about where a Lua function is defined
     * @param L lua state
     * @param stack_index which index on the stack that contains the lua function ref
     * @param out_function_info pointer to the function information that is filled out
     * @return true on success, out_function_info is only touched if the function succeeds
     */
    static bool GetLuaFunctionRefInfo(lua_State* L, int stack_index, LuaFunctionInfo* out_function_info)
    {
        lua_Debug ar;
        lua_pushvalue(L, stack_index);
        if (lua_getinfo(L, ">Sn", &ar))
        {
            out_function_info->m_FileName = &ar.source[1];  // Skip source prefix character
            out_function_info->m_LineNumber = ar.linedefined;
            out_function_info->m_OptionalName = ar.name;
            return true;
        }
        return false;
    }

    // Fast length limited string concatenation that assume we already point to
    // the end of the string. Returns the new end of the string so we do not need
    // to calculate the length of the input string or output string
    static char* ConcatString(char* w_ptr, const char* w_ptr_end, const char* str)
    {
        while ((w_ptr != w_ptr_end) && str && *str)
        {
            *w_ptr++ = *str++;
        }
        return w_ptr;
    }

    /**
    * To reduce the overhead of the profiler when calling lua functions we avoid using dmSnPrintf.
    * dmSnPrintf uses vsnprintf with variable number of arguments but we only need string concatenation for
    * the most part. Also, we use our knownledge when building the string to get the string length directly
    * without resorting to strlen.
    * Building this string is particularly expensive on low end devices and using this more optimal way reduces
    * the overhead of the profiler when enabled.
    */
    const char* GetProfilerString(lua_State* L, int optional_callback_index, const char* source_file_name, const char* function_name, const char* optional_message_name, char* buffer, uint32_t buffer_size)
    {
        if (!dmProfile::IsInitialized())
            return 0;

        char* w_ptr = buffer;
        const char* w_ptr_end = buffer + buffer_size - 1;

        const char* function_source = source_file_name;

        if (optional_callback_index != 0)
        {
            LuaFunctionInfo fi;
            if (dmScript::GetLuaFunctionRefInfo(L, optional_callback_index, &fi))
            {
                if (fi.m_FileName)
                {
                    function_source = fi.m_FileName;
                }
                if (fi.m_OptionalName)
                {
                    w_ptr = ConcatString(w_ptr, w_ptr_end, fi.m_OptionalName);
                }
                else
                {
                    char function_line_number_buffer[16];
                    dmSnPrintf(function_line_number_buffer, sizeof(function_line_number_buffer), "l(%d)", fi.m_LineNumber);
                    w_ptr = ConcatString(w_ptr, w_ptr_end, function_line_number_buffer);
                }
            }
            else
            {
                w_ptr = ConcatString(w_ptr, w_ptr_end, "<unknown>");
            }

        }
        else
        {
            w_ptr = ConcatString(w_ptr, w_ptr_end, function_name);
        }

        if (optional_message_name)
        {
            w_ptr = ConcatString(w_ptr, w_ptr_end, "[");
            w_ptr = ConcatString(w_ptr, w_ptr_end, optional_message_name);
            w_ptr = ConcatString(w_ptr, w_ptr_end, "]");
        }
        w_ptr = ConcatString(w_ptr, w_ptr_end, "@");
        w_ptr = ConcatString(w_ptr, w_ptr_end, function_source);
        *w_ptr++ = 0;

        return buffer;
    }

    const char* GetTableStringValue(lua_State* L, int table_index, const char* key, const char* default_value)
    {
        DM_LUA_STACK_CHECK(L, 0);
        const char* r = default_value;

        lua_getfield(L, table_index, key);
        if (!lua_isnil(L, -1)) {

            int actual_lua_type = lua_type(L, -1);
            if (actual_lua_type != LUA_TSTRING) {
                dmLogError("Lua conversion expected table key '%s' to be a string but got %s",
                    key, lua_typename(L, actual_lua_type));
            } else {
                r = lua_tostring(L, -1);
            }

        }
        lua_pop(L, 1);
        return r;
    }

    int GetTableIntValue(lua_State* L, int table_index, const char* key, int default_value)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int r = default_value;

        lua_getfield(L, table_index, key);
        if (!lua_isnil(L, -1)) {

            int actual_lua_type = lua_type(L, -1);
            if (actual_lua_type != LUA_TNUMBER) {
                dmLogError("Lua conversion expected table key '%s' to be a number but got %s",
                    key, lua_typename(L, actual_lua_type));
            } else {
                r = lua_tointeger(L, -1);
            }

        }
        lua_pop(L, 1);
        return r;
    }

    bool CheckBoolean(lua_State* L, int index)
    {
        if (lua_isboolean(L, index))
        {
            return lua_toboolean(L, index);
        }
        return luaL_error(L, "Argument %d must be a boolean", index);
    }

    void PushBoolean(lua_State* L, bool v)
    {
        lua_pushboolean(L, v);
    }
} // dmScript
