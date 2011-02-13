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
    };

    dmResource::CreateResult ResCameraCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::CreateResult ResCameraDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResCameraRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_CAMERA_H
