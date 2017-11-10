#include "script_collectionproxy.h"
#include "script_resource_liveupdate.h"

#include <script/script.h>
#include <liveupdate/liveupdate.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <components/comp_collection_proxy.h>

namespace dmGameSystem
{
    void ScriptCollectionProxyFinalize(const ScriptLibContext& context)
    {
    }

    dmhash_t GetCollectionProxyUrlHash(lua_State* L, int index)
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

    int CollectionProxy_MissingResources(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t compUrlHash = GetCollectionProxyUrlHash(L, 1);

        if (compUrlHash == 0)
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "Unable to find collection proxy component.");
        }

        char** buffer = 0x0;
        uint32_t resourceCount = dmLiveUpdate::GetMissingResources(compUrlHash, &buffer);

        lua_createtable(L, resourceCount, 0);
        for (uint32_t i = 0; i < resourceCount; ++i)
        {
            lua_pushnumber(L, i + 1);
            lua_pushstring(L, buffer[i]);
            lua_settable(L, -3);
        }

        assert(lua_gettop(L) == top+1);
        return 1;
    }

    static const luaL_reg Module_methods[] =
    {
        {"missing_resources", CollectionProxy_MissingResources},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "collectionproxy", Module_methods);

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

    void ScriptCollectionProxyRegister(const ScriptLibContext& context)
    {
        LuaInit(context.m_LuaState);
    }
};
