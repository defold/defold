#ifndef DM_GAMESYS_RES_EMITTER_H
#define DM_GAMESYS_RES_EMITTER_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResEmitterCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResEmitterDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResEmitterRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_EMITTER_H
