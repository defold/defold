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

#include "res_input_binding.h"

#include <ddf/ddf.h>

#include <input/input.h>

#include <input/input_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResInputBindingCreate(const dmResource::ResourceCreateParams* params)
    {
        dmInputDDF::InputBinding* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmInputDDF_InputBinding_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmInput::HBinding binding = dmInput::NewBinding((dmInput::HContext) params->m_Context);
        dmInput::SetBinding(binding, ddf);
        dmDDF::FreeMessage((void*)ddf);

        dmResource::SetResource(params->m_Resource, binding);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResInputBindingDestroy(const dmResource::ResourceDestroyParams* params)
    {
        dmInput::DeleteBinding((dmInput::HBinding)dmResource::GetResource(params->m_Resource));
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResInputBindingRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmInputDDF::InputBinding* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmInputDDF_InputBinding_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmInput::HBinding binding = (dmInput::HBinding)dmResource::GetResource(params->m_Resource);
        dmInput::SetBinding(binding, ddf);
        dmDDF::FreeMessage((void*)ddf);
        return dmResource::RESULT_OK;
    }
}
