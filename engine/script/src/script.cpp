#include "script.h"

#include <dlib/log.h>

#include "script_private.h"
#include "script_hash.h"
#include "script_msg.h"
#include "script_vmath.h"
#include "script_sys.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript
{
    HContext NewContext(dmConfigFile::HConfig config_file)
    {
        Context* context = new Context();
        context->m_ConfigFile = config_file;
        return context;
    }

    void DeleteContext(HContext context)
    {
        delete context;
    }

    ScriptParams::ScriptParams()
    : m_Context(0x0)
    , m_ResolvePathCallback(0x0)
    , m_GetURLCallback(0x0)
    , m_GetUserDataCallback(0x0)
    {

    }

    void Initialize(lua_State* L, const ScriptParams& params)
    {
        InitializeHash(L);
        InitializeMsg(L);
        InitializeVmath(L);
        InitializeSys(L);

        lua_pushlightuserdata(L, (void*)params.m_Context);
        lua_setglobal(L, SCRIPT_CONTEXT);

        lua_pushlightuserdata(L, (void*)params.m_ResolvePathCallback);
        lua_setglobal(L, SCRIPT_RESOLVE_PATH_CALLBACK);

        lua_pushlightuserdata(L, (void*)params.m_GetURLCallback);
        lua_setglobal(L, SCRIPT_GET_URL_CALLBACK);

        lua_pushlightuserdata(L, (void*)params.m_GetUserDataCallback);
        lua_setglobal(L, SCRIPT_GET_USER_DATA_CALLBACK);
    }

}
