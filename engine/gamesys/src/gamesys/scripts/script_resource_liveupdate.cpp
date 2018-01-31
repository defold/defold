#include "script_resource_liveupdate.h"
#include <liveupdate/liveupdate.h>

#include <script/script.h>
#include <dlib/log.h>

namespace dmLiveUpdate
{

    int Resource_GetCurrentManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, dmLiveUpdate::CURRENT_MANIFEST);
        return 1;
    }

    static void Callback_StoreResource(StoreResourceCallbackData* callback_data)
    {
        lua_State* L = (lua_State*) callback_data->m_L;
        DM_LUA_STACK_CHECK(L, 0);
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback_data->m_Callback);
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback_data->m_Self);
        lua_pushvalue(L, -1);

        dmScript::SetInstance(L);
        if (dmScript::IsInstanceValid(L))
        {
            lua_pushstring(L, callback_data->m_HexDigest);
            lua_pushboolean(L, callback_data->m_Status);
            int ret = lua_pcall(L, 3, 0, 0);
            if (ret != 0)
            {
                dmLogError("Error while running store_resource callback for resource: %s", lua_tostring(L, -1));
            }
        }
        else
        {
            dmLogError("Could not run store_resource callback since the instance has been deleted.");
            lua_pop(L, 2);
        }

        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_ResourceRef);
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_HexDigestRef);
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_Self);
    }

    static void Callback_StoreManifest(StoreManifestCallbackData* callback_data)
    {
        lua_State* L = (lua_State*) callback_data->m_L;
        DM_LUA_STACK_CHECK(L, 0);
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback_data->m_Callback);
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback_data->m_Self);
        lua_pushvalue(L, -1);

        dmScript::SetInstance(L);
        if (dmScript::IsInstanceValid(L))
        {
            lua_pushinteger(L, callback_data->m_ManifestIndex);
            lua_pushboolean(L, callback_data->m_Status);
            int ret = lua_pcall(L, 3, 0, 0);
            if (ret != 0)
            {
                dmLogError("Error while running store_manifest callback for manifest: %s", lua_tostring(L, -1));
            }
        }
        else
        {
            dmLogError("Could not run store_manifest callback since the instance has been deleted.");
            lua_pop(L, 2);
        }

        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_Self);
    }

    static void Callback_CreateManifest(CreateManifestCallbackData* callback_data)
    {
        lua_State* L = (lua_State*) callback_data->m_L;
        DM_LUA_STACK_CHECK(L, 0);
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback_data->m_Callback);
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback_data->m_Self);
        lua_pushvalue(L, -1);

        dmScript::SetInstance(L);
        if (dmScript::IsInstanceValid(L))
        {
            lua_pushinteger(L, callback_data->m_ManifestIndex);
            lua_pushboolean(L, callback_data->m_Status);
            int ret = lua_pcall(L, 3, 0, 0);
            if (ret != 0)
            {
                dmLogError("Error while running create_manifest callback for manifest: %s", lua_tostring(L, -1));
            }
        }
        else
        {
            dmLogError("Could not run create_manifest callback since the instance has been deleted.");
            lua_pop(L, 2);
        }

        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_Self);
    }

    int Resource_CreateManifest(lua_State* L)
    {
        dmLogInfo(" ### Resource_CreateManifest")
        int top = lua_gettop(L);
        size_t manifestLength = 0;
        const char* manifestData = luaL_checklstring(L, 1, &manifestLength);

        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

        CreateManifestCallbackData cb;
        cb.m_L = dmScript::GetMainThread(L);
        dmScript::GetInstance(L);
        cb.m_Callback = callback;
        cb.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
        cb.m_ManifestIndex = -1;
        cb.m_Status = true;

        dmResource::Manifest* manifest = new dmResource::Manifest();
        // TODO call async
        dmResource::Result result = dmResource::ParseManifestDDF((uint8_t*) manifestData, manifestLength, manifest->m_DDF);

        if (result == dmResource::RESULT_OK)
        {
            cb.m_ManifestIndex = dmLiveUpdate::AddManifest(manifest);
            if (cb.m_ManifestIndex == -1)
            {
                cb.m_Status = false;
                delete manifest;
                dmLogError("The manifest buffer is full (%d/%d)", MAX_MANIFEST_COUNT, MAX_MANIFEST_COUNT);
            }
        }
        else
        {
            cb.m_Status = false;
            delete manifest;
            dmLogError("The manifest could not be parsed");
        }

        Callback_CreateManifest(&cb);
        assert(top == lua_gettop(L));
        return 0;

    }

    int Resource_DestroyManifest(lua_State* L)
    {
        dmLogInfo(" ### Resource_DestroyManifest")
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

        dmLogInfo("Successfully destroy manifest with index: %i", manifestIndex);
        assert(lua_gettop(L) == top);
        return 0;
    }

    int Resource_StoreResource(lua_State* L)
    {
        int top = lua_gettop(L);

        int manifestIndex = luaL_checkint(L, 1);
        dmResource::Manifest* manifest = dmLiveUpdate::GetManifest(manifestIndex);
        if (manifest == 0x0)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }

        size_t buf_len = 0;
        const char* buf = luaL_checklstring(L, 2, &buf_len);
        size_t hex_digest_length = 0;
        const char* hex_digest = luaL_checklstring(L, 3, &hex_digest_length);
        luaL_checktype(L, 4, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        int buf_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
        lua_pushvalue(L, 3);
        int hex_digest_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
        lua_pushvalue(L, 4);
        int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);
        dmLiveUpdate::Result res;

        dmResourceArchive::LiveUpdateResource resource((const uint8_t*) buf, buf_len);
        if (buf_len < sizeof(dmResourceArchive::LiveUpdateResourceHeader))
        {
            resource.m_Header = 0x0;
            dmLogError("The liveupdate resource could not be verified, header information is missing for resource: %s", hex_digest);
            // fall through here to run callback with status failed as well
        }
        dmLiveUpdate::StoreResourceCallbackData cb;
        cb.m_L = dmScript::GetMainThread(L);
        dmScript::GetInstance(L);
        cb.m_Callback = callback;
        cb.m_ResourceRef = buf_ref;
        cb.m_HexDigestRef = hex_digest_ref;
        cb.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
        cb.m_HexDigest = hex_digest;
        res = dmLiveUpdate::StoreResourceAsync(manifest, hex_digest, hex_digest_length, &resource, Callback_StoreResource, cb);

        switch(res)
        {
            case dmLiveUpdate::RESULT_INVALID_HEADER:
                dmLogError("The liveupdate resource could not be verified, header information is missing for resource: %s", hex_digest);
            break;

            case dmLiveUpdate::RESULT_MEM_ERROR:
                dmLogError("Verification of liveupdate resource failed, missing manifest/data for resource: %s", hex_digest);
            break;

            case dmLiveUpdate::RESULT_INVALID_RESOURCE:
                dmLogError("Verification of liveupdate resource failed for expected hash for resource: %s", hex_digest);
            break;

            default:
            break;
        }

        assert(lua_gettop(L) == top);
        return 0;
    }

    int Resource_StoreManifest(lua_State* L)
    {
        dmLogInfo(" ### Resource_StoreManifest")
        int top = lua_gettop(L);

        int manifestIndex = luaL_checkint(L, 1);
        dmResource::Manifest* manifest = dmLiveUpdate::GetManifest(manifestIndex);
        if (manifest == 0x0)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "The manifest identifier does not exist");
        }

        // - (TODO) Verify manifest

        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

        dmLiveUpdate::StoreManifestCallbackData cb;
        cb.m_L = dmScript::GetMainThread(L);
        dmScript::GetInstance(L);
        cb.m_Callback = callback;
        cb.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
        cb.m_ManifestIndex = manifestIndex;
        // TODO do async
        cb.m_Status = dmLiveUpdate::StoreManifest(manifest) == dmLiveUpdate::RESULT_OK;
        Callback_StoreManifest(&cb);

        assert(lua_gettop(L) == top);
        return 0;
    }

};
