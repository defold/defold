#include "script.h"

#include <dlib/dstrings.h>
#include <dlib/index_pool.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/pprint.h>
#include <dlib/profile.h>
#include <extension/extension.h>

#include "script_private.h"
#include "script_hash.h"
#include "script_msg.h"
#include "script_vmath.h"
#include "script_buffer.h"
#include "script_sys.h"
#include "script_module.h"
#include "script_image.h"
#include "script_json.h"
#include "script_http.h"
#include "script_zlib.h"
#include "script_html5.h"
#include "script_luasocket.h"
#include "script_bitop.h"

extern "C"
{
#include <lua/lualib.h>
}

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

    const char* INSTANCE_NAME = "__dm_script_instance__";
    const int MAX_PPRINT_TABLE_CALL_DEPTH = 32;

    const char* META_TABLE_RESOLVE_PATH     = "__resolve_path";
    const char* META_TABLE_GET_URL          = "__get_url";
    const char* META_TABLE_GET_USER_DATA    = "__get_user_data";
    const char* META_TABLE_IS_VALID         = "__is_valid";

    // A debug value for profiling lua references
    int g_LuaReferenceCount = 0;

    HContext NewContext(dmConfigFile::HConfig config_file, dmResource::HFactory factory, bool enable_extensions)
    {
        Context* context = new Context();
        context->m_Modules.SetCapacity(127, 256);
        context->m_PathToModule.SetCapacity(127, 256);
        context->m_HashInstances.SetCapacity(443, 256);
        context->m_ConfigFile = config_file;
        context->m_ResourceFactory = factory;
        context->m_EnableExtensions = enable_extensions;
        memset(context->m_InitializedExtensions, 0, sizeof(context->m_InitializedExtensions));
        context->m_LuaState = lua_open();
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
        int top = lua_gettop(L);

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
                return luaL_error(L, "wrong number of arguments");
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int Lua_Math_Randomseed (lua_State *L)
    {
        // More or less from lmathlib.c
        int top = lua_gettop(L);
        lua_getglobal(L, RANDOM_SEED);
        uint32_t* seed = (uint32_t*) lua_touserdata(L, -1);
        *seed = luaL_checkint(L, 1);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return 0;
    }

    void Initialize(HContext context)
    {
        lua_State* L = context->m_LuaState;
        int top = lua_gettop(L);
        (void)top;

        luaL_openlibs(L);

        InitializeHash(L);
        InitializeMsg(L);
        InitializeVmath(L);
        InitializeBuffer(L);
        InitializeSys(L);
        InitializeModule(L);
        InitializeImage(L);
        InitializeJson(L);
        InitializeHttp(L, context->m_ConfigFile);
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

            lua_pushcfunction(L, Lua_Math_Random);
            lua_setfield(L, -2, "random");

            lua_pushcfunction(L, Lua_Math_Randomseed);
            lua_setfield(L, -2, "randomseed");
        } else {
            dmLogWarning("math library not loaded")
        }

        lua_pop(L, 1);

        lua_pushlightuserdata(L, (void*)context);
        lua_setglobal(L, SCRIPT_CONTEXT);

        lua_pushlightuserdata(L, (void*)L);
        lua_setglobal(L, SCRIPT_MAIN_THREAD);

#define BIT_INDEX(b) ((b) / sizeof(uint32_t))
#define BIT_OFFSET(b) ((b) % sizeof(uint32_t))

        if (context->m_EnableExtensions) {
            const dmExtension::Desc* ed = dmExtension::GetFirstExtension();
            uint32_t i = 0;
            while (ed) {
                dmExtension::Params p;
                p.m_ConfigFile = context->m_ConfigFile;
                p.m_L = L;
                dmExtension::Result r = ed->Initialize(&p);
                if (r == dmExtension::RESULT_OK) {
                    context->m_InitializedExtensions[BIT_INDEX(i)] |= 1 << BIT_OFFSET(i);
                } else {
                    dmLogError("Failed to initialize extension: %s", ed->m_Name);
                }
                ++i;
                ed = ed->m_Next;
            }
        }

        assert(top == lua_gettop(L));
    }

    void UpdateExtensions(HContext context)
    {
        if (context->m_EnableExtensions) {
            const dmExtension::Desc* ed = dmExtension::GetFirstExtension();
            uint32_t i = 0;
            while (ed) {
                if (ed->Update)
                {
                    dmExtension::Params p;
                    p.m_ConfigFile = context->m_ConfigFile;
                    p.m_L = context->m_LuaState;
                    if (context->m_InitializedExtensions[BIT_INDEX(i)] & (1 << BIT_OFFSET(i))) {
                        dmExtension::Result r = ed->Update(&p);
                        if (r != dmExtension::RESULT_OK) {
                            dmLogError("Failed to update extension: %s", ed->m_Name);
                        }
                    }
                }
                ++i;
                ed = ed->m_Next;
            }
        }
    }

    void Finalize(HContext context)
    {
        lua_State* L = context->m_LuaState;
        FinalizeHttp(L);

        if (context->m_EnableExtensions) {
            const dmExtension::Desc* ed = dmExtension::GetFirstExtension();
            uint32_t i = 0;
            while (ed) {
                if (ed->Finalize)
                {
                    dmExtension::Params p;
                    p.m_ConfigFile = context->m_ConfigFile;
                    p.m_L = L;
                    if (context->m_InitializedExtensions[BIT_INDEX(i)] & (1 << BIT_OFFSET(i))) {
                        dmExtension::Result r = ed->Finalize(&p);
                        if (r != dmExtension::RESULT_OK) {
                            dmLogError("Failed to finalize extension: %s", ed->m_Name);
                        }
                    }
                }
                ++i;
                ed = ed->m_Next;
            }
        }
        if (context) {
            // context might be NULL in tests. Should probably be forbidden though
            memset(context->m_InitializedExtensions, 0, sizeof(context->m_InitializedExtensions));
        }

        lua_getglobal(L, RANDOM_SEED);
        uint32_t* seed = (uint32_t*) lua_touserdata(L, -1);
        free(seed);
    }
#undef BIT_INDEX
#undef BIT_OFFSET

    lua_State* GetLuaState(HContext context) {
        if (context != 0x0) {
            return context->m_LuaState;
        }
        return 0x0;
    }

    int LuaPrint(lua_State* L)
    {
        int n = lua_gettop(L);
        lua_getglobal(L, "tostring");
        char buffer[2048];
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

    static int DoLuaPPrintTable(lua_State*L, int index, dmPPrint::Printer* printer, int call_depth) {
        int top = lua_gettop(L);

        lua_pushvalue(L, index);
        lua_pushnil(L);

        printer->Printf("{\n");
        printer->Indent(2);

        while (lua_next(L, -2) != 0) {
            int value_type = lua_type(L, -1);

            lua_pushvalue(L, -2);
            const char *s1;
            const char *s2;
            lua_getglobal(L, "tostring");
            lua_pushvalue(L, -2);
            lua_call(L, 1, 1);
            s1 = lua_tostring(L, -1);
            if (s1 == 0x0)
                return luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
            lua_pop(L, 1);

            lua_getglobal(L, "tostring");
            lua_pushvalue(L, -3);
            lua_call(L, 1, 1);
            s2 = lua_tostring(L, -1);
            if (s2 == 0x0)
                return luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
            lua_pop(L, 1);

            if (value_type == LUA_TTABLE) {
                if (MAX_PPRINT_TABLE_CALL_DEPTH > ++call_depth) {
                    printer->Printf("%s = ", s1);
                    DoLuaPPrintTable(L, -2, printer, call_depth);
                } else {
                    printer->Printf("%s...\n", s1);
                    printer->Printf("Printing truncated. Circular refs?\n");
                }
            } else {
                printer->Printf("%s = %s,\n", s1, s2);
            }

            lua_pop(L, 2);
        }

        printer->Indent(-2);
        printer->Printf("}\n");

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
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
        int n = lua_gettop(L);

        char buf[2048];
        dmPPrint::Printer printer(buf, sizeof(buf));
        if (lua_type(L, 1) == LUA_TTABLE) {
            printer.Printf("\n");
            DoLuaPPrintTable(L, 1, &printer, 0);
        } else {
            lua_getglobal(L, "tostring");
            lua_pushvalue(L, 1);
            lua_call(L, 1, 1);
            const char* s = lua_tostring(L, -1);
            if (s == 0x0)
                return luaL_error(L, LUA_QL("tostring") " must return a string to " LUA_QL("print"));
            printer.Printf("%s", s);
            lua_pop(L, 1);
        }

        dmLogUserDebug("%s", buf);
        assert(n == lua_gettop(L));
        return 0;
    }

    void GetInstance(lua_State* L)
    {
        lua_getglobal(L, INSTANCE_NAME);
    }

    void SetInstance(lua_State* L)
    {
        lua_setglobal(L, INSTANCE_NAME);
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

    bool IsUserType(lua_State* L, int idx, const char* type)
    {
        int top = lua_gettop(L);
        bool result = false;
        if (lua_type(L, idx) == LUA_TUSERDATA)
        {
            // Object meta table
            if (lua_getmetatable(L, idx))
            {
                // Correct meta table
                lua_getfield(L, LUA_REGISTRYINDEX, type);
                // Compare them
                if (lua_rawequal(L, -1, -2))
                {
                    result = true;
                }
            }
        }
        lua_pop(L, lua_gettop(L) - top);
        return result;
    }

    void* CheckUserType(lua_State* L, int idx, const char* type, const char* error_message)
    {
        luaL_checktype(L, idx, LUA_TUSERDATA);
        // from https://github.com/keplerproject/lua-compat-5.3/blob/master/c-api/compat-5.3.c#L292-L306
        void* object = lua_touserdata(L, idx);
        lua_getmetatable(L, idx);
        luaL_getmetatable(L, type);
        int res = lua_rawequal(L, -1, -2);
        lua_pop(L, 2);
        if (!res) {
            if (error_message == 0x0) {
                luaL_typerror(L, idx, type);
            }
            else {
                luaL_error(L, "%s", error_message);
            }
        }
        return object;
    }

    void RegisterUserType(lua_State* L, const char* name, const luaL_reg methods[], const luaL_reg meta[]) {
        luaL_register(L, name, methods);   // create methods table, add it to the globals
        int methods_idx = lua_gettop(L);
        luaL_newmetatable(L, name);                         // create metatable for ScriptInstance, add it to the Lua registry
        int metatable_idx = lua_gettop(L);
        luaL_register(L, 0, meta);                   // fill metatable

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, methods_idx);                       // dup methods table
        lua_settable(L, metatable_idx);
        lua_pop(L, 2);
    }

    static bool GetMetaFunction(lua_State* L, int index, const char* meta_table_key) {
        if (lua_getmetatable(L, index)) {
            lua_pushstring(L, meta_table_key);
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
        int top = lua_gettop(L);
        (void)top;
        GetInstance(L);
        if (GetMetaFunction(L, -1, META_TABLE_RESOLVE_PATH)) {
            lua_pushvalue(L, -2);
            lua_pushlstring(L, path, path_size);
            lua_call(L, 2, 1);
            out_hash = CheckHash(L, -1);
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return true;
        }
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return false;
    }

    bool GetURL(lua_State* L, dmMessage::URL& out_url) {
        int top = lua_gettop(L);
        (void)top;
        GetInstance(L);
        if (GetMetaFunction(L, -1, META_TABLE_GET_URL)) {
            lua_pushvalue(L, -2);
            lua_call(L, 1, 1);
            out_url = *CheckURL(L, -1);
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return true;
        }
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return false;
    }

    bool GetUserData(lua_State* L, uintptr_t& out_user_data, const char* user_type) {
        int top = lua_gettop(L);
        (void)top;
        GetInstance(L);
        if (!dmScript::IsUserType(L, -1, user_type)) {
            lua_pop(L, 1);
            return false;
        }
        if (GetMetaFunction(L, -1, META_TABLE_GET_USER_DATA)) {
            lua_pushvalue(L, -2);
            lua_call(L, 1, 1);
            out_user_data = (uintptr_t)lua_touserdata(L, -1);
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return true;
        }
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return false;
    }

    bool IsValidInstance(lua_State* L) {
        int top = lua_gettop(L);
        (void)top;
        GetInstance(L);
        if (GetMetaFunction(L, -1, META_TABLE_IS_VALID)) {
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

    static int BacktraceErrorHandler(lua_State *m_state) {
        if (!lua_isstring(m_state, 1))
            return 1;

        lua_createtable(m_state, 0, 2);
        lua_pushvalue(m_state, 1);
        lua_setfield(m_state, -2, "error");

        lua_getfield(m_state, LUA_GLOBALSINDEX, "debug");
        if (!lua_istable(m_state, -1)) {
            lua_pop(m_state, 2);
            return 1;
        }
        lua_getfield(m_state, -1, "traceback");
        if (!lua_isfunction(m_state, -1)) {
            lua_pop(m_state, 3);
            return 1;
        }

        lua_pushstring(m_state, "");
        lua_pushinteger(m_state, 2);
        lua_call(m_state, 2, 1);  /* call debug.traceback */
        lua_setfield(m_state, -3, "traceback");
        lua_pop(m_state, 1);
        return 1;
    }

    static int PCallInternal(lua_State* L, int nargs, int nresult, int in_error_handler) {
        lua_pushcfunction(L, BacktraceErrorHandler);
        int err_index = lua_gettop(L) - nargs - 1;
        lua_insert(L, err_index);
        int result = lua_pcall(L, nargs, nresult, err_index);
        lua_remove(L, err_index);
        if (result == LUA_ERRMEM) {
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
            dmLogError("%s%s", lua_tostring(L, -2), lua_tostring(L, -1));
            lua_getfield(L, LUA_GLOBALSINDEX, "debug");
            if (lua_istable(L, -1)) {
                lua_pushstring(L, SCRIPT_ERROR_HANDLER_VAR);
                lua_rawget(L, -2);
                if (lua_isfunction(L, -1)) {
                    lua_pushstring(L, "lua"); // 1st arg: source = 'lua'
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

    LuaStackCheck::LuaStackCheck(lua_State* L, int diff) : m_L(L), m_Top(lua_gettop(L)), m_Diff(diff)
    {
        assert(m_Diff >= 0);
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
        m_Diff = -1;
        return lua_error(m_L);
    }

    void LuaStackCheck::Verify(int diff)
    {
        uint32_t expected = m_Top + diff;
        uint32_t actual = lua_gettop(m_L);
        if (expected != actual)
        {
            dmLogError("Unbalanced Lua stack, expected (%d), actual (%d)", expected, actual);
            assert(expected == actual);
        }
    }

    LuaStackCheck::~LuaStackCheck()
    {
        if (m_Diff >= 0) {
            Verify(m_Diff);
        }
    }

    void RegisterCallback(lua_State* L, int index, LuaCallbackInfo* cbk)
    {
        if(cbk->m_Callback != LUA_NOREF)
        {
            dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
            dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
        }

        cbk->m_L = dmScript::GetMainThread(L);

        luaL_checktype(L, index, LUA_TFUNCTION);
        lua_pushvalue(L, index);
        cbk->m_Callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

        dmScript::GetInstance(L);
        cbk->m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
    }

    bool IsValidCallback(LuaCallbackInfo* cbk)
    {
        if (cbk->m_Callback == LUA_NOREF ||
            cbk->m_Self == LUA_NOREF ||
            cbk->m_L == NULL) {
            return false;
        }
        return true;
    }

    void UnregisterCallback(LuaCallbackInfo* cbk)
    {
        if(cbk->m_Callback != LUA_NOREF)
        {
            dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Callback);
            dmScript::Unref(cbk->m_L, LUA_REGISTRYINDEX, cbk->m_Self);
            cbk->m_Callback = LUA_NOREF;
            cbk->m_Self = LUA_NOREF;
            cbk->m_L = 0;
        }
        else
        {
            if (cbk->m_L)
                luaL_error(cbk->m_L, "Failed to unregister callback (it was not registered)");
            else
                dmLogWarning("Failed to unregister callback (it was not registered)");
        }
    }

    bool InvokeCallback(LuaCallbackInfo* cbk, LuaCallbackUserFn fn, void* user_context)
    {
        if(cbk->m_Callback == LUA_NOREF)
        {
            luaL_error(cbk->m_L, "Failed to invoke callback (it was not registered)");
            return false;
        }

        lua_State* L = cbk->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_Callback);
        lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_Self); // Setup self (the script instance)
        lua_pushvalue(L, -1);
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            lua_pop(L, 2);
            DM_LUA_ERROR("Could not run callback because the instance has been deleted");
            return false;
        }

        int user_args_start = lua_gettop(L);

        if (fn)
            fn(L, user_context);

        int user_args_end = lua_gettop(L);

        int number_of_arguments = 1 + user_args_end - user_args_start; // instance + number of arguments that the user pushed
        int ret = dmScript::PCall(L, number_of_arguments, 0);
        if (ret != 0) {
            dmLogError("Error running callback: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            return false;
        }

        return true;
    }

    const char* GetTableStringValue(lua_State* L, int table_index, const char* key, const char* default_value)
    {
        int top = lua_gettop(L);
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

        assert(top == lua_gettop(L));
        return r;
    }

    int GetTableIntValue(lua_State* L, int table_index, const char* key, int default_value)
    {
        int top = lua_gettop(L);
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

        assert(top == lua_gettop(L));
        return r;
    }

    /*
        The timers are stored in a flat array with no holes which we scan sequenctially on each update.

        When a timer is removed the last timer in the list may change location (EraseSwap).

        This is to keep the array sweep fast and handle cases where multiple short lived timers are
        created followed by one long-lived timer. If we did not re-shuffle and keep holes we would keep
        scanning the array to the end where the only live timer exists skipping all the holes.
        How much of an issue this is is guesswork at this point.

        The timer identity is an index into an indirection layer combined with a generation counter,
        this makes it possible to reuse the index for the indirection layer without risk of using
        stale indexes - the caller to CancelTimer is allowed to call with an id of a timer that already
        has expired.

        We also keep track of all timers associated with a specific owner, we do this by
        creating a linked list (using indirected lookup indexes) inside the timer array and just
        holding the most recently added timer to a specific owner in a hash table for fast lookup.

        This is to avoid scanning the entire timer array for a specific owner for items to remove.

        Each game object needs to call CancelTimers for its owner to clean up potential timers
        that has not yet been cancelled. We don't want each GO to scan the entire array for timers at destuction.
        The m_OwnerToFirstId make the cleanup of a owner with zero timers fast and the linked list
        makes it possible to remove individual timers and still keep the m_OwnerToFirstId lookup valid.

        m_IndexLookup

            Lookup index               Timer array index
             0 <---------------------   3
             1                       |  0  
             2 <------------------   |  2
             3                    |  |  4
             4                    |  |  1
                                  |  |
        m_OwnerToFirstId          |  |
                                  |  |
            Script context        |  |  Lookup index
             1                    |   -- 0
             2                     ----- 2

           -----------------------------------
        0 | m_Id: 0x0000 0001                 | <--------------
          | m_Owner 1                         |            |   |
          | m_PrevOwnerLookupIndex 3          | --------   |   |
          | m_NextOwnerLookupIndex 4          | ---     |  |   |
           -----------------------------------     |    |  |   |
        1 | m_Id: 0x0002 0004                 | <--     |  |   |
          | m_Owner 1                         |         |  |   |
          | m_PrevOwnerLookupIndex 1          | --------|--    |
          | m_NextOwnerLookupIndex -1         |         |      |
           -----------------------------------          |      |
        2 | m_Id: 0x0000 0002                 |         |      |   <-   m_OwnerToFirstId[2] -> m_IndexLookup[2] -> m_Timers[2]
          | m_Owner 2                         |         |      |
          | m_PrevOwnerLookupIndex -1         |         |      |
          | m_NextOwnerLookupIndex -1         |         |      |
           -----------------------------------          |      |
        3 | m_Id: 0x0001 0000                 | <----   |      |   <-   m_OwnerToFirstId[1] -> m_IndexLookup[0] -> m_Timers[3]
          | m_Owner 1                         |      |  |      |
          | m_PrevOwnerLookupIndex -1         |      |  |      |
          | m_NextOwnerLookupIndex 3          | --   |  |      |
           -----------------------------------    |  |  |      |
        4 | m_Id: 0x0000 0003                 | <----|--       |
          | m_Owner 1                         |      |         |
          | m_PrevOwnerLookupIndex 0          | -----          |
          | m_NextOwnerLookupIndex 1          | ---------------
           -----------------------------------
    */

    struct Timer
    {
        TimerTrigger    m_Trigger;
        uintptr_t       m_Owner;

        // Store complete timer id with generation here to identify stale timer ids
        HTimer          m_Id;

        // How much time remaining until the timer fires
        float           m_Remaining;

        // The timer interval, we need to keep this for repeating timers
        float           m_Interval;

        int             m_SelfRef;
        int             m_CallbackRef;

        // Linked list of all timers with same owner
        uint16_t        m_PrevOwnerLookupIndex;
        uint16_t        m_NextOwnerLookupIndex;

        // Flag if the timer should repeat
        uint32_t        m_Repeat : 1;
        // Flag if the timer is alive
        uint32_t        m_IsAlive : 1;
    };

    #define INVALID_TIMER_LOOKUP_INDEX  0xffffu
    #define INITIAL_TIMER_CAPACITY      256u
    #define MAX_TIMER_CAPACITY          65000u  // Needs to be less that 65536 since 65535 is reserved for invalid index
    #define MIN_TIMER_CAPACITY_GROWTH   2048u

    struct TimerContext
    {
        dmArray<Timer>                      m_Timers;
        dmArray<uint16_t>                   m_IndexLookup;
        dmIndexPool<uint16_t>               m_IndexPool;
        dmHashTable<uintptr_t, HTimer>      m_OwnerToFirstId;
        uint16_t                            m_Version;   // Incremented to avoid collisions each time we push timer indexes back to the m_IndexPool
        uint16_t                            m_InUpdate : 1;
    };

    static uint16_t GetLookupIndex(HTimer id)
    {
        return (uint16_t)(id & 0xffffu);
    }

    static HTimer MakeId(uint16_t generation, uint16_t lookup_index)
    {
        return (((uint32_t)generation) << 16) | (lookup_index);
    }

    static Timer* AllocateTimer(HTimerContext timer_context, uintptr_t owner)
    {
        assert(timer_context != 0x0);
        uint32_t timer_count = timer_context->m_Timers.Size();
        if (timer_count == MAX_TIMER_CAPACITY)
        {
            dmLogError("Timer could not be stored since the timer buffer is full (%d).", MAX_TIMER_CAPACITY);
            return 0x0;
        }

        HTimer* id_ptr = timer_context->m_OwnerToFirstId.Get(owner);
        if ((id_ptr == 0x0) && timer_context->m_OwnerToFirstId.Full())
        {
            dmLogError("Timer could not be stored since the owner lookup buffer is full (%d).", timer_context->m_OwnerToFirstId.Size());
            return 0x0;
        }

        if (timer_context->m_IndexPool.Remaining() == 0)
        {
            // Growth heuristic is to grow with the mean of MIN_TIMER_CAPACITY_GROWTH and half current capacity, and at least MIN_TIMER_CAPACITY_GROWTH
            uint32_t old_capacity = timer_context->m_IndexPool.Capacity();
            uint32_t growth = dmMath::Min(MIN_TIMER_CAPACITY_GROWTH, (MIN_TIMER_CAPACITY_GROWTH + old_capacity / 2) / 2);
            uint32_t capacity = dmMath::Min(old_capacity + growth, MAX_TIMER_CAPACITY);
            timer_context->m_IndexPool.SetCapacity(capacity);
            timer_context->m_IndexLookup.SetCapacity(capacity);
            timer_context->m_IndexLookup.SetSize(capacity);
            memset(&timer_context->m_IndexLookup[old_capacity], 0u, (capacity - old_capacity) * sizeof(uint16_t));
        }

        HTimer id = MakeId(timer_context->m_Version, timer_context->m_IndexPool.Pop());

        if (timer_context->m_Timers.Full())
        {
            // Growth heuristic is to grow with the mean of MIN_TIMER_CAPACITY_GROWTH and half current capacity, and at least MIN_TIMER_CAPACITY_GROWTH
            uint32_t capacity = timer_context->m_Timers.Capacity();
            uint32_t growth = dmMath::Min(MIN_TIMER_CAPACITY_GROWTH, (MIN_TIMER_CAPACITY_GROWTH + capacity / 2) / 2);
            capacity = dmMath::Min(capacity + growth, MAX_TIMER_CAPACITY);
            timer_context->m_Timers.SetCapacity(capacity);
        }

        timer_context->m_Timers.SetSize(timer_count + 1);
        Timer& timer = timer_context->m_Timers[timer_count];
        timer.m_Id = id;
        timer.m_Owner = owner;

        timer.m_PrevOwnerLookupIndex = INVALID_TIMER_LOOKUP_INDEX;
        if (id_ptr == 0x0)
        {
            timer.m_NextOwnerLookupIndex = INVALID_TIMER_LOOKUP_INDEX;
        }
        else
        {
            uint16_t next_lookup_index = GetLookupIndex(*id_ptr);
            uint32_t next_timer_index = timer_context->m_IndexLookup[next_lookup_index];
            Timer& nextTimer = timer_context->m_Timers[next_timer_index];
            nextTimer.m_PrevOwnerLookupIndex = GetLookupIndex(id);
            timer.m_NextOwnerLookupIndex = next_lookup_index;
        }

        uint16_t lookup_index = GetLookupIndex(id);

        timer_context->m_IndexLookup[lookup_index] = timer_count;
        if (id_ptr == 0x0)
        {
            timer_context->m_OwnerToFirstId.Put(owner, id);
        }
        else
        {
            *id_ptr = id;
        }
        return &timer;
    }

    static void EraseTimer(HTimerContext timer_context, uint32_t timer_index)
    {
        Timer& movedTimer = timer_context->m_Timers.EraseSwap(timer_index);

        if (timer_index < timer_context->m_Timers.Size())
        {
            uint16_t moved_lookup_index = GetLookupIndex(movedTimer.m_Id);
            timer_context->m_IndexLookup[moved_lookup_index] = timer_index;
        }
    }

    static void FreeTimer(HTimerContext timer_context, Timer& timer)
    {
        assert(timer_context != 0x0);
        assert(timer.m_IsAlive == 0);

        uint16_t lookup_index = GetLookupIndex(timer.m_Id);
        uint16_t timer_index = timer_context->m_IndexLookup[lookup_index];
        timer_context->m_IndexPool.Push(lookup_index);

        uint16_t prev_lookup_index = timer.m_PrevOwnerLookupIndex;
        uint16_t next_lookup_index = timer.m_NextOwnerLookupIndex;

        bool is_first_timer = INVALID_TIMER_LOOKUP_INDEX == prev_lookup_index;
        bool is_last_timer = INVALID_TIMER_LOOKUP_INDEX == next_lookup_index;

        if (is_first_timer && is_last_timer)
        {
            timer_context->m_OwnerToFirstId.Erase(timer.m_Owner);
        }
        else
        {
            if (!is_last_timer)
            {
                uint32_t next_timer_index = timer_context->m_IndexLookup[next_lookup_index];
                Timer& next_timer = timer_context->m_Timers[next_timer_index];
                next_timer.m_PrevOwnerLookupIndex = prev_lookup_index;

                if (is_first_timer)
                {
                    HTimer* id_ptr = timer_context->m_OwnerToFirstId.Get(timer.m_Owner);
                    assert(id_ptr != 0x0);
                    *id_ptr = next_timer.m_Id;
                }
            }

            if (!is_first_timer)
            {
                uint32_t prev_timer_index = timer_context->m_IndexLookup[prev_lookup_index];
                Timer& prev_timer = timer_context->m_Timers[prev_timer_index];
                prev_timer.m_NextOwnerLookupIndex = next_lookup_index;
            }
        }

        EraseTimer(timer_context, timer_index);
    }

    HTimerContext NewTimerContext(uint16_t max_owner_count)
    {
        TimerContext* timer_context = new TimerContext();
        timer_context->m_Timers.SetCapacity(INITIAL_TIMER_CAPACITY);
        timer_context->m_IndexLookup.SetCapacity(INITIAL_TIMER_CAPACITY);
        timer_context->m_IndexLookup.SetSize(INITIAL_TIMER_CAPACITY);
        memset(&timer_context->m_IndexLookup[0], 0u, INITIAL_TIMER_CAPACITY * sizeof(uint16_t));
        timer_context->m_IndexPool.SetCapacity(INITIAL_TIMER_CAPACITY);
        const uint32_t table_count = dmMath::Max(1, max_owner_count/3);
        timer_context->m_OwnerToFirstId.SetCapacity(table_count, max_owner_count);
        timer_context->m_Version = 0;
        timer_context->m_InUpdate = 0;
        return timer_context;
    }

    void DeleteTimerContext(HTimerContext timer_context)
    {
        assert(timer_context->m_InUpdate == 0);
        delete timer_context;
    }

    void UpdateTimerContext(HTimerContext timer_context, float dt)
    {
        assert(timer_context != 0x0);
        DM_PROFILE(TimerContext, "Update");

        timer_context->m_InUpdate = 1;

        // We only scan timers for trigger if the timer *existed at entry to UpdateTimerContext*, any timers added
        // in a trigger callback will always be added at the end of m_Timers and not triggered in this scope.
        uint32_t size = timer_context->m_Timers.Size();
        DM_COUNTER("timerc", size);

        uint32_t i = 0;
        while (i < size)
        {
            Timer& timer = timer_context->m_Timers[i];
            if (timer.m_IsAlive == 1)
            {
                assert(timer.m_Remaining >= 0.0f);

                timer.m_Remaining -= dt;
                if (timer.m_Remaining <= 0.0f)
                {
                    assert(timer.m_Trigger != 0x0);

                    timer.m_IsAlive = timer.m_Repeat;

                    float elapsed_time = timer.m_Interval - timer.m_Remaining;

                    timer.m_Trigger(timer_context, timer.m_Id, elapsed_time, timer.m_Owner, timer.m_SelfRef, timer.m_CallbackRef);

                    if (timer.m_Repeat == 1)
                    {
                        if (timer.m_Interval == 0.0f)
                        {
                            timer.m_Remaining = timer.m_Interval;
                        }
                        else if (timer.m_Remaining == 0.0f)
                        {
                            timer.m_Remaining = timer.m_Interval;
                        }
                        else if (timer.m_Remaining < 0.0f)
                        {
                            float wrapped_count = ((-timer.m_Remaining) / timer.m_Interval) + 1.f;
                            float offset_to_next_trigger  = floor(wrapped_count) * timer.m_Interval;
                            timer.m_Remaining += offset_to_next_trigger;
                            assert(timer.m_Remaining >= 0.f);
                        }
                    }
                }
            }
            ++i;
        }
        timer_context->m_InUpdate = 0;

        size = timer_context->m_Timers.Size();
        uint32_t original_size = size;
        i = 0;
        while (i < size)
        {
            Timer& timer = timer_context->m_Timers[i];
            if (timer.m_IsAlive == 0)
            {
                FreeTimer(timer_context, timer);
                --size;
            }
            else
            {
                ++i;
            }
        }

        if (size != original_size)
        {
            ++timer_context->m_Version;            
        }
    }

    HTimer AddTimer(HTimerContext timer_context,
                    float delay,
                    bool repeat,
                    TimerTrigger timer_trigger,
                    uintptr_t owner,
                    int self_ref,
                    int callback_ref)
    {
        assert(timer_context != 0x0);
        assert(delay >= 0.f);
        Timer* timer = AllocateTimer(timer_context, owner);
        if (timer == 0x0)
        {
            return INVALID_TIMER_ID;
        }

        timer->m_Interval = delay;
        timer->m_Remaining = delay;
        timer->m_SelfRef = self_ref;
        timer->m_CallbackRef = callback_ref;
        timer->m_Trigger = timer_trigger;
        timer->m_Repeat = repeat;
        timer->m_IsAlive = 1;

        return timer->m_Id;
    }

    bool CancelTimer(HTimerContext timer_context, HTimer id)
    {
        assert(timer_context != 0x0);
        uint16_t lookup_index = GetLookupIndex(id);
        if (lookup_index >= timer_context->m_IndexLookup.Size())
        {
            return false;
        }

        uint16_t timer_index = timer_context->m_IndexLookup[lookup_index];
        if (timer_index >= timer_context->m_Timers.Size())
        {
            return false;
        }

        Timer& timer = timer_context->m_Timers[timer_index];
        if (timer.m_Id != id)
        {
            return false;
        }

        bool cancelled = timer.m_IsAlive == 1;
        timer.m_IsAlive = 0;

        if (timer_context->m_InUpdate == 0)
        {
            FreeTimer(timer_context, timer);
            ++timer_context->m_Version;
        }
        return cancelled;
    }

    uint32_t CancelTimers(HTimerContext timer_context, uintptr_t owner)
    {
        assert(timer_context != 0x0);
        HTimer* id_ptr = timer_context->m_OwnerToFirstId.Get(owner);
        if (id_ptr == 0x0)
            return 0u;

        ++timer_context->m_Version;

        uint32_t cancelled_count = 0u;
        uint16_t lookup_index = GetLookupIndex(*id_ptr);
        do
        {
            timer_context->m_IndexPool.Push(lookup_index);

            uint16_t timer_index = timer_context->m_IndexLookup[lookup_index];
            Timer& timer = timer_context->m_Timers[timer_index];
            lookup_index = timer.m_NextOwnerLookupIndex;

            if (timer.m_IsAlive == 1)
            {
                timer.m_IsAlive = 0;
                ++cancelled_count;
            }

            if (timer_context->m_InUpdate == 0)
            {
                EraseTimer(timer_context, timer_index);
            }
        } while (lookup_index != INVALID_TIMER_LOOKUP_INDEX);

        timer_context->m_OwnerToFirstId.Erase(owner);
        return cancelled_count;
    }

    uint32_t GetAliveTimers(HTimerContext timer_context)
    {
        uint32_t alive_timers = 0u;
        uint32_t size = timer_context->m_Timers.Size();
        uint32_t i = 0;
        while (i < size)
        {
            Timer& timer = timer_context->m_Timers[i];
            if(timer.m_IsAlive == 1)
            {
                ++alive_timers;
            }
            ++i;
        }
        return alive_timers;
    }
    
}
