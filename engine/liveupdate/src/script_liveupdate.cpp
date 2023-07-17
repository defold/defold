// Copyright 2020-2023 The Defold Foundation
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

#include "script_liveupdate.h"
#include "liveupdate.h"

#include <dlib/log.h>
#include <extension/extension.h>
#include <resource/resource_manifest.h>
#include <resource/resource_verify.h>
#include <script/script.h>

namespace dmLiveUpdate
{
    const int MANIFEST_MAGIC_VALUE = 0x0ac83fcc; // Totally made up and never used. We need to deprecate this /MAWE

    extern const char* LIVEUPDATE_LEGACY_MOUNT_NAME; // "_liveupdate";

    struct LiveUpdateScriptContext
    {
        dmResource::HFactory m_Factory;
    } g_LUScriptCtx;

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

    struct MountArchiveCallbackData
    {
        dmScript::LuaCallbackInfo*  m_Callback;
    };

    // ******************************************************************************************

    static int Resource_GetCurrentManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, MANIFEST_MAGIC_VALUE);
        return 1;
    }

    static int Resource_IsUsingLiveUpdateData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        bool uses_lu = dmLiveUpdate::HasLiveUpdateMount();
        lua_pushboolean(L, uses_lu);
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

    static int Resource_StoreResource(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        // manifest index in first arg [luaL_checkint(L, 1)] deprecated

        // dmResource::Manifest* manifest = dmLiveUpdate::GetCurrentManifest();
        // if (manifest == 0x0)
        // {
        //     return DM_LUA_ERROR("The manifest identifier does not exist");
        // }

        // The resource data (including the liveupdate header)
        size_t buf_len = 0;
        const char* buf = luaL_checklstring(L, 2, &buf_len);
        // The hash digest of the resource (which is also the filename)
        size_t hex_digest_length = 0;
        const char* hex_digest = luaL_checklstring(L, 3, &hex_digest_length);

        lua_pushvalue(L, 2);
        int buf_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
        lua_pushvalue(L, 3);
        int hex_digest_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);


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

        dmLiveUpdate::Result res = dmLiveUpdate::StoreResourceAsync(hex_digest, hex_digest_length, &resource, Callback_StoreResource, cb);

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

    static void Callback_StoreManifest(int _result, void* _cbk)
    {
        dmLiveUpdate::Result result = (dmLiveUpdate::Result)_result;
        dmScript::LuaCallbackInfo* cbk = (dmScript::LuaCallbackInfo*)_cbk;
        if (!dmScript::IsCallbackValid(cbk))
            return;

        lua_State* L = dmScript::GetCallbackLuaContext(cbk);
        DM_LUA_STACK_CHECK(L, 0)

        if (!dmScript::SetupCallback(cbk))
        {
            dmLogError("Failed to setup callback");
            return;
        }

        lua_pushinteger(L, result);

        dmScript::PCall(L, 2, 0); // instance + 1

        dmScript::TeardownCallback(cbk);
        dmScript::DestroyCallback(cbk);
    }

    static int Resource_StoreManifest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        size_t manifest_len = 0;
        const uint8_t* manifest_data = (const uint8_t*)luaL_checklstring(L, 1, &manifest_len);

        dmScript::LuaCallbackInfo* cbk = dmScript::CreateCallback(L, 2);

        dmLiveUpdate::Result res = dmLiveUpdate::StoreManifestAsync(manifest_data, manifest_len, Callback_StoreManifest, cbk);
        if (dmLiveUpdate::RESULT_OK != res)
        {
            dmLogError("The liveupdate manifest could not be stored: %s", dmLiveUpdate::ResultToString(res));
            dmScript::DestroyCallback(cbk);
        }
        return 0;

        // dmResource::Manifest* manifest = 0;
        // dmResource::Result result = dmResource::LoadManifestFromBuffer(manifest_data, manifest_len, &manifest);

        // if (result == dmResource::RESULT_OK)
        // {
        //     const char* public_key_path = dmResource::GetPublicKeyPath(g_LUScriptCtx.m_Factory);
        //     result = dmResource::VerifyManifest(manifest, public_key_path);
        //     if (result != dmResource::RESULT_OK)
        //     {
        //         dmLogError("Manifest verification failed. Manifest was not stored. %d %s", result, dmResource::ResultToString(result));
        //     }

        //     dmLogWarning("Currently disabled verification of existance of resources in liveupdate archive");
        //     //result = dmResource::VerifyResourcesBundled(dmResourceArchive::HArchiveIndexContainer archive, const dmResource::Manifest* manifest);

        //     // result = dmLiveUpdate::VerifyManifestReferences(manifest);
        //     // if (result != dmResource::RESULT_OK)
        //     // {
        //     //     dmLogError("Manifest references non existing resources. Manifest was not stored. %d %s", result, dmResource::ResultToString(result));
        //     // }
        // }
        // else
        // {
        //     dmLogError("Failed to parse manifest, result: %s", dmResource::ResultToString(result));
        // }

        // // Store
        // if (dmResource::RESULT_OK == result)
        // {
        //     result = dmLiveUpdate::StoreManifestToMutableArchive(manifest);
        // }
        // dmResource::DeleteManifest(manifest);

        // Callback_StoreManifest(cbk, result);
        // dmScript::DestroyCallback(cbk);

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

    static int Resource_StoreArchive(lua_State* L)
    {
dmLogWarning("%s TEMPORARILY DISABLED!", __FUNCTION__);
        // DM_LUA_STACK_CHECK(L, 0);

        // int top = lua_gettop(L);

        // const char* path = luaL_checkstring(L, 1);

        // StoreArchiveCallbackData* cb = new StoreArchiveCallbackData;
        // cb->m_Callback = dmScript::CreateCallback(L, 2);
        // cb->m_Path = strdup(path);

        // bool verify_archive = true;
        // if (top > 2 && !lua_isnil(L, 3)) {
        //     luaL_checktype(L, 3, LUA_TTABLE);
        //     lua_pushvalue(L, 3);
        //     lua_pushnil(L);
        //     while (lua_next(L, -2)) {
        //         const char* attr = lua_tostring(L, -2);
        //         if (strcmp(attr, "verify") == 0)
        //         {
        //             verify_archive = lua_toboolean(L, -1);
        //         }
        //         lua_pop(L, 1);
        //     }
        //     lua_pop(L, 1);
        // }

        // dmLiveUpdate::Result res = dmLiveUpdate::StoreArchiveAsync(path, Callback_StoreArchive, cb, verify_archive);

        // if (dmLiveUpdate::RESULT_OK != res)
        // {
        //     dmLogError("Failed to store archive: %d", res);
        //     Callback_StoreArchive(false, (void*)cb);
        // }

        return 0;
    }

    // ************************************************************************************
    // .zip support

    // static void Callback_Mount_Common(dmScript::LuaCallbackInfo* cbk, const char* path, dmResourceProvider::HArchive archive, bool status)
    // {
    //     if (!dmScript::IsCallbackValid(cbk))
    //         return;

    //     lua_State* L = dmScript::GetCallbackLuaContext(cbk);
    //     DM_LUA_STACK_CHECK(L, 0);

    //     if (!dmScript::SetupCallback(cbk))
    //     {
    //         dmLogError("Failed to setup callback");
    //         return;
    //     }

    //     lua_pushstring(L, path);
    //     lua_pushboolean(L, archive != 0);
    //     lua_pushboolean(L, status);

    //     dmScript::PCall(L, 4, 0); // instance + 3

    //     dmScript::TeardownCallback(cbk);
    //     dmScript::DestroyCallback(cbk);
    // }

    // static void Callback_MountUnMountArchive(const char* path, dmResourceProvider::HArchive archive, bool status, void* _data)
    // {

    //     StoreArchiveCallbackData* callback_data = (StoreArchiveCallbackData*)_data;
    //     Callback_Mount_Common(callback_data->m_Callback, path, archive, status);
    //     delete callback_data;
    // }

    // // ***********

    // static int Resource_MountArchive(lua_State* L)
    // {
    //     DM_LUA_STACK_CHECK(L, 0);

    //     const char* path = luaL_checkstring(L, 1);

    //     MountArchiveCallbackData* cb = new MountArchiveCallbackData;
    //     cb->m_Callback = dmScript::CreateCallback(L, 2);

    //     dmLiveUpdate::Result res = dmLiveUpdate::MountArchive(path, Callback_MountUnMountArchive, cb);
    //     if (dmLiveUpdate::RESULT_OK != res)
    //     {
    //         return DM_LUA_ERROR("Failed to mount archive: %d", res);
    //     }
    //     return 0;
    // }

    // // ***********

    // static int Resource_UnmountArchive(lua_State* L)
    // {
    //     DM_LUA_STACK_CHECK(L, 0);
    //     dmLiveUpdate::HMount mount = (dmLiveUpdate::HMount)luaL_checkinteger(L, 1);

    //     MountArchiveCallbackData* cb = new MountArchiveCallbackData;
    //     cb->m_Callback = dmScript::CreateCallback(L, 2);

    //     dmLiveUpdate::Result res = dmLiveUpdate::UnmountArchive(mount, Callback_MountUnMountArchive, cb);
    //     if (dmLiveUpdate::RESULT_OK != res)
    //     {
    //         return DM_LUA_ERROR("Failed to mount archive: %d", res);
    //     }
    //     return 0;
    // }

    // *********************

    // static int Resource_VerifyZipArchive(lua_State* L)
    // {
    //     // char app_support_path[DMPATH_MAX_PATH];
    //     // if (dmResource::RESULT_OK != dmResource::GetApplicationSupportPath(manifest, app_support_path, (uint32_t)sizeof(app_support_path)))

    //     const char* public_key_path = dmResource::GetPublicKeyPath(factory);

    //     return 0;
    // }

    // ************************************************************************************

    static const luaL_reg Module_methods[] =
    {
        //{"verify_zip_archive", dmLiveUpdate::Resource_VerifyZipArchive},

        {"get_current_manifest", dmLiveUpdate::Resource_GetCurrentManifest},        /// bogus data, and never used?
        {"is_using_liveupdate_data", dmLiveUpdate::Resource_IsUsingLiveUpdateData},
        {"store_resource", dmLiveUpdate::Resource_StoreResource}, // Stores a single resource
        {"store_manifest", dmLiveUpdate::Resource_StoreManifest}, // Store a .dmanifest file
        {"store_archive", dmLiveUpdate::Resource_StoreArchive},   // Store a .zip archive

// New api
        // {"mount_archive", dmLiveUpdate::Resource_MountArchive},     // Store a link to a .zip archive
        // {"unmount_archive", dmLiveUpdate::Resource_UnmountArchive}, // Remove a link to a .zip archive
        // {"get_mounts",    dmLiveUpdate::Resource_GetMounts},      // Gets a list of the current mounts

        {0, 0}
    };


#define DEPRECATE_LU_FUNCTION(LUA_NAME, CPP_NAME) \
    static int Deprecated_ ## CPP_NAME(lua_State* L) \
    { \
        dmLogOnceWarning(dmScript::DEPRECATION_FUNCTION_FMT, "resource", LUA_NAME, "liveupdate", LUA_NAME); \
        return dmLiveUpdate:: CPP_NAME (L); \
    }

DEPRECATE_LU_FUNCTION("get_current_manifest", Resource_GetCurrentManifest);
DEPRECATE_LU_FUNCTION("is_using_liveupdate_data", Resource_IsUsingLiveUpdateData);
DEPRECATE_LU_FUNCTION("store_resource", Resource_StoreResource);
DEPRECATE_LU_FUNCTION("store_manifest", Resource_StoreManifest);
DEPRECATE_LU_FUNCTION("store_archive", Resource_StoreArchive);

    // The deprecated ones
    static const luaL_reg ResourceModule_methods[] =
    {
        {"get_current_manifest", Deprecated_Resource_GetCurrentManifest},
        {"is_using_liveupdate_data", Deprecated_Resource_IsUsingLiveUpdateData},
        {"store_resource", Deprecated_Resource_StoreResource},
        {"store_manifest", Deprecated_Resource_StoreManifest},
        {"store_archive", Deprecated_Resource_StoreArchive},
        {0, 0}
    };


#define SETCONSTANT(_NAME) \
        lua_pushnumber(L, (lua_Number)dmLiveUpdate::RESULT_ ## _NAME); \
        lua_setfield(L, -2, "LIVEUPDATE_" #_NAME );\

    static void SetConstants(lua_State* L)
    {
        SETCONSTANT(OK);
        SETCONSTANT(INVALID_HEADER);
        SETCONSTANT(MEM_ERROR);
        SETCONSTANT(INVALID_RESOURCE);
        SETCONSTANT(VERSION_MISMATCH);
        SETCONSTANT(ENGINE_VERSION_MISMATCH);
        SETCONSTANT(SIGNATURE_MISMATCH);
        SETCONSTANT(SCHEME_MISMATCH);
        SETCONSTANT(BUNDLED_RESOURCE_MISMATCH);
        SETCONSTANT(FORMAT_ERROR);
        SETCONSTANT(IO_ERROR);
        SETCONSTANT(INVAL);
        SETCONSTANT(UNKNOWN);
    }

#undef SETCONSTANT

    // LiveUpdate functionality in resource namespace
    static void LuaInitDeprecated(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "resource", ResourceModule_methods); // get or create the resource module!
        SetConstants(L);

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

    static void LuaInit(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "liveupdate", Module_methods);
        SetConstants(L);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }


    void ScriptInit(lua_State* L, dmResource::HFactory factory)
    {
        memset(&g_LUScriptCtx, 0, sizeof(g_LUScriptCtx));
        g_LUScriptCtx.m_Factory = factory;

        LuaInit(L);
        LuaInitDeprecated(L);
    }
};

