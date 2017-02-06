#include "script_resource_liveupdate.h"
#include <liveupdate/liveupdate.h>

#include <script/script.h>
#include <dlib/log.h>

namespace dmLiveUpdate
{

    struct StoreResourceEntry
    {

        StoreResourceEntry() {
            memset(this, 0x0, sizeof(*this));
        }

        lua_State*  m_L;
        int         m_Callback;
        int         m_Self;
        const char* m_HexDigest;
        bool        m_Status;
    };

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

    static void Callback_StoreResource(StoreResourceEntry* entry)
    {
        lua_State* L = entry->m_L;
        DM_LUA_STACK_CHECK(L, 0);

        lua_rawgeti(L, LUA_REGISTRYINDEX, entry->m_Callback);
        lua_rawgeti(L, LUA_REGISTRYINDEX, entry->m_Self);
        lua_pushvalue(L, -1);

        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run store_resource callback since the instance has been deleted.");
            lua_pop(L, 2);
            return;
        }

        lua_pushstring(L, entry->m_HexDigest);
        lua_pushboolean(L, entry->m_Status);

        int ret = lua_pcall(L, 3, 0, 0);
        if (ret != 0)
        {
            dmLogError("Error while running store_resource callback: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    int Resource_StoreResource(lua_State* L)
    {
        int top = lua_gettop(L);

        int manifestIndex = luaL_checkint(L, 1);

        size_t buflen = 0;
        const char* buf = luaL_checklstring(L, 2, &buflen);
        dmResourceArchive::LiveUpdateResource resource((const uint8_t*) buf, buflen);

        size_t hexDigestLength = 0;
        const char* hexDigest = luaL_checklstring(L, 3, &hexDigestLength);

        luaL_checktype(L, 4, LUA_TFUNCTION);
        lua_pushvalue(L, 4);
        int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

        dmResource::Manifest* manifest = dmLiveUpdate::GetManifest(manifestIndex);

        if (manifest == 0x0)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }

        // BEGIN Temporary solution until async
        StoreResourceEntry entry;
        entry.m_L = dmScript::GetMainThread(L);
        dmScript::GetInstance(L);
        entry.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
        entry.m_Callback = callback;
        entry.m_HexDigest = hexDigest;
        entry.m_Status = dmLiveUpdate::StoreResource(manifest, hexDigest, hexDigestLength, &resource);
        Callback_StoreResource(&entry);
        // END Temporary solution until async

        assert(lua_gettop(L) == top);
        return 0;
    }

    int Resource_StoreManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        return 0;
    }

};
