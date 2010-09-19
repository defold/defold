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
        dmGraphics::HVertexProgram prog = dmGraphics::CreateVertexProgram(buffer, buffer_size);
        if (prog == 0 )
            return dmResource::CREATE_RESULT_UNKNOWN;

        resource->m_Resource = (void*) prog;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResVertexProgramDestroy(dmResource::HFactory factory,
                                                  void* context,
                                                  dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DestroyVertexProgram((dmGraphics::HVertexProgram) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }
}
