#ifndef DM_GAMESYS_RES_PARTICLEFX_H
#define DM_GAMESYS_RES_PARTICLEFX_H

#include <stdint.h>

#include <resource/resource.h>

#include <particle/particle.h>

namespace dmGameSystem
{
    dmResource::Result ResParticleFXPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResParticleFXCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResParticleFXDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResParticleFXRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_PARTICLEFX_H
