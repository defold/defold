#include "res_fragment_program.h"

#include <graphics/graphics.h>

namespace dmGameSystem
{
    dmResource::Result ResFragmentProgramCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext) params.m_Context;
        dmGraphics::HFragmentProgram prog = dmGraphics::NewFragmentProgram(graphics_context, params.m_Buffer, params.m_BufferSize);
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        params.m_Resource->m_Resource = (void*) prog;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmGraphics::DeleteFragmentProgram((dmGraphics::HFragmentProgram) params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGraphics::HFragmentProgram prog = (dmGraphics::HFragmentProgram)params.m_Resource->m_Resource;
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        bool success = dmGraphics::ReloadFragmentProgram(prog, params.m_Buffer, params.m_BufferSize);
        return success ? dmResource::RESULT_OK : dmResource::RESULT_FORMAT_ERROR;
    }

    dmResource::Result ResFragmentProgramGetInfo(dmResource::ResourceGetInfoParams& params)
    {
        // Todo: This returns the shaders source size. It's currently the best we can do (ES2.0) since there is no way of retreiving the binary size of a compiled shader.
        // The shader source is allocated and used in the driver, so this is not irrelevant data.
        params.m_DataSize = dmGraphics::GetFragmentProgramSourceSize((dmGraphics::HVertexProgram) params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

}
