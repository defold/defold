// Copyright 2020-2025 The Defold Foundation
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

#include "script_collectionproxy.h"

#include <dlib/dstrings.h>
#include <script/script.h>
#include <gameobject/script.h>
#include <gameobject/gameobject.h>
#include <dmsdk/gamesys/script.h>
#include <resource/resource_util.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <components/comp_collection_proxy.h>

namespace dmGameSystem
{
    static dmhash_t GetCollectionUrlHashFromCollectionProxy(lua_State* L, int index, dmResource::HFactory* factory)
    {
        dmMessage::URL sender;
        dmMessage::URL receiver;
        dmScript::ResolveURL(L, index, &receiver, 0x0);

        dmScript::GetURL(L, &sender);
        dmGameObject::HInstance sender_instance = dmGameSystem::CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);
        dmGameObject::HInstance receiver_instance = dmGameObject::GetInstanceFromIdentifier(collection, receiver.m_Path);

        if (receiver_instance == 0x0)
        {
            return 0;
        }

        uint16_t component_index = 0;
        dmGameObject::GetComponentIndex(receiver_instance, receiver.m_Fragment, &component_index);
        dmGameObject::HComponent user_data = 0;
        dmGameSystem::HCollectionProxyWorld world = 0;
        dmGameObject::GetComponentFromLua(L, index, collection, COLLECTION_PROXY_EXT, &user_data, &receiver, (dmGameObject::HComponentWorld*)&world);

        if (factory)
            *factory = dmGameObject::GetFactory(receiver_instance);

        return dmGameSystem::GetCollectionUrlHashFromComponent(world, dmGameObject::GetIdentifier(receiver_instance), component_index);
    }

    struct GetResourceHashContext
    {
        lua_State* m_L;
        int        m_Index;
    };

    static void GetResourceHashCallback(void* context, const dmResource::SGetDependenciesResult* result)
    {
        GetResourceHashContext* ctx = (GetResourceHashContext*)context;
        lua_State* L = ctx->m_L;

        char hash_buffer[64*2+1];
        dmResource::BytesToHexString(result->m_HashDigest, result->m_HashDigestLength, hash_buffer, sizeof(hash_buffer));

        lua_pushnumber(L, ctx->m_Index++);
        lua_pushlstring(L, hash_buffer, result->m_HashDigestLength*2);
        lua_settable(L, -3);
    }

    static int CollectionProxy_GetResourcesInternal(lua_State* L, bool only_missing)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmResource::HFactory factory;
        dmhash_t url_hash = GetCollectionUrlHashFromCollectionProxy(L, 1, &factory);

        if (url_hash == 0)
        {
            return DM_LUA_ERROR("Unable to find collection proxy component.");
        }

        lua_newtable(L);

        GetResourceHashContext ctx;
        ctx.m_L = L;
        ctx.m_Index = 1;

        dmResource::SGetDependenciesParams params;
        params.m_UrlHash = url_hash;
        params.m_OnlyMissing = only_missing;
        params.m_Recursive = false;
        params.m_IncludeRequestedUrl = true;
        dmResource::GetDependencies(factory, &params, GetResourceHashCallback, &ctx);

        return 1;
    }

    // See doc in comp_collection_proxy.cpp
    static int CollectionProxy_MissingResources(lua_State* L)
    {
        return CollectionProxy_GetResourcesInternal(L, true);
    }

    // See doc in comp_collection_proxy.cpp
    static int CollectionProxy_GetResources(lua_State* L)
    {
        return CollectionProxy_GetResourcesInternal(L, false);
    }

    // See doc in comp_collection_proxy.cpp
    static int CollectionProxy_SetCollection(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2)
        CollectionProxyWorld* world;
        CollectionProxyComponent* component;
        dmMessage::URL url; // for reporting errors only
        dmScript::GetComponentFromLua(L, 1, COLLECTION_PROXY_EXT, (void**)&world, (void**)&component, &url);


        const char* path = 0;
        if (!lua_isnil(L, 2))
        {
            path = luaL_checkstring(L, 2);

            // check that the path is a .collectionc
            const char* ext = dmResource::GetExtFromPath(path);
            if (!ext || strcmp(ext, ".collectionc") != 0)
            {
                return luaL_error(L, "Trying to set '%s' as collection to '%s:%s#%s'. Only .collectionc resources are allowed",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
            }
        }

        SetCollectionForProxyPathResult result = CollectionProxySetCollectionPath(world, component, path);

        switch (result)
        {
            case SET_COLLECTION_PATH_RESULT_OK:
                lua_pushboolean(L, 1);
                lua_pushnil(L);
                break;
            case SET_COLLECTION_PATH_RESULT_COLLECTION_LOADING:
                lua_pushboolean(L, 0);
                lua_pushnumber(L, SET_COLLECTION_PATH_RESULT_COLLECTION_LOADING);
                dmLogError("Cannot set collection `%s` for a collectionproxy '%s:%s#%s' while it's loading",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
                break;
            case SET_COLLECTION_PATH_RESULT_COLLECTION_ALREADY_LOADED:
                lua_pushboolean(L, 0);
                lua_pushnumber(L, SET_COLLECTION_PATH_RESULT_COLLECTION_ALREADY_LOADED);
                dmLogError("Cannot set collection `%s` for the already loaded collectionproxy '%s:%s#%s'",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
                break;
            case SET_COLLECTION_PATH_RESULT_COLLECTION_NOT_EXCLUDED:
                lua_pushboolean(L, 0);
                lua_pushnumber(L, SET_COLLECTION_PATH_RESULT_COLLECTION_NOT_EXCLUDED);
                dmLogError("Cannot set collection `%s` for a collectionproxy '%s:%s#%s' which isn't excluded",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
                break;
        }

        return 2;
    }

    static const luaL_reg Module_methods[] =
    {
        {"missing_resources", CollectionProxy_MissingResources},
        {"get_resources", CollectionProxy_GetResources},
        {"set_collection", CollectionProxy_SetCollection},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "collectionproxy", Module_methods);

        lua_pushnumber(L, (lua_Number) SET_COLLECTION_PATH_RESULT_COLLECTION_LOADING);
        lua_setfield(L, -2, "RESULT_LOADING");


        lua_pushnumber(L, (lua_Number) SET_COLLECTION_PATH_RESULT_COLLECTION_ALREADY_LOADED);
        lua_setfield(L, -2, "RESULT_ALREADY_LOADED");


        lua_pushnumber(L, (lua_Number) SET_COLLECTION_PATH_RESULT_COLLECTION_NOT_EXCLUDED);
        lua_setfield(L, -2, "RESULT_NOT_EXCLUDED");

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

    void ScriptCollectionProxyFinalize(const ScriptLibContext& context)
    {
    }

    void ScriptCollectionProxyRegister(const ScriptLibContext& context)
    {
        LuaInit(context.m_LuaState);
    }
};
