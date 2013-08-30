#include "script.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
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
    : m_Context(0x0)
    , m_ResolvePathCallback(0x0)
    , m_GetURLCallback(0x0)
    , m_GetUserDataCallback(0x0)
    {

    }

    int LuaPrint(lua_State* L);

    void Initialize(lua_State* L, const ScriptParams& params)
    {
        InitializeHash(L);
        InitializeMsg(L);
        InitializeVmath(L);
        InitializeSys(L);
        InitializeModule(L);
        InitializeImage(L);
        InitializeJson(L);
        InitializeHttp(L);
        InitializeZlib(L);

        lua_register(L, "print", LuaPrint);

        lua_pushlightuserdata(L, (void*)params.m_Context);
        lua_setglobal(L, SCRIPT_CONTEXT);

        lua_pushlightuserdata(L, (void*)params.m_ResolvePathCallback);
        lua_setglobal(L, SCRIPT_RESOLVE_PATH_CALLBACK);

        lua_pushlightuserdata(L, (void*)params.m_GetURLCallback);
        lua_setglobal(L, SCRIPT_GET_URL_CALLBACK);

        lua_pushlightuserdata(L, (void*)params.m_GetUserDataCallback);
        lua_setglobal(L, SCRIPT_GET_USER_DATA_CALLBACK);

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
        return 0;
    }

}
