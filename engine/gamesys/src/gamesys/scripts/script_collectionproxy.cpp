#include "script_collectionproxy.h"
#include "script_resource_liveupdate.h"

#include <script/script.h>
#include "../gamesys.h"

namespace dmGameSystem
{

    static const luaL_reg Module_methods[] =
    {
        {"missing_resources", dmLiveUpdate::CollectionProxy_MissingResources},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "collectionproxy", Module_methods);

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

    void ScriptCollectionProxyRegister(const ScriptLibContext& context)
    {
        LuaInit(context.m_LuaState);
    }

    void ScriptCollectionProxyFinalize(const ScriptLibContext& context)
    {
    }

};
