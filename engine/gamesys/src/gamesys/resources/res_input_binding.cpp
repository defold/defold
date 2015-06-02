#include "res_input_binding.h"

#include <ddf/ddf.h>

#include <input/input.h>

#include <input/input_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResInputBindingCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     void* preload_data,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmInputDDF::InputBinding* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmInputDDF_InputBinding_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmInput::HBinding binding = dmInput::NewBinding((dmInput::HContext)context);
        dmInput::SetBinding(binding, ddf);
        resource->m_Resource = (void*)binding;
        dmDDF::FreeMessage((void*)ddf);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResInputBindingDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmInput::DeleteBinding((dmInput::HBinding)resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResInputBindingRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmInputDDF::InputBinding* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmInputDDF_InputBinding_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmInput::HBinding binding = (dmInput::HBinding)resource->m_Resource;
        dmInput::SetBinding(binding, ddf);
        dmDDF::FreeMessage((void*)ddf);
        return dmResource::RESULT_OK;
    }
}
