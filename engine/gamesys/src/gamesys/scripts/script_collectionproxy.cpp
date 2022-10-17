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

#include "script_collectionproxy.h"
#include "script_resource_liveupdate.h"

#include <script/script.h>
#include <gameobject/script.h>
#include <liveupdate/liveupdate.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <components/comp_collection_proxy.h>

namespace dmGameSystem
{
    static dmhash_t GetCollectionProxyUrlHash(lua_State* L, int index)
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

        return dmGameSystem::GetUrlHashFromComponent(world, dmGameObject::GetIdentifier(receiver_instance), component_index);
    }

    struct GetResourceHashContext
    {
        lua_State* m_L;
        int        m_Index;
    };

    static void GetResourceHashCallback(void* context, const char* hex_digest, uint32_t length)
    {
        GetResourceHashContext* ctx = (GetResourceHashContext*)context;
        lua_State* L = ctx->m_L;

        lua_pushnumber(L, ctx->m_Index++);
        lua_pushlstring(L, hex_digest, length);
        lua_settable(L, -3);
    }

    // See doc in comp_cxollection_proxy.cpp
    static int CollectionProxy_MissingResources(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmhash_t url_hash = GetCollectionProxyUrlHash(L, 1);

        if (url_hash == 0)
        {
            return DM_LUA_ERROR("Unable to find collection proxy component.");
        }

        lua_newtable(L);

        GetResourceHashContext ctx;
        ctx.m_L = L;
        ctx.m_Index = 1;
        dmLiveUpdate::GetMissingResources(url_hash, GetResourceHashCallback, &ctx);

        return 1;
    }

    // See doc in comp_cxollection_proxy.cpp
    static int CollectionProxy_GetResources(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmhash_t url_hash = GetCollectionProxyUrlHash(L, 1);

        if (url_hash == 0)
        {
            return DM_LUA_ERROR("Unable to find collection proxy component.");
        }

        lua_newtable(L);

        GetResourceHashContext ctx;
        ctx.m_L = L;
        ctx.m_Index = 1;
        dmLiveUpdate::GetResources(url_hash, GetResourceHashCallback, &ctx);

        return 1;
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
