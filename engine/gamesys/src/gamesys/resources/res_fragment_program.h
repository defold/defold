#ifndef DM_GAMESYS_RES_FRAGMENT_PROGRAM_H
#define DM_GAMESYS_RES_FRAGMENT_PROGRAM_H

#include <resource/resource.h>
#include <graphics/graphics.h>
#include <graphics/graphics_ddf.h>

namespace dmGameSystem
{
    struct FragmentProgramResource
    {
        dmGraphics::ShaderDesc* m_ShaderDDF;
        dmGraphics::HFragmentProgram m_Program;
    };

    dmResource::Result ResFragmentProgramPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResFragmentProgramCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResFragmentProgramDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResFragmentProgramRecreate(const dmResource::ResourceRecreateParams& params);

}

#endif // DM_GAMESYS_RES_FRAGMENT_PROGRAM_H
