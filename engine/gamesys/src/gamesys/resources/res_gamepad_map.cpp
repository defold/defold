#include "res_gamepad_map.h"

#include <input/input_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResGamepadMapCreate(const dmResource::ResourceCreateParams& params)
    {
        dmInputDDF::GamepadMaps* gamepad_maps;
        dmDDF::Result e = dmDDF::LoadMessage<dmInputDDF::GamepadMaps>(params.m_Buffer, params.m_BufferSize, &gamepad_maps);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        params.m_Resource->m_Resource = (void*) gamepad_maps;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGamepadMapDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGamepadMapRecreate(const dmResource::ResourceRecreateParams& params)
    {
        // TODO: Implement me!
        return dmResource::RESULT_NOT_SUPPORTED;
    }
}
