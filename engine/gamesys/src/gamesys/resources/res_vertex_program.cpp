#include "res_vertex_program.h"

#include <graphics/graphics.h>

namespace dmGameSystem
{
    dmResource::Result ResVertexProgramCreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HVertexProgram prog = dmGraphics::NewVertexProgram(graphics_context, buffer, buffer_size);
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        resource->m_Resource = (void*) (uintptr_t) prog;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResVertexProgramDestroy(dmResource::HFactory factory,
                                                  void* context,
                                                  dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::DeleteVertexProgram(graphics_context, (dmGraphics::HVertexProgram) (uintptr_t) resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResVertexProgramRecreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HVertexProgram prog = (dmGraphics::HVertexProgram) (uintptr_t)resource->m_Resource;
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        dmGraphics::ReloadVertexProgram(graphics_context, prog, buffer, buffer_size);
        return dmResource::RESULT_OK;
    }
}
