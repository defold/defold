#ifndef DM_GAMESYS_RES_CAMERA_H
#define DM_GAMESYS_RES_CAMERA_H

#include <stdint.h>

#include <resource/resource.h>

#include "camera_ddf.h"

namespace dmGameSystem
{
    struct CameraResource
    {
        dmGamesysDDF::CameraDesc* m_DDF;
        uint32_t m_DDFSize;
    };

    dmResource::Result ResCameraCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResCameraDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResCameraRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResCameraGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_CAMERA_H
