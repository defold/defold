#include "script.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/pprint.h>
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

    const char* META_TABLE_RESOLVE_PATH             = "__resolve_path";
    const char* META_TABLE_GET_URL                  = "__get_url";
    const char* META_TABLE_GET_USER_DATA            = "__get_user_data";
    const char* META_TABLE_IS_VALID                 = "__is_valid";
    const char* META_GET_INSTANCE_CONTEXT_TABLE_REF = "__get_instance_context_table_ref";

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
        context->m_EnableExtensions = enable_extensions;
        memset(context->m_InitializedExtensions, 0, sizeof(context->m_InitializedExtensions));
        context->m_LuaState = lua_open();
        context->m_ContextTableRef = LUA_NOREF;
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

        lua_newtable(L);
        context->m_ContextTableRef = Ref(L, LUA_REGISTRYINDEX);

        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->Initialize != 0x0)
            {
                (*l)->Initialize(context);
            }
        }

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

        for (HScriptExtension* l = context->m_ScriptExtensions.Begin(); l != context->m_ScriptExtensions.End(); ++l)
        {
            if ((*l)->Finalize != 0x0)
            {
                (*l)->Finalize(context);
            }
        }

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
        lua_pop(L, 1);

        Unref(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
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

    bool SetContextValue(HContext context)
    {
        assert(context != 0x0);
        lua_State* L = context->m_LuaState;

        DM_LUA_STACK_CHECK(L, -2);

        lua_rawgeti(L, LUA_REGISTRYINDEX, context->m_ContextTableRef);
        // [-3] key
        // [-2] value
        // [-1] context table

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 3);
            return false;
        }

        lua_insert(L, -3);
        // [-3] context table
        // [-2] key
        // [-1] value

        lua_settable(L, -3);
        // [-1] context table

        lua_pop(L, 1);
        return true;
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

        if (!GetMetaFunction(L, -1, META_GET_INSTANCE_CONTEXT_TABLE_REF))
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
        int top = lua_gettop(L);
        (void)top;
        GetInstance(L);
        int instance_type = lua_type(L, -1);
        // We assume that all users of SetInstance puts some form of user data/light user data, it is an assumption that works for now
        uintptr_t id = (instance_type == LUA_TLIGHTUSERDATA || instance_type == LUA_TUSERDATA) ? (uintptr_t)lua_touserdata(L, -1) : 0;
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
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
        if (script_world == 0x0)
        {
            return;
        }
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

    LuaStackCheck::LuaStackCheck(lua_State* L, int diff) : m_L(L), m_Top(lua_gettop(L)), m_Diff(diff)
    {
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
            dmLogError("Unbalanced Lua stack, expected (%d), actual (%d)", expected, actual);
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
    
    LuaCallbackInfo* CreateCallback(lua_State* L, int callback_stack_index)
    {
        luaL_checktype(L, callback_stack_index, LUA_TFUNCTION);

        DM_LUA_STACK_CHECK(L, 0);

        GetInstance(L);
        // [-1] instance

        if (!GetMetaFunction(L, -1, META_GET_INSTANCE_CONTEXT_TABLE_REF)) {
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

    bool IsValidCallback(LuaCallbackInfo* cbk)
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

    void DeleteCallback(LuaCallbackInfo* cbk)
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
            if (L)
                luaL_error(L, "Failed to unregister callback (it was not registered)");
            else
                dmLogWarning("Failed to unregister callback (it was not registered)");
        }
    }

    bool InvokeCallback(LuaCallbackInfo* cbk, LuaCallbackUserFn fn, void* user_context)
    {
        if(cbk->m_CallbackInfoRef == LUA_NOREF)
        {
            dmLogWarning("Failed to invoke callback (it was not registered)");
            return false;
        }

        lua_State* L = cbk->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        GetInstance(L);
        // [-1] old instance

        lua_rawgeti(L, LUA_REGISTRYINDEX, cbk->m_ContextTableRef);
        // [-2] old instance
        // [-1] context table

        if (lua_type(L, -1) != LUA_TTABLE)
        {
            lua_pop(L, 2);
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
            return false;
        }

        int user_args_start = lua_gettop(L);

        if (fn)
            fn(L, user_context);

        int user_args_end = lua_gettop(L);

        int number_of_arguments = 1 + user_args_end - user_args_start; // instance + number of arguments that the user pushed
        int ret = PCall(L, number_of_arguments, 0);

        if (ret != 0) {
            // [-2] old instance
            // [-1] context table

            lua_pop(L, 1);
            // [-1] old instance

            SetInstance(L);
            return false;
        }
        // [-2] old instance
        // [-1] context table
        lua_pop(L, 1);

        SetInstance(L);
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

}
