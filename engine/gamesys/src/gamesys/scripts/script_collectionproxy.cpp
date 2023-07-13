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

#include "script_collectionproxy.h"

#include <script/script.h>
#include <gameobject/script.h>
#include <gameobject/gameobject.h>
#include <resource/resource_util.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <components/comp_collection_proxy.h>

namespace dmGameSystem
{
    static dmhash_t GetCollectionProxyUrlHash(lua_State* L, int index, dmResource::HFactory* factory)
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
        uintptr_t user_data = 0;
        dmGameSystem::HCollectionProxyWorld world = 0;
        dmGameObject::GetComponentUserDataFromLua(L, index, collection, "collectionproxyc", &user_data, &receiver, (void**)&world);

        if (factory)
            *factory = dmGameObject::GetFactory(receiver_instance);

        return dmGameSystem::GetUrlHashFromComponent(world, dmGameObject::GetIdentifier(receiver_instance), component_index);
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
        dmhash_t url_hash = GetCollectionProxyUrlHash(L, 1, &factory);

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

    static const luaL_reg Module_methods[] =
    {
        {"missing_resources", CollectionProxy_MissingResources},
        {"get_resources", CollectionProxy_GetResources},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "collectionproxy", Module_methods);

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
