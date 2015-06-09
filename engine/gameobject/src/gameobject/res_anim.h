#ifndef DM_GAMEOBJECT_RES_ANIM_H
#define DM_GAMEOBJECT_RES_ANIM_H

#include <resource/resource.h>

namespace dmGameObject
{
    dmResource::Result ResAnimCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             void *preload_data,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename);

    dmResource::Result ResAnimDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource);

}

#endif // DM_GAMEOBJECT_RES_ANIM_H
