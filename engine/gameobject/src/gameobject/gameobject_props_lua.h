#ifndef GAMEOBJECT_PROPS_DDF_H
#define GAMEOBJECT_PROPS_DDF_H

#include <ddf/ddf.h>

#include "gameobject.h"
#include "gameobject_props.h"

#include "../proto/gameobject_ddf.h"

namespace dmGameObject
{
    bool CreatePropertyDataUserDataLua(lua_State* L, uint8_t* buffer, uint32_t buffer_size, uintptr_t* user_data);
    void DestroyPropertyDataUserDataLua(uintptr_t user_data);

    PropertyResult GetPropertyCallbackLua(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);
}

#endif // GAMEOBJECT_PROPS_DDF_H
