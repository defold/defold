#ifndef DM_GAMEOBJECT_RES_LUA_H
#define DM_GAMEOBJECT_RES_LUA_H

#include <stdint.h>

#include <resource/resource.h>
#include <script/script.h>
#include "../proto/lua_ddf.h"

namespace dmGameObject
{
    struct LuaScript
    {
        LuaScript(dmLuaDDF::LuaModule* lua_module) :
            m_LuaModule(lua_module) {}

        dmLuaDDF::LuaModule* m_LuaModule;

    };

    dmResource::Result ResLuaCreate(dmResource::HFactory factory,
                                    void* context,
                                    const void* buffer, uint32_t buffer_size,
                                    void* preload_data,
                                    dmResource::SResourceDescriptor* resource,
                                    const char* filename);

    dmResource::Result ResLuaDestroy(dmResource::HFactory factory,
                                     void* context,
                                     dmResource::SResourceDescriptor* resource);

    dmResource::Result ResLuaRecreate(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename);
}

#endif // DM_GAMEOBJECT_RES_LUA_H
