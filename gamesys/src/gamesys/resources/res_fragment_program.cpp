#include "res_fragment_program.h"

#include <graphics/graphics.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResFragmentProgramCreate(dmResource::HFactory factory,
                                                   void* context,
                                                   const void* buffer, uint32_t buffer_size,
                                                   dmResource::SResourceDescriptor* resource,
                                                   const char* filename)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext)context;
        dmGraphics::HFragmentProgram prog = dmGraphics::NewFragmentProgram(graphics_context, buffer, buffer_size);
        if (prog == 0 )
            return dmResource::CREATE_RESULT_UNKNOWN;

        resource->m_Resource = (void*) prog;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResFragmentProgramDestroy(dmResource::HFactory factory,
                                                    void* context,
                                                    dmResource::SResourceDescriptor* resource)
    {
        dmGraphics::DeleteFragmentProgram((dmGraphics::HFragmentProgram) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResFragmentProgramRecreate(dmResource::HFactory factory,
                                                 void* context,
                                                 const void* buffer, uint32_t buffer_size,
                                                 dmResource::SResourceDescriptor* resource,
                                                 const char* filename)
    {
        dmGraphics::HFragmentProgram prog = (dmGraphics::HFragmentProgram)resource->m_Resource;
        if (prog == 0 )
            return dmResource::CREATE_RESULT_UNKNOWN;

        dmGraphics::ReloadFragmentProgram(prog, buffer, buffer_size);
        return dmResource::CREATE_RESULT_OK;
    }
}
