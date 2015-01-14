#include "res_fragment_program.h"

#include <graphics/graphics.h>

namespace dmGameSystem
{
    dmResource::Result ResFragmentProgramCreate(dmResource::HFactory factory,
                                                   void* context,
                                                   const void* buffer, uint32_t buffer_size,
                                                   dmResource::SResourceDescriptor* resource,
                                                   const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HFragmentProgram prog = dmGraphics::NewFragmentProgram(graphics_context, buffer, buffer_size);
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        resource->m_Resource = (void*) (uintptr_t) prog;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramDestroy(dmResource::HFactory factory,
                                                    void* context,
                                                    dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        uintptr_t r = (uintptr_t) resource->m_Resource;
        dmGraphics::DeleteFragmentProgram(graphics_context, (dmGraphics::HFragmentProgram) r);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFragmentProgramRecreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HFragmentProgram prog = (dmGraphics::HFragmentProgram) (uintptr_t) resource->m_Resource;
        if (prog == 0 )
            return dmResource::RESULT_FORMAT_ERROR;

        dmGraphics::ReloadFragmentProgram(graphics_context, prog, buffer, buffer_size);
        return dmResource::RESULT_OK;
    }
}
