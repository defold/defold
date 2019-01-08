#ifndef DM_GAMEOBJECT_RES_PROTOTYPE_H
#define DM_GAMEOBJECT_RES_PROTOTYPE_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameObject
{
    dmResource::Result ResPrototypePreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResPrototypeCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResPrototypeDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResPrototypeRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMEOBJECT_RES_PROTOTYPE_H
