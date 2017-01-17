#include "liveupdate.h"
#include "liveupdate_private.h"

#include <resource/resource.h>

namespace dmLiveUpdate
{

    const int MAX_MANIFESTS = 8;
    const int CURRENT_MANIFEST = 0x0ac83fcc;

    struct LiveUpdate
    {
        LiveUpdate()
        {
            memset(this, 0x0, sizeof(*this));
        }

        dmResource::Manifest* m_Manifest;
        dmResource::Manifest* m_Manifests[MAX_MANIFESTS];
    };

    LiveUpdate g_LiveUpdate;

    /** ***********************************************************************
     ** LiveUpdate utility functions
     ********************************************************************** **/


    /** ***********************************************************************
     ** Lua API functions
     ********************************************************************** **/

    int LiveUpdate_GetCurrentManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_CreateManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_DestroyManifest(lua_State* L)
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

    int LiveUpdate_VerifyManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int LiveUpdate_StoreManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }


    /** ***********************************************************************
     ** Engine integration
     ********************************************************************** **/

    static const luaL_reg Resource_methods[] =
    {
        {"get_current_manifest", LiveUpdate_GetCurrentManifest},
        {"create_manifest", LiveUpdate_CreateManifest},
        {"destroy_manifest", LiveUpdate_DestroyManifest},
        {"verify_resource", LiveUpdate_VerifyResource},
        {"store_resource", LiveUpdate_StoreResource},
        {"verify_manifest", LiveUpdate_VerifyManifest},
        {"store_manifest", LiveUpdate_StoreManifest},
        {0, 0}
    };

    static const luaL_reg CollectionProxy_methods[] =
    {
        {"missing_resources", LiveUpdate_MissingResources},
        {0, 0}
    };

    void LuaInit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, "resource", Resource_methods);
        lua_pop(L, 1);

        luaL_register(L, "collectionproxy", CollectionProxy_methods);
        lua_pop(L, 1);
    }

    void Initialize(const dmGameSystem::ScriptLibContext& context)
    {
        g_LiveUpdate.m_Manifest = dmResource::GetManifest(context.m_Factory);
        dmLiveUpdate::LuaInit(context.m_LuaState);
    }

    void Finalize(const dmGameSystem::ScriptLibContext& context)
    {
        g_LiveUpdate.m_Manifest = 0x0;
    }

};
