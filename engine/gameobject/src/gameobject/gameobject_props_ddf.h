#ifndef GAMEOBJECT_PROPS_DDF_H
#define GAMEOBJECT_PROPS_DDF_H

#include <ddf/ddf.h>

#include "gameobject.h"
#include "gameobject_props.h"

#include "../proto/gameobject/gameobject_ddf.h"
#include "../proto/gameobject/properties_ddf.h"

namespace dmGameObject
{
    bool CreatePropertySetUserData(const dmPropertiesDDF::PropertyDeclarations* prop_descs, uintptr_t* user_data, uint32_t* userdata_size);
    void DestroyPropertySetUserData(uintptr_t user_data);

    PropertyResult GetPropertyCallbackDDF(const HProperties properties, uintptr_t user_data, dmhash_t id, PropertyVar& out_var);
}

#endif // GAMEOBJECT_PROPS_DDF_H
