#include "res_fragment_program.h"

#include <graphics/graphics.h>

namespace dmGameSystem
{
    dmResource::Result ResFragmentProgramCreate(dmResource::HFactory factory,
                                                   void* context,
                                                   const void* buffer, uint32_t buffer_size,
                                                   void* preload_data,
                                                   dmResource::SResourceDescriptor* resource,
                                                   const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HFragmentProgram prog = dmGraphics::NewFragmentProgram(graphics_context, buffer, buffer_size);
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        resource->m_Resource = (void*) prog;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramDestroy(dmResource::HFactory factory,
                                                    void* context,
                                                    dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DeleteFragmentProgram((dmGraphics::HFragmentProgram) resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramRecreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HFragmentProgram prog = (dmGraphics::HFragmentProgram)resource->m_Resource;
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        dmGraphics::ReloadFragmentProgram(prog, buffer, buffer_size);
        return dmResource::RESULT_OK;
    }
}
