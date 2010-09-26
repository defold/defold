#include "res_gamepad_map.h"

#include <input/input_ddf.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResGamepadMapCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmInputDDF::GamepadMaps* gamepad_maps;
        dmDDF::Result e = dmDDF::LoadMessage<dmInputDDF::GamepadMaps>(buffer, buffer_size, &gamepad_maps);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        resource->m_Resource = (void*) gamepad_maps;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResGamepadMapDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmDDF::FreeMessage((void*) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResGamepadMapRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}
