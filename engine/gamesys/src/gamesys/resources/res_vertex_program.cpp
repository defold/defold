#include "res_vertex_program.h"

#include <graphics/graphics.h>

namespace dmGameSystem
{
    dmResource::Result ResVertexProgramCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext) params.m_Context;
        dmGraphics::HVertexProgram prog = dmGraphics::NewVertexProgram(graphics_context, params.m_Buffer, params.m_BufferSize);
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        params.m_Resource->m_Resource = (void*) prog;
        params.m_Resource->m_ResourceSize = params.m_BufferSize;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResVertexProgramDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmGraphics::DeleteVertexProgram((dmGraphics::HVertexProgram) params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResVertexProgramRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGraphics::HVertexProgram prog = (dmGraphics::HVertexProgram)params.m_Resource->m_Resource;
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        if(!dmGraphics::ReloadVertexProgram(prog, params.m_Buffer, params.m_BufferSize))
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        params.m_Resource->m_ResourceSize = params.m_BufferSize;
        return  dmResource::RESULT_OK;
    }
}
