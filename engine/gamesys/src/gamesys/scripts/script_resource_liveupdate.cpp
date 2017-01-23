#include "script_resource_liveupdate.h"
#include <liveupdate/liveupdate.h>

#include <script/script.h>

namespace dmLiveUpdate
{

    int Resource_GetCurrentManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, dmLiveUpdate::CURRENT_MANIFEST);
        return 1;
    }

    int Resource_CreateManifest(lua_State* L)
    {
        int top = lua_gettop(L);
        size_t manifestLength = 0;
        const char* manifestData = luaL_checklstring(L, 1, &manifestLength);

        dmResource::Manifest* manifest = new dmResource::Manifest();
        dmResource::Result result = dmResource::ParseManifestDDF((uint8_t*) manifestData, manifestLength, manifest->m_DDF);

        if (result != dmResource::RESULT_OK)
        {
            delete manifest;
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest could not be parsed");
        }

        int manifestIndex = dmLiveUpdate::AddManifest(manifest);

        if (manifestIndex == -1)
        {
            delete manifest;
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest buffer is full (%d/%d)", MAX_MANIFEST_COUNT, MAX_MANIFEST_COUNT);
        }

        lua_pushnumber(L, manifestIndex);
        assert(lua_gettop(L) == (top + 1));
        return 1;
    }

    int Resource_DestroyManifest(lua_State* L)
    {
        int top = lua_gettop(L);
        int manifestIndex = luaL_checkint(L, 1);

        if (manifestIndex == dmLiveUpdate::CURRENT_MANIFEST)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "Cannot destroy the current manifest");
        }

        if (!dmLiveUpdate::RemoveManifest(manifestIndex))
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }

        assert(lua_gettop(L) == top);
        return 0;
    }

    int CollectionProxy_MissingResources(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        const char* path = luaL_checkstring(L, 1);

        char** buffer = 0x0;
        uint32_t resourceCount = dmLiveUpdate::GetMissingResources(path, &buffer);

        lua_createtable(L, resourceCount, 0);
        for (uint32_t i = 0; i < resourceCount; ++i)
        {
            lua_pushnumber(L, i + 1);
            lua_pushstring(L, buffer[i]);
            lua_settable(L, -3);
        }

        return 1;
    }

    int Resource_VerifyResource(lua_State* L)
    {
        int top = lua_gettop(L);

        int manifestIndex = luaL_checkint(L, 1);

        size_t hexDigestLength = 0;
        const char* hexDigest = luaL_checklstring(L, 2, &hexDigestLength);

        size_t buflen = 0;
        const char* buf = luaL_checklstring(L, 3, &buflen);

        dmResource::Manifest* manifest = dmLiveUpdate::GetManifest(manifestIndex);

        if (manifest == 0x0)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }

        bool validResource = dmLiveUpdate::VerifyResource(manifest, hexDigest, hexDigestLength, buf, buflen);
        lua_pushboolean(L, (int) validResource);

        assert(lua_gettop(L) == (top + 1));
        return 1;
    }

    // resource.store_resource(buffer) OLD
    // resource.store_resource(manifest_reference, expected_hash, buffer) // include resource verification before storing
    int Resource_StoreResource(lua_State* L)
    {
        int top = lua_gettop(L);

        int manifestIndex = luaL_checkint(L, 1);

        size_t hexDigestLength = 0;
        const char* hexDigest = luaL_checklstring(L, 2, &hexDigestLength);

        size_t buflen = 0;
        const char* buf = luaL_checklstring(L, 3, &buflen);

        dmResource::Manifest* manifest = dmLiveUpdate::GetManifest(manifestIndex);

        if (manifest == 0x0)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }
        
        bool resourceStored = dmLiveUpdate::StoreResource(manifest, hexDigest, hexDigestLength, buf, buflen, proj_id);
        lua_pushboolean(L, (int) resourceStored);

        assert(lua_gettop(L) == top + 1);
        return 1;
    }

    int Resource_VerifyManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

    int Resource_StoreManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

};
