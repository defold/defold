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
#include "gamesys_private.h"

#include <dmsdk/resource/resource.h>
#include <dmsdk/graphics/graphics.h>

#include <resource/resource.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <render/render.h>
#include <render/render_target_ddf.h>

namespace dmGameSystem
{
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
        assert(ddf->m_ColorAttachments.m_Count <= dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS);

        const dmGraphics::BufferType color_buffer_flags[] = {
            dmGraphics::BUFFER_TYPE_COLOR0_BIT,
            dmGraphics::BUFFER_TYPE_COLOR1_BIT,
            dmGraphics::BUFFER_TYPE_COLOR2_BIT,
            dmGraphics::BUFFER_TYPE_COLOR3_BIT,
        };
        for (int i = 0; i < ddf->m_ColorAttachments.m_Count; ++i)
        {
            buffer_type_flags                                  |= color_buffer_flags[i];
            dmRenderDDF::RenderTargetDesc::ColorAttachment& att = ddf->m_ColorAttachments[i];
            dmGraphics::TextureFormat format                    = TextureImageToTextureFormat(att.m_Format);

            params.m_ColorBufferCreationParams[i].m_Type           = dmGraphics::TEXTURE_TYPE_2D;
            params.m_ColorBufferCreationParams[i].m_Width          = att.m_Width;
            params.m_ColorBufferCreationParams[i].m_Height         = att.m_Height;
            params.m_ColorBufferCreationParams[i].m_OriginalWidth  = att.m_Width;
            params.m_ColorBufferCreationParams[i].m_OriginalHeight = att.m_Height;
            params.m_ColorBufferCreationParams[i].m_MipMapCount    = 1;

            params.m_ColorBufferParams[i].m_Data     = 0;
            params.m_ColorBufferParams[i].m_DataSize = 0;
            params.m_ColorBufferParams[i].m_Format   = format;
            params.m_ColorBufferParams[i].m_Width    = att.m_Width;
            params.m_ColorBufferParams[i].m_Height   = att.m_Height;
            params.m_ColorBufferParams[i].m_Depth    = 1;

            params.m_ColorBufferLoadOps[i]  = dmGraphics::ATTACHMENT_OP_DONT_CARE;
            params.m_ColorBufferStoreOps[i] = dmGraphics::ATTACHMENT_OP_STORE;
        }

        if (ddf->m_DepthStencilAttachment.m_Width > 0 && ddf->m_DepthStencilAttachment.m_Height > 0)
        {
            // TODO: At some point in the future we should support these formats in the engine,
            //       but for now we will always create a render target with whatever default format we use.
            const bool has_depth   = true;
            const bool has_stencil = true;

            if (has_depth)
            {
                buffer_type_flags |= dmGraphics::BUFFER_TYPE_DEPTH_BIT;
                params.m_DepthBufferCreationParams.m_Type           = dmGraphics::TEXTURE_TYPE_2D;
                params.m_DepthBufferCreationParams.m_Width          = ddf->m_DepthStencilAttachment.m_Width;
                params.m_DepthBufferCreationParams.m_Height         = ddf->m_DepthStencilAttachment.m_Height;
                params.m_DepthBufferCreationParams.m_OriginalWidth  = ddf->m_DepthStencilAttachment.m_Width;
                params.m_DepthBufferCreationParams.m_OriginalHeight = ddf->m_DepthStencilAttachment.m_Height;
                params.m_DepthBufferCreationParams.m_MipMapCount    = 1;

                params.m_DepthBufferParams.m_Data     = 0;
                params.m_DepthBufferParams.m_DataSize = 0;
                params.m_DepthBufferParams.m_Format   = dmGraphics::TEXTURE_FORMAT_DEPTH;
                params.m_DepthBufferParams.m_Width    = ddf->m_DepthStencilAttachment.m_Width;
                params.m_DepthBufferParams.m_Height   = ddf->m_DepthStencilAttachment.m_Height;
                params.m_DepthBufferParams.m_Depth    = 1;

                params.m_DepthTexture = ddf->m_DepthStencilAttachment.m_TextureStorage;
            }

            if (has_stencil)
            {
                buffer_type_flags |= dmGraphics::BUFFER_TYPE_STENCIL_BIT;

                params.m_StencilBufferCreationParams.m_Type           = dmGraphics::TEXTURE_TYPE_2D;
                params.m_StencilBufferCreationParams.m_Width          = ddf->m_DepthStencilAttachment.m_Width;
                params.m_StencilBufferCreationParams.m_Height         = ddf->m_DepthStencilAttachment.m_Height;
                params.m_StencilBufferCreationParams.m_OriginalWidth  = ddf->m_DepthStencilAttachment.m_Width;
                params.m_StencilBufferCreationParams.m_OriginalHeight = ddf->m_DepthStencilAttachment.m_Height;
                params.m_StencilBufferCreationParams.m_MipMapCount    = 1;

                params.m_StencilBufferParams.m_Data     = 0;
                params.m_StencilBufferParams.m_DataSize = 0;
                params.m_StencilBufferParams.m_Format   = dmGraphics::TEXTURE_FORMAT_STENCIL;
                params.m_StencilBufferParams.m_Width    = ddf->m_DepthStencilAttachment.m_Width;
                params.m_StencilBufferParams.m_Height   = ddf->m_DepthStencilAttachment.m_Height;
                params.m_StencilBufferParams.m_Depth    = 1;
                params.m_StencilTexture                 = false; // Currently not supported
            }
        }
    }

    dmResource::Result ResRenderTargetCreate(const dmResource::ResourceCreateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;

        dmRenderDDF::RenderTargetDesc* ddf = (dmRenderDDF::RenderTargetDesc*) params.m_PreloadData;

        uint32_t buffer_type_flags = 0;
        dmGraphics::RenderTargetCreationParams rt_params = {};
        GetRenderTargetParams(ddf, buffer_type_flags, rt_params);

        dmDDF::FreeMessage(ddf);

        RenderTargetResource* rt_resource = new RenderTargetResource();
        rt_resource->m_RenderTarget       = dmGraphics::NewRenderTarget(dmRender::GetGraphicsContext(render_context), buffer_type_flags, rt_params);

        char path_buffer[256];
        dmSnPrintf(path_buffer, sizeof(path_buffer), "%s.texturec", params.m_Filename);

        dmGraphics::TextureImage texture_image = {};

        dmArray<uint8_t> ddf_buffer;
        dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(&texture_image, dmGraphics::TextureImage::m_DDFDescriptor, ddf_buffer);
        assert(ddf_result == dmDDF::RESULT_OK);

        // CreateResource will set ref to 1, no need to call Get on the path again
        dmResource::Result res = dmResource::CreateResource(params.m_Factory, path_buffer, ddf_buffer.Begin(), ddf_buffer.Size(), (void**) &rt_resource->m_TextureResource);

        if (res != dmResource::RESULT_OK)
        {
            delete rt_resource;
            return res;
        }

        rt_resource->m_TextureResource->m_Texture = dmGraphics::GetRenderTargetTexture(rt_resource->m_RenderTarget, dmGraphics::BUFFER_TYPE_COLOR0_BIT);
        dmResource::SetResource(params.m_Resource, (void*) rt_resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderTargetDestroy(const dmResource::ResourceDestroyParams& params)
    {
        RenderTargetResource* rt_resource = (RenderTargetResource*) dmResource::GetResource(params.m_Resource);
        assert(dmGraphics::GetAssetType(rt_resource->m_RenderTarget) == dmGraphics::ASSET_TYPE_RENDER_TARGET);
        dmGraphics::DeleteRenderTarget(rt_resource->m_RenderTarget);
        dmResource::Release(params.m_Factory, rt_resource->m_TextureResource);

        delete rt_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderTargetRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRenderDDF::RenderTargetDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRenderDDF_RenderTargetDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        RenderTargetResource* rt_resource = (RenderTargetResource*) dmResource::GetResource(params.m_Resource);

        uint32_t buffer_type_flags = 0;
        dmGraphics::RenderTargetCreationParams rt_params = {};
        GetRenderTargetParams(ddf, buffer_type_flags, rt_params);
        dmDDF::FreeMessage(ddf);

        if (rt_resource->m_RenderTarget)
        {
            dmGraphics::DeleteRenderTarget(rt_resource->m_RenderTarget);
        }

        dmRender::HRenderContext render_context  = (dmRender::HRenderContext) params.m_Context;
        rt_resource->m_RenderTarget              = dmGraphics::NewRenderTarget(dmRender::GetGraphicsContext(render_context), buffer_type_flags, rt_params);
        // The texture is owned by the RT, so no need to delete the old one.
        rt_resource->m_TextureResource->m_Texture = dmGraphics::GetRenderTargetTexture(rt_resource->m_RenderTarget, dmGraphics::BUFFER_TYPE_COLOR0_BIT);

        return dmResource::RESULT_OK;
    }
}
