#ifndef DM_GAMESYS_INPUT_BINDING_H
#define DM_GAMESYS_INPUT_BINDING_H

#include <resource/resource.h>
#include <gameobject/gameobject.h>

namespace dmGameSystem
{
    dmResource::Result ResInputBindingCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void *preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResInputBindingDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResInputBindingRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_INPUT_BINDING_H
