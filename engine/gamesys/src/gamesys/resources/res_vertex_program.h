#ifndef DM_GAMESYS_RES_VERTEX_PROGRAM_H
#define DM_GAMESYS_RES_VERTEX_PROGRAM_H

#include <resource/resource.h>
#include <graphics/graphics.h>
#include <graphics/graphics_ddf.h>

namespace dmGameSystem
{
    struct VertexProgramResource
    {
        dmGraphics::ShaderDesc* m_ShaderDDF;
        dmGraphics::HVertexProgram m_Program;
    };

    dmResource::Result ResVertexProgramPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResVertexProgramCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResVertexProgramDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResVertexProgramRecreate(const dmResource::ResourceRecreateParams& params);

}

#endif // DM_GAMESYS_RES_VERTEX_PROGRAM_H
