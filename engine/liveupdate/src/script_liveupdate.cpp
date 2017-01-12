#include "liveupdate_private.h"
#include <extension/extension.h>
#include <script/script.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

namespace dmLiveUpdate
{


    int LiveUpdate_GetCurrentManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_MissingResources(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_VerifyResource(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_StoreResource(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    static const luaL_reg LiveUpdate_methods[] =
    {
        {"get_current_manifest", LiveUpdate_GetCurrentManifest},
        {"missing_resources", LiveUpdate_MissingResources},
        {"verify_resource", LiveUpdate_VerifyResource},
        {"store_resource", LiveUpdate_StoreResource},
        {0, 0}
    };

    void LuaInit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, LIB_NAME, LiveUpdate_methods);
    }
};