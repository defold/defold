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
#include "script_graphics.h"
#include "script_json.h"
#include "script_zlib.h"
#include "script_html5.h"
#include "script_luasocket.h"
#include "script_bitop.h"
#include "script_timer.h"

extern "C"
{
#include <dmsdk/dlua/dlua.h>
}

DM_PROPERTY_GROUP(rmtp_Script, "", 0);

namespace dmScript
{
    /*# Built-ins API documentation
     *
     * Built-in scripting functions.
     *
     * @document
     * @name Built-ins
     * @namespace builtins
     * @language Lua
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
    const char META_GET_INSTANCE_DATA_TABLE_REF[]    = "__get_instance_data_table_ref";
    const char META_GET_UNIQUE_SCRIPT_ID[]           = "__get_unique_script_id";

    const char SCRIPT_METATABLE_TYPE_HASH_KEY_NAME[] = "__dmengine_type";
    static const uint32_t SCRIPT_METATABLE_TYPE_HASH_KEY = dmHashBufferNoReverse32(SCRIPT_METATABLE_TYPE_HASH_KEY_NAME, sizeof(SCRIPT_METATABLE_TYPE_HASH_KEY_NAME) - 1);

    // A debug value for profiling lua references
    int g_LuaReferenceCount = 0;
    const uint32_t INVALID_SCRIPT_ID = 0xFFFFFFFF;

    HContext NewContext(const ContextParams& params)
    {
        Context* context = new Context();
        context->m_Modules.SetCapacity(127, 256);
        context->m_PathToModule.SetCapacity(127, 256);
        context->m_HashInstances.SetCapacity(443, 256);
        context->m_ScriptExtensions.SetCapacity(8);
        context->m_ConfigFile = params.m_ConfigFile;
        context->m_ResourceFactory = params.m_Factory;
        context->m_GraphicsContext = params.m_GraphicsContext;
        context->m_LuaState = dlua_open();
#if defined(DM_SANITIZE_THREAD) && defined(__linux__) && !defined(ANDROID) && defined(__aarch64__)
        // Linux arm64 TSan reserves large shadow-memory ranges. This can make
        // LuaJIT's mmap probe fail to reserve a valid Lua state range, so retry
        // state creation before treating it as a real allocation failure.
        for (int i = 0; context->m_LuaState == 0 && i < 16; ++i)
        {
            context->m_LuaState = dlua_open();
        }
#endif
        if (context->m_LuaState == 0)
        {
            dmLogFatal("Failed to create Lua state.");
            assert(context->m_LuaState != 0);
        }
        context->m_ContextTableRef = DLUA_NOREF;
        context->m_ContextWeakTableRef = DLUA_NOREF;
        return context;
    }

    void DeleteContext(HContext context)
    {
        ClearModules(context);
        dlua_close(context->m_LuaState);
        delete context;
    }

    int LuaPrint(dlua_State* L);
    int LuaPPrint(dlua_State* L);

#define RANDOM_SEED "__random_seed"

    static int Lua_Math_Random (dlua_State *L)
    {
        // More or less from lmathlib.c
        DM_LUA_STACK_CHECK(L, 1);

        dlua_getglobal(L, RANDOM_SEED);
        uint32_t* seed = (uint32_t*) dlua_touserdata(L, -1);
        dlua_pop(L, 1);

        // NOTE: + 1 changed from original lua implementation
        // Otherwise upper + 1 when dmMath::Rand() returns DM_RAND_MAX
        // However no proof for correctness
        dlua_Number r = (dlua_Number)dmMath::Rand(seed) / (dlua_Number)(DM_RAND_MAX + 1);
        switch (dlua_gettop(L)) {
            case 0: {
                dlua_pushnumber(L, r);
                break;
            }
            case 1: {
                int u = dluaL_checkint(L, 1);
                dluaL_argcheck(L, 1<=u, 1, "interval is empty");
                dlua_pushnumber(L, floor(r*u)+1);  /* int between 1 and `u' */
                break;
            }
            case 2: {
                int l = dluaL_checkint(L, 1);
                int u = dluaL_checkint(L, 2);
                dluaL_argcheck(L, l<=u, 2, "interval is empty");
                dlua_pushnumber(L, floor(r*(u-l+1))+l);  /* int between `l' and `u' */
                break;
            }
            default:
                return DM_LUA_ERROR("wrong number of arguments");
        }
        return 1;
    }

    static int Lua_Math_Randomseed (dlua_State *L)
    {
        // More or less from lmathlib.c
        DM_LUA_STACK_CHECK(L, 0);
        dlua_getglobal(L, RANDOM_SEED);
        uint32_t* seed = (uint32_t*) dlua_touserdata(L, -1);
        *seed = dluaL_checkint(L, 1);
        // discard first value to avoid repeated values
        dmMath::Rand(seed);
        dlua_pop(L, 1);
        return 0;
    }

    void Initialize(HContext context)
    {
        dlua_State* L = context->m_LuaState;
        DM_LUA_STACK_CHECK(L, 0);

        dluaL_openlibs(L);

        // To support 'math.mod' even though it's been deprecated in 5.1 and removed in 5.2
        dlua_getglobal(L, "math");
        dlua_getfield(L, -1, "fmod");
        dlua_setfield(L, -2, "mod");
        dlua_pop(L, 1);

        InitializeHash(L);
        InitializeMsg(L);
        InitializeVmath(L);
        InitializeSys(L);
        InitializeModule(L);
        InitializeJson(L);
        InitializeZlib(L);
        InitializeHtml5(L);
        InitializeLuasocket(L);
        InitializeBitop(L);
        InitializeGraphics(L, context->m_GraphicsContext);

        dlua_register(L, "print", LuaPrint);
        dlua_register(L, "pprint", LuaPPrint);

        dlua_getglobal(L, "math");
        if (!dlua_isnil(L, -1)) {
            uint32_t *seed = (uint32_t*) malloc(sizeof(uint32_t));
            *seed = 0;
            dlua_pushlightuserdata(L, seed);
            dlua_setglobal(L, RANDOM_SEED);
            // discard first value to avoid repeated values
            dmMath::Rand(seed);

            dlua_pushcfunction(L, Lua_Math_Random);
            dlua_setfield(L, -2, "random");

            dlua_pushcfunction(L, Lua_Math_Randomseed);
            dlua_setfield(L, -2, "randomseed");
        } else {
            dmLogWarning("math library not loaded")
        }

        dlua_pop(L, 1);

        dlua_pushlightuserdata(L, (void*)context);
        SCRIPT_CONTEXT_HASH = dmScript::SetGlobal(L, SCRIPT_CONTEXT);

        dlua_pushlightuserdata(L, (void*)L);
        dlua_setglobal(L, SCRIPT_MAIN_THREAD);

        // Create main context table
        dlua_newtable(L);
        // [-1] context_table
        // Create weak subtable
        dlua_newtable(L);
        // [-2] context_table
        // [-1] weak_table
        dlua_newtable(L);
        // [-3] context_table
        // [-2] weak_table
        // [-1] mt
        dlua_pushliteral(L, "__mode");
        dlua_pushliteral(L, "v");
        dlua_settable(L, -3);         // mt.__mode = "v"
        dlua_setmetatable(L, -2);     // setmetatable(weak_table, mt)
        // [-2] context_table
        // [-1] weak_table

        // Now store weak_table ref separately
        context->m_ContextWeakTableRef = Ref(L, DLUA_REGISTRYINDEX);

        // Finally store context_table
        context->m_ContextTableRef = Ref(L, DLUA_REGISTRYINDEX);

        InitializeTimer(context);

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
        dlua_State* L = context->m_LuaState;

        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->Finalize != 0x0)
            {
                (*l)->Finalize(context);
            }
        }

        dlua_getglobal(L, RANDOM_SEED);
        uint32_t* seed = (uint32_t*) dlua_touserdata(L, -1);
        free(seed);
        dlua_pop(L, 1);

        Unref(L, DLUA_REGISTRYINDEX, context->m_ContextTableRef);
        Unref(L, DLUA_REGISTRYINDEX, context->m_ContextWeakTableRef);
        context->m_ContextTableRef = DLUA_NOREF;
        context->m_ContextWeakTableRef = DLUA_NOREF;
    }

    dlua_State* GetLuaState(HContext context)
    {
        if (context != 0x0) {
            return context->m_LuaState;
        }
        return 0x0;
    }

    // From ldblib.c (getthread)
    static dlua_State* GetLuaThread(dlua_State *L, int *arg)
    {
        if (dlua_isthread(L, 1)) {
            *arg = 1;
            return dlua_tothread(L, 1);
        }
        else {
            *arg = 0;
            return L;
        }
    }

    // From https://zeux.io/2010/11/07/lua-callstack-with-c-debugger/
    // and also
    // https://github.com/defold/defold/blob/dev/engine/lua/src/lua/ldblib.c#L321
    void GetLuaTraceback(dlua_State* L, const char* infostring, void (*cbk)(dlua_State* L, dlua_Debug* entry, void* ctx), void* ctx)
    {
        const int LEVELS1 = 12;  // size of the first part of the stack
        const int LEVELS2 = 10;  // size of the second part of the stack

        int firstpart = 1;
        int arg = 0;
        dlua_State* L1 = GetLuaThread(L, &arg);

        dlua_Debug entry;
        int level = 0;

        if (dlua_isnumber(L, arg+2)) {
            level = (int)dlua_tointeger(L, arg+2);
            dlua_pop(L, 1); // problematic?!
        }
        else {
            level = (L == L1) ? 1 : 0;  // level 0 may be this own function
        }

        if (dlua_gettop(L) != arg && !dlua_isstring(L, arg+1)) return;  // message is not a string

        while (dlua_getstack(L1, level++, &entry))
        {
            if (level > LEVELS1 && firstpart) {
                // no more than `LEVELS2' more levels?
                if (!dlua_getstack(L1, level+LEVELS2, &entry)) {
                    level--;  // keep going
                } else {
                    dlua_pushliteral(L, "\n\t...");  // too many levels
                    while (dlua_getstack(L1, level+LEVELS2, &entry))  // find last levels
                        level++;
                }
                firstpart = 0;
                continue;
            }
            int status = dlua_getinfo(L1, infostring, &entry);
            if (!status)
                continue;

            cbk(L1, &entry, ctx);
        }
    }

    // From ldblib.c function db_errorfb()
    uint32_t WriteLuaTracebackEntry(dlua_Debug* entry, char* buffer, uint32_t buffer_size)
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
        return context ? context->m_ConfigFile : 0;
    }

    dmResource::HFactory GetResourceFactory(HContext context)
    {
        return context ? context->m_ResourceFactory : 0;
    }

    int LuaPrint(dlua_State* L)
    {
        int n = dlua_gettop(L);
        dlua_getglobal(L, "tostring");
        char buffer[dmLog::MAX_STRING_SIZE];
        buffer[0] = 0;
        for (int i = 1; i <= n; ++i)
        {
            const char *s;
            dlua_pushvalue(L, -1);
            dlua_pushvalue(L, i);
            dlua_call(L, 1, 1);
            s = dlua_tostring(L, -1);
            if (s == 0x0)
                return dluaL_error(L, "'tostring' must return a string to 'print'");
            if (i > 1)
                dmStrlCat(buffer, "\t", sizeof(buffer));
            dmStrlCat(buffer, s, sizeof(buffer));
            dlua_pop(L, 1);
        }
        dmLogUserDebug("%s", buffer);
        dlua_pop(L, 1);
        assert(n == dlua_gettop(L));
        return 0;
    }

    static const char* PushValueAsString(dlua_State* L, int index)
    {
        dlua_pushvalue(L, index);
        // [-1] value
        dlua_getglobal(L, "tostring");
        // [-2] value
        // [-1] tostring()
        dlua_insert(L, -2);
        // [-2] tostring()
        // [-1] value
        dlua_call(L, 1, 1);
        // [-1] result
        const char* result = dlua_tostring(L, -1);
        if (result == 0x0)
        {
            dlua_pop(L, 1);
        }
        return result;
    }

    static int DoLuaPPrintTable(dlua_State* L, int index, dmPPrint::Printer* printer, dmHashTable<uintptr_t, bool>& printed_tables) {
        DM_LUA_STACK_CHECK(L, 0);

        const void* table_data = (const void*)dlua_topointer(L, index);

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

        dlua_pushvalue(L, index);
        dlua_pushnil(L);
        // [-2] table
        // [-1] key

        if(dlua_next(L, -2) == 0)
        {
            // [-1] table
            printer->Printf("{ } --[[%p]]", table_data);
            dlua_pop(L, 1);
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
            dlua_Type value_type = dlua_type(L, -1);

            const char* key_string = PushValueAsString(L, -2);
            if (key_string == 0x0)
            {
                return dluaL_error(L, "'tostring' must return a string to 'print'");
            }
            // [-4] table
            // [-3] key
            // [-2] value
            // [-1] key name

            printer->Printf("%s = ", key_string);
            dlua_pop(L, 1);
            // [-3] table
            // [-2] key
            // [-1] value

            if (value_type == DLUA_TTABLE)
            {
                DoLuaPPrintTable(L, -1, printer, printed_tables);
            }
            else if (value_type == DLUA_TSTRING)
            {
                printer->Printf("\"%s\"", dlua_tostring(L, -1));
            }
            else
            {
                const char* value_string = PushValueAsString(L, -1);
                if (value_string == 0x0)
                {
                    return dluaL_error(L, "'tostring' must return a string to 'print'");
                }
                // [-4] table
                // [-3] key
                // [-2] value
                // [-1] value name

                printer->Printf("%s", value_string);
                dlua_pop(L, 1);
                // [-3] table
                // [-2] key
                // [-1] value
            }

            dlua_pop(L, 1);
            // [-2] table
            // [-1] key
            is_first = false;
        } while (dlua_next(L, -2) != 0);

        // [-1] table

        printer->Indent(-2);
        printer->Printf("\n");
        printer->Printf("}");

        printed_tables.Erase((uintptr_t)table_data);

        dlua_pop(L, 1);
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
    int LuaPPrint(dlua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int n = dlua_gettop(L);

        char buf[dmLog::MAX_STRING_SIZE];
        dmPPrint::Printer printer(buf, sizeof(buf));
        dmHashTable<uintptr_t, bool> printed_tables;
        for (int s = 1; s <= n; ++s)
        {
            printed_tables.Clear();
            if (dlua_type(L, s) == DLUA_TTABLE)
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
                    return dluaL_error(L, "'tostring' must return a string to 'print'");
                }
                printer.Printf("%s%s", value_str, (n > s) ? ",\n" : "");
                dlua_pop(L, 1);
            }
        }

        dmLogUserDebug("%s", buf);
        return 0;
    }

    HContext GetScriptContext(dlua_State* L)
    {
        GetGlobal(L, SCRIPT_CONTEXT_HASH);
        HContext context = (HContext)dlua_touserdata(L, -1);
        dlua_pop(L, 1);
        return context;
    }

    uint32_t SetGlobal(dlua_State* L, const char* name)
    {
        size_t name_length = strlen(name);
        uint32_t name_hash = dmHashBuffer32(name, name_length);
        // [-1] instance

        dlua_pushlstring(L, name, name_length);
        // [-1] name
        // [-2] instance

        dlua_pushvalue(L, -2);
        // [-1] instance
        // [-2] name
        // [-3] instance

        dlua_settable(L, DLUA_GLOBALSINDEX);
        // [-1] instance

        dlua_pushinteger(L, (dlua_Integer)name_hash);
        // [-1] name_hash
        // [-2] instance

        dlua_insert(L, -2);
        // [-1] instance
        // [-2] name_hash

        dlua_settable(L, DLUA_GLOBALSINDEX);

        return name_hash;
    }

    void GetGlobal(dlua_State*L, uint32_t name_hash)
    {
        dlua_pushinteger(L, (dlua_Integer)name_hash);
        // [-1] name_hash

        dlua_gettable(L, DLUA_GLOBALSINDEX);
        // [-1] instance
    }

    void GetInstance(dlua_State* L)
    {
        dlua_pushinteger(L, (dlua_Integer)INSTANCE_NAME_HASH);
        // [-1] name_hash

        dlua_gettable(L, DLUA_GLOBALSINDEX);
        // [-1] instance
    }

    void SetInstance(dlua_State* L)
    {
        // [-1] instance

        dlua_pushinteger(L, (dlua_Integer)INSTANCE_NAME_HASH);
        // [-1] name_hash
        // [-2] instance

        dlua_insert(L, -2);
        // [-1] instance
        // [-2] name_hash

        dlua_settable(L, DLUA_GLOBALSINDEX);
    }

    bool IsInstanceValid(dlua_State* L)
    {
        return IsValidInstance(L);
    }

    dlua_State* GetMainThread(dlua_State* L)
    {
        dlua_getglobal(L, SCRIPT_MAIN_THREAD);
        dlua_State* main_thread = (dlua_State*)dlua_touserdata(L, -1);
        dlua_pop(L, 1);

        return main_thread;
    }

    uint32_t SetUserType(dlua_State* L, int meta_table_index, const char* name)
    {
        DM_LUA_STACK_CHECK(L, 0);

        uint32_t type_hash = dmHashBuffer32(name, strlen(name));

        dlua_pushvalue(L, meta_table_index);
        // [-1] meta table

        dlua_pushinteger(L, SCRIPT_METATABLE_TYPE_HASH_KEY);
        // [-1] SCRIPT_METATABLE_TYPE_HASH_KEY
        // [-2] meta table

        dlua_pushinteger(L, (dlua_Integer)type_hash);
        // [-1] type_hash
        // [-2] SCRIPT_METATABLE_TYPE_HASH_KEY
        // [-3] meta table

        dlua_settable(L, -3);
        // [-1] meta table

        dlua_pop(L, 1);

        return type_hash;
    }


    uint32_t RegisterUserTypeLocal(dlua_State* L, const char* name, const dluaL_reg meta[])
    {
        DM_LUA_STACK_CHECK(L, 0);

        dluaL_newmetatable(L, name);
        uint32_t type_hash = SetUserType(L, -1, name);

        dluaL_register (L, 0, meta);
        dlua_pushvalue(L, -1);
        dlua_setfield(L, -1, "__index");
        dlua_pop(L, 1);

        return type_hash;
    }


    uint32_t RegisterUserType(dlua_State* L, const char* name, const dluaL_reg methods[], const dluaL_reg meta[]) {
        DM_LUA_STACK_CHECK(L, 0);

        dluaL_register(L, name, methods);   // create methods table, add it to the globals
        int methods_idx = dlua_gettop(L);
        dluaL_newmetatable(L, name);                         // create metatable for ScriptInstance, add it to the Lua registry

        uint32_t type_hash = SetUserType(L, -1, name);

        int metatable_idx = dlua_gettop(L);
        dluaL_register(L, 0, meta);                   // fill metatable

        dlua_pushliteral(L, "__metatable");
        dlua_pushvalue(L, methods_idx);                       // dup methods table
        dlua_settable(L, metatable_idx);
        dlua_pop(L, 2);

        return type_hash;
    }

    uint32_t GetUserType(dlua_State* L, int user_data_index)
    {
        DM_LUA_STACK_CHECK(L, 0);
        dlua_pushvalue(L, user_data_index);
        // [-1] user data
        dlua_Integer type_hash = 0;
        if (dlua_type(L, -1) == DLUA_TUSERDATA)
        {
            if (dlua_getmetatable(L, -1))
            {
                // [-1] meta table
                // [-2] user data

                dlua_pushinteger(L, SCRIPT_METATABLE_TYPE_HASH_KEY);
                // [-1] SCRIPT_METATABLE_TYPE_HASH_KEY
                // [-2] meta table
                // [-3] user data

                dlua_rawget(L, -2);
                // [-1] type hash
                // [-2] meta table
                // [-3] user data

                type_hash = dlua_tointeger(L, -1);
                dlua_pop(L, 2);
                // [-1] user data
            }
        }
        dlua_pop(L, 1);
        return (uint32_t)type_hash;
    }

    void* ToUserType(dlua_State* L, int user_data_index, uint32_t type_hash)
    {
        if (GetUserType(L, user_data_index) == type_hash)
        {
            return dlua_touserdata(L, user_data_index);
        }
        return 0;
    }

    void* CheckUserType(dlua_State* L, int user_data_index, uint32_t type_hash, const char* error_message)
    {
        void* result = ToUserType(L, user_data_index, type_hash);
        if (result == 0)
        {
            if (error_message == 0x0) {
                const char* type = (const char*)dmHashReverse32(type_hash, 0);
                dluaL_typerror(L, user_data_index, type);
            }
            else {
                dluaL_error(L, "%s", error_message);
            }
        }
        return result;
    }

    static bool GetMetaFunction(dlua_State* L, int index, const char* meta_table_key, size_t meta_table_key_length) {
        if (dlua_getmetatable(L, index)) {
            dlua_pushlstring(L, meta_table_key, meta_table_key_length);
            dlua_rawget(L, -2);
            dlua_remove(L, -2);
            if (dlua_isnil(L, -1)) {
                dlua_pop(L, 1);
                return false;
            } else {
                return true;
            }
        }
        return false;
    }

    bool ResolvePath(dlua_State* L, const char* path, uint32_t path_size, dmhash_t& out_hash) {
        DM_LUA_STACK_CHECK(L, 0);
        GetInstance(L);
        if (GetMetaFunction(L, -1, META_TABLE_RESOLVE_PATH, sizeof(META_TABLE_RESOLVE_PATH) - 1)) {
            dlua_pushvalue(L, -2);
            dlua_pushlstring(L, path, path_size);
            dlua_call(L, 2, 1);
            out_hash = CheckHash(L, -1);
            dlua_pop(L, 2);
            return true;
        }
        dlua_pop(L, 1);
        return false;
    }

    bool GetURL(dlua_State* L, dmMessage::URL& out_url)
    {
        DM_LUA_STACK_CHECK(L, 0);
        GetInstance(L);

        int res = dluaL_callmeta(L, -1, META_TABLE_GET_URL);
        if (res != 1)
        {
            dlua_pop(L, 1);
            return false;
        }
        // We either get the URL or not, if the return value is
        // non-null it must be an URL or we have done bad coding
        // on our end and did not follow the contract of META_TABLE_GET_URL
        dmMessage::URL* url = (dmMessage::URL*)dlua_touserdata(L, -1);
        if (url)
        {
            out_url = *url;
            dlua_pop(L, 2);
            return true;
        }

        // If the URL is null, we call CheckURL to trigger a proper
        // lua type error.
        (void)CheckURL(L, -1);
        return false;
    }

    bool CheckURL(dlua_State* L, dmMessage::URL* out_url)
    {
        bool result = GetURL(L, out_url);
        if (!result)
        {
            return dluaL_error(L, "No URL could be found in the current script environment.");
        }
        return result;
    }

    bool GetUserData(dlua_State* L, uintptr_t* out_user_data, uint32_t user_type_hash) {
        DM_LUA_STACK_CHECK(L, 0);

        GetInstance(L);
        // [-1] instance

        if (dlua_type(L, -1) != DLUA_TUSERDATA)
        {
            dlua_pop(L, 1);
            return false;
        }

        if (!dlua_getmetatable(L, -1))
        {
            dlua_pop(L, 1);
            return false;
        }
        // [-1] meta table
        // [-2] instance

        dlua_pushinteger(L, SCRIPT_METATABLE_TYPE_HASH_KEY);
        // [-1] SCRIPT_METATABLE_TYPE_HASH_KEY
        // [-2] meta table
        // [-3] instance

        dlua_rawget(L, -2);
        // [-1] type hash
        // [-2] meta table
        // [-3] instance

        if (dlua_tointeger(L, -1) != user_type_hash)
        {
            dlua_pop(L, 3);
            return false;
        }

        dlua_pop(L, 1);
        // [-1] meta table
        // [-2] instance

        dlua_pushlstring(L, META_TABLE_GET_USER_DATA, sizeof(META_TABLE_GET_USER_DATA) - 1);
        // [-1] META_TABLE_GET_USER_DATA
        // [-2] meta table
        // [-3] instance

        dlua_rawget(L, -2);
        // [-1] get_user_data method
        // [-2] meta table
        // [-3] instance

        if (dlua_isnil(L, -1)) {
            dlua_pop(L, 3);
            return false;
        }

        dlua_pushvalue(L, -3);
        // [-1] instance
        // [-2] get_user_data
        // [-3] meta table
        // [-4] instance

        dlua_call(L, 1, 1);
        // [-1] user data
        // [-2] meta table
        // [-3] instance

        *out_user_data = (uintptr_t)dlua_touserdata(L, -1);
        dlua_pop(L, 3);
        return true;
    }

    bool IsValidInstance(dlua_State* L) {
        int top = dlua_gettop(L);
        (void)top;
        GetInstance(L);
        if (GetMetaFunction(L, -1, META_TABLE_IS_VALID, sizeof(META_TABLE_IS_VALID) - 1)) {
            dlua_pushvalue(L, -2);
            dlua_call(L, 1, 1);
            assert(top + 2 == dlua_gettop(L));
            bool result = dlua_toboolean(L, -1);
            dlua_pop(L, 2);
            assert(top == dlua_gettop(L));
            return result;
        }
        dlua_pop(L, 1);
        assert(top == dlua_gettop(L));
        return false;
    }

    void SetContextValue(HContext context)
    {
        assert(context != 0x0);
        dlua_State* L = context->m_LuaState;

        DM_LUA_STACK_CHECK(L, -2);

        dlua_rawgeti(L, DLUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-3] key
        // [-2] value
        // [-1] context table

        assert(dlua_type(L, -1) == DLUA_TTABLE);

        dlua_insert(L, -3);
        // [-3] context table
        // [-2] key
        // [-1] value

        dlua_settable(L, -3);
        // [-1] context table

        dlua_pop(L, 1);
    }

    void GetContextValue(HContext context)
    {
        assert(context != 0x0);
        dlua_State* L = context->m_LuaState;

        DM_LUA_STACK_CHECK(L, 0);

        dlua_rawgeti(L, DLUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-2] key
        // [-1] context table

        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            dlua_pop(L, 2);
            dlua_pushnil(L);
            // [-1] DLUA_TNIL
            return;
        }

        dlua_insert(L, -2);
        // [-2] context table
        // [-1] key
        dlua_gettable(L, -2);
        // [-2] context table
        // [-1] value

        dlua_remove(L, -2);
        // [-1] value
    }

    static void GetInstanceContextTable(dlua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        GetInstance(L);
        // [-1] instance

        if (!GetMetaFunction(L, -1, META_GET_INSTANCE_CONTEXT_TABLE_REF, sizeof(META_GET_INSTANCE_CONTEXT_TABLE_REF) - 1))
        {
            dlua_pop(L, 1);
            dlua_pushnil(L);
            return;
        }
        // [-2] instance
        // [-1] META_GET_INSTANCE_CONTEXT_TABLE_REF()

        dlua_insert(L, -2);
        // [-2] META_GET_INSTANCE_CONTEXT_TABLE_REF()
        // [-1] instance

        dlua_call(L, 1, 1);
        // [-1] instance context table ref or DLUA_NOREF
        assert(dlua_type(L, -1) == DLUA_TNUMBER);

        int context_table_ref = dlua_tonumber(L, -1);
        dlua_pop(L, 1);

        if (context_table_ref == DLUA_NOREF)
        {
            dlua_pushnil(L);
            // [-1] DLUA_TNIL
            return;
        }

        dlua_rawgeti(L, DLUA_REGISTRYINDEX, context_table_ref);
        // [-1] instance context table
    }

    uintptr_t GetInstanceId(dlua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        GetInstance(L);
        dlua_Type instance_type = dlua_type(L, -1);
        // We assume that all users of SetInstance puts some form of user data/light user data, it is an assumption that works for now
        uintptr_t id = (instance_type == DLUA_TLIGHTUSERDATA || instance_type == DLUA_TUSERDATA) ? (uintptr_t)dlua_touserdata(L, -1) : 0;
        dlua_pop(L, 1);
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
        dlua_State* L = script_world->m_Context->m_LuaState;
        dlua_rawgeti(L, DLUA_REGISTRYINDEX, script_world->m_WorldContextTableRef);
        // [-3] key
        // [-2] value
        // [-1] context table

        dlua_insert(L, -3);
        // [-3] context table
        // [-2] key
        // [-1] value

        dlua_settable(L, -3);
        // [-1] context table

        dlua_pop(L, 1);
    }

    void GetScriptWorldContextValue(HScriptWorld script_world)
    {
        dlua_State* L = script_world->m_Context->m_LuaState;
        dlua_rawgeti(L, DLUA_REGISTRYINDEX, script_world->m_WorldContextTableRef);
        // [-2] key
        // [-1] context table

        dlua_insert(L, -2);
        // [-2] context table
        // [-1] key

        dlua_gettable(L, -2);
        // [-2] context table
        // [-1] value

        dlua_insert(L, -2);
        // [-2] value
        // [-1] context table

        dlua_pop(L, 1);
        // [-1] value
    }

    uint32_t GenerateUniqueScriptId()
    {
        static uint32_t counter = 0;

        if (counter == INVALID_SCRIPT_ID - 1)
        {
            counter = 0;
        }
        else
        {
            ++counter;
        }

        return counter;
    }

    HScriptWorld NewScriptWorld(HContext context)
    {
        HScriptWorld script_world = (ScriptWorld*)malloc(sizeof(ScriptWorld));
        assert(script_world != 0x0);
        script_world->m_Context = context;
        dlua_State* L = script_world->m_Context->m_LuaState;
        dlua_newtable(L);
        script_world->m_WorldContextTableRef = Ref(L, DLUA_REGISTRYINDEX);
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
        dlua_State* L = script_world->m_Context->m_LuaState;
        Unref(L, DLUA_REGISTRYINDEX, script_world->m_WorldContextTableRef);

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

    bool SetInstanceContextValue(dlua_State* L)
    {
        // [-2] key
        // [-1] value

        DM_LUA_STACK_CHECK(L, -2);

        GetInstanceContextTable(L);
        // [-3] key
        // [-2] value
        // [-1] instance context table or DLUA_TNIL

        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            dlua_pop(L, 3);
            return false;
        }
        // [-3] key
        // [-2] value
        // [-1] instance context table

        dlua_insert(L, -3);
        // [-3] instance context table
        // [-2] key
        // [-1] value

        dlua_settable(L, -3);
        // [-1] instance context table

        dlua_pop(L, 1);
        return true;
    }

    void GetInstanceContextValue(dlua_State* L)
    {
        // [-1] key
        DM_LUA_STACK_CHECK(L, 0);

        GetInstanceContextTable(L);
        // [-2] key
        // [-1] instance context table or DLUA_TNIL

        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            dlua_pop(L, 2);

            dlua_pushnil(L);
            // [-1] DLUA_TNIL
            return;
        }
        // [-2] key
        // [-1] instance context table

        dlua_insert(L, -2);
        // [-2] instance context table
        // [-1] key

        dlua_gettable(L, -2);
        // [-2] instance context table
        // [-1] value

        dlua_insert(L, -2);
        // [-2] value
        // [-1] instance context table

        dlua_pop(L, 1);
        // [-1] value
    }

    int RefInInstance(dlua_State* L)
    {
        // [-1] value
        DM_LUA_STACK_CHECK(L, -1);

        GetInstanceContextTable(L);
        // [-2] value
        // [-1] instance context table or DLUA_TNIL

        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            // [-2] value
            // [-1] DLUA_TNIL

            dlua_pop(L, 2);
            return DLUA_NOREF;
        }
        // [-2] value
        // [-1] instance context table

        dlua_insert(L, -2);
        // [-2] instance context table
        // [-1] value

        int instance_ref = dluaL_ref(L, -2);
        // [-1] instance context table

        dlua_pop(L, 1);

        return instance_ref;
    }

    void UnrefInInstance(dlua_State* L, int ref)
    {
        DM_LUA_STACK_CHECK(L, 0);

        GetInstanceContextTable(L);
        // [-1] instance context table or DLUA_TNIL

        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            // [-1] DLUA_TNIL

            dlua_pop(L, 1);
            return;
        }
        // [-1] instance context table

        dluaL_unref(L, -1, ref);
        // [-1] instance context table

        dlua_pop(L, 1);
    }

    void ResolveInInstance(dlua_State* L, int ref)
    {
        DM_LUA_STACK_CHECK(L, 1);

        GetInstanceContextTable(L);
        // [-1] instance context table or DLUA_TNIL

        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            dlua_pop(L, 1);

            dlua_pushnil(L);
            // [-1] DLUA_TNIL
            return;
        }
        // [-1] instance context table

        dlua_rawgeti(L, -1, ref);
        // [-2] instance context table
        // [-1] value

        dlua_insert(L, -2);
        // [-2] value
        // [-1] instance context table

        dlua_pop(L, 1);
        // [-1] value
    }


    struct LuaCallstackCtx
    {
        bool     m_First;
        char*    m_Buffer;
        uint32_t m_BufferSize;
    };

    static void GetLuaStackTraceCbk(dlua_State* L, dlua_Debug* entry, void* _ctx)
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

    static int BacktraceErrorHandler(dlua_State *m_state) {
        dlua_createtable(m_state, 0, 2);
        
        // First, generate traceback BEFORE we modify the stack
        // We need to do this first because GetLuaTraceback expects string errors
        char traceback[1024];
        traceback[0] = '\0'; // Initialize the buffer
        LuaCallstackCtx ctx;
        ctx.m_First = true;
        ctx.m_Buffer = traceback;
        ctx.m_BufferSize = sizeof(traceback);
        
        // If error is not a string, temporarily convert it for traceback generation
        if (!dlua_isstring(m_state, 1))
        {
            // Replace stack position 1 with string version for GetLuaTraceback
            dlua_getglobal(m_state, "tostring");
            if (dlua_isfunction(m_state, -1))
            {
                dlua_pushvalue(m_state, 1); // Push the original error value
                int result = dlua_pcall(m_state, 1, 1, 0);
                if (result == 0 && dlua_isstring(m_state, -1))
                {
                    dlua_replace(m_state, 1); // Replace original error with string version
                }
                else
                {
                    dlua_pop(m_state, 1); // Remove failed result
                    const char* type_name = dlua_typename(m_state, dlua_type(m_state, 1));
                    dlua_pushfstring(m_state, "(%s)", type_name);
                    dlua_replace(m_state, 1); // Replace with type name
                }
            }
            else
            {
                dlua_pop(m_state, 1); // Remove non-function value
                const char* type_name = dlua_typename(m_state, dlua_type(m_state, 1));
                dlua_pushfstring(m_state, "(%s)", type_name);
                dlua_replace(m_state, 1); // Replace with type name
            }
        }
        
        // Now generate traceback with string error message
        dmScript::GetLuaTraceback(m_state, "Sln", GetLuaStackTraceCbk, &ctx);
        
        // Store the converted error message
        dlua_pushvalue(m_state, 1);
        dlua_setfield(m_state, -2, "error");

        // Store the traceback
        dlua_pushstring(m_state, traceback);
        dlua_setfield(m_state, -2, "traceback");

        return 1;
    }

    static int PCallInternal(dlua_State* L, int nargs, int nresult, int in_error_handler) {
        dlua_pushcfunction(L, BacktraceErrorHandler);
        int err_index = dlua_gettop(L) - nargs - 1;
        dlua_insert(L, err_index);
        int result = dlua_pcall(L, nargs, nresult, err_index);
        dlua_remove(L, err_index);
        if (result == DLUA_ERRMEM) {
            dlua_pop(L, 1);  // Pop BacktraceErrorHandler since it will not be called on OOM
            dmLogError("Lua memory allocation error.");
        } else if (result != 0) {
            // extract the individual fields for printing and passing
            dlua_getfield(L, -1, "error");
            dlua_getfield(L, -2, "traceback");
            // if handling error that happened during the error handling, print it and clean up and exit
            if (in_error_handler) {
                const char* error_msg = dlua_tostring(L, -2);
                const char* traceback_msg = dlua_tostring(L, -1);
                dmLogError("In error handler: %s%s", 
                          error_msg ? error_msg : "(error value could not be converted to string)", 
                          traceback_msg ? traceback_msg : "(traceback unavailable)");
                dlua_pop(L, 3);
                return result;
            }
            // print before calling the error handler
            const char* error_msg = dlua_tostring(L, -2);
            const char* traceback_msg = dlua_tostring(L, -1);
            dmLogError("%s\n%s", 
                      error_msg ? error_msg : "(error value could not be converted to string)", 
                      traceback_msg ? traceback_msg : "(traceback unavailable)");
            dlua_getfield(L, DLUA_GLOBALSINDEX, "debug");
            if (dlua_istable(L, -1)) {
                dlua_pushliteral(L, SCRIPT_ERROR_HANDLER_VAR);
                dlua_rawget(L, -2);
                if (dlua_isfunction(L, -1)) {
                    dlua_pushlstring(L, "lua", 3); // 1st arg: source = 'lua'
                    dlua_pushvalue(L, -5);     // 2nd arg: error
                    dlua_pushvalue(L, -5);     // 3rd arg: traceback
                    PCallInternal(L, 3, 0, 1);
                } else {
                    if (!dlua_isnil(L, -1)) {
                        dmLogError("Registered error handler is not a function");
                    }
                    dlua_pop(L, 1);
                }
            }
            dlua_pop(L, 4); // debug value, traceback, error, table
        }
        return result;
    }

    int PCall(dlua_State* L, int nargs, int nresult) {
        return PCallInternal(L, nargs, nresult, 0);
    }

    int Ref(dlua_State* L, int table)
    {
        ++g_LuaReferenceCount;
        return dluaL_ref(L, table);
    }

    void Unref(dlua_State* L, int table, int reference)
    {
        if (reference == DLUA_NOREF)
        {
            return;
        }
        if (g_LuaReferenceCount <= 0)
        {
            dmLogError("Unbalanced number of Lua refs - possibly double calls to dmScript::Unref");
        }
        --g_LuaReferenceCount;
        dluaL_unref(L, table, reference);
    }

    int GetLuaRefCount()
    {
        return g_LuaReferenceCount;
    }

    void ClearLuaRefCount()
    {
        g_LuaReferenceCount = 0;
    }

    uint32_t GetLuaGCCount(dlua_State* L)
    {
        return (uint32_t)dlua_gc(L, DLUA_GCCOUNT, 0);
    }

    LuaStackCheck::LuaStackCheck(dlua_State* L, int diff, const char* filename, int linenumber) : m_L(L), m_Filename(filename), m_Linenumber(linenumber), m_Top(dlua_gettop(L)), m_Diff(diff)
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
        dluaL_where(m_L, 1);
        dlua_pushvfstring(m_L, fmt, argp);
        va_end(argp);
        dlua_concat(m_L, 2);
        m_Diff = -0x800000;
        return dlua_error(m_L);
    }

    void LuaStackCheck::Verify(int diff)
    {
        int32_t expected = m_Top + diff;
        int32_t actual = dlua_gettop(m_L);
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
        LuaCallbackInfo() : m_L(0), m_ContextTableRef(DLUA_NOREF), m_Callback(DLUA_NOREF), m_Self(DLUA_NOREF) {}
        dlua_State* m_L;
        int        m_ContextTableRef;
        int        m_CallbackInfoRef;
        int        m_Callback;
        int        m_Self;
        uint32_t   m_UniqueScriptId;
    };

    void PrintStack(dlua_State* L)
    {
        int top = dlua_gettop(L);
        int bottom = 1;
        dlua_getglobal(L, "tostring");
        for(int i = top; i >= bottom; i--)
        {
            dlua_pushvalue(L, -1);
            dlua_pushvalue(L, i);
            dlua_pcall(L, 1, 1, 0);
            const char *str = dlua_tostring(L, -1);
            if (str) {
                dmLogInfo("%2d: %s\n", i, str);
            }else{
                dmLogInfo("%2d: %s\n", i, dluaL_typename(L, i));
            }
            dlua_pop(L, 1);
        }
        dlua_pop(L, 1);
    }

    LuaCallbackInfo* CreateCallback(dlua_State* L, int callback_stack_index)
    {
        dluaL_checktype(L, callback_stack_index, DLUA_TFUNCTION);

        DM_LUA_STACK_CHECK(L, 0);

        GetInstance(L);
        // [-1] instance

        if (!GetMetaFunction(L, -1, META_GET_INSTANCE_CONTEXT_TABLE_REF, sizeof(META_GET_INSTANCE_CONTEXT_TABLE_REF) - 1))
        {
            dmLogError("CreateCallback failed: missing %s meta function for instance context table", "INSTANCE_CONTEXT");
            dlua_pop(L, 1);
            return 0x0;
        }
        // [-2] instance
        // [-1] META_GET_INSTANCE_CONTEXT_TABLE_REF()

        dlua_pushvalue(L, -2);
        // [-3] instance
        // [-2] META_GET_INSTANCE_CONTEXT_TABLE_REF()
        // [-1] instance

        dlua_call(L, 1, 1);
        // [-2] instance
        // [-1] instance context table ref
        assert(dlua_type(L, -1) == DLUA_TNUMBER);

        int context_table_ref = dlua_tonumber(L, -1);

        dlua_pop(L, 1);
        // [-1] instance

        uint32_t unique_script_id = INVALID_SCRIPT_ID;
        if (!GetMetaFunction(L, -1, META_GET_UNIQUE_SCRIPT_ID, sizeof(META_GET_UNIQUE_SCRIPT_ID) - 1))
        {
            dmLogError("CreateCallback failed: missing %s meta function for instance context table", "UNIQUE_SCRIPT");
            dlua_pop(L, 1);
            return 0x0;
        }
        // [-2] instance
        // [-1] META_GET_UNIQUE_SCRIPT_ID()
        dlua_pushvalue(L, -2); // push instance
        // [-3] instance
        // [-2] META_GET_UNIQUE_SCRIPT_ID()
        // [-1] instance
        dlua_call(L, 1, 1);    // call __get_unique_script_id(self)
        // [-2] instance
        // [-1] unique script id
        unique_script_id = (uint32_t)dlua_tointeger(L, -1);
        dlua_pop(L, 1);
        // [-1] instance

        dlua_pop(L, 1);

        dlua_pushvalue(L, callback_stack_index);
        // [-1] callback

        dlua_rawgeti(L, DLUA_REGISTRYINDEX, context_table_ref);
        // [-2] callback
        // [-1] context table
        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            dmLogError("CreateCallback failed: expected context table (DLUA_TTABLE), got %s", dlua_typename(L, dlua_type(L, -1)));
            dlua_pop(L, 2);
            return 0x0;
        }

        dlua_insert(L, -2);
        // [-2] context table
        // [-1] callback

        LuaCallbackInfo* cbk = (LuaCallbackInfo*)dlua_newuserdata(L, sizeof(LuaCallbackInfo));
        // [-3] context table
        // [-2] callback
        // [-1] LuaCallbackInfo

        cbk->m_UniqueScriptId = unique_script_id;
        cbk->m_L = GetMainThread(L);
        cbk->m_ContextTableRef = context_table_ref;

        // For the callback ref (that can actually outlive the script instance)
        // we want to add to the lua debug count
        cbk->m_CallbackInfoRef = dmScript::Ref(L, DLUA_REGISTRYINDEX);
        // [-2] context table
        // [-1] callback

        // We do not use dmScript::Unref for refs in the context local table as we don't
        // want to count those refs the ref debug count shown in the profiler

        cbk->m_Callback = dluaL_ref(L, -2);
        // [-1] context table

        GetInstance(L);
        // [-1] context table
        // [-2] instance

        cbk->m_Self = dluaL_ref(L, -2);
        // [-1] context table

        dlua_pop(L, 1);

        return cbk;
     }

    static bool IsCallbackInstanceValid(LuaCallbackInfo* cbk)
    {
        if (cbk->m_UniqueScriptId == INVALID_SCRIPT_ID)
        {
            return false;
        }
        dlua_State* L = cbk->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        GetInstance(L);
        // [-1] old instance
        dlua_rawgeti(L, DLUA_REGISTRYINDEX, cbk->m_ContextTableRef);
        // [-2] old instance
        // [-1] context table
        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            dlua_pop(L, 2);
            cbk->m_UniqueScriptId  = INVALID_SCRIPT_ID;
            return false;
        }

        const int context_table_stack_index = dlua_gettop(L);
        dlua_rawgeti(L, context_table_stack_index, cbk->m_Self);
        // [-3] old instance
        // [-2] context table
        // [-1] instance
        if (dlua_isnil(L, -1))
        {
            dlua_pop(L, 3);
            cbk->m_UniqueScriptId = INVALID_SCRIPT_ID;
            return false;
        }

        dlua_pushvalue(L, -1);
        // [-4] old instance
        // [-3] context table
        // [-2] instance
        // [-1] instance
        SetInstance(L);
        // [-3] old instance
        // [-2] context table
        // [-1] instance
        uint32_t unique_script_id = INVALID_SCRIPT_ID;
        if (GetMetaFunction(L, -1, META_GET_UNIQUE_SCRIPT_ID, sizeof(META_GET_UNIQUE_SCRIPT_ID) - 1))
        {
            // [-4] old instance
            // [-3] context table
            // [-2] instance
            // [-1] META_GET_UNIQUE_SCRIPT_ID()
            dlua_pushvalue(L, -2);
            // [-5] old instance
            // [-4] context table
            // [-3] instance
            // [-2] META_GET_UNIQUE_SCRIPT_ID()
            // [-1] instance
            dlua_call(L, 1, 1);
            // [-4] old instance
            // [-3] context table
            // [-2] instance
            // [-1] unique script id
            unique_script_id = (uint32_t)dlua_tointeger(L, -1);
            dlua_pop(L, 1);
            // [-3] old instance
            // [-2] context table
            // [-1] instance
        }

        dlua_pop(L, 2);
        // [-1] old instance
        SetInstance(L);

        if (cbk->m_UniqueScriptId != unique_script_id)
        {
            return false;
        }

        return true;
    }

    bool IsCallbackValid(LuaCallbackInfo* cbk)
    {
        if (cbk == NULL ||
            cbk->m_L == NULL ||
            cbk->m_ContextTableRef == DLUA_NOREF ||
            cbk->m_CallbackInfoRef == DLUA_NOREF ||
            cbk->m_Callback == DLUA_NOREF ||
            cbk->m_Self == DLUA_NOREF ||
            cbk->m_UniqueScriptId == INVALID_SCRIPT_ID)
        {
            return false;
        }
        if (IsCallbackInstanceValid(cbk))
        {
            return true;
        }
        cbk->m_UniqueScriptId = INVALID_SCRIPT_ID;
        return false;
    }

    dlua_State* GetCallbackLuaContext(LuaCallbackInfo* cbk)
    {
        return cbk ? cbk->m_L : 0;
    }

    void DestroyCallback(LuaCallbackInfo* cbk)
    {
        dlua_State* L = cbk->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        if(cbk->m_ContextTableRef != DLUA_NOREF)
        {
            dlua_rawgeti(L, DLUA_REGISTRYINDEX, cbk->m_ContextTableRef);
            if (dlua_type(L, -1) == DLUA_TTABLE)
            {
                if (IsCallbackInstanceValid(cbk))
                {
                    // We do not use dmScript::Unref for refs in the context local table as we don't
                    // want to count those refs the ref debug count shown in the profiler
                    dluaL_unref(L, -1, cbk->m_Self);
                    dluaL_unref(L, -1, cbk->m_Callback);
                }
                else
                {
                    cbk->m_UniqueScriptId = INVALID_SCRIPT_ID;
                }
            }
            
            // For the callback (that can actually outlive the script instance)
            // we want to add to the lua debug count
            if (cbk->m_CallbackInfoRef != DLUA_NOREF)
            {
                dmScript::Unref(L, DLUA_REGISTRYINDEX, cbk->m_CallbackInfoRef);
            }
            dlua_pop(L, 1);

            cbk->m_Self = DLUA_NOREF;
            cbk->m_Callback = DLUA_NOREF;
            cbk->m_CallbackInfoRef = DLUA_NOREF;
            cbk->m_ContextTableRef = DLUA_NOREF;
            cbk->m_UniqueScriptId = INVALID_SCRIPT_ID;
            return;
        }
        else
        {
            dmLogWarning("Failed to unregister callback (it was not registered)");
        }
    }

    bool SetupCallback(LuaCallbackInfo* cbk)
    {
        dlua_State* L = cbk->m_L;
        int top = dlua_gettop(L);
        if(cbk->m_CallbackInfoRef == DLUA_NOREF)
        {
            dmLogWarning("Failed to invoke callback (it was not registered)");
            assert(top == dlua_gettop(L));
            return false;
        }

        GetInstance(L);
        // [-1] old instance

        dlua_rawgeti(L, DLUA_REGISTRYINDEX, cbk->m_ContextTableRef);
        // [-2] old instance
        // [-1] context table

        if (dlua_type(L, -1) != DLUA_TTABLE)
        {
            dlua_pop(L, 2);
            assert(top == dlua_gettop(L));
            return false;
        }

        const int context_table_stack_index = dlua_gettop(L);

        dlua_rawgeti(L, context_table_stack_index, cbk->m_Callback);
        // [-3] old instance
        // [-2] context table
        // [-1] callback

        if (dlua_type(L, -1) != DLUA_TFUNCTION)
        {
            dlua_pop(L, 3);
            assert(top == dlua_gettop(L));
            return false;
        }

        dlua_rawgeti(L, context_table_stack_index, cbk->m_Self); // Setup self (the script instance)
        // [-4] old instance
        // [-3] context table
        // [-2] callback
        // [-1] self

        if (dlua_isnil(L, -1))
        {
            dlua_pop(L, 4);
            assert(top == dlua_gettop(L));
            return false;
        }

        dlua_pushvalue(L, -1);
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
            dlua_pop(L, 3);
            // [-1] old instance

            SetInstance(L);
            assert(top == dlua_gettop(L));
            return false;
        }

        assert((top + 4) == dlua_gettop(L));
        return true;
    }

    void TeardownCallback(LuaCallbackInfo* cbk)
    {
        dlua_State* L = cbk->m_L;
        // [-2] old instance
        // [-1] context table
        dlua_pop(L, 1);

        SetInstance(L);
    }

    bool InvokeCallback(LuaCallbackInfo* cbk, LuaCallbackUserFn fn, void* user_context)
    {
        dlua_State* L = cbk->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        if (!SetupCallback(cbk)) {
            return false;
        }
        // [-4] old instance
        // [-3] context table
        // [-2] callback
        // [-1] self

        int user_args_start = dlua_gettop(L);

        if (fn)
            fn(L, user_context);

        int user_args_end = dlua_gettop(L);

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
    static bool GetLuaFunctionRefInfo(dlua_State* L, int stack_index, LuaFunctionInfo* out_function_info)
    {
        dlua_Debug ar;
        dlua_pushvalue(L, stack_index);
        if (dlua_getinfo(L, ">Sn", &ar))
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
    const char* GetProfilerString(dlua_State* L, int optional_callback_index, const char* source_file_name, const char* function_name, const char* optional_message_name, char* buffer, uint32_t buffer_size)
    {
        if (!ProfileIsInitialized())
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

    const char* GetTableStringValue(dlua_State* L, int table_index, const char* key, const char* default_value)
    {
        DM_LUA_STACK_CHECK(L, 0);
        const char* r = default_value;

        dlua_getfield(L, table_index, key);
        if (!dlua_isnil(L, -1)) {

            dlua_Type actual_lua_type = dlua_type(L, -1);
            if (actual_lua_type != DLUA_TSTRING) {
                dmLogError("Lua conversion expected table key '%s' to be a string but got %s",
                    key, dlua_typename(L, actual_lua_type));
            } else {
                r = dlua_tostring(L, -1);
            }

        }
        dlua_pop(L, 1);
        return r;
    }

    int GetTableIntValue(dlua_State* L, int table_index, const char* key, int default_value)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int r = default_value;

        dlua_getfield(L, table_index, key);
        if (!dlua_isnil(L, -1)) {

            dlua_Type actual_lua_type = dlua_type(L, -1);
            if (actual_lua_type != DLUA_TNUMBER) {
                dmLogError("Lua conversion expected table key '%s' to be a number but got %s",
                    key, dlua_typename(L, actual_lua_type));
            } else {
                r = dlua_tointeger(L, -1);
            }

        }
        dlua_pop(L, 1);
        return r;
    }

    bool CheckBoolean(dlua_State* L, int index)
    {
        if (dlua_isboolean(L, index))
        {
            return dlua_toboolean(L, index);
        }
        return dluaL_error(L, "Argument %d must be a boolean", index);
    }

    void PushBoolean(dlua_State* L, bool v)
    {
        dlua_pushboolean(L, v);
    }
} // dmScript
