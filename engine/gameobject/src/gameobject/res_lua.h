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
            m_LuaModule(lua_module), m_ScriptContext(0), m_LuaState(0), m_NameHash(0), m_ModuleHash(0) {}

        dmLuaDDF::LuaModule* m_LuaModule;

        // The following variables are used for reloading modules, see gameobject_script_util.cpp
        dmScript::HContext   m_ScriptContext;
        lua_State*           m_LuaState;
        uint64_t             m_NameHash;
        uint64_t             m_ModuleHash;
    };

    dmResource::Result ResLuaCreate(dmResource::HFactory factory,
                                    void* context,
                                    const void* buffer, uint32_t buffer_size,
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
