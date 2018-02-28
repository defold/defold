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
        params.m_Resource->m_ResourceSize = params.m_BufferSize;
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

        if(!dmGraphics::ReloadFragmentProgram(prog, params.m_Buffer, params.m_BufferSize))
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        params.m_Resource->m_ResourceSize = params.m_BufferSize;
        return  dmResource::RESULT_OK;
    }
}
