#ifndef GAMEOBJECT_PROPS_DDF_H
#define GAMEOBJECT_PROPS_DDF_H

#include <ddf/ddf.h>

#include "gameobject.h"
#include "gameobject_props.h"

#include "../proto/gameobject_ddf.h"

namespace dmGameObject
{
    bool CreatePropertyDataUserData(const dmGameObjectDDF::PropertyDesc* prop_descs, uint32_t prop_desc_count, uintptr_t* user_data);
    void DestroyPropertyDataUserData(uintptr_t user_data);

    PropertyResult GetPropertyCallbackDDF(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);
}

#endif // GAMEOBJECT_PROPS_DDF_H
