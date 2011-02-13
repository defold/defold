#ifndef DM_GAMEOBJECT_RES_SCRIPT_H
#define DM_GAMEOBJECT_RES_SCRIPT_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameObject
{
    dmResource::CreateResult ResScriptCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename);

    dmResource::CreateResult ResScriptDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResScriptRecreate(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename);
}

#endif // DM_GAMEOBJECT_RES_SCRIPT_H
