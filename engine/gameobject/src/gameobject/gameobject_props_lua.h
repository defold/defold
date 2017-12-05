#ifndef GAMEOBJECT_PROPS_LUA_H
#define GAMEOBJECT_PROPS_LUA_H

#include <ddf/ddf.h>

#include "gameobject.h"
#include "gameobject_props.h"

#include "../proto/gameobject/gameobject_ddf.h"

namespace dmGameObject
{
    PropertyResult LuaToVar(lua_State* L, int index, PropertyVar& out_var);
    void LuaPushVar(lua_State* L, const PropertyVar& var);

    PropertyResult CreatePropertySetUserDataLua(void* component_context, uint8_t* buffer, uint32_t buffer_size, uintptr_t* user_data);
    void DestroyPropertySetUserDataLua(uintptr_t user_data);

    PropertyResult GetPropertyCallbackLua(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);
}

#endif // GAMEOBJECT_PROPS_DDF_H
