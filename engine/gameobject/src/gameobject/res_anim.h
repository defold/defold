#ifndef DM_GAMEOBJECT_RES_ANIM_H
#define DM_GAMEOBJECT_RES_ANIM_H

#include <resource/resource.h>

namespace dmGameObject
{
    dmResource::Result ResAnimCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResAnimDestroy(const dmResource::ResourceDestroyParams& params);

}

#endif // DM_GAMEOBJECT_RES_ANIM_H
