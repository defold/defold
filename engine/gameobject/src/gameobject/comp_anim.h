#ifndef DM_GAMEOBJECT_COMP_ANIM_H
#define DM_GAMEOBJECT_COMP_ANIM_H

#include <stdint.h>

#include "gameobject.h"

namespace dmGameObject
{
    CreateResult CompAnimNewWorld(const ComponentNewWorldParams& params);

    CreateResult CompAnimDeleteWorld(const ComponentDeleteWorldParams& params);

    CreateResult CompAnimAddToUpdate(const ComponentAddToUpdateParams& params);

    UpdateResult CompAnimUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& result);
}

#endif // DM_GAMEOBJECT_COMP_ANIM_H
