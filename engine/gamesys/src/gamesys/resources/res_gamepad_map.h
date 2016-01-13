#ifndef DM_GAMESYS_GAMEPAD_MAP_H
#define DM_GAMESYS_GAMEPAD_MAP_H

#include <resource/resource.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmResource::Result ResGamepadMapCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResGamepadMapDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResGamepadMapRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_GAMEPAD_MAP_H
