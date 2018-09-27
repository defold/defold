#ifndef GAMEOBJECT_PROPS_DDF_H
#define GAMEOBJECT_PROPS_DDF_H

#include "gameobject_props.h"
#include "../proto/gameobject/properties_ddf.h"

namespace dmGameObject
{
    HPropertyContainer CreatePropertyContainerDDF(const dmPropertiesDDF::PropertyDeclarations* prop_descs);
}

#endif // GAMEOBJECT_PROPS_DDF_H
