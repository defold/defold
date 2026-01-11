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

#include "res_light.h"

#include <gamesys/gamesys_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResLightCreate(const dmResource::ResourceCreateParams* params)
    {
        dmGameSystemDDF::LightDesc* light_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &light_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmGameSystemDDF::LightDesc** light_resource = new dmGameSystemDDF::LightDesc*;
        *light_resource = light_desc;
        dmResource::SetResource(params->m_Resource, light_resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightDestroy(const dmResource::ResourceDestroyParams* params)
    {
        dmGameSystemDDF::LightDesc** light_resource = (dmGameSystemDDF::LightDesc**) dmResource::GetResource(params->m_Resource);
        dmDDF::FreeMessage(*light_resource);
        delete light_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmGameSystemDDF::LightDesc* light_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &light_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmGameSystemDDF::LightDesc** light_resource = (dmGameSystemDDF::LightDesc**) dmResource::GetResource(params->m_Resource);
        dmDDF::FreeMessage(*light_resource);
        *light_resource = light_desc;
        return dmResource::RESULT_OK;
    }
}
