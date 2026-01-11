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

#include "res_collection_proxy.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResource(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, CollectionProxyResource* resource)
    {
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        return dmResource::RESULT_OK;
    }

    void ReleaseResources(dmResource::HFactory factory, CollectionProxyResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
    }

    dmResource::Result ResCollectionProxyCreate(const dmResource::ResourceCreateParams* params)
    {
        CollectionProxyResource* cspr = new CollectionProxyResource();
        dmResource::Result r = AcquireResource(params->m_Factory, params->m_Buffer, params->m_BufferSize, cspr);
        if (r == dmResource::RESULT_OK)
        {
            cspr->m_UrlHash = dmHashString64(params->m_Filename);
            dmResource::SetResource(params->m_Resource, cspr);
        }
        else
        {
            ReleaseResources(params->m_Factory, cspr);
        }
        return r;
    }

    dmResource::Result ResCollectionProxyDestroy(const dmResource::ResourceDestroyParams* params)
    {
        CollectionProxyResource* cspr = (CollectionProxyResource*) dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, cspr);
        delete cspr;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionProxyRecreate(const dmResource::ResourceRecreateParams* params)
    {
        CollectionProxyResource tmp_cspr;
        dmResource::Result r = AcquireResource(params->m_Factory, params->m_Buffer, params->m_BufferSize, &tmp_cspr);
        if (r == dmResource::RESULT_OK)
        {
            CollectionProxyResource* cspr = (CollectionProxyResource*) dmResource::GetResource(params->m_Resource);
            ReleaseResources(params->m_Factory, cspr);
            *cspr = tmp_cspr;
        }
        else
        {
            ReleaseResources(params->m_Factory, &tmp_cspr);
        }
        return r;
    }
}
