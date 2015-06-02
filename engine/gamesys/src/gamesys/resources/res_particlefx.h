#ifndef DM_GAMESYS_RES_PARTICLEFX_H
#define DM_GAMESYS_RES_PARTICLEFX_H

#include <stdint.h>

#include <resource/resource.h>

#include <particle/particle.h>

namespace dmGameSystem
{
    dmResource::Result ResParticleFXCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void *preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResParticleFXDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResParticleFXRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_PARTICLEFX_H
