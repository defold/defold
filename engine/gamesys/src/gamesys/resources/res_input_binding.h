#ifndef DM_GAMESYS_INPUT_BINDING_H
#define DM_GAMESYS_INPUT_BINDING_H

#include <resource/resource.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmResource::Result ResInputBindingCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResInputBindingDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResInputBindingRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResInputBindingGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_INPUT_BINDING_H
