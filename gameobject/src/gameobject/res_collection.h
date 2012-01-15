#ifndef DM_GAMEOBJECT_RES_COLLECTION_H
#define DM_GAMEOBJECT_RES_COLLECTION_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameObject
{
    dmResource::Result ResCollectionCreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename);

    dmResource::Result ResCollectionDestroy(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource);
}

#endif // DM_GAMEOBJECT_RES_COLLECTION_H
