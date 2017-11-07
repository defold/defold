#ifndef DM_GAMESYS_RES_TEXTURE_H
#define DM_GAMESYS_RES_TEXTURE_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResTexturePreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResTextureCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResTexturePostCreate(const dmResource::ResourcePostCreateParams& params);

    dmResource::Result ResTextureDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResTextureRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif
