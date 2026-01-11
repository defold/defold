// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "res_gamepad_map.h"

#include <input/input_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResGamepadMapCreate(const dmResource::ResourceCreateParams* params)
    {
        dmInputDDF::GamepadMaps* gamepad_maps;
        dmDDF::Result e = dmDDF::LoadMessage<dmInputDDF::GamepadMaps>(params->m_Buffer, params->m_BufferSize, &gamepad_maps);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::SetResource(params->m_Resource, gamepad_maps);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGamepadMapDestroy(const dmResource::ResourceDestroyParams* params)
    {
        dmDDF::FreeMessage((void*) dmResource::GetResource(params->m_Resource));
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResGamepadMapRecreate(const dmResource::ResourceRecreateParams* params)
    {
        // TODO: Implement me!
        return dmResource::RESULT_NOT_SUPPORTED;
    }
}
