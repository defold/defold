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

#include "res_render_target.h"
#include "res_texture.h"
#include "gamesys_private.h"

#include <dmsdk/graphics/graphics.h>

#include <resource/resource.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <render/render.h>
#include <render/render_target_ddf.h>

namespace dmGameSystem
{
    dmResource::Result ResRenderTargetPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmRenderDDF::RenderTargetDesc* RenderTarget;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmRenderDDF_RenderTargetDesc_DESCRIPTOR, (void**) &RenderTarget);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params->m_PreloadData = RenderTarget;
        return dmResource::RESULT_OK;
    }

    static void GetRenderTargetParams(dmRenderDDF::RenderTargetDesc* ddf, uint32_t& buffer_type_flags, dmGraphics::RenderTargetCreationParams& params)
    {
        assert(ddf->m_ColorAttachments.m_Count <= dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS);

        for (int i = 0; i < ddf->m_ColorAttachments.m_Count; ++i)
        {
            buffer_type_flags                                  |= dmGraphics::GetBufferTypeFromIndex(i);
            dmRenderDDF::RenderTargetDesc::ColorAttachment& att = ddf->m_ColorAttachments[i];
            dmGraphics::TextureFormat format                    = TextureImageToTextureFormat(att.m_Format);

            params.m_ColorBufferCreationParams[i].m_Type           = dmGraphics::TEXTURE_TYPE_2D;
            params.m_ColorBufferCreationParams[i].m_Width          = att.m_Width;
            params.m_ColorBufferCreationParams[i].m_Height         = att.m_Height;
            params.m_ColorBufferCreationParams[i].m_OriginalWidth  = att.m_Width;
            params.m_ColorBufferCreationParams[i].m_OriginalHeight = att.m_Height;
            params.m_ColorBufferCreationParams[i].m_MipMapCount    = 1;

            params.m_ColorBufferParams[i].m_Data       = 0;
            params.m_ColorBufferParams[i].m_DataSize   = 0;
            params.m_ColorBufferParams[i].m_Format     = format;
            params.m_ColorBufferParams[i].m_Width      = att.m_Width;
            params.m_ColorBufferParams[i].m_Height     = att.m_Height;
            params.m_ColorBufferParams[i].m_Depth      = 1;
            params.m_ColorBufferParams[i].m_LayerCount = 1;

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

                params.m_DepthBufferParams.m_Data       = 0;
                params.m_DepthBufferParams.m_DataSize   = 0;
                params.m_DepthBufferParams.m_Format     = dmGraphics::TEXTURE_FORMAT_DEPTH;
                params.m_DepthBufferParams.m_Width      = ddf->m_DepthStencilAttachment.m_Width;
                params.m_DepthBufferParams.m_Height     = ddf->m_DepthStencilAttachment.m_Height;
                params.m_DepthBufferParams.m_Depth      = 1;
                params.m_DepthBufferParams.m_LayerCount = 1;

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

                params.m_StencilBufferParams.m_Data       = 0;
                params.m_StencilBufferParams.m_DataSize   = 0;
                params.m_StencilBufferParams.m_Format     = dmGraphics::TEXTURE_FORMAT_STENCIL;
                params.m_StencilBufferParams.m_Width      = ddf->m_DepthStencilAttachment.m_Width;
                params.m_StencilBufferParams.m_Height     = ddf->m_DepthStencilAttachment.m_Height;
                params.m_StencilBufferParams.m_Depth      = 1;
                params.m_StencilBufferParams.m_LayerCount = 1;
                params.m_StencilTexture                   = false; // Currently not supported
            }
        }
    }

    static void DestroyResources(dmGraphics::HContext graphics_context, dmResource::HFactory factory, RenderTargetResource* rt_resource)
    {
        assert(dmGraphics::GetAssetType(rt_resource->m_RenderTarget) == dmGraphics::ASSET_TYPE_RENDER_TARGET);
        dmGraphics::DeleteRenderTarget(graphics_context, rt_resource->m_RenderTarget);

        for (int i = 0; i < dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt_resource->m_ColorAttachmentResources[i])
            {
                dmResource::Release(factory, rt_resource->m_ColorAttachmentResources[i]);
            }
        }

        if (rt_resource->m_DepthAttachmentPath)
        {
            dmResource::Release(factory, rt_resource->m_DepthAttachmentResource);
        }

        delete rt_resource;
    }

    static inline void FillTextureAttachmentResourcePath(const char* file_name, dmGraphics::BufferType buffer_type, char* buf, uint32_t buf_size)
    {
        if (dmGraphics::IsColorBufferType(buffer_type))
        {
            dmSnPrintf(buf, buf_size, "%s_color_%d.texturec", file_name, dmGraphics::GetBufferTypeIndex(buffer_type));
        }
        else if (buffer_type == dmGraphics::BUFFER_TYPE_DEPTH_BIT)
        {
            dmSnPrintf(buf, buf_size, "%s_depth.texturec", file_name);
        }
        else if (buffer_type == dmGraphics::BUFFER_TYPE_STENCIL_BIT)
        {
            dmSnPrintf(buf, buf_size, "%s_stencil.texturec", file_name);
        }
    }

    static dmResource::Result CreateAttachmentTexture(dmGraphics::HContext graphics_context,
        dmResource::HFactory factory,
        RenderTargetResource* rt_resource, dmGraphics::BufferType buffer_type, const char* path,
        char* path_buffer, uint32_t path_buffer_size, dmArray<uint8_t>& buffer)
    {
        if (dmGraphics::IsColorBufferType(buffer_type))
        {
            uint32_t buffer_index = dmGraphics::GetBufferTypeIndex(buffer_type);
            assert(rt_resource->m_ColorAttachmentResources[buffer_index] == 0x0);
            FillTextureAttachmentResourcePath(path, buffer_type, path_buffer, path_buffer_size);
            dmResource::Result res = dmResource::CreateResource(factory, path_buffer, buffer.Begin(), buffer.Size(), (void**) &rt_resource->m_ColorAttachmentResources[buffer_index]);
            if (res != dmResource::RESULT_OK)
            {
                return res;
            }

            rt_resource->m_ColorAttachmentResources[buffer_index]->m_Texture = dmGraphics::GetRenderTargetTexture(graphics_context, rt_resource->m_RenderTarget, buffer_type);
            rt_resource->m_ColorAttachmentPaths[buffer_index]                = dmHashString64(path_buffer);
        }
        else if (buffer_type == dmGraphics::BUFFER_TYPE_DEPTH_BIT)
        {
            assert(rt_resource->m_DepthAttachmentResource == 0x0);
            FillTextureAttachmentResourcePath(path, dmGraphics::BUFFER_TYPE_DEPTH_BIT, path_buffer, path_buffer_size);
            dmResource::Result res = dmResource::CreateResource(factory, path_buffer, buffer.Begin(), buffer.Size(), (void**) &rt_resource->m_DepthAttachmentResource);
            if (res != dmResource::RESULT_OK)
            {
                return res;
            }

            rt_resource->m_DepthAttachmentResource->m_Texture = dmGraphics::GetRenderTargetTexture(graphics_context, rt_resource->m_RenderTarget, dmGraphics::BUFFER_TYPE_DEPTH_BIT);
            rt_resource->m_DepthAttachmentPath                = dmHashString64(path_buffer);
        }
        return dmResource::RESULT_OK;
    }

    static dmResource::Result CreateAttachmentResources(dmGraphics::HContext graphics_context, dmResource::HFactory factory, RenderTargetResource* rt_resource, const char* path, uint32_t num_color_textures, bool depth_texture)
    {
        // Create 'dummy' texture resources for each color buffer (and depth/stencil buffer)
        // so that each attachment can be used as a texture elsewhere in the engine.
        dmGraphics::TextureImage texture_image = {};
        dmArray<uint8_t> texture_resource_buffer;
        FillTextureResourceBuffer(&texture_image, texture_resource_buffer);

        char path_buffer[256];
        for (int i = 0; i < num_color_textures; ++i)
        {
            dmResource::Result res = CreateAttachmentTexture(graphics_context, factory, rt_resource, dmGraphics::GetBufferTypeFromIndex(i), path, path_buffer, sizeof(path_buffer), texture_resource_buffer);
            if (res != dmResource::RESULT_OK)
            {
                DestroyResources(graphics_context, factory, rt_resource);
                return res;
            }
        }

        if (depth_texture)
        {
            dmResource::Result res = CreateAttachmentTexture(graphics_context, factory, rt_resource, dmGraphics::BUFFER_TYPE_DEPTH_BIT, path, path_buffer, sizeof(path_buffer), texture_resource_buffer);
            if (res != dmResource::RESULT_OK)
            {
                DestroyResources(graphics_context, factory, rt_resource);
                return res;
            }
        }
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderTargetCreate(const dmResource::ResourceCreateParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        dmRenderDDF::RenderTargetDesc* ddf = (dmRenderDDF::RenderTargetDesc*) params->m_PreloadData;

        uint32_t num_color_textures = ddf->m_ColorAttachments.m_Count;
        uint32_t buffer_type_flags = 0;
        dmGraphics::RenderTargetCreationParams rt_params = {};
        GetRenderTargetParams(ddf, buffer_type_flags, rt_params);

        dmDDF::FreeMessage(ddf);

        RenderTargetResource* rt_resource = new RenderTargetResource();
        rt_resource->m_RenderTarget       = dmGraphics::NewRenderTarget(graphics_context, buffer_type_flags, rt_params);

        dmResource::Result res = CreateAttachmentResources(graphics_context, params->m_Factory, rt_resource, params->m_Filename, num_color_textures, rt_params.m_DepthTexture);
        if (res != dmResource::RESULT_OK)
        {
            DestroyResources(graphics_context, params->m_Factory, rt_resource);
            return res;
        }

        dmResource::SetResource(params->m_Resource, (void*) rt_resource);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderTargetDestroy(const dmResource::ResourceDestroyParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        RenderTargetResource* rt_resource = (RenderTargetResource*) dmResource::GetResource(params->m_Resource);
        DestroyResources(graphics_context, params->m_Factory, rt_resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRenderTargetRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        dmRenderDDF::RenderTargetDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmRenderDDF_RenderTargetDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        RenderTargetResource* rt_resource = (RenderTargetResource*) dmResource::GetResource(params->m_Resource);

        uint32_t num_color_textures = ddf->m_ColorAttachments.m_Count;
        uint32_t buffer_type_flags = 0;
        dmGraphics::RenderTargetCreationParams rt_params = {};
        GetRenderTargetParams(ddf, buffer_type_flags, rt_params);
        dmDDF::FreeMessage(ddf);

        if (rt_resource->m_RenderTarget)
        {
            dmGraphics::DeleteRenderTarget(graphics_context, rt_resource->m_RenderTarget);
        }

        rt_resource->m_RenderTarget              = dmGraphics::NewRenderTarget(dmRender::GetGraphicsContext(render_context), buffer_type_flags, rt_params);

        // Clear out any existing resources and recreate new ones for the updated resource.
        for (int i = 0; i < dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt_resource->m_ColorAttachmentResources[i])
            {
                dmResource::Release(params->m_Factory, rt_resource->m_ColorAttachmentResources[i]);
            }
        }

        if (rt_resource->m_DepthAttachmentResource)
        {
            dmResource::Release(params->m_Factory, rt_resource->m_DepthAttachmentResource);
        }

        // Clear out any reference to existing resources and paths
        memset(rt_resource->m_ColorAttachmentResources, 0x0, sizeof(rt_resource->m_ColorAttachmentResources));
        memset(rt_resource->m_ColorAttachmentPaths, 0x0, sizeof(rt_resource->m_ColorAttachmentPaths));
        memset(&rt_resource->m_DepthAttachmentResource, 0x0, sizeof(rt_resource->m_DepthAttachmentResource));
        memset(&rt_resource->m_DepthAttachmentPath, 0x0, sizeof(rt_resource->m_DepthAttachmentPath));

        dmResource::Result res = CreateAttachmentResources(graphics_context, params->m_Factory, rt_resource, params->m_Filename, num_color_textures, rt_params.m_DepthTexture);
        if (res != dmResource::RESULT_OK)
        {
            DestroyResources(graphics_context, params->m_Factory, rt_resource);
            return res;
        }

        return dmResource::RESULT_OK;
    }
}
