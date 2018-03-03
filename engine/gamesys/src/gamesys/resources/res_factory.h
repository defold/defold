#ifndef DM_GAMESYS_RES_FACTORY_H
#define DM_GAMESYS_RES_FACTORY_H

#include <stdint.h>

#include <resource/resource.h>
#include <gameobject/gameobject.h>

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    struct FactoryResource
    {
        dmGameSystemDDF::FactoryDesc* m_FactoryDesc;
        dmGameObject::HPrototype m_Prototype;
    };

    dmResource::Result ResFactoryPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResFactoryCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResFactoryDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResFactoryRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResFactoryGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_FACTORY_H
