#ifndef DMENGINE_RES_RENDERSCRIPT_H
#define DMENGINE_RES_RENDERSCRIPT_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmEngine
{
    dmResource::CreateResult ResRenderScriptCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);

    dmResource::CreateResult ResRenderScriptDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResRenderScriptRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DMENGINE_RES_RENDERSCRIPT_H
