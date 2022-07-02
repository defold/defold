// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "script_resource_liveupdate.h"
#include <liveupdate/liveupdate.h>

#include <script/script.h>
#include <dlib/log.h>
#include <extension/extension.h>

namespace dmLiveUpdate
{
    //Callback data from store resource function
    struct StoreResourceCallbackData
    {
        dmScript::LuaCallbackInfo*  m_Callback;
        int                         m_ResourceRef;
        int                         m_HexDigestRef;
        const char*                 m_HexDigest;
    };

    struct StoreArchiveCallbackData
    {
        dmScript::LuaCallbackInfo*  m_Callback;
        const char*                 m_Path;
    };

    int Resource_GetCurrentManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, dmLiveUpdate::CURRENT_MANIFEST);
        return 1;
    }

    int Resource_IsUsingLiveUpdateData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        int type = dmLiveUpdate::GetLiveupdateType();
        lua_pushboolean(L, type != -1);
        return 1;
    }

    static void Callback_StoreResource(bool status, void* _data)
    {
        StoreResourceCallbackData* callback_data = (StoreResourceCallbackData*)_data;

        if (!dmScript::IsCallbackValid(callback_data->m_Callback))
            return;

        lua_State* L = dmScript::GetCallbackLuaContext(callback_data->m_Callback);
        DM_LUA_STACK_CHECK(L, 0)

        if (!dmScript::SetupCallback(callback_data->m_Callback))
        {
            dmLogError("Failed to setup callback");
            return;
        }

        lua_pushstring(L, callback_data->m_HexDigest);
        lua_pushboolean(L, status);

        dmScript::PCall(L, 3, 0); // instance + 2

        dmScript::TeardownCallback(callback_data->m_Callback);
        dmScript::DestroyCallback(callback_data->m_Callback);

        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_ResourceRef);
        dmScript::Unref(L, LUA_REGISTRYINDEX, callback_data->m_HexDigestRef);
        delete callback_data;
    }

    int Resource_StoreResource(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        // manifest index in first arg [luaL_checkint(L, 1)] deprecated
        dmResource::Manifest* manifest = dmLiveUpdate::GetCurrentManifest();
        if (manifest == 0x0)
        {
            return DM_LUA_ERROR("The manifest identifier does not exist");
        }

        size_t buf_len = 0;
        const char* buf = luaL_checklstring(L, 2, &buf_len);
        size_t hex_digest_length = 0;
        const char* hex_digest = luaL_checklstring(L, 3, &hex_digest_length);
        lua_pushvalue(L, 2);
        int buf_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
        lua_pushvalue(L, 3);
        int hex_digest_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);

        dmLiveUpdate::Result res;
        dmResourceArchive::LiveUpdateResource resource((const uint8_t*) buf, buf_len);
        if (buf_len < sizeof(dmResourceArchive::LiveUpdateResourceHeader))
        {
            resource.m_Header = 0x0;
            dmLogError("The liveupdate resource could not be verified, header information is missing for resource: %s", hex_digest);
            // fall through here to run callback with status failed as well
        }

        StoreResourceCallbackData* cb = new StoreResourceCallbackData;
        cb->m_Callback = dmScript::CreateCallback(L, 4);
        cb->m_HexDigest = hex_digest;
        cb->m_HexDigestRef = hex_digest_ref;
        cb->m_ResourceRef = buf_ref;
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

        return 0;
    }

    static void Callback_StoreManifest(dmScript::LuaCallbackInfo* cbk, int status)
    {
        if (!dmScript::IsCallbackValid(cbk))
            return;

        lua_State* L = dmScript::GetCallbackLuaContext(cbk);
        DM_LUA_STACK_CHECK(L, 0)

        if (!dmScript::SetupCallback(cbk))
        {
            dmLogError("Failed to setup callback");
            return;
        }

        lua_pushinteger(L, status);

        dmScript::PCall(L, 2, 0); // instance + 1

        dmScript::TeardownCallback(cbk);
    }

    int Resource_StoreManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        size_t manifest_len = 0;
        const char* manifest_data = luaL_checklstring(L, 1, &manifest_len);

        dmScript::LuaCallbackInfo* cbk = dmScript::CreateCallback(L, 2);

        dmResource::Manifest* manifest = new dmResource::Manifest();

        // Create
        Result result = dmLiveUpdate::ParseManifestBin((uint8_t*) manifest_data, (uint32_t)manifest_len, manifest);
        if (result == RESULT_OK)
        {
            // Verify
            result = dmLiveUpdate::VerifyManifest(manifest);
            if (result == RESULT_SCHEME_MISMATCH)
            {
                dmLogWarning("Scheme mismatch, manifest storage is only supported for bundled package. Manifest was not stored.");
            }
            else if (result != RESULT_OK)
            {
                dmLogError("Manifest verification failed. Manifest was not stored.");
            }

            result = dmLiveUpdate::VerifyManifestReferences(manifest);
            if (result != RESULT_OK)
            {
                dmLogError("Manifest references non existing resources. Manifest was not stored.");
            }
        }
        else
        {
            dmLogError("Failed to parse manifest, result: %i", result);
        }

        // Store
        if (result == RESULT_OK)
        {
            result = dmLiveUpdate::StoreManifest(manifest);
        }
        delete manifest;

        Callback_StoreManifest(cbk, result);
        dmScript::DestroyCallback(cbk);

        return 0;
    }

    static void Callback_StoreArchive(bool status, void* _data)
    {
        StoreArchiveCallbackData* callback_data = (StoreArchiveCallbackData*)_data;

        if (!dmScript::IsCallbackValid(callback_data->m_Callback))
            return;

        lua_State* L = dmScript::GetCallbackLuaContext(callback_data->m_Callback);
        DM_LUA_STACK_CHECK(L, 0)

        if (!dmScript::SetupCallback(callback_data->m_Callback))
        {
            dmLogError("Failed to setup callback");
            return;
        }

        lua_pushstring(L, callback_data->m_Path);
        lua_pushboolean(L, status);

        dmScript::PCall(L, 3, 0); // instance + 2

        dmScript::TeardownCallback(callback_data->m_Callback);
        dmScript::DestroyCallback(callback_data->m_Callback);
        free((void*)callback_data->m_Path);
        delete callback_data;
    }

    int Resource_StoreArchive(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        int top = lua_gettop(L);

        const char* path = luaL_checkstring(L, 1);

        StoreArchiveCallbackData* cb = new StoreArchiveCallbackData;
        cb->m_Callback = dmScript::CreateCallback(L, 2);
        cb->m_Path = strdup(path);

        bool verify_archive = true;
        if (top > 2 && !lua_isnil(L, 3)) {
            luaL_checktype(L, 3, LUA_TTABLE);
            lua_pushvalue(L, 3);
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                const char* attr = lua_tostring(L, -2);
                if (strcmp(attr, "verify") == 0)
                {
                    verify_archive = lua_toboolean(L, -1);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }

        dmLiveUpdate::Result res = dmLiveUpdate::StoreArchiveAsync(path, Callback_StoreArchive, cb, verify_archive);

        if (dmLiveUpdate::RESULT_OK != res)
        {
            dmLogError("Failed to store archive: %d", res);
            Callback_StoreArchive(false, (void*)cb);
        }

        return 0;
    }

    static const luaL_reg Module_methods[] =
    {
        {"get_current_manifest", dmLiveUpdate::Resource_GetCurrentManifest},
        {"is_using_liveupdate_data", dmLiveUpdate::Resource_IsUsingLiveUpdateData},
        {"store_resource", dmLiveUpdate::Resource_StoreResource}, // Stores a single resource
        {"store_manifest", dmLiveUpdate::Resource_StoreManifest}, // Store a .dmanifest file
        {"store_archive", dmLiveUpdate::Resource_StoreArchive},   // Store a .zip archive

        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "liveupdate", Module_methods);

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

    static dmExtension::Result InitializeLiveUpdate(dmExtension::Params* params)
    {
        LuaInit(params->m_L);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result FinalizeLiveUpdate(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }
};

DM_DECLARE_EXTENSION(LiveUpdateExt, "LiveUpdate", 0, 0, dmLiveUpdate::InitializeLiveUpdate, 0, 0, dmLiveUpdate::FinalizeLiveUpdate)
