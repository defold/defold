#ifndef DM_GAMESYS_RES_FACTORY_H
#define DM_GAMESYS_RES_FACTORY_H

#include <stdint.h>

#include <resource/resource.h>

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    struct FactoryResource
    {
        dmGameSystemDDF::FactoryDesc*   m_FactoryDesc;
        void*                           m_Prototype;
    };

    dmResource::Result ResFactoryPreload(dmResource::HFactory factory,
            dmResource::HPreloadHintInfo hint_info,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void** preload_data,
            const char* filename);

    dmResource::Result ResFactoryCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::Result ResFactoryDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::Result ResFactoryRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_FACTORY_H
