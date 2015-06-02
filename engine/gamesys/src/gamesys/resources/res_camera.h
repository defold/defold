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

    dmResource::Result ResCameraCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void *preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResCameraDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResCameraRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_CAMERA_H
