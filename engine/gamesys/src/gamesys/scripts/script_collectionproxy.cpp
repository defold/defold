#include "script_collectionproxy.h"
#include "script_resource_liveupdate.h"

#include <script/script.h>
#include "../gamesys.h"
#include <components/comp_collection_proxy.h>

namespace dmGameSystem
{

    static const luaL_reg Module_methods[] =
    {
        {"missing_resources", dmLiveUpdate::CollectionProxy_MissingResources},
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

        uint8_t component_index = 0;
        dmGameObject::GetComponentIndex(receiver_instance, receiver.m_Fragment, &component_index);
        uintptr_t user_data = 0;
        dmGameSystem::CollectionProxyWorld* world = 0;
        dmGameObject::GetComponentUserDataFromLua(L, index, collection, "collectionproxyc", &user_data, &receiver, (void**)&world);

        dmhash_t comp_url_hash = 0;
        for (uint32_t i = 0; i < world->m_Components.Size(); ++i)
        {
            dmGameSystem::CollectionProxyComponent* c = &world->m_Components[i];
            if (c->m_ComponentIndex == component_index)
            {
                comp_url_hash = c->m_Resource->m_UrlHash;
                break;
            }
        }

        return comp_url_hash;
    }

};
