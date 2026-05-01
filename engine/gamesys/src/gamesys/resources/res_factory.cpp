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

#include "res_factory.h"
#include <dlib/dstrings.h>

#include <gameobject/gameobject.h>

namespace dmGameSystem
{

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmGameSystemDDF::FactoryDesc* factory_desc, FactoryResource* factory_res)
    {
        factory_res->m_LoadDynamically = factory_desc->m_LoadDynamically;
        factory_res->m_DynamicPrototype = factory_desc->m_DynamicPrototype;
        factory_res->m_PrototypePath = strdup(factory_desc->m_Prototype);
        if(factory_res->m_LoadDynamically)
        {
            return dmResource::RESULT_OK;
        }
        return dmResource::Get(factory, factory_res->m_PrototypePath, (void **)&factory_res->m_Prototype);
    }

    static void ReleaseResources(dmResource::HFactory factory, FactoryResource* factory_res)
    {
        if(factory_res->m_Prototype)
            dmResource::Release(factory, factory_res->m_Prototype);
        free((void*)factory_res->m_PrototypePath);
    }

    dmResource::Result ResFactoryLoadResource(dmResource::HFactory factory, const char* goc_path, bool load_dynamically, bool dynamic_prototype, FactoryResource** out_res)
    {
        FactoryResource* factory_res = new FactoryResource;
        memset(factory_res, 0, sizeof(FactoryResource));
        factory_res->m_PrototypePath = strdup(goc_path);
        factory_res->m_LoadDynamically = load_dynamically;
        factory_res->m_DynamicPrototype = dynamic_prototype;
        *out_res = factory_res;
        if(factory_res->m_LoadDynamically)
        {
            return dmResource::RESULT_OK;
        }
        return dmResource::Get(factory, factory_res->m_PrototypePath, (void **)&factory_res->m_Prototype);
    }

    void ResFactoryDestroyResource(dmResource::HFactory factory, FactoryResource* resource)
    {
        ReleaseResources(factory, resource);
        delete resource;
    }

    dmResource::Result ResFactoryPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmGameSystemDDF::FactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;
        if((!ddf->m_LoadDynamically) && (params->m_HintInfo))
        {
            dmResource::PreloadHint(params->m_HintInfo, ddf->m_Prototype);
        }
        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryCreate(const dmResource::ResourceCreateParams* params)
    {
        dmGameSystemDDF::FactoryDesc* ddf = (dmGameSystemDDF::FactoryDesc*) params->m_PreloadData;
        FactoryResource* factory_res = new FactoryResource;
        memset(factory_res, 0, sizeof(FactoryResource));
        dmResource::Result res = AcquireResources(params->m_Factory, ddf, factory_res);
        dmDDF::FreeMessage(ddf);

        if(res == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, factory_res);
        }
        else
        {
            ResFactoryDestroyResource(params->m_Factory, factory_res);
        }
        return res;
    }

    dmResource::Result ResFactoryDestroy(const dmResource::ResourceDestroyParams* params)
    {
        FactoryResource* factory_res = (FactoryResource*) dmResource::GetResource(params->m_Resource);
        ResFactoryDestroyResource(params->m_Factory, factory_res);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFactoryRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmGameSystemDDF::FactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
            return dmResource::RESULT_DDF_ERROR;

        FactoryResource tmp_factory_res;
        memset(&tmp_factory_res, 0, sizeof(FactoryResource));
        dmResource::Result r = AcquireResources(params->m_Factory, ddf, &tmp_factory_res);
        dmDDF::FreeMessage(ddf);

        if (r == dmResource::RESULT_OK)
        {
            FactoryResource* factory_res = (FactoryResource*) dmResource::GetResource(params->m_Resource);
            ReleaseResources(params->m_Factory, factory_res);
            *factory_res = tmp_factory_res;
        }
        else
        {
            ReleaseResources(params->m_Factory, &tmp_factory_res);
        }
        return r;
    }
}
