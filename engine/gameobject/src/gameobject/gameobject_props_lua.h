#ifndef GAMEOBJECT_PROPS_LUA_H
#define GAMEOBJECT_PROPS_LUA_H

#include "gameobject_props.h"

namespace dmGameObject
{
    PropertyResult LuaToVar(lua_State* L, int index, PropertyVar& out_var);
    void LuaPushVar(lua_State* L, const PropertyVar& var);

    HPropertyContainer CreatePropertyContainerFromLua(void* component_context, uint8_t* buffer, uint32_t buffer_size);
}

#endif // GAMEOBJECT_PROPS_DDF_H
