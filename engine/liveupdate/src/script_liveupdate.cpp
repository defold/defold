// Copyright 2020-2026 The Defold Foundation
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

#include <string.h>

#include <dlib/log.h>
#include <dlib/uri.h>
#include <extension/extension.hpp>
#include <resource/resource_mounts.h>
#include <resource/providers/provider.h>
#include <script/script.h>

namespace dmLiveUpdate
{
    struct LiveUpdateScriptContext
    {
        dmResource::HFactory m_Factory;
    } g_LUScriptCtx;

    // Mount support

    // *********************

    static int Resource_GetMounts(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmResourceMounts::HContext mounts = dmResource::GetMountsContext(g_LUScriptCtx.m_Factory);
        dmMutex::HMutex mutex = dmResourceMounts::GetMutex(mounts);
        DM_MUTEX_SCOPED_LOCK(mutex);

        uint32_t count = dmResourceMounts::GetNumMounts(mounts);

        lua_createtable(L, count, 0);

        for (uint32_t i = 0; i < count; ++i)
        {
            dmResourceMounts::SGetMountResult info;
            dmResource::Result result = dmResourceMounts::GetMountByIndex(mounts, i, &info);
            if (dmResource::RESULT_OK == result)
            {
                dmURI::Parts uri;
                dmResourceProvider::GetUri(info.m_Archive, &uri);

                lua_pushinteger(L, i+1);
                lua_newtable(L);

                    lua_pushinteger(L, info.m_Priority);
                    lua_setfield(L, -2, "priority");

                    dmScript::PushHash(L, info.m_NameHash);
                    lua_setfield(L, -2, "name");

                    if (uri.m_Location[0] == '\0')
                        lua_pushfstring(L, "%s:%s", uri.m_Scheme, uri.m_Path);
                    else
                        lua_pushfstring(L, "%s:%s/%s", uri.m_Scheme, uri.m_Location, uri.m_Path);
                    lua_setfield(L, -2, "uri");

                lua_settable(L, -3);
            }
        }

        return 1;
    }

    // *********************

    static int Resource_RemoveMount(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        const char* name = luaL_checkstring(L, 1);

        if (name[0] == '_')
            return DM_LUA_ERROR("Cannot remove base mounts: %s", name);

        dmLiveUpdate::Result result = dmLiveUpdate::RemoveMountSync(name);
        lua_pushinteger(L, result);
        return 1;
    }

    // *********************

    static void Callback_AddMount(dmhash_t name_hash, const char* uri, int result, void* _cbk)
    {
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

        dmScript::PushHash(L, name_hash);
        lua_pushstring(L, uri);
        lua_pushinteger(L, result);

        dmScript::PCall(L, 4, 0); // instance + 3

        dmScript::TeardownCallback(cbk);
        dmScript::DestroyCallback(cbk);
    }

    static int Resource_AddMount(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        const char* name = luaL_checkstring(L, 1);
        const char* uri = luaL_checkstring(L, 2);
        int priority = luaL_checkinteger(L, 3);

        // validate args
        if (priority < 0)
            return DM_LUA_ERROR("Priority needs to be a positive number: %d", priority);

        if (name[0] == '_')
            return DM_LUA_ERROR("Name must not start with '_': %s", name);

        // options at #5

        dmScript::LuaCallbackInfo* cbk = dmScript::CreateCallback(L, 4);
        dmLiveUpdate::Result res = dmLiveUpdate::AddMountAsync(dmHashString64(name), uri, priority, Callback_AddMount, cbk);
        if (dmLiveUpdate::RESULT_OK != res)
        {
            dmLogError("The liveupdate mount '%s' - '%s' was not stored: %s", name, uri, dmLiveUpdate::ResultToString(res));
            dmScript::DestroyCallback(cbk);
        }

        lua_pushinteger(L, res);
        return 1;
    }

    static int Resource_IsBuiltWithExcludedFiles(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        bool result = dmLiveUpdate::IsBuiltWithExcludedFiles();
        lua_pushboolean(L, result);
        return 1;
    }

    // ************************************************************************************

    static const luaL_reg Module_methods[] =
    {
        {"get_mounts",                      dmLiveUpdate::Resource_GetMounts},      // Gets a list of the current mounts
        {"add_mount",                       dmLiveUpdate::Resource_AddMount},
        {"remove_mount",                    dmLiveUpdate::Resource_RemoveMount},
        {"is_built_with_excluded_files",    dmLiveUpdate::Resource_IsBuiltWithExcludedFiles},

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
        SETCONSTANT(NOT_INITIALIZED);
        SETCONSTANT(UNKNOWN);
    }

#undef SETCONSTANT

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
    }
};
