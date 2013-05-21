#ifndef DM_GAMEOBJECT_COMP_ANIM_H
#define DM_GAMEOBJECT_COMP_ANIM_H

#include <stdint.h>

#include "gameobject.h"

namespace dmGameObject
{
    CreateResult CompAnimNewWorld(const ComponentNewWorldParams& params);

    CreateResult CompAnimDeleteWorld(const ComponentDeleteWorldParams& params);

    UpdateResult CompAnimUpdate(const ComponentsUpdateParams& params);
}

#endif // DM_GAMEOBJECT_COMP_ANIM_H
