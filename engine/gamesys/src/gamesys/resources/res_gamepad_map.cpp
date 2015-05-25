#include "res_gamepad_map.h"

#include <input/input_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResGamepadMapCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     void* preload_data,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmInputDDF::GamepadMaps* gamepad_maps;
        dmDDF::Result e = dmDDF::LoadMessage<dmInputDDF::GamepadMaps>(buffer, buffer_size, &gamepad_maps);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        resource->m_Resource = (void*) gamepad_maps;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGamepadMapDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmDDF::FreeMessage((void*) resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGamepadMapRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::RESULT_NOT_SUPPORTED;
    }
}
