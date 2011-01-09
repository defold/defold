#include "res_vertex_program.h"

#include <graphics/graphics_device.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResVertexProgramCreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HVertexProgram prog = dmGraphics::NewVertexProgram(buffer, buffer_size);
        if (prog == 0 )
            return dmResource::CREATE_RESULT_UNKNOWN;

        resource->m_Resource = (void*) prog;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResVertexProgramDestroy(dmResource::HFactory factory,
                                                  void* context,
                                                  dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DeleteVertexProgram((dmGraphics::HVertexProgram) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResVertexProgramRecreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HVertexProgram prog = (dmGraphics::HVertexProgram)resource->m_Resource;
        if (prog == 0 )
            return dmResource::CREATE_RESULT_UNKNOWN;

        dmGraphics::ReloadVertexProgram(prog, buffer, buffer_size);
        return dmResource::CREATE_RESULT_OK;
    }
}
