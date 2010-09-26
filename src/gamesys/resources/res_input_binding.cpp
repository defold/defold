#include "res_input_binding.h"

#include <ddf/ddf.h>

#include <input/input.h>

#include <input/input_ddf.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResInputBindingCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmInputDDF::InputBinding* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmInputDDF_InputBinding_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        resource->m_Resource = (void*)ddf;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResInputBindingDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmDDF::FreeMessage((void*)resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResInputBindingRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}
