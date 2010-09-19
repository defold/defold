#ifndef DM_GAMESYS_SOUND_DATA_H
#define DM_GAMESYS_SOUND_DATA_H

#include <resource/resource.h>
#include <physics/physics.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResSoundDataCreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename);

    dmResource::CreateResult ResSoundDataDestroy(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource);
}

#endif
