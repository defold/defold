// Copyright 2020-2026 The Defold Foundation
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

#include "res_data.h"

namespace dmGameSystem
{
    struct DataResource
    {
        dmGameSystemDDF::Data* m_DDF;
    };

    dmResource::Result ResDataPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmGameSystemDDF::Data* ddf = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameSystemDDF::Data>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDataCreate(const dmResource::ResourceCreateParams* params)
    {
        DataResource* resource = new DataResource();
        resource->m_DDF = (dmGameSystemDDF::Data*)params->m_PreloadData;

        dmResource::SetResource(params->m_Resource, resource);
        dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDataDestroy(const dmResource::ResourceDestroyParams* params)
    {
        DataResource* resource = (DataResource*)dmResource::GetResource(params->m_Resource);
        if (resource == 0x0)
        {
            return dmResource::RESULT_OK;
        }

        if (resource->m_DDF != 0x0)
        {
            dmDDF::FreeMessage(resource->m_DDF);
            resource->m_DDF = 0x0;
        }

        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDataRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmGameSystemDDF::Data* ddf = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameSystemDDF::Data>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        DataResource* resource = (DataResource*)dmResource::GetResource(params->m_Resource);
        if (resource->m_DDF != 0x0)
        {
            dmDDF::FreeMessage(resource->m_DDF);
        }

        resource->m_DDF = ddf;
        dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);
        return dmResource::RESULT_OK;
    }
}

