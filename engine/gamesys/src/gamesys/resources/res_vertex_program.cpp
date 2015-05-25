#include "res_vertex_program.h"

#include <graphics/graphics.h>

namespace dmGameSystem
{
    dmResource::Result ResVertexProgramCreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 void* preload_data,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HVertexProgram prog = dmGraphics::NewVertexProgram(graphics_context, buffer, buffer_size);
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        resource->m_Resource = (void*) prog;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResVertexProgramDestroy(dmResource::HFactory factory,
                                                  void* context,
                                                  dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DeleteVertexProgram((dmGraphics::HVertexProgram) resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResVertexProgramRecreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HVertexProgram prog = (dmGraphics::HVertexProgram)resource->m_Resource;
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        dmGraphics::ReloadVertexProgram(prog, buffer, buffer_size);
        return dmResource::RESULT_OK;
    }
}
