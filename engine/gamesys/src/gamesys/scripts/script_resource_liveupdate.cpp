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

    int Resource_StoreResource(lua_State* L)
    {
        int top = lua_gettop(L);

        // manifest index in first arg [luaL_checkint(L, 1)] deprecated
        dmResource::Manifest* manifest = dmLiveUpdate::GetCurrentManifest();
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
            lua_pushinteger(L, callback_data->m_Status);
            int ret = lua_pcall(L, 2, 0, 0);
            if (ret != 0)
            {
                dmLogError("Error while running store_manifest callback");
            }
        }
        else
        {
            dmLogError("Could not run store_manifest callback since the instance has been deleted.");
            lua_pop(L, 1);
        }

        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_Callback);
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_Self);
    }

    int Resource_StoreManifest(lua_State* L)
    {
        int top = lua_gettop(L);
        size_t manifest_len = 0;
        const char* manifest_data = luaL_checklstring(L, 1, &manifest_len);

        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_pushvalue(L, 2);
        int callback = dmScript::Ref(L, LUA_REGISTRYINDEX);

        dmLiveUpdate::StoreManifestCallbackData cb;
        cb.m_L = dmScript::GetMainThread(L);
        dmScript::GetInstance(L);
        cb.m_Callback = callback;
        cb.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
        
        dmResource::Manifest* manifest = new dmResource::Manifest();

        // TODO create/verify/store async
        // Create
        Result result = ParseManifestBin((uint8_t*) manifest_data, manifest_len, manifest);
        if (result == RESULT_OK)
        {
            // Verify
            result = dmLiveUpdate::VerifyManifest(manifest);
            if (result == RESULT_SCHEME_MISMATCH)
            {
                dmLogWarning("Scheme mismatch, manifest storage is only supported for bundled package. Manifest was not stored.");
            }
            if (result != RESULT_OK)
            {
                dmLogError("Manifest verification failed. Manifest was not stored");
            }
        }
        else
        {
            dmLogError("Failed to parse manifest, result: %i", result);
        }

        // Store
        if (result == RESULT_OK)
            result = dmLiveUpdate::StoreManifest(manifest);

        cb.m_Status = result;
        Callback_StoreManifest(&cb);

        delete manifest;

        assert(top == lua_gettop(L));
        return 0;
    }

};
