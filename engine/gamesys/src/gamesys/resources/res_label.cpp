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

#include <string.h>

#include "res_label.h"
#include <dlib/log.h>
#include <gamesys/label_ddf.h>
#include <dmsdk/gamesys/resources/res_material.h>
#include <dmsdk/gamesys/resources/res_font.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, LabelResource* resource, const char* filename)
    {
        dmResource::Result result;
        result = dmResource::Get(factory, resource->m_DDF->m_Material, (void**)&resource->m_Material);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }
        if(dmRender::GetMaterialVertexSpace(resource->m_Material->m_Material) != dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD)
        {
            dmLogError("Failed to create Label component. This component only supports materials with the Vertex Space property set to 'vertex-space-world'");
            return dmResource::RESULT_NOT_SUPPORTED;
        }
        result = dmResource::Get(factory, resource->m_DDF->m_Font, (void**) &resource->m_Font);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        return dmResource::RESULT_OK;
    }

    void ReleaseResources(dmResource::HFactory factory, LabelResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
        if (resource->m_Font != 0x0)
            dmResource::Release(factory, resource->m_Font);
    }

    dmResource::Result ResLabelPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmGameSystemDDF::LabelDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Material);
        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Font);

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLabelCreate(const dmResource::ResourceCreateParams* params)
    {
        LabelResource* resource = new LabelResource();
        memset(resource, 0, sizeof(LabelResource));
        resource->m_DDF = (dmGameSystemDDF::LabelDesc*) params->m_PreloadData;

        dmResource::Result r = AcquireResources(params->m_Factory, resource, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, resource);
        }
        else
        {
            ReleaseResources(params->m_Factory, resource);
            delete resource;
        }
        return r;
    }

    dmResource::Result ResLabelDestroy(const dmResource::ResourceDestroyParams* params)
    {
        LabelResource* resource = (LabelResource*) dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, resource);
        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLabelRecreate(const dmResource::ResourceRecreateParams* params)
    {
        LabelResource tmp_resource;
        memset(&tmp_resource, 0, sizeof(LabelResource));
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &tmp_resource.m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result r = AcquireResources(params->m_Factory, &tmp_resource, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            LabelResource* resource = (LabelResource*)dmResource::GetResource(params->m_Resource);
            ReleaseResources(params->m_Factory, resource);
            *resource = tmp_resource;
        }
        else
        {
            ReleaseResources(params->m_Factory, &tmp_resource);
        }
        return r;
    }
}
