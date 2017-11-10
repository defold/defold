#ifndef DM_GAMEOBJECT_RES_LUA_H
#define DM_GAMEOBJECT_RES_LUA_H

#include <stdint.h>

#include <resource/resource.h>
#include <script/script.h>
#include "../proto/gameobject/lua_ddf.h"

namespace dmGameObject
{
    struct LuaScript
    {
        LuaScript(dmLuaDDF::LuaModule* lua_module) :
            m_LuaModule(lua_module) {}

        dmLuaDDF::LuaModule* m_LuaModule;

    };

    dmResource::Result ResLuaCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResLuaDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResLuaRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMEOBJECT_RES_LUA_H
