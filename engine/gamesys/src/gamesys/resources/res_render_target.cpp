// Copyright 2020-2023 The Defold Foundation
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

#include "res_render_target.h"
#include "res_texture.h"

#include <dmsdk/resource/resource.h>
#include <resource/resource.h>
#include <dlib/log.h>

#include <render/render.h>

namespace dmGameSystem
{
    static dmResource::Result AcquireResources(dmResource::HFactory factory, TextureResource* resource, const char* filename)
    {
        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, TextureResource* resource)
    {
        // if (resource->m_DDF != 0x0)
        //     dmDDF::FreeMessage(resource->m_DDF);
    }

    dmResource::Result ResRenderTargetPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRenderDDF::RenderTargetDesc* RenderTarget;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRenderDDF_RenderTargetDesc_DESCRIPTOR, (void**) &RenderTarget);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params.m_PreloadData = RenderTarget;
        return dmResource::RESULT_OK;
    }

    static void GetRenderTargetParams(dmRenderDDF::RenderTargetDesc* ddf, uint32_t& buffer_type_flags, dmGraphics::RenderTargetCreationParams& params)
    {
        dmGraphics::TextureFormat attachment_format = TextureImageToTextureFormat(ddf->m_AttachmentFormat);
        dmGraphics::TextureType attachment_type     = TextureImageToTextureType(ddf->m_AttachmentType);
        assert(attachment_type == dmGraphics::TEXTURE_TYPE_2D);
        assert(ddf->m_AttachmentCount < dmGraphics::MAX_ATTACHMENT_COUNT);

        dmGraphics::BufferType color_buffer_flags[] = {
            dmGraphics::BUFFER_TYPE_COLOR0_BIT,
            dmGraphics::BUFFER_TYPE_COLOR1_BIT,
            dmGraphics::BUFFER_TYPE_COLOR2_BIT,
            dmGraphics::BUFFER_TYPE_COLOR3_BIT,
        };

        for (int i = 0; i < ddf->m_AttachmentCount; ++i)
        {
            params.m_ColorBufferCreationParams[i].m_Type           = attachment_type;
            params.m_ColorBufferCreationParams[i].m_Width          = ddf->m_Width;
            params.m_ColorBufferCreationParams[i].m_Height         = ddf->m_Height;
            params.m_ColorBufferCreationParams[i].m_OriginalWidth  = ddf->m_Width;
            params.m_ColorBufferCreationParams[i].m_OriginalHeight = ddf->m_Height;
            params.m_ColorBufferCreationParams[i].m_MipMapCount    = 1;

            params.m_ColorBufferParams[i].m_Data     = 0;
            params.m_ColorBufferParams[i].m_DataSize = 0;
            params.m_ColorBufferParams[i].m_Format   = attachment_format;
            params.m_ColorBufferParams[i].m_Width    = ddf->m_Width;
            params.m_ColorBufferParams[i].m_Height   = ddf->m_Height;
            params.m_ColorBufferParams[i].m_Depth    = 1;

            buffer_type_flags |= color_buffer_flags[i];
        }

        if (ddf->m_DepthStencil)
        {
            buffer_type_flags |= dmGraphics::BUFFER_TYPE_DEPTH_BIT;
            buffer_type_flags |= dmGraphics::BUFFER_TYPE_STENCIL_BIT;

            params.m_DepthBufferCreationParams.m_Type           = dmGraphics::TEXTURE_TYPE_2D;
            params.m_DepthBufferCreationParams.m_Width          = ddf->m_Width;
            params.m_DepthBufferCreationParams.m_Height         = ddf->m_Height;
            params.m_DepthBufferCreationParams.m_OriginalWidth  = ddf->m_Width;
            params.m_DepthBufferCreationParams.m_OriginalHeight = ddf->m_Height;
            params.m_DepthBufferCreationParams.m_MipMapCount    = 1;

            params.m_DepthBufferParams.m_Data     = 0;
            params.m_DepthBufferParams.m_DataSize = 0;
            params.m_DepthBufferParams.m_Format   = dmGraphics::TEXTURE_FORMAT_DEPTH;
            params.m_DepthBufferParams.m_Width    = ddf->m_Width;
            params.m_DepthBufferParams.m_Height   = ddf->m_Height;
            params.m_DepthBufferParams.m_Depth    = 1;

            params.m_StencilBufferCreationParams.m_Type           = dmGraphics::TEXTURE_TYPE_2D;
            params.m_StencilBufferCreationParams.m_Width          = ddf->m_Width;
            params.m_StencilBufferCreationParams.m_Height         = ddf->m_Height;
            params.m_StencilBufferCreationParams.m_OriginalWidth  = ddf->m_Width;
            params.m_StencilBufferCreationParams.m_OriginalHeight = ddf->m_Height;
            params.m_StencilBufferCreationParams.m_MipMapCount    = 1;

            params.m_StencilBufferParams.m_Data     = 0;
            params.m_StencilBufferParams.m_DataSize = 0;
            params.m_StencilBufferParams.m_Format   = dmGraphics::TEXTURE_FORMAT_STENCIL;
            params.m_StencilBufferParams.m_Width    = ddf->m_Width;
            params.m_StencilBufferParams.m_Height   = ddf->m_Height;
            params.m_StencilBufferParams.m_Depth    = 1;
        }
    }

    dmResource::Result ResRenderTargetCreate(const dmResource::ResourceCreateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;

        dmRenderDDF::RenderTargetDesc* ddf = (dmRenderDDF::RenderTargetDesc*) params.m_PreloadData;
        // dmResource::Result r = AcquireResources(params.m_Factory, rt_resource, params.m_Filename);

        uint32_t buffer_type_flags = 0;
        dmGraphics::RenderTargetCreationParams rt_params = {};
        GetRenderTargetParams(ddf, buffer_type_flags, rt_params);

        dmDDF::FreeMessage(ddf);

        TextureResource* rt_resource = new TextureResource();
        rt_resource->m_IsTexture     = 0;
        rt_resource->m_RenderTarget  = dmGraphics::NewRenderTarget(
            dmRender::GetGraphicsContext(render_context),
            buffer_type_flags,
            rt_params);

        dmResource::SetResource(params.m_Resource, (void*) rt_resource);

        // if (r == dmResource::RESULT_OK)
        // {
        //     dmResource::SetResource(params.m_Resource, (void*) rt_resource);
        // }
        // else
        // {
        //     ReleaseResources(params.m_Factory, rt_resource);
        //     delete rt_resource;
        // }
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderTargetDestroy(const dmResource::ResourceDestroyParams& params)
    {
        TextureResource* rt_resource = (TextureResource*)dmResource::GetResource(params.m_Resource);
        assert(!rt_resource->m_IsTexture);

        ReleaseResources(params.m_Factory, rt_resource);

        dmGraphics::DeleteRenderTarget(rt_resource->m_RenderTarget);

        delete rt_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderTargetRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRenderDDF::RenderTargetDesc* rt;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRenderDDF_RenderTargetDesc_DESCRIPTOR, (void**) &rt);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        // RenderTargetResource* rt_resource = (RenderTargetResource*)dmResource::GetResource(params.m_Resource);
        // ReleaseResources(params.m_Factory, rt_resource);
        // rt_resource->m_DDF = rt;
        // return AcquireResources(params.m_Factory, rt_resource, params.m_Filename);
        return dmResource::RESULT_OK;
    }
}
