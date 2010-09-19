#ifndef DM_GAMESYS_RES_VERTEX_PROGRAM_H
#define DM_GAMESYS_RES_VERTEX_PROGRAM_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResVertexProgramCreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename);

    dmResource::CreateResult ResVertexProgramDestroy(dmResource::HFactory factory,
                                                  void* context,
                                                  dmResource::SResourceDescriptor* resource);
}

#endif // DM_GAMESYS_RES_VERTEX_PROGRAM_H
