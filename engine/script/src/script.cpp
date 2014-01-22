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
#include "script_sys.h"
#include "script_module.h"
#include "script_image.h"
#include "script_json.h"
#include "script_http.h"
#include "script_zlib.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    const char* INSTANCE_NAME = "__dm_script_instance__";

    HContext NewContext(dmConfigFile::HConfig config_file, dmResource::HFactory factory)
    {
        Context* context = new Context();
        context->m_Modules.SetCapacity(127, 256);
        context->m_HashInstances.SetCapacity(443, 256);
        context->m_ConfigFile = config_file;
        context->m_ResourceFactory = factory;
        memset(context->m_InitializedExtensions, 0, sizeof(context->m_InitializedExtensions));
        return context;
    }

    void DeleteContext(HContext context)
    {
        ClearModules(context);
        delete context;
    }

    ScriptParams::ScriptParams()
    {
        memset(this, 0, sizeof(ScriptParams));
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

        lua_Number r = (lua_Number)dmMath::Rand(seed) / (lua_Number)DM_RAND_MAX;
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

    void Initialize(lua_State* L, const ScriptParams& params)
    {
        int top = lua_gettop(L);

        InitializeHash(L);
        InitializeMsg(L);
        InitializeVmath(L);
        InitializeSys(L);
        InitializeModule(L);
        InitializeImage(L);
        InitializeJson(L);
        InitializeHttp(L, params.m_Context->m_ConfigFile);
        InitializeZlib(L);

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

        lua_pushlightuserdata(L, (void*)params.m_Context);
        lua_setglobal(L, SCRIPT_CONTEXT);

        lua_pushlightuserdata(L, (void*)params.m_ResolvePathCallback);
        lua_setglobal(L, SCRIPT_RESOLVE_PATH_CALLBACK);

        lua_pushlightuserdata(L, (void*)params.m_GetURLCallback);
        lua_setglobal(L, SCRIPT_GET_URL_CALLBACK);

        lua_pushlightuserdata(L, (void*)params.m_GetUserDataCallback);
        lua_setglobal(L, SCRIPT_GET_USER_DATA_CALLBACK);

        lua_pushlightuserdata(L, (void*)params.m_ValidateInstanceCallback);
        lua_setglobal(L, SCRIPT_VALIDATE_INSTANCE_CALLBACK);

#define BIT_INDEX(b) ((b) / sizeof(uint32_t))
#define BIT_OFFSET(b) ((b) % sizeof(uint32_t))

        const dmExtension::Desc* ed = dmExtension::GetFirstExtension();
        uint32_t i = 0;
        while (ed) {
            dmExtension::Params p;
            p.m_ConfigFile = params.m_Context->m_ConfigFile;
            p.m_L = L;
            dmExtension::Result r = ed->Initialize(&p);
            if (r == dmExtension::RESULT_OK) {
                params.m_Context->m_InitializedExtensions[BIT_INDEX(i)] |= 1 << BIT_OFFSET(i);
            } else {
                dmLogError("Failed to initialize extension: %s", ed->m_Name);
            }
            ++i;
            ed = ed->m_Next;
        }

        assert(top == lua_gettop(L));
    }

    void Finalize(lua_State* L, HContext context)
    {
        FinalizeHttp(L);

        const dmExtension::Desc* ed = dmExtension::GetFirstExtension();
        uint32_t i = 0;
        while (ed) {
            dmExtension::Params p;
            p.m_ConfigFile = context->m_ConfigFile;
            p.m_L = L;
            if (context->m_InitializedExtensions[BIT_INDEX(i)] & (1 << BIT_OFFSET(i))) {
                dmExtension::Result r = ed->Finalize(&p);
                if (r != dmExtension::RESULT_OK) {
                    dmLogError("Failed to finalize extension: %s", ed->m_Name);
                }
            }
            ++i;
            ed = ed->m_Next;
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

    int LuaPrint(lua_State* L)
    {
        int n = lua_gettop(L);
        lua_getglobal(L, "tostring");
        char buffer[256];
        buffer[0] = 0;
        for (int i = 1; i <= n; ++i)
        {
            const char *s;
            lua_pushvalue(L, -1);
            lua_pushvalue(L, i);
            lua_call(L, 1, 1);
            s = lua_tostring(L, -1);
            if (s == 0x0)
                return luaL_error(L, LUA_QL("tostring") " must return a string to ", LUA_QL("print"));
            if (i > 1)
                dmStrlCat(buffer, "\t", 256);
            dmStrlCat(buffer, s, 256);
            lua_pop(L, 1);
        }
        dmLogUserDebug("%s", buffer);
        lua_pop(L, 1);
        assert(n == lua_gettop(L));
        return 0;
    }

    static int DoLuaPPrintTable(lua_State*L, int index, dmPPrint::Printer* printer) {
        int top = lua_gettop(L);

        lua_pushvalue(L, index);
        lua_pushnil(L);

        printer->Printf("{\n");
        printer->Indent(2);

        while (lua_next(L, -2) != 0) {
            int key_type = lua_type(L, -2);
            int value_type = lua_type(L, -1);

            lua_pushvalue(L, -2);
            const char *s1;
            const char *s2;
            lua_getglobal(L, "tostring");
            lua_pushvalue(L, -2);
            lua_call(L, 1, 1);
            s1 = lua_tostring(L, -1);
            if (s1 == 0x0)
                return luaL_error(L, LUA_QL("tostring") " must return a string to ", LUA_QL("print"));
            lua_pop(L, 1);

            lua_getglobal(L, "tostring");
            lua_pushvalue(L, -3);
            lua_call(L, 1, 1);
            s2 = lua_tostring(L, -1);
            if (s2 == 0x0)
                return luaL_error(L, LUA_QL("tostring") " must return a string to ", LUA_QL("print"));
            lua_pop(L, 1);

            if (value_type == LUA_TTABLE) {
                printer->Printf("%s = ", s1);
                DoLuaPPrintTable(L, -2, printer);
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
     * Pretty printing of lua values
     *
     * @name pprint
     * @param v value to print
     */
    int LuaPPrint(lua_State* L)
    {
        int n = lua_gettop(L);

        char buf[2048];
        dmPPrint::Printer printer(buf, sizeof(buf));
        if (lua_type(L, 1) == LUA_TTABLE) {
            printer.Printf("\n");
            DoLuaPPrintTable(L, 1, &printer);
        } else {
            lua_getglobal(L, "tostring");
            lua_pushvalue(L, 1);
            lua_call(L, 1, 1);
            const char* s = lua_tostring(L, -1);
            if (s == 0x0)
                return luaL_error(L, LUA_QL("tostring") " must return a string to ", LUA_QL("print"));
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
        int top = lua_gettop(L);
        (void)top;
        lua_getglobal(L, SCRIPT_VALIDATE_INSTANCE_CALLBACK);
        ValidateInstanceCallback callback = (ValidateInstanceCallback)lua_touserdata(L, -1);
        lua_pop(L, 1);
        if (callback == 0x0)
        {
            dmLogFatal("ValidateInstanceCallback not set, impossible to validate.");
        }
        assert(callback != 0x0);
        bool result = callback(L);
        assert(top == lua_gettop(L));
        return result;
    }
}
