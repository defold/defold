#ifndef DM_GAMESYS_RES_VERTEX_PROGRAM_H
#define DM_GAMESYS_RES_VERTEX_PROGRAM_H

#include <stdint.h>

#include <resource/resource.h>

namespace dmGameSystem
{
    dmResource::Result ResVertexProgramCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResVertexProgramDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResVertexProgramRecreate(const dmResource::ResourceRecreateParams& params);

    dmResource::Result ResVertexProgramGetInfo(dmResource::ResourceGetInfoParams& params);
}

#endif // DM_GAMESYS_RES_VERTEX_PROGRAM_H
