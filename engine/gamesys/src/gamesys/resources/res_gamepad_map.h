#ifndef DM_GAMESYS_GAMEPAD_MAP_H
#define DM_GAMESYS_GAMEPAD_MAP_H

#include <resource/resource.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmResource::Result ResGamepadMapCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void *preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResGamepadMapDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResGamepadMapRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_GAMEPAD_MAP_H
