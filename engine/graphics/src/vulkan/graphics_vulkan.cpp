// Copyright 2020-2022 The Defold Foundation
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

#include <dlib/math.h>
#include <dlib/array.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <dmsdk/vectormath/cpp/vectormath_aos.h>

#include "../graphics_private.h"
#include "../graphics_native.h"
#include "../graphics_adapter.h"
#include "graphics_vulkan_defines.h"
#include "graphics_vulkan_private.h"

DM_PROPERTY_EXTERN(rmtp_DrawCalls);

namespace dmGraphics
{
    static GraphicsAdapterFunctionTable VulkanRegisterFunctionTable();
    static bool                         VulkanIsSupported();
    static const int8_t    g_vulkan_adapter_priority = 0;
    static GraphicsAdapter g_vulkan_adapter(ADAPTER_TYPE_VULKAN);

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterVulkan, &g_vulkan_adapter, VulkanIsSupported, VulkanRegisterFunctionTable, g_vulkan_adapter_priority);

    static const char* VkResultToStr(VkResult res);
    #define CHECK_VK_ERROR(result) \
    { \
        if(g_VulkanContext->m_VerifyGraphicsCalls && result != VK_SUCCESS) { \
            dmLogError("Vulkan Error (%s:%d) %s", __FILE__, __LINE__, VkResultToStr(result)); \
            assert(0); \
        } \
    }

    Context* g_VulkanContext = 0;

    static HTexture VulkanNewTexture(HContext context, const TextureCreationParams& params);
    static void     VulkanSetTexture(HTexture texture, const TextureParams& params);
    static void     CopyToTexture(HContext context, const TextureParams& params, bool useStageBuffer, uint32_t texDataSize, void* texDataPtr, Texture* textureOut);
    static VkFormat GetVulkanFormatFromTextureFormat(TextureFormat format);

    #define DM_VK_RESULT_TO_STR_CASE(x) case x: return #x
    static const char* VkResultToStr(VkResult res)
    {
        switch(res)
        {
            DM_VK_RESULT_TO_STR_CASE(VK_SUCCESS);
            DM_VK_RESULT_TO_STR_CASE(VK_NOT_READY);
            DM_VK_RESULT_TO_STR_CASE(VK_TIMEOUT);
            DM_VK_RESULT_TO_STR_CASE(VK_EVENT_SET);
            DM_VK_RESULT_TO_STR_CASE(VK_EVENT_RESET);
            DM_VK_RESULT_TO_STR_CASE(VK_INCOMPLETE);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INITIALIZATION_FAILED);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_DEVICE_LOST);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_MEMORY_MAP_FAILED);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_LAYER_NOT_PRESENT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_TOO_MANY_OBJECTS);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FRAGMENTED_POOL);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_SURFACE_LOST_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_SUBOPTIMAL_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_OUT_OF_DATE_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INVALID_SHADER_NV);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FRAGMENTATION_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_NOT_PERMITTED_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_RESULT_MAX_ENUM);
            default: break;
        }

        return "UNKNOWN_ERROR";
    }
    #undef DM_VK_RESULT_TO_STR_CASE

    #define DM_TEXTURE_FORMAT_TO_STR_CASE(x) case TEXTURE_FORMAT_##x: return #x;
    static const char* TextureFormatToString(TextureFormat format)
    {
        switch(format)
        {
            DM_TEXTURE_FORMAT_TO_STR_CASE(LUMINANCE);
            DM_TEXTURE_FORMAT_TO_STR_CASE(LUMINANCE_ALPHA);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_16BPP);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_16BPP);
            DM_TEXTURE_FORMAT_TO_STR_CASE(DEPTH);
            DM_TEXTURE_FORMAT_TO_STR_CASE(STENCIL);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_PVRTC_2BPPV1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_PVRTC_4BPPV1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_PVRTC_2BPPV1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_PVRTC_4BPPV1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_ETC1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_ETC2);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_ASTC_4x4);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_BC1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_BC3);
            DM_TEXTURE_FORMAT_TO_STR_CASE(R_BC4);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RG_BC5);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_BC7);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB16F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB32F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA16F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA32F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(R16F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RG16F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(R32F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RG32F);
            default:break;
        }
        return "UNKNOWN_FORMAT";
    }
    #undef DM_TEXTURE_FORMAT_TO_STR_CASE


    Context::Context(const ContextParams& params, const VkInstance vk_instance)
    : m_MainRenderTarget(0)
    {
        memset(this, 0, sizeof(*this));
        m_Instance                = vk_instance;
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_VerifyGraphicsCalls     = params.m_VerifyGraphicsCalls;
        m_UseValidationLayers     = params.m_UseValidationLayers;
        m_RenderDocSupport        = params.m_RenderDocSupport;

        DM_STATIC_ASSERT(sizeof(m_TextureFormatSupport)*4 >= TEXTURE_FORMAT_COUNT, Invalid_Struct_Size );
    }

    static inline uint32_t GetNextRenderTargetId()
    {
        static uint32_t next_id = 1;

        // DM_RENDERTARGET_BACKBUFFER_ID is taken for the main framebuffer
        if (next_id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            next_id = DM_RENDERTARGET_BACKBUFFER_ID + 1;
        }

        return next_id++;
    }

    static VkResult CreateMainFrameSyncObjects(VkDevice vk_device, uint8_t frame_resource_count, FrameResource* frame_resources_out)
    {
        VkSemaphoreCreateInfo vk_create_semaphore_info;
        memset(&vk_create_semaphore_info, 0, sizeof(vk_create_semaphore_info));
        vk_create_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo vk_create_fence_info;
        memset(&vk_create_fence_info, 0, sizeof(vk_create_fence_info));
        vk_create_fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vk_create_fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for(uint8_t i=0; i < frame_resource_count; i++)
        {
            if (vkCreateSemaphore(vk_device, &vk_create_semaphore_info, 0, &frame_resources_out[i].m_ImageAvailable) != VK_SUCCESS ||
                vkCreateSemaphore(vk_device, &vk_create_semaphore_info, 0, &frame_resources_out[i].m_RenderFinished) != VK_SUCCESS ||
                vkCreateFence(vk_device, &vk_create_fence_info, 0, &frame_resources_out[i].m_SubmitFence) != VK_SUCCESS)
            {
                return VK_ERROR_INITIALIZATION_FAILED;
            }
        }

        return VK_SUCCESS;
    }

    static VkResult CreateMainScratchBuffers(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        uint8_t swap_chain_image_count, uint32_t scratch_buffer_size, uint16_t descriptor_count,
        DescriptorAllocator* descriptor_allocators_out, ScratchBuffer* scratch_buffers_out)
    {
        memset(scratch_buffers_out, 0, sizeof(ScratchBuffer) * swap_chain_image_count);
        memset(descriptor_allocators_out, 0, sizeof(DescriptorAllocator) * swap_chain_image_count);
        for(uint8_t i=0; i < swap_chain_image_count; i++)
        {
            VkResult res = CreateDescriptorAllocator(vk_device, descriptor_count, &descriptor_allocators_out[i]);
            if (res != VK_SUCCESS)
            {
                return res;
            }

            res = CreateScratchBuffer(vk_physical_device, vk_device, scratch_buffer_size, true, &descriptor_allocators_out[i], &scratch_buffers_out[i]);
            if (res != VK_SUCCESS)
            {
                return res;
            }
        }

        return VK_SUCCESS;
    }

    static VkSamplerAddressMode GetVulkanSamplerAddressMode(TextureWrap wrap)
    {
        const VkSamplerAddressMode address_mode_lut[] = {
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
        };
        return address_mode_lut[wrap];
    }

    static int16_t CreateVulkanTextureSampler(VkDevice vk_device, dmArray<TextureSampler>& texture_samplers,
        TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, uint8_t maxLod, float max_anisotropy)
    {
        VkFilter             vk_mag_filter;
        VkFilter             vk_min_filter;
        VkSamplerMipmapMode  vk_mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        // No conversions needed for wrap modes
        float max_lod                  = (float) maxLod;
        VkSamplerAddressMode vk_wrap_u = GetVulkanSamplerAddressMode(uwrap);
        VkSamplerAddressMode vk_wrap_v = GetVulkanSamplerAddressMode(vwrap);

        // Convert mag filter to Vulkan type
        if (magfilter == TEXTURE_FILTER_NEAREST)
        {
            vk_mag_filter = VK_FILTER_NEAREST;
        }
        else if (magfilter == TEXTURE_FILTER_LINEAR)
        {
            vk_mag_filter = VK_FILTER_LINEAR;
        }
        else
        {
            assert(0 && "Unsupported type for mag filter");
        }

        // Convert minfilter to Vulkan type, the conversions are
        // taken from https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkSamplerCreateInfo.html
        // and should match the OpenGL functionality.
        if (minfilter == TEXTURE_FILTER_NEAREST)
        {
            vk_min_filter = VK_FILTER_NEAREST;
            max_lod       = 0.25f;
        }
        else if (minfilter == TEXTURE_FILTER_LINEAR)
        {
            vk_min_filter = VK_FILTER_LINEAR;
            max_lod       = 0.25f;
        }
        else if (minfilter == TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST)
        {
            vk_min_filter  = VK_FILTER_NEAREST;
        }
        else if (minfilter == TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
        {
            vk_min_filter  = VK_FILTER_LINEAR;
        }
        else if (minfilter == TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR)
        {
            vk_min_filter  = VK_FILTER_NEAREST;
            vk_mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
        else if (minfilter == TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR)
        {
            vk_min_filter  = VK_FILTER_LINEAR;
            vk_mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
        else
        {
            assert(0 && "Unsupported type for min filter");
        }

        TextureSampler new_sampler  = {};
        new_sampler.m_MinFilter     = minfilter;
        new_sampler.m_MagFilter     = magfilter;
        new_sampler.m_AddressModeU  = uwrap;
        new_sampler.m_AddressModeV  = vwrap;
        new_sampler.m_MaxLod        = maxLod;
        new_sampler.m_MaxAnisotropy = max_anisotropy;

        uint32_t sampler_index = texture_samplers.Size();

        if (texture_samplers.Full())
        {
            texture_samplers.OffsetCapacity(1);
        }

        VkResult res = CreateTextureSampler(vk_device,
            vk_min_filter, vk_mag_filter, vk_mipmap_mode, vk_wrap_u, vk_wrap_v,
            0.0, max_lod, max_anisotropy, &new_sampler.m_Sampler);
        CHECK_VK_ERROR(res);

        texture_samplers.Push(new_sampler);
        return (int16_t) sampler_index;
    }

    static int16_t GetTextureSamplerIndex(dmArray<TextureSampler>& texture_samplers, TextureFilter minfilter, TextureFilter magfilter,
        TextureWrap uwrap, TextureWrap vwrap, uint8_t maxLod, float max_anisotropy)
    {
        for (uint32_t i=0; i < texture_samplers.Size(); i++)
        {
            TextureSampler& sampler = texture_samplers[i];
            if (sampler.m_MagFilter     == magfilter &&
                sampler.m_MinFilter     == minfilter &&
                sampler.m_AddressModeU  == uwrap     &&
                sampler.m_AddressModeV  == vwrap     &&
                sampler.m_MaxLod        == maxLod    &&
                sampler.m_MaxAnisotropy == max_anisotropy)
            {
                return (uint8_t) i;
            }
        }

        return -1;
    }

    static bool EndRenderPass(HContext context)
    {
        assert(context->m_CurrentRenderTarget);
        if (!context->m_CurrentRenderTarget->m_IsBound)
        {
            return false;
        }

        vkCmdEndRenderPass(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex]);
        context->m_CurrentRenderTarget->m_IsBound = 0;
        return true;
    }

    static void BeginRenderPass(HContext context, RenderTarget* rt)
    {
        assert(context->m_CurrentRenderTarget);
        if (context->m_CurrentRenderTarget->m_Id == rt->m_Id &&
            context->m_CurrentRenderTarget->m_IsBound)
        {
            return;
        }

        // If we bind a render pass without explicitly unbinding
        // the current render pass, we must first unbind it.
        if (context->m_CurrentRenderTarget->m_IsBound)
        {
            EndRenderPass(context);
        }

        VkClearValue vk_clear_values[MAX_BUFFER_COLOR_ATTACHMENTS + 1];
        memset(vk_clear_values, 0, sizeof(vk_clear_values));

        // Clear color
        for (int i = 0; i < rt->m_ColorAttachmentCount; ++i)
        {
            vk_clear_values[i].color.float32[3]     = 1.0f;
        }

        // Clear depth
        vk_clear_values[1].depthStencil.depth   = 1.0f;
        vk_clear_values[1].depthStencil.stencil = 0;

        VkRenderPassBeginInfo vk_render_pass_begin_info;
        vk_render_pass_begin_info.sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vk_render_pass_begin_info.renderPass          = rt->m_RenderPass;
        vk_render_pass_begin_info.framebuffer         = rt->m_Framebuffer;
        vk_render_pass_begin_info.pNext               = 0;
        vk_render_pass_begin_info.renderArea.offset.x = 0;
        vk_render_pass_begin_info.renderArea.offset.y = 0;
        vk_render_pass_begin_info.renderArea.extent   = rt->m_Extent;
        vk_render_pass_begin_info.clearValueCount = rt->m_ColorAttachmentCount + 1;
        vk_render_pass_begin_info.pClearValues    = vk_clear_values;

        vkCmdBeginRenderPass(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex], &vk_render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        context->m_CurrentRenderTarget = rt;
        context->m_CurrentRenderTarget->m_IsBound = 1;
    }

    static void GetDepthFormatAndTiling(VkPhysicalDevice vk_physical_device, const VkFormat* vk_format_list, uint8_t vk_format_list_size, VkFormat* vk_format_out, VkImageTiling* vk_tiling_out)
    {
        // Depth formats are optional, so we need to query
        // what available formats we have.
        const VkFormat vk_format_list_default[] = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        const VkFormat* vk_formats_to_test = vk_format_list ? vk_format_list : vk_format_list_default;
        uint8_t vk_formats_to_test_size    = vk_format_list ? vk_format_list_size : sizeof(vk_format_list_default) / sizeof(vk_format_list_default[0]);

        // Check if we can use optimal tiling for this format, otherwise
        // we try to find the best format that supports linear
        VkImageTiling vk_image_tiling = VK_IMAGE_TILING_OPTIMAL;
        VkFormat vk_depth_format      = GetSupportedTilingFormat(vk_physical_device, &vk_formats_to_test[0],
            vk_formats_to_test_size, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        if (vk_depth_format == VK_FORMAT_UNDEFINED)
        {
            vk_image_tiling = VK_IMAGE_TILING_LINEAR;
            vk_depth_format = GetSupportedTilingFormat(vk_physical_device, &vk_formats_to_test[0],
                vk_formats_to_test_size, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }

        *vk_format_out = vk_depth_format;
        *vk_tiling_out = vk_image_tiling;
    }

    static VkResult CreateDepthStencilTexture(HContext context, VkFormat vk_depth_format, VkImageTiling vk_depth_tiling,
        uint32_t width, uint32_t height, VkSampleCountFlagBits vk_sample_count, Texture* depth_stencil_texture_out)
    {
        const VkPhysicalDevice vk_physical_device = context->m_PhysicalDevice.m_Device;
        const VkDevice vk_device                  = context->m_LogicalDevice.m_Device;

        // The aspect flag indicates what the image should be used for,
        // it is usually color or stencil | depth.
        VkImageAspectFlags vk_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (vk_depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            vk_depth_format == VK_FORMAT_D24_UNORM_S8_UINT  ||
            vk_depth_format == VK_FORMAT_D16_UNORM_S8_UINT)
        {
            vk_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkResult res = CreateTexture2D(vk_physical_device, vk_device, width, height, 1,
            vk_sample_count, vk_depth_format, vk_depth_tiling, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_aspect, VK_IMAGE_LAYOUT_UNDEFINED, depth_stencil_texture_out);
        CHECK_VK_ERROR(res);

        if (res == VK_SUCCESS)
        {
            res = TransitionImageLayout(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_LogicalDevice.m_GraphicsQueue, depth_stencil_texture_out->m_Handle.m_Image, vk_aspect,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            CHECK_VK_ERROR(res);
        }

        return res;
    }

    VkResult CreateMainFrameBuffers(HContext context)
    {
        assert(context->m_SwapChain);

        // We need to create a framebuffer per swap chain image
        // so that they can be used in different states in the rendering pipeline
        context->m_MainFrameBuffers.SetCapacity(context->m_SwapChain->m_Images.Size());
        context->m_MainFrameBuffers.SetSize(context->m_SwapChain->m_Images.Size());

        SwapChain* swapChain = context->m_SwapChain;
        VkResult res;

        for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
        {
            // All swap chain images can share the same depth buffer,
            // that's why we create a single depth buffer at the start and reuse it.
            // Same thing goes for the resolve buffer if we are rendering with
            // multisampling enabled.
            VkImageView& vk_image_view_swap      = swapChain->m_ImageViews[i];
            VkImageView& vk_image_view_depth     = context->m_MainTextureDepthStencil.m_Handle.m_ImageView;
            VkImageView& vk_image_view_resolve   = swapChain->m_ResolveTexture->m_Handle.m_ImageView;
            VkImageView  vk_image_attachments[3];
            uint8_t num_attachments;

            if (swapChain->HasMultiSampling())
            {
                vk_image_attachments[0] = vk_image_view_resolve;
                vk_image_attachments[1] = vk_image_view_depth;
                vk_image_attachments[2] = vk_image_view_swap;
                num_attachments = 3;
            }
            else
            {
                vk_image_attachments[0] = vk_image_view_swap;
                vk_image_attachments[1] = vk_image_view_depth;
                num_attachments = 2;
            }

            res = CreateFramebuffer(context->m_LogicalDevice.m_Device, context->m_MainRenderPass,
                swapChain->m_ImageExtent.width, swapChain->m_ImageExtent.height,
                vk_image_attachments, num_attachments, &context->m_MainFrameBuffers[i]);
            CHECK_VK_ERROR(res);
        }

        return res;
    }

    VkResult DestroyMainFrameBuffers(HContext context)
    {
        for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
        {
            DestroyFrameBuffer(context->m_LogicalDevice.m_Device, context->m_MainFrameBuffers[i]);
        }
        context->m_MainFrameBuffers.SetCapacity(0);
        context->m_MainFrameBuffers.SetSize(0);
        return VK_SUCCESS;
    }

    static VkResult SetupMainRenderTarget(HContext context)
    {
        // Initialize the dummy rendertarget for the main framebuffer
        // The m_Framebuffer construct will be rotated sequentially
        // with the framebuffer objects created per swap chain.
        RenderTarget& rt          = context->m_MainRenderTarget;
        rt.m_RenderPass           = context->m_MainRenderPass;
        rt.m_Framebuffer          = context->m_MainFrameBuffers[0];
        rt.m_Extent               = context->m_SwapChain->m_ImageExtent;
        rt.m_ColorAttachmentCount = 1;

        return VK_SUCCESS;
    }

    static VkResult CreateMainRenderingResources(HContext context)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;

        // Create depth/stencil buffer
        VkFormat vk_depth_format;
        VkImageTiling vk_depth_tiling;
        GetDepthFormatAndTiling(context->m_PhysicalDevice.m_Device, 0, 0, &vk_depth_format, &vk_depth_tiling);
        memset(&context->m_MainTextureDepthStencil, 0, sizeof(context->m_MainTextureDepthStencil));
        VkResult res = CreateDepthStencilTexture(context,
            vk_depth_format, vk_depth_tiling,
            context->m_SwapChain->m_ImageExtent.width,
            context->m_SwapChain->m_ImageExtent.height,
            context->m_SwapChain->m_SampleCountFlag,
            &context->m_MainTextureDepthStencil);
        CHECK_VK_ERROR(res);

        // Create main render pass with two attachments
        RenderPassAttachment  attachments[3];
        RenderPassAttachment* attachment_resolve = 0;
        attachments[0].m_Format      = context->m_SwapChain->m_SurfaceFormat.format;
        attachments[0].m_ImageLayout = context->m_SwapChain->HasMultiSampling() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[1].m_Format      = context->m_MainTextureDepthStencil.m_Format;
        attachments[1].m_ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Set third attachment as framebuffer resolve attachment
        if (context->m_SwapChain->HasMultiSampling())
        {
            attachments[2].m_Format      = context->m_SwapChain->m_SurfaceFormat.format;
            attachments[2].m_ImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            attachment_resolve = &attachments[2];
        }

        res = CreateRenderPass(vk_device, context->m_SwapChain->m_SampleCountFlag, attachments, 1, &attachments[1], attachment_resolve, &context->m_MainRenderPass);
        CHECK_VK_ERROR(res);

        res = CreateMainFrameBuffers(context);
        CHECK_VK_ERROR(res);

        res = SetupMainRenderTarget(context);
        CHECK_VK_ERROR(res);
        context->m_CurrentRenderTarget = &context->m_MainRenderTarget;

        // Create main command buffers, one for each swap chain image
        const uint32_t num_swap_chain_images = context->m_SwapChain->m_Images.Size();
        context->m_MainCommandBuffers.SetCapacity(num_swap_chain_images);
        context->m_MainCommandBuffers.SetSize(num_swap_chain_images);

        res = CreateCommandBuffers(vk_device,
            context->m_LogicalDevice.m_CommandPool,
            context->m_MainCommandBuffers.Size(),
            context->m_MainCommandBuffers.Begin());
        CHECK_VK_ERROR(res);

        // Create an additional single-time buffer for device uploading
        CreateCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &context->m_MainCommandBufferUploadHelper);

        // Create main resources-to-destroy lists, one for each command buffer
        for (uint32_t i = 0; i < num_swap_chain_images; ++i)
        {
            context->m_MainResourcesToDestroy[i] = new ResourcesToDestroyList;
            context->m_MainResourcesToDestroy[i]->SetCapacity(8);
        }

        // Create main sync objects
        res = CreateMainFrameSyncObjects(vk_device, g_max_frames_in_flight, context->m_FrameResources);
        CHECK_VK_ERROR(res);


        // Create scratch buffer and descriptor allocators, one for each swap chain image
        //   Note: These constants are guessed and equals roughly 256 draw calls and 64kb
        //         of uniform memory per scratch buffer. The scratch buffer can dynamically
        //         grow, this is just a starting point.
        const uint16_t descriptor_count_per_pool = 512;
        const uint32_t buffer_size               = 256 * descriptor_count_per_pool;

        context->m_MainScratchBuffers.SetCapacity(num_swap_chain_images);
        context->m_MainScratchBuffers.SetSize(num_swap_chain_images);
        context->m_MainDescriptorAllocators.SetCapacity(num_swap_chain_images);
        context->m_MainDescriptorAllocators.SetSize(num_swap_chain_images);

        res = CreateMainScratchBuffers(context->m_PhysicalDevice.m_Device, vk_device,
            num_swap_chain_images, buffer_size, descriptor_count_per_pool,
            context->m_MainDescriptorAllocators.Begin(), context->m_MainScratchBuffers.Begin());
        CHECK_VK_ERROR(res);

        context->m_PipelineState = GetDefaultPipelineState();

        // Create default texture sampler
        CreateVulkanTextureSampler(vk_device, context->m_TextureSamplers, TEXTURE_FILTER_LINEAR, TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, 1, 1.0f);

        // Create default dummy texture
        TextureCreationParams default_texture_creation_params;
        default_texture_creation_params.m_Width          = 1;
        default_texture_creation_params.m_Height         = 1;
        default_texture_creation_params.m_OriginalWidth  = default_texture_creation_params.m_Width;
        default_texture_creation_params.m_OriginalHeight = default_texture_creation_params.m_Height;

        const uint8_t default_texture_data[] = { 255, 0, 255, 255 };

        TextureParams default_texture_params;
        default_texture_params.m_Width  = 1;
        default_texture_params.m_Height = 1;
        default_texture_params.m_Data   = default_texture_data;
        default_texture_params.m_Format = TEXTURE_FORMAT_RGBA;

        context->m_DefaultTexture = VulkanNewTexture(context, default_texture_creation_params);
        VulkanSetTexture(context->m_DefaultTexture, default_texture_params);

        for (int i = 0; i < DM_MAX_TEXTURE_UNITS; ++i)
        {
            context->m_TextureUnits[i] = context->m_DefaultTexture;
        }

        return res;
    }

    void SwapChainChanged(HContext context, uint32_t* width, uint32_t* height, VkResult (*cb)(void* ctx), void* cb_ctx)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;
        // Flush all current commands
        SynchronizeDevice(vk_device);

        DestroyMainFrameBuffers(context);

        // Destroy main Depth/Stencil buffer
        Texture* depth_stencil_texture = &context->m_MainTextureDepthStencil;
        DestroyDeviceBuffer(vk_device, &depth_stencil_texture->m_DeviceBuffer.m_Handle);
        DestroyTexture(vk_device, &depth_stencil_texture->m_Handle);

        DestroySwapChain(vk_device, context->m_SwapChain);

        // At this point, we've destroyed the framebuffers, depth/stencil textures, and the swapchain
        // and the platform may do extra setup
        if (cb)
        {
            VkResult res = cb(cb_ctx);
            CHECK_VK_ERROR(res);
        }

        // Update swap chain capabilities
        SwapChainCapabilities swap_chain_capabilities;
        GetSwapChainCapabilities(context->m_PhysicalDevice.m_Device, context->m_WindowSurface, swap_chain_capabilities);
        context->m_SwapChainCapabilities.Swap(swap_chain_capabilities);

        // Create the swap chain
        VkResult res = UpdateSwapChain(&context->m_PhysicalDevice, &context->m_LogicalDevice, width, height, true, context->m_SwapChainCapabilities, context->m_SwapChain);
        CHECK_VK_ERROR(res);

        // Create the main Depth/Stencil buffer
        VkFormat vk_depth_format;
        VkImageTiling vk_depth_tiling;
        GetDepthFormatAndTiling(context->m_PhysicalDevice.m_Device, 0, 0, &vk_depth_format, &vk_depth_tiling);
        res = CreateDepthStencilTexture(context,
            vk_depth_format, vk_depth_tiling,
            context->m_SwapChain->m_ImageExtent.width,
            context->m_SwapChain->m_ImageExtent.height,
            context->m_SwapChain->m_SampleCountFlag,
            &context->m_MainTextureDepthStencil);
        CHECK_VK_ERROR(res);

        context->m_WindowWidth  = context->m_SwapChain->m_ImageExtent.width;
        context->m_WindowHeight = context->m_SwapChain->m_ImageExtent.height;

        res = CreateMainFrameBuffers(context);
        CHECK_VK_ERROR(res);

        res = SetupMainRenderTarget(context);
        CHECK_VK_ERROR(res);

        // Flush once again to make sure all transitions are complete
        SynchronizeDevice(vk_device);
    }

    void FlushResourcesToDestroy(VkDevice vk_device, ResourcesToDestroyList* resource_list)
    {
        if (resource_list->Size() > 0)
        {
            for (uint32_t i = 0; i < resource_list->Size(); ++i)
            {
                ResourceToDestroy& resource = resource_list->Begin()[i];

                switch(resource.m_ResourceType)
                {
                    case RESOURCE_TYPE_DEVICE_BUFFER:
                        DestroyDeviceBuffer(vk_device, &resource.m_DeviceBuffer);
                        break;
                    case RESOURCE_TYPE_TEXTURE:
                        DestroyTexture(vk_device, &resource.m_Texture);
                        break;
                    case RESOURCE_TYPE_DESCRIPTOR_ALLOCATOR:
                        DestroyDescriptorAllocator(vk_device, &resource.m_DescriptorAllocator);
                        break;
                    case RESOURCE_TYPE_PROGRAM:
                        DestroyProgram(vk_device, &resource.m_Program);
                        break;
                    default:
                        assert(0);
                        break;
                }
            }

            resource_list->SetSize(0);
        }
    }

    static inline void SetViewportHelper(VkCommandBuffer vk_command_buffer, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        VkViewport vk_viewport;
        vk_viewport.x        = (float) x;
        vk_viewport.y        = (float) y;
        vk_viewport.width    = (float) width;
        vk_viewport.height   = (float) height;
        vk_viewport.minDepth = 0.0f;
        vk_viewport.maxDepth = 1.0f;
        vkCmdSetViewport(vk_command_buffer, 0, 1, &vk_viewport);
    }

    static bool IsDeviceExtensionSupported(PhysicalDevice* device, const char* ext_name)
    {
        for (uint32_t j=0; j < device->m_DeviceExtensionCount; ++j)
        {
            if (dmStrCaseCmp(device->m_DeviceExtensions[j].extensionName, ext_name) == 0)
            {
                return true;
            }
        }

        return false;
    }

    static bool VulkanIsExtensionSupported(HContext context, const char* ext_name)
    {
        return IsDeviceExtensionSupported(&context->m_PhysicalDevice, ext_name);
    }

    static uint32_t VulkanGetNumSupportedExtensions(HContext context)
    {
        return context->m_PhysicalDevice.m_DeviceExtensionCount;
    }

    static const char* VulkanGetSupportedExtension(HContext context, uint32_t index)
    {
        return context->m_PhysicalDevice.m_DeviceExtensions[index].extensionName;
    }

    static bool VulkanIsMultiTargetRenderingSupported(HContext context)
    {
        return true;
    }

    static void SetupSupportedTextureFormats(HContext context)
    {
    #if defined(__MACH__)
        // Check for optional extensions so that we can enable them if they exist
        if (VulkanIsExtensionSupported(context, VK_IMG_FORMAT_PVRTC_EXTENSION_NAME))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
        }
    #endif

        if (context->m_PhysicalDevice.m_Features.textureCompressionETC2)
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_ETC2;
        }

        if (context->m_PhysicalDevice.m_Features.textureCompressionBC)
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_BC1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_BC3;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_BC7;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_R_BC4;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RG_BC5;
        }

        if (context->m_PhysicalDevice.m_Features.textureCompressionASTC_LDR)
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_ASTC_4x4;
        }

        TextureFormat texture_formats[] = { TEXTURE_FORMAT_LUMINANCE,
                                            TEXTURE_FORMAT_LUMINANCE_ALPHA,
                                            TEXTURE_FORMAT_RGBA,

                                            TEXTURE_FORMAT_RGB16F,
                                            TEXTURE_FORMAT_RGB32F,
                                            TEXTURE_FORMAT_RGBA16F,
                                            TEXTURE_FORMAT_RGBA32F,
                                            TEXTURE_FORMAT_R16F,
                                            TEXTURE_FORMAT_RG16F,
                                            TEXTURE_FORMAT_R32F,
                                            TEXTURE_FORMAT_RG32F,

                                            // Apparently these aren't supported on macOS/Metal
                                            TEXTURE_FORMAT_RGB_16BPP,
                                            TEXTURE_FORMAT_RGBA_16BPP,
                                        };

        // RGB isn't supported in Vulkan as a texture format, but we still need to supply it to the engine
        // Later in the vulkan pipeline when the texture is created, we will convert it internally to RGBA
        context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;

        // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkImageCreateInfo.html
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(texture_formats); ++i)
        {
            TextureFormat texture_format = texture_formats[i];
            VkFormatProperties vk_format_properties;
            VkFormat vk_format = GetVulkanFormatFromTextureFormat(texture_format);
            GetFormatProperties(context->m_PhysicalDevice.m_Device, vk_format, &vk_format_properties);
            if (vk_format_properties.linearTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT ||
                vk_format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
            {
                context->m_TextureFormatSupport |= 1 << texture_format;
            }
        }
    }

    bool InitializeVulkan(HContext context, const WindowParams* params)
    {
        VkResult res = CreateWindowSurface(context->m_Instance, &context->m_WindowSurface, params->m_HighDPI);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create window surface for Vulkan, reason: %s.", VkResultToStr(res));
            return false;
        }

        uint32_t device_count = GetPhysicalDeviceCount(context->m_Instance);

        if (device_count == 0)
        {
            dmLogError("Could not get any Vulkan devices.");
            return false;
        }

        PhysicalDevice* device_list     = new PhysicalDevice[device_count];
        PhysicalDevice* selected_device = NULL;
        GetPhysicalDevices(context->m_Instance, &device_list, device_count);

        // Required device extensions. These must be present for anything to work.
        dmArray<const char*> device_extensions;
        device_extensions.SetCapacity(2);
        device_extensions.Push(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        // From spec:
        // "Allow negative height to be specified in the VkViewport::height field to
        // perform y-inversion of the clip-space to framebuffer-space transform.
        // This allows apps to avoid having to use gl_Position.y = -gl_Position.y
        // in shaders also targeting other APIs."
        device_extensions.Push(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

        QueueFamily selected_queue_family;
        SwapChainCapabilities selected_swap_chain_capabilities;
        for (uint32_t i = 0; i < device_count; ++i)
        {
            #define DESTROY_AND_CONTINUE(d) \
                DestroyPhysicalDevice(d); \
                continue;

            PhysicalDevice* device = &device_list[i];

            // Make sure we have a graphics and present queue available
            QueueFamily queue_family = GetQueueFamily(device, context->m_WindowSurface);
            if (!queue_family.IsValid())
            {
                dmLogError("Device selection failed for device %s: Could not get a valid queue family.", device->m_Properties.deviceName);
                DESTROY_AND_CONTINUE(device)
            }

            // Make sure all device extensions are supported
            bool all_extensions_found = true;
            for (uint32_t ext_i = 0; ext_i < device_extensions.Size(); ++ext_i)
            {
                if (!IsDeviceExtensionSupported(device, device_extensions[ext_i]))
                {
                    all_extensions_found = false;
                    break;
                }
            }

            if (!all_extensions_found)
            {
                dmLogError("Device selection failed for device %s: Could not find all required device extensions.", device->m_Properties.deviceName);
                DESTROY_AND_CONTINUE(device)
            }

            // Make sure device has swap chain support
            GetSwapChainCapabilities(device->m_Device, context->m_WindowSurface, selected_swap_chain_capabilities);

            if (selected_swap_chain_capabilities.m_SurfaceFormats.Size() == 0 ||
                selected_swap_chain_capabilities.m_PresentModes.Size() == 0)
            {
                dmLogError("Device selection failed for device %s: Could not find a valid swap chain.", device->m_Properties.deviceName);
                DESTROY_AND_CONTINUE(device)
            }

            dmLogInfo("Vulkan device selected: %s", device->m_Properties.deviceName);

            selected_device = device;
            selected_queue_family = queue_family;
            break;

            #undef DESTROY_AND_CONTINUE
        }

        LogicalDevice logical_device;
        uint32_t created_width  = params->m_Width;
        uint32_t created_height = params->m_Height;
        const bool want_vsync   = true;
        VkSampleCountFlagBits vk_closest_multisample_flag;

        uint16_t validation_layers_count;
        const char** validation_layers = GetValidationLayers(&validation_layers_count, context->m_UseValidationLayers, context->m_RenderDocSupport);
        Texture* resolveTexture = new Texture;

        if (selected_device == NULL)
        {
            dmLogError("Could not select a suitable Vulkan device.");
            goto bail;
        }

        context->m_PhysicalDevice = *selected_device;

        SetupSupportedTextureFormats(context);

    #if defined(__MACH__)
        // Check for optional extensions so that we can enable them if they exist
        if (VulkanIsExtensionSupported(context, VK_IMG_FORMAT_PVRTC_EXTENSION_NAME))
        {
            device_extensions.OffsetCapacity(1);
            device_extensions.Push(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME);
        }
    #endif

        res = CreateLogicalDevice(selected_device, context->m_WindowSurface, selected_queue_family,
            device_extensions.Begin(), (uint8_t)device_extensions.Size(),
            validation_layers, (uint8_t)validation_layers_count, &logical_device);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a logical Vulkan device, reason: %s", VkResultToStr(res));
            goto bail;
        }

        context->m_LogicalDevice    = logical_device;
        vk_closest_multisample_flag = GetClosestSampleCountFlag(selected_device, BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT, params->m_Samples);

        // Create swap chain
        InitializeVulkanTexture(resolveTexture);
        context->m_SwapChainCapabilities.Swap(selected_swap_chain_capabilities);
        context->m_SwapChain = new SwapChain(context->m_WindowSurface, vk_closest_multisample_flag, context->m_SwapChainCapabilities, selected_queue_family, resolveTexture);

        res = UpdateSwapChain(&context->m_PhysicalDevice, &context->m_LogicalDevice, &created_width, &created_height, want_vsync, context->m_SwapChainCapabilities, context->m_SwapChain);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a swap chain for Vulkan, reason: %s", VkResultToStr(res));
            goto bail;
        }

        delete[] device_list;

        context->m_PipelineCache.SetCapacity(32,64);
        context->m_TextureSamplers.SetCapacity(4);

        // Create framebuffers, default renderpass etc.
        res = CreateMainRenderingResources(context);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create main rendering resources for Vulkan, reason: %s", VkResultToStr(res));
            goto bail;
        }

        return true;
bail:
        if (context->m_SwapChain)
            delete context->m_SwapChain;
        if (device_list)
            delete[] device_list;
        return false;
    }

    static bool VulkanIsSupported()
    {
    #if ANDROID
        if (!LoadVulkanLibrary())
        {
            dmLogError("Could not load Vulkan functions.");
            return 0x0;
        }
    #endif

        VkInstance inst;
        VkResult res = CreateInstance(&inst, 0, 0, 0, 0, 0, 0);

        if (res == VK_SUCCESS)
        {
        #if ANDROID
            LoadVulkanFunctions(inst);
        #endif
            DestroyInstance(&inst);
        }

        return res == VK_SUCCESS;
    }

    static HContext VulkanNewContext(const ContextParams& params)
    {
        if (g_VulkanContext == 0x0)
        {
            if (!NativeInit(params))
            {
                return 0x0;
            }

            uint16_t extension_names_count;
            const char** extension_names = GetExtensionNames(&extension_names_count);
            uint16_t validation_layers_count;
            const char** validation_layers = GetValidationLayers(&validation_layers_count, params.m_UseValidationLayers, params.m_RenderDocSupport);
            uint16_t validation_layers_ext_count;
            const char** validation_layers_ext = GetValidationLayersExt(&validation_layers_ext_count);

            VkInstance vk_instance;
            if (CreateInstance(&vk_instance,
                                extension_names, extension_names_count,
                                validation_layers, validation_layers_count,
                                validation_layers_ext, validation_layers_ext_count) != VK_SUCCESS)
            {
                dmLogError("Could not create Vulkan instance");
                return 0x0;
            }

        #if ANDROID
            LoadVulkanFunctions(vk_instance);
        #endif

            g_VulkanContext = new Context(params, vk_instance);

            return g_VulkanContext;
        }
        return 0x0;
    }

    static void VulkanDeleteContext(HContext context)
    {
        if (context != 0x0)
        {
            if (context->m_Instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(context->m_Instance, 0);
                context->m_Instance = VK_NULL_HANDLE;
            }

            delete context;
            g_VulkanContext = 0x0;
        }
    }

    static bool VulkanInitialize()
    {
        return true;
    }

    static void VulkanFinalize()
    {
        NativeExit();
    }

    static void VulkanGetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    static void VulkanBeginFrame(HContext context)
    {
        NativeBeginFrame(context);

        FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameInFlight];

        VkDevice vk_device = context->m_LogicalDevice.m_Device;

        vkWaitForFences(vk_device, 1, &current_frame_resource.m_SubmitFence, VK_TRUE, UINT64_MAX);
        vkResetFences(vk_device, 1, &current_frame_resource.m_SubmitFence);

        VkResult res      = context->m_SwapChain->Advance(vk_device, current_frame_resource.m_ImageAvailable);
        uint32_t frame_ix = context->m_SwapChain->m_ImageIndex;

        if (res != VK_SUCCESS)
        {
            if (res == VK_ERROR_OUT_OF_DATE_KHR)
            {
                uint32_t width, height;
                VulkanGetNativeWindowSize(&width, &height);
                context->m_WindowWidth  = width;
                context->m_WindowHeight = height;
                SwapChainChanged(context, &context->m_WindowWidth, &context->m_WindowHeight, 0, 0);
                res = context->m_SwapChain->Advance(vk_device, current_frame_resource.m_ImageAvailable);
                CHECK_VK_ERROR(res);
            }
            else if (res == VK_SUBOPTIMAL_KHR)
            {
                // Presenting the swap chain will still work but not optimally, but we should still notify.
                dmLogOnceWarning("Vulkan swapchain is out of date, reason: VK_SUBOPTIMAL_KHR.");
            }
            else
            {
                dmLogOnceError("Vulkan swapchain is out of date, reason: %s.", VkResultToStr(res));
                return;
            }
        }

        if (context->m_MainResourcesToDestroy[frame_ix]->Size() > 0)
        {
            FlushResourcesToDestroy(vk_device, context->m_MainResourcesToDestroy[frame_ix]);
        }

        // Reset the scratch buffer for this swapchain image, so we can reuse its descriptors
        // for the uniform resource bindings.
        ScratchBuffer* scratchBuffer = &context->m_MainScratchBuffers[frame_ix];
        ResetScratchBuffer(context->m_LogicalDevice.m_Device, scratchBuffer);

        // TODO: Investigate if we don't have to map the memory every frame
        res = scratchBuffer->m_DeviceBuffer.MapMemory(vk_device);
        CHECK_VK_ERROR(res);

        VkCommandBufferBeginInfo vk_command_buffer_begin_info;

        vk_command_buffer_begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vk_command_buffer_begin_info.flags            = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        vk_command_buffer_begin_info.pInheritanceInfo = 0;
        vk_command_buffer_begin_info.pNext            = 0;

        vkBeginCommandBuffer(context->m_MainCommandBuffers[frame_ix], &vk_command_buffer_begin_info);
        context->m_FrameBegun                     = 1;
        context->m_MainRenderTarget.m_Framebuffer = context->m_MainFrameBuffers[frame_ix];

        BeginRenderPass(context, context->m_CurrentRenderTarget);
    }

    static void VulkanFlip(HContext context)
    {
        DM_PROFILE(__FUNCTION__);
        uint32_t frame_ix = context->m_SwapChain->m_ImageIndex;
        FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameInFlight];

        if (!EndRenderPass(context))
        {
            assert(0);
            return;
        }

        context->m_MainScratchBuffers[frame_ix].m_DeviceBuffer.UnmapMemory(context->m_LogicalDevice.m_Device);

        VkResult res = vkEndCommandBuffer(context->m_MainCommandBuffers[frame_ix]);
        CHECK_VK_ERROR(res);

        VkPipelineStageFlags vk_pipeline_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo vk_submit_info;
        vk_submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        vk_submit_info.pNext                = 0;
        vk_submit_info.waitSemaphoreCount   = 1;
        vk_submit_info.pWaitSemaphores      = &current_frame_resource.m_ImageAvailable;
        vk_submit_info.pWaitDstStageMask    = &vk_pipeline_stage_flags;
        vk_submit_info.commandBufferCount   = 1;
        vk_submit_info.pCommandBuffers      = &context->m_MainCommandBuffers[frame_ix];
        vk_submit_info.signalSemaphoreCount = 1;
        vk_submit_info.pSignalSemaphores    = &current_frame_resource.m_RenderFinished;

        res = vkQueueSubmit(context->m_LogicalDevice.m_GraphicsQueue, 1, &vk_submit_info, current_frame_resource.m_SubmitFence);
        CHECK_VK_ERROR(res);

        VkPresentInfoKHR vk_present_info;
        vk_present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        vk_present_info.pNext              = 0;
        vk_present_info.waitSemaphoreCount = 1;
        vk_present_info.pWaitSemaphores    = &current_frame_resource.m_RenderFinished;
        vk_present_info.swapchainCount     = 1;
        vk_present_info.pSwapchains        = &context->m_SwapChain->m_SwapChain;
        vk_present_info.pImageIndices      = &frame_ix;
        vk_present_info.pResults           = 0;

        res = vkQueuePresentKHR(context->m_LogicalDevice.m_PresentQueue, &vk_present_info);
        CHECK_VK_ERROR(res);

        // Advance frame index
        context->m_CurrentFrameInFlight = (context->m_CurrentFrameInFlight + 1) % g_max_frames_in_flight;
        context->m_FrameBegun           = 0;

        NativeSwapBuffers(context);
    }

    static void VulkanSetSwapInterval(HContext context, uint32_t swap_interval)
    {}

    static void VulkanClear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        DM_PROFILE(__FUNCTION__);
        assert(context->m_CurrentRenderTarget);

        uint32_t attachment_count = 0;
        VkClearAttachment vk_clear_attachments[MAX_BUFFER_COLOR_ATTACHMENTS + 1];
        memset(vk_clear_attachments, 0, sizeof(vk_clear_attachments));

        VkClearRect vk_clear_rect;
        vk_clear_rect.rect.offset.x      = 0;
        vk_clear_rect.rect.offset.y      = 0;
        vk_clear_rect.rect.extent.width  = context->m_CurrentRenderTarget->m_Extent.width;
        vk_clear_rect.rect.extent.height = context->m_CurrentRenderTarget->m_Extent.height;
        vk_clear_rect.baseArrayLayer     = 0;
        vk_clear_rect.layerCount         = 1;

        bool has_depth_stencil_texture = context->m_CurrentRenderTarget->m_Id == DM_RENDERTARGET_BACKBUFFER_ID || context->m_CurrentRenderTarget->m_TextureDepthStencil;

        float r = ((float)red)/255.0f;
        float g = ((float)green)/255.0f;
        float b = ((float)blue)/255.0f;
        float a = ((float)alpha)/255.0f;

        for (int i = 0; i < context->m_CurrentRenderTarget->m_ColorAttachmentCount; ++i)
        {
            VkClearAttachment& vk_color_attachment          = vk_clear_attachments[attachment_count++];
            vk_color_attachment.aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
            vk_color_attachment.colorAttachment             = i;
            vk_color_attachment.clearValue.color.float32[0] = r;
            vk_color_attachment.clearValue.color.float32[1] = g;
            vk_color_attachment.clearValue.color.float32[2] = b;
            vk_color_attachment.clearValue.color.float32[3] = a;
        }

        // Clear depth / stencil
        if (has_depth_stencil_texture && (flags & (BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT)))
        {
            VkImageAspectFlags vk_aspect = 0;
            if (flags & BUFFER_TYPE_DEPTH_BIT)
            {
                vk_aspect |= BUFFER_TYPE_DEPTH_BIT;
            }

            if (flags & BUFFER_TYPE_STENCIL_BIT)
            {
                vk_aspect |= BUFFER_TYPE_STENCIL_BIT;
            }

            VkClearAttachment& vk_depth_attachment              = vk_clear_attachments[attachment_count++];
            vk_depth_attachment.aspectMask                      = vk_aspect;
            vk_depth_attachment.clearValue.depthStencil.stencil = stencil;
            vk_depth_attachment.clearValue.depthStencil.depth   = depth;
        }

        vkCmdClearAttachments(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
            attachment_count, vk_clear_attachments, 1, &vk_clear_rect);

    }

    static void DeviceBufferUploadHelper(HContext context, const void* data, uint32_t size, uint32_t offset, DeviceBuffer* bufferOut)
    {
        VkResult res;

        if (bufferOut->m_Destroyed || bufferOut->m_Handle.m_Buffer == VK_NULL_HANDLE)
        {
            res = CreateDeviceBuffer(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device,
                size, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, bufferOut);
            CHECK_VK_ERROR(res);
        }

        VkCommandBuffer vk_command_buffer;
        if (context->m_FrameBegun)
        {
            vk_command_buffer = context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex];
        }
        else
        {
            VkCommandBufferBeginInfo vk_command_buffer_begin_info;
            memset(&vk_command_buffer_begin_info, 0, sizeof(VkCommandBufferBeginInfo));

            vk_command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vk_command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vk_command_buffer                  = context->m_MainCommandBufferUploadHelper;
            res = vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info);
            CHECK_VK_ERROR(res);
        }

        res = WriteToDeviceBuffer(context->m_LogicalDevice.m_Device, size, offset, data, bufferOut);
        CHECK_VK_ERROR(res);

        if (!context->m_FrameBegun)
        {
            vkEndCommandBuffer(vk_command_buffer);
            vkResetCommandBuffer(vk_command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        }
    }

    template <typename T>
    static void DestroyResourceDeferred(ResourcesToDestroyList* resource_list, T* resource)
    {
        if (resource->m_Destroyed)
        {
            return;
        }

        ResourceToDestroy resource_to_destroy;
        resource_to_destroy.m_ResourceType = resource->GetType();

        switch(resource_to_destroy.m_ResourceType)
        {
            case RESOURCE_TYPE_TEXTURE:
                resource_to_destroy.m_Texture = ((Texture*) resource)->m_Handle;
                DestroyResourceDeferred(resource_list, &((Texture*) resource)->m_DeviceBuffer);
                break;
            case RESOURCE_TYPE_DESCRIPTOR_ALLOCATOR:
                resource_to_destroy.m_DescriptorAllocator = ((DescriptorAllocator*) resource)->m_Handle;
                break;
            case RESOURCE_TYPE_DEVICE_BUFFER:
                resource_to_destroy.m_DeviceBuffer = ((DeviceBuffer*) resource)->m_Handle;
                break;
            case RESOURCE_TYPE_PROGRAM:
                resource_to_destroy.m_Program = ((Program*) resource)->m_Handle;
                break;
            default:
                assert(0);
                break;
        }

        if (resource_list->Full())
        {
            resource_list->OffsetCapacity(2);
        }

        resource_list->Push(resource_to_destroy);
        resource->m_Destroyed = 1;
    }

    static Pipeline* GetOrCreatePipeline(VkDevice vk_device, VkSampleCountFlagBits vk_sample_count,
        const PipelineState pipelineState, PipelineCache& pipelineCache,
        Program* program, RenderTarget* rt, DeviceBuffer* vertexBuffer, HVertexDeclaration vertexDeclaration)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &program->m_Hash, sizeof(program->m_Hash));
        dmHashUpdateBuffer64(&pipeline_hash_state, &pipelineState, sizeof(pipelineState));
        dmHashUpdateBuffer64(&pipeline_hash_state, &vertexDeclaration->m_Hash, sizeof(vertexDeclaration->m_Hash));
        dmHashUpdateBuffer64(&pipeline_hash_state, &rt->m_Id, sizeof(rt->m_Id));
        dmHashUpdateBuffer64(&pipeline_hash_state, &vk_sample_count, sizeof(vk_sample_count));
        uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);

        Pipeline* cached_pipeline = pipelineCache.Get(pipeline_hash);

        if (!cached_pipeline)
        {
            Pipeline new_pipeline;
            memset(&new_pipeline, 0, sizeof(new_pipeline));

            VkRect2D vk_scissor;
            vk_scissor.extent   = rt->m_Extent;
            vk_scissor.offset.x = 0;
            vk_scissor.offset.y = 0;

            VkResult res = CreatePipeline(vk_device, vk_scissor, vk_sample_count, pipelineState, program, vertexBuffer, vertexDeclaration, rt, &new_pipeline);
            CHECK_VK_ERROR(res);

            if (pipelineCache.Full())
            {
                pipelineCache.SetCapacity(32, pipelineCache.Capacity() + 4);
            }

            pipelineCache.Put(pipeline_hash, new_pipeline);
            cached_pipeline = pipelineCache.Get(pipeline_hash);

            dmLogDebug("Created new VK Pipeline with hash %llu", (unsigned long long) pipeline_hash);
        }

        return cached_pipeline;
    }

    static HVertexBuffer VulkanNewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DeviceBuffer* buffer = new DeviceBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        if (size > 0)
        {
            DeviceBufferUploadHelper(context, data, size, 0, buffer);
        }

        return (HVertexBuffer) buffer;
    }

    static void VulkanDeleteVertexBuffer(HVertexBuffer buffer)
    {
        if (!buffer)
            return;
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;

        if (!buffer_ptr->m_Destroyed)
        {
            DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], buffer_ptr);
        }
        delete buffer_ptr;
    }

    static void VulkanSetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);

        if (size == 0)
        {
            return;
        }

        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;

        if (!buffer_ptr->m_Destroyed)
        {
            DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], buffer_ptr);
        }

        DeviceBufferUploadHelper(g_VulkanContext, data, size, 0, buffer_ptr);
    }

    static void VulkanSetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(__FUNCTION__);
        assert(size > 0);
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        assert(offset + size <= buffer_ptr->m_MemorySize);
        DeviceBufferUploadHelper(g_VulkanContext, data, size, offset, buffer_ptr);
    }

    static uint32_t VulkanGetMaxElementsVertices(HContext context)
    {
        return context->m_PhysicalDevice.m_Properties.limits.maxDrawIndexedIndexValue;
    }

    static HIndexBuffer VulkanNewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        assert(size > 0);
        DeviceBuffer* buffer = new DeviceBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        DeviceBufferUploadHelper(g_VulkanContext, data, size, 0, (DeviceBuffer*) buffer);
        return (HIndexBuffer) buffer;
    }

    static void VulkanDeleteIndexBuffer(HIndexBuffer buffer)
    {
        if (!buffer)
            return;
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        if (!buffer_ptr->m_Destroyed)
        {
            DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], buffer_ptr);
        }
        delete buffer_ptr;
    }

    static void VulkanSetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);

        if (size == 0)
        {
            return;
        }

        assert(buffer);

        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;

        if (!buffer_ptr->m_Destroyed && size != buffer_ptr->m_MemorySize)
        {
            DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], buffer_ptr);
        }

        DeviceBufferUploadHelper(g_VulkanContext, data, size, 0, buffer_ptr);
    }

    static void VulkanSetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(__FUNCTION__);
        assert(buffer);
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        assert(offset + size < buffer_ptr->m_MemorySize);
        DeviceBufferUploadHelper(g_VulkanContext, data, size, 0, buffer_ptr);
    }

    static bool VulkanIsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        // From VkPhysicalDeviceFeatures spec:
        //   "fullDrawIndexUint32 - If this feature is supported, maxDrawIndexedIndexValue must be 2^32-1;
        //   otherwise it must be no smaller than 2^24-1."
        return true;
    }

    static inline uint32_t GetShaderTypeSize(ShaderDesc::ShaderDataType type)
    {
        const uint8_t conversion_table[] = {
            0,  // SHADER_TYPE_UNKNOWN
            4,  // SHADER_TYPE_INT
            4,  // SHADER_TYPE_UINT
            4,  // SHADER_TYPE_FLOAT
            8,  // SHADER_TYPE_VEC2
            12, // SHADER_TYPE_VEC3
            16, // SHADER_TYPE_VEC4
            16, // SHADER_TYPE_MAT2
            36, // SHADER_TYPE_MAT3
            64, // SHADER_TYPE_MAT4
            4,  // SHADER_TYPE_SAMPLER2D
            4,  // SHADER_TYPE_SAMPLER3D
            4,  // SHADER_TYPE_SAMPLER_CUBE
        };

        return conversion_table[type];
    }

    static inline uint32_t GetGraphicsTypeSize(Type type)
    {
        if (type == TYPE_BYTE || type == TYPE_UNSIGNED_BYTE)
        {
            return 1;
        }
        else if (type == TYPE_SHORT || type == TYPE_UNSIGNED_SHORT)
        {
            return 2;
        }
        else if (type == TYPE_INT || type == TYPE_UNSIGNED_INT || type == TYPE_FLOAT)
        {
            return 4;
        }
        else if (type == TYPE_FLOAT_VEC4)
        {
            return 16;
        }
        else if (type == TYPE_FLOAT_MAT4)
        {
            return 64;
        }
        assert(0 && "Unsupported data type");
        return 0;
    }

    static inline VkFormat GetVulkanFormatFromTypeAndSize(Type type, uint16_t size)
    {
        if (type == TYPE_FLOAT)
        {
            if (size == 1)     return VK_FORMAT_R32_SFLOAT;
            else if(size == 2) return VK_FORMAT_R32G32_SFLOAT;
            else if(size == 3) return VK_FORMAT_R32G32B32_SFLOAT;
            else if(size == 4) return VK_FORMAT_R32G32B32A32_SFLOAT;
        }
        else if (type == TYPE_UNSIGNED_BYTE)
        {
            if (size == 1)     return VK_FORMAT_R8_UINT;
            else if(size == 2) return VK_FORMAT_R8G8_UINT;
            else if(size == 3) return VK_FORMAT_R8G8B8_UINT;
            else if(size == 4) return VK_FORMAT_R8G8B8A8_UINT;
        }
        else if (type == TYPE_UNSIGNED_SHORT)
        {
            if (size == 1)     return VK_FORMAT_R16_UINT;
            else if(size == 2) return VK_FORMAT_R16G16_UINT;
            else if(size == 3) return VK_FORMAT_R16G16B16_UINT;
            else if(size == 4) return VK_FORMAT_R16G16B16A16_UINT;
        }
        else if (type == TYPE_FLOAT_MAT4)
        {
            return VK_FORMAT_R32_SFLOAT;
        }
        else if (type == TYPE_FLOAT_VEC4)
        {
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        }

        assert(0 && "Unable to deduce type from dmGraphics::Type");
        return VK_FORMAT_UNDEFINED;
    }

    static VertexDeclaration* CreateAndFillVertexDeclaration(HashState64* hash, VertexElement* element, uint32_t count)
    {
        assert(element);
        assert(count <= DM_MAX_VERTEX_STREAM_COUNT);

        VertexDeclaration* vd = new VertexDeclaration();
        memset(vd, 0, sizeof(VertexDeclaration));

        vd->m_StreamCount = count;

        for (uint32_t i = 0; i < count; ++i)
        {
            VertexElement& el           = element[i];
            vd->m_Streams[i].m_NameHash = dmHashString64(el.m_Name);
            vd->m_Streams[i].m_Format   = GetVulkanFormatFromTypeAndSize(el.m_Type, el.m_Size);
            vd->m_Streams[i].m_Offset   = vd->m_Stride;
            vd->m_Streams[i].m_Location = 0;
            vd->m_Stride               += el.m_Size * GetGraphicsTypeSize(el.m_Type);

            dmHashUpdateBuffer64(hash, &el.m_Size, sizeof(el.m_Size));
            dmHashUpdateBuffer64(hash, &el.m_Type, sizeof(el.m_Type));
            dmHashUpdateBuffer64(hash, &vd->m_Streams[i].m_Format, sizeof(vd->m_Streams[i].m_Format));
        }

        return vd;
    }

    static HVertexDeclaration VulkanNewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, element, count);
        dmHashUpdateBuffer64(&decl_hash_state, &vd->m_Stride, sizeof(vd->m_Stride));
        vd->m_Hash = dmHashFinal64(&decl_hash_state);
        return vd;
    }

    static HVertexDeclaration VulkanNewVertexDeclarationStride(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, element, count);
        dmHashUpdateBuffer64(&decl_hash_state, &stride, sizeof(stride));
        vd->m_Stride          = stride;
        vd->m_Hash            = dmHashFinal64(&decl_hash_state);
        return vd;
    }

    bool VulkanSetStreamOffset(HVertexDeclaration vertex_declaration, uint32_t stream_index, uint16_t offset)
    {
        if (stream_index >= vertex_declaration->m_StreamCount) {
            return false;
        }
        vertex_declaration->m_Streams[stream_index].m_Offset = offset;
        return true;
    }

    static void VulkanDeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete (VertexDeclaration*) vertex_declaration;
    }

    static void VulkanEnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        context->m_CurrentVertexBuffer      = (DeviceBuffer*) vertex_buffer;
        context->m_CurrentVertexDeclaration = (VertexDeclaration*) vertex_declaration;
    }

    static void VulkanEnableVertexDeclarationProgram(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        Program* program_ptr = (Program*) program;
        VulkanEnableVertexDeclaration(context, vertex_declaration, vertex_buffer);

        for (uint32_t i=0; i < vertex_declaration->m_StreamCount; i++)
        {
            VertexDeclaration::Stream& stream = vertex_declaration->m_Streams[i];

            stream.m_Location = 0xffff;

            ShaderModule* vertex_shader = program_ptr->m_VertexModule;

            for (uint32_t j=0; j < vertex_shader->m_AttributeCount; j++)
            {
                if (vertex_shader->m_Attributes[j].m_NameHash == stream.m_NameHash)
                {
                    stream.m_Location = vertex_shader->m_Attributes[j].m_Binding;
                    break;
                }
            }
        }
    }

    static void VulkanDisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        context->m_CurrentVertexDeclaration = 0;
    }

    static inline bool IsUniformTextureSampler(ShaderResourceBinding uniform)
    {
        return uniform.m_Type == ShaderDesc::SHADER_TYPE_SAMPLER2D ||
               uniform.m_Type == ShaderDesc::SHADER_TYPE_SAMPLER3D ||
               uniform.m_Type == ShaderDesc::SHADER_TYPE_SAMPLER_CUBE;
    }

    static void UpdateDescriptorSets(
        VkDevice            vk_device,
        VkDescriptorSet     vk_descriptor_set,
        Program*            program,
        Program::ModuleType module_type,
        ScratchBuffer*      scratch_buffer,
        uint32_t            dynamic_alignment,
        uint32_t*           dynamic_offsets_out)
    {
        ShaderModule* shader_module;
        uint32_t*     uniform_data_offsets;
        uint32_t*     dynamic_offsets = dynamic_offsets_out;

        if (module_type == Program::MODULE_TYPE_VERTEX)
        {
            shader_module         = program->m_VertexModule;
            uniform_data_offsets  = program->m_UniformDataOffsets;
        }
        else if (module_type == Program::MODULE_TYPE_FRAGMENT)
        {
            shader_module        = program->m_FragmentModule;
            uniform_data_offsets = &program->m_UniformDataOffsets[program->m_VertexModule->m_UniformCount];
            dynamic_offsets      = &dynamic_offsets_out[program->m_VertexModule->m_UniformCount];
        }
        else
        {
            assert(0);
        }

        if (shader_module->m_UniformCount == 0)
        {
            return;
        }

        const uint8_t max_write_descriptors = 16;
        uint16_t uniforms_to_write          = shader_module->m_UniformCount;
        uint16_t uniform_to_write_index     = 0;
        uint16_t uniform_index              = 0;
        uint16_t image_to_write_index       = 0;
        uint16_t buffer_to_write_index      = 0;
        VkWriteDescriptorSet vk_write_descriptors[max_write_descriptors];
        VkDescriptorImageInfo vk_write_image_descriptors[max_write_descriptors];
        VkDescriptorBufferInfo vk_write_buffer_descriptors[max_write_descriptors];

        while(uniforms_to_write > 0)
        {
            ShaderResourceBinding& res = shader_module->m_Uniforms[uniform_index++];
            VkWriteDescriptorSet& vk_write_desc_info = vk_write_descriptors[uniform_to_write_index++];
            vk_write_desc_info.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vk_write_desc_info.pNext            = 0;
            vk_write_desc_info.dstSet           = vk_descriptor_set;
            vk_write_desc_info.dstBinding       = res.m_Binding;
            vk_write_desc_info.dstArrayElement  = 0;
            vk_write_desc_info.descriptorCount  = 1;
            vk_write_desc_info.pImageInfo       = 0;
            vk_write_desc_info.pBufferInfo      = 0;
            vk_write_desc_info.pTexelBufferView = 0;

            if (IsUniformTextureSampler(res))
            {
                Texture* texture = g_VulkanContext->m_TextureUnits[res.m_TextureUnit];
                VkDescriptorImageInfo& vk_image_info = vk_write_image_descriptors[image_to_write_index++];
                vk_image_info.imageLayout         = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                vk_image_info.imageView           = texture->m_Handle.m_ImageView;
                vk_image_info.sampler             = g_VulkanContext->m_TextureSamplers[texture->m_TextureSamplerIndex].m_Sampler;
                vk_write_desc_info.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                vk_write_desc_info.pImageInfo     = &vk_image_info;
            }
            else
            {
                dynamic_offsets[res.m_UniformDataIndex] = (uint32_t) scratch_buffer->m_MappedDataCursor;
                const uint32_t uniform_size_nonalign    = GetShaderTypeSize(res.m_Type) * res.m_ElementCount;
                const uint32_t uniform_size             = DM_ALIGN(uniform_size_nonalign, dynamic_alignment);

                // Copy client data to aligned host memory
                // The data_offset here is the offset into the programs uniform data,
                // i.e the source buffer.
                const uint32_t data_offset = uniform_data_offsets[res.m_UniformDataIndex];
                memcpy(&((uint8_t*)scratch_buffer->m_DeviceBuffer.m_MappedDataPtr)[scratch_buffer->m_MappedDataCursor],
                    &program->m_UniformData[data_offset], uniform_size_nonalign);

                // Note in the spec about the offset being zero:
                //   "For VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC and VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC descriptor types,
                //    offset is the base offset from which the dynamic offset is applied and range is the static size
                //    used for all dynamic offsets."
                VkDescriptorBufferInfo& vk_buffer_info = vk_write_buffer_descriptors[buffer_to_write_index++];
                vk_buffer_info.buffer = scratch_buffer->m_DeviceBuffer.m_Handle.m_Buffer;
                vk_buffer_info.offset = 0;
                vk_buffer_info.range  = uniform_size;
                vk_write_desc_info.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                vk_write_desc_info.pBufferInfo      = &vk_buffer_info;

                scratch_buffer->m_MappedDataCursor += uniform_size;
            }

            uniforms_to_write--;

            // Commit and restart if we reached max descriptors per batch
            if (uniform_to_write_index == max_write_descriptors)
            {
                vkUpdateDescriptorSets(vk_device, max_write_descriptors, vk_write_descriptors, 0, 0);
                uniform_to_write_index = 0;
                image_to_write_index = 0;
                buffer_to_write_index = 0;
            }
        }

        if (uniform_to_write_index > 0)
        {
            vkUpdateDescriptorSets(vk_device, uniform_to_write_index, vk_write_descriptors, 0, 0);
        }
    }

    static VkResult CommitUniforms(VkCommandBuffer vk_command_buffer, VkDevice vk_device,
        Program* program_ptr, ScratchBuffer* scratch_buffer,
        uint32_t* dynamic_offsets, const uint32_t alignment)
    {
        VkDescriptorSet* vk_descriptor_set_list = 0x0;
        VkResult res = scratch_buffer->m_DescriptorAllocator->Allocate(vk_device, program_ptr->m_Handle.m_DescriptorSetLayout, DM_MAX_SET_COUNT, &vk_descriptor_set_list);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        const uint32_t num_uniform_buffers = program_ptr->m_VertexModule->m_UniformBufferCount + program_ptr->m_FragmentModule->m_UniformBufferCount;
        VkDescriptorSet vs_set = vk_descriptor_set_list[Program::MODULE_TYPE_VERTEX];
        VkDescriptorSet fs_set = vk_descriptor_set_list[Program::MODULE_TYPE_FRAGMENT];

        UpdateDescriptorSets(vk_device, vs_set, program_ptr,
            Program::MODULE_TYPE_VERTEX, scratch_buffer,
            alignment, dynamic_offsets);
        UpdateDescriptorSets(vk_device, fs_set, program_ptr,
            Program::MODULE_TYPE_FRAGMENT, scratch_buffer,
            alignment, dynamic_offsets);

        vkCmdBindDescriptorSets(vk_command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS, program_ptr->m_Handle.m_PipelineLayout,
            0, Program::MODULE_TYPE_COUNT, vk_descriptor_set_list,
            num_uniform_buffers, dynamic_offsets);

        return VK_SUCCESS;
    }

    static VkResult ResizeDescriptorAllocator(HContext context, DescriptorAllocator* allocator, uint32_t newDescriptorCount)
    {
        DestroyResourceDeferred(context->m_MainResourcesToDestroy[context->m_SwapChain->m_ImageIndex], allocator);
        return CreateDescriptorAllocator(context->m_LogicalDevice.m_Device, newDescriptorCount, allocator);
    }

    static VkResult ResizeScratchBuffer(HContext context, uint32_t newDataSize, ScratchBuffer* scratchBuffer)
    {
        // Put old buffer on the delete queue so we don't mess the descriptors already in-use
        DestroyResourceDeferred(context->m_MainResourcesToDestroy[context->m_SwapChain->m_ImageIndex], &scratchBuffer->m_DeviceBuffer);

        VkResult res = CreateScratchBuffer(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device,
            newDataSize, false, scratchBuffer->m_DescriptorAllocator, scratchBuffer);
        scratchBuffer->m_DeviceBuffer.MapMemory(context->m_LogicalDevice.m_Device);

        return res;
    }

    static void DrawSetup(HContext context, VkCommandBuffer vk_command_buffer, ScratchBuffer* scratchBuffer, DeviceBuffer* indexBuffer, Type indexBufferType)
    {
        DeviceBuffer* vertex_buffer = context->m_CurrentVertexBuffer;
        Program* program_ptr        = context->m_CurrentProgram;
        VkDevice vk_device          = context->m_LogicalDevice.m_Device;

        // Ensure there is room in the descriptor allocator to support this draw call
        bool resize_desc_allocator = (scratchBuffer->m_DescriptorAllocator->m_DescriptorIndex + DM_MAX_SET_COUNT) >
            scratchBuffer->m_DescriptorAllocator->m_DescriptorMax;
        bool resize_scratch_buffer = (program_ptr->m_VertexModule->m_UniformDataSizeAligned +
            program_ptr->m_FragmentModule->m_UniformDataSizeAligned) > (scratchBuffer->m_DeviceBuffer.m_MemorySize - scratchBuffer->m_MappedDataCursor);

        const uint8_t descriptor_increase = 32;
        if (resize_desc_allocator)
        {
            VkResult res = ResizeDescriptorAllocator(context, scratchBuffer->m_DescriptorAllocator, scratchBuffer->m_DescriptorAllocator->m_DescriptorMax + descriptor_increase);
            CHECK_VK_ERROR(res);
        }

        if (resize_scratch_buffer)
        {
            const uint32_t bytes_increase = 256 * descriptor_increase;
            VkResult res = ResizeScratchBuffer(context, scratchBuffer->m_DeviceBuffer.m_MemorySize + bytes_increase, scratchBuffer);
            CHECK_VK_ERROR(res);
        }

        // Ensure we have enough room in the dynamic offset buffer to support the uniforms for this draw call
        const uint32_t num_uniform_buffers = program_ptr->m_VertexModule->m_UniformBufferCount + program_ptr->m_FragmentModule->m_UniformBufferCount;

        if (context->m_DynamicOffsetBufferSize < num_uniform_buffers)
        {
            if (context->m_DynamicOffsetBuffer == 0x0)
            {
                context->m_DynamicOffsetBuffer = (uint32_t*) malloc(sizeof(uint32_t) * num_uniform_buffers);
            }
            else
            {
                context->m_DynamicOffsetBuffer = (uint32_t*) realloc(context->m_DynamicOffsetBuffer, sizeof(uint32_t) * num_uniform_buffers);
            }

            context->m_DynamicOffsetBufferSize = num_uniform_buffers;
        }

        // Write the uniform data to the descriptors
        uint32_t dynamic_alignment = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment;
        VkResult res = CommitUniforms(vk_command_buffer, vk_device,
            program_ptr, scratchBuffer, context->m_DynamicOffsetBuffer, dynamic_alignment);
        CHECK_VK_ERROR(res);

        // If the culling, or viewport has changed, make sure to flip the
        // culling flag if we are rendering to the backbuffer.
        // This is needed because we are rendering with a negative viewport
        // which means that the face direction is inverted.
        if (context->m_CullFaceChanged || context->m_ViewportChanged)
        {
            if (context->m_CurrentRenderTarget->m_Id != DM_RENDERTARGET_BACKBUFFER_ID)
            {
                if (context->m_PipelineState.m_CullFaceType == FACE_TYPE_BACK)
                {
                    context->m_PipelineState.m_CullFaceType = FACE_TYPE_FRONT;
                }
                else if (context->m_PipelineState.m_CullFaceType == FACE_TYPE_FRONT)
                {
                    context->m_PipelineState.m_CullFaceType = FACE_TYPE_BACK;
                }
            }
            context->m_CullFaceChanged = 0;
        }
        // Update the viewport
        if (context->m_ViewportChanged)
        {
            Viewport& vp = context->m_MainViewport;

            // If we are rendering to the backbuffer, we must invert the viewport on
            // the y axis. Otherwise we just use the values as-is.
            // If we don't, all FBO rendering will be upside down.
            if (context->m_CurrentRenderTarget->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
            {
                SetViewportHelper(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
                    vp.m_X, (context->m_WindowHeight - vp.m_Y), vp.m_W, -vp.m_H);
            }
            else
            {
                SetViewportHelper(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
                    vp.m_X, vp.m_Y, vp.m_W, vp.m_H);
            }

            VkRect2D vk_scissor;
            vk_scissor.extent   = context->m_CurrentRenderTarget->m_Extent;
            vk_scissor.offset.x = 0;
            vk_scissor.offset.y = 0;

            vkCmdSetScissor(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex], 0, 1, &vk_scissor);

            context->m_ViewportChanged = 0;
        }

        // Get the pipeline for the active draw state
        VkSampleCountFlagBits vk_sample_count = VK_SAMPLE_COUNT_1_BIT;
        if (context->m_CurrentRenderTarget->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            vk_sample_count = context->m_SwapChain->m_SampleCountFlag;
        }

        Pipeline* pipeline = GetOrCreatePipeline(vk_device, vk_sample_count,
            context->m_PipelineState, context->m_PipelineCache,
            program_ptr, context->m_CurrentRenderTarget,
            vertex_buffer, context->m_CurrentVertexDeclaration);
        vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);


        // Bind the indexbuffer
        if (indexBuffer)
        {
            assert(indexBufferType == TYPE_UNSIGNED_SHORT || indexBufferType == TYPE_UNSIGNED_INT);
            VkIndexType vk_index_type = VK_INDEX_TYPE_UINT16;

            if (indexBufferType == TYPE_UNSIGNED_INT)
            {
                vk_index_type = VK_INDEX_TYPE_UINT32;
            }

            vkCmdBindIndexBuffer(vk_command_buffer, indexBuffer->m_Handle.m_Buffer, 0, vk_index_type);
        }

        // Bind the vertex buffers
        VkBuffer vk_vertex_buffer             = vertex_buffer->m_Handle.m_Buffer;
        VkDeviceSize vk_vertex_buffer_offsets = 0;
        vkCmdBindVertexBuffers(vk_command_buffer, 0, 1, &vk_vertex_buffer, &vk_vertex_buffer_offsets);
    }

    void VulkanHashVertexDeclaration(HashState32 *state, HVertexDeclaration vertex_declaration)
    {
        uint16_t stream_count = vertex_declaration->m_StreamCount;
        for (int i = 0; i < stream_count; ++i)
        {
            VertexDeclaration::Stream& stream = vertex_declaration->m_Streams[i];
            dmHashUpdateBuffer32(state, &stream.m_NameHash, sizeof(stream.m_NameHash));
            dmHashUpdateBuffer32(state, &stream.m_Location, sizeof(stream.m_Location));
            dmHashUpdateBuffer32(state, &stream.m_Offset, sizeof(stream.m_Offset));
            dmHashUpdateBuffer32(state, &stream.m_Format, sizeof(stream.m_Format));
        }
    }

    static void VulkanDrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);
        assert(context->m_FrameBegun);
        const uint8_t image_ix = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
        context->m_PipelineState.m_PrimtiveType = prim_type;
        DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[image_ix], (DeviceBuffer*) index_buffer, type);

        // The 'first' value that comes in is intended to be a byte offset,
        // but vkCmdDrawIndexed only operates with actual offset values into the index buffer
        uint32_t index_offset = first / (type == TYPE_UNSIGNED_SHORT ? 2 : 4);
        vkCmdDrawIndexed(vk_command_buffer, count, 1, index_offset, 0, 0);
    }

    static void VulkanDraw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);
        assert(context->m_FrameBegun);
        const uint8_t image_ix = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
        context->m_PipelineState.m_PrimtiveType = prim_type;
        DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[image_ix], 0, TYPE_BYTE);
        vkCmdDraw(vk_command_buffer, count, 1, first, 0);
    }

    static void CreateShaderResourceBindings(ShaderModule* shader, ShaderDesc::Shader* ddf, uint32_t dynamicAlignment)
    {
        if (ddf->m_Uniforms.m_Count > 0)
        {
            shader->m_Uniforms                 = new ShaderResourceBinding[ddf->m_Uniforms.m_Count];
            shader->m_UniformCount             = ddf->m_Uniforms.m_Count;
            uint32_t uniform_data_size_aligned = 0;
            uint32_t texture_sampler_count     = 0;
            uint32_t uniform_buffer_count      = 0;
            int32_t  last_binding              = -1;

            for (uint32_t i=0; i < ddf->m_Uniforms.m_Count; i++)
            {
                ShaderResourceBinding& res = shader->m_Uniforms[i];
                res.m_Binding              = ddf->m_Uniforms[i].m_Binding;
                res.m_Set                  = ddf->m_Uniforms[i].m_Set;
                res.m_Type                 = ddf->m_Uniforms[i].m_Type;
                res.m_ElementCount         = ddf->m_Uniforms[i].m_ElementCount;
                res.m_Name                 = strdup(ddf->m_Uniforms[i].m_Name);
                res.m_NameHash             = 0;

                assert(res.m_Set <= 1);
                assert(res.m_Binding >= last_binding);
                last_binding = res.m_Binding;

                if (IsUniformTextureSampler(res))
                {
                    res.m_TextureUnit = 0;
                    texture_sampler_count++;
                }
                else
                {
                    res.m_UniformDataIndex     = uniform_buffer_count;
                    uniform_data_size_aligned += DM_ALIGN(GetShaderTypeSize(res.m_Type) * res.m_ElementCount, dynamicAlignment);
                    uniform_buffer_count++;
                }
            }

            shader->m_UniformDataSizeAligned = uniform_data_size_aligned;
            shader->m_UniformBufferCount     = uniform_buffer_count;
        }

        if (ddf->m_Attributes.m_Count > 0)
        {
            shader->m_Attributes     = new ShaderResourceBinding[ddf->m_Attributes.m_Count];
            shader->m_AttributeCount = ddf->m_Attributes.m_Count;
            int32_t  last_binding    = -1;

            for (uint32_t i=0; i < ddf->m_Attributes.m_Count; i++)
            {
                ShaderResourceBinding& res = shader->m_Attributes[i];
                res.m_Binding              = ddf->m_Attributes[i].m_Binding;
                res.m_Set                  = ddf->m_Attributes[i].m_Set;
                res.m_Type                 = ddf->m_Attributes[i].m_Type;
                res.m_Name                 = strdup(ddf->m_Attributes[i].m_Name);
                res.m_NameHash             = dmHashString64(res.m_Name);

                assert(res.m_Binding >= last_binding);
                last_binding = res.m_Binding;
            }
        }
    }

    static HVertexProgram VulkanNewVertexProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        ShaderModule* shader = new ShaderModule;
        memset(shader, 0, sizeof(*shader));
        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf->m_Source.m_Data, ddf->m_Source.m_Count, shader);
        CHECK_VK_ERROR(res);
        CreateShaderResourceBindings(shader, ddf, (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment);
        return (HVertexProgram) shader;
    }

    static HFragmentProgram VulkanNewFragmentProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        ShaderModule* shader = new ShaderModule;
        memset(shader, 0, sizeof(*shader));
        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf->m_Source.m_Data, ddf->m_Source.m_Count, shader);
        CHECK_VK_ERROR(res);
        CreateShaderResourceBindings(shader, ddf, (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment);
        return (HFragmentProgram) shader;
    }

    static void CreateProgramUniforms(ShaderModule* module, VkShaderStageFlags vk_stage_flag,
        uint32_t byte_offset_base, uint32_t* byte_offset_list_out, uint32_t byte_offset_list_size,
        uint32_t* byte_offset_end_out, VkDescriptorSetLayoutBinding* vk_bindings_out)
    {
        uint32_t byte_offset         = byte_offset_base;
        uint32_t num_uniform_buffers = 0;
        for(uint32_t i=0; i < module->m_UniformCount; i++)
        {
            // Process uniform data size
            ShaderResourceBinding& res = module->m_Uniforms[i];
            assert(res.m_Type         != ShaderDesc::SHADER_TYPE_UNKNOWN);

            // Process samplers
            VkDescriptorType vk_descriptor_type;

            // Texture samplers don't need to allocate any memory
            if (IsUniformTextureSampler(res))
            {
                vk_descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
            else
            {
                assert(num_uniform_buffers < byte_offset_list_size);
                byte_offset_list_out[res.m_UniformDataIndex] = byte_offset;
                byte_offset                                 += GetShaderTypeSize(res.m_Type) * res.m_ElementCount;
                vk_descriptor_type                           = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                num_uniform_buffers++;
            }

            // Process descriptor layout
            VkDescriptorSetLayoutBinding& vk_desc = vk_bindings_out[i];
            vk_desc.binding                       = module->m_Uniforms[i].m_Binding;
            vk_desc.descriptorType                = vk_descriptor_type;
            vk_desc.descriptorCount               = 1;
            vk_desc.stageFlags                    = vk_stage_flag;
            vk_desc.pImmutableSamplers            = 0;
        }

        *byte_offset_end_out = byte_offset;
    }

    static void CreateProgram(HContext context, Program* program, ShaderModule* vertex_module, ShaderModule* fragment_module)
    {
        // Set pipeline creation info
        VkPipelineShaderStageCreateInfo vk_vertex_shader_create_info;
        memset(&vk_vertex_shader_create_info, 0, sizeof(vk_vertex_shader_create_info));

        vk_vertex_shader_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vk_vertex_shader_create_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vk_vertex_shader_create_info.module = vertex_module->m_Module;
        vk_vertex_shader_create_info.pName  = "main";

        VkPipelineShaderStageCreateInfo vk_fragment_shader_create_info;
        memset(&vk_fragment_shader_create_info, 0, sizeof(VkPipelineShaderStageCreateInfo));

        vk_fragment_shader_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vk_fragment_shader_create_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        vk_fragment_shader_create_info.module = fragment_module->m_Module;
        vk_fragment_shader_create_info.pName  = "main";

        program->m_PipelineStageInfo[Program::MODULE_TYPE_VERTEX]      = vk_vertex_shader_create_info;
        program->m_PipelineStageInfo[Program::MODULE_TYPE_FRAGMENT]    = vk_fragment_shader_create_info;
        program->m_Hash               = 0;
        program->m_UniformDataOffsets = 0;
        program->m_UniformData        = 0;
        program->m_VertexModule       = vertex_module;
        program->m_FragmentModule     = fragment_module;

        HashState64 program_hash;
        dmHashInit64(&program_hash, false);

        if (vertex_module->m_AttributeCount > 0)
        {
            for (uint32_t i=0; i < vertex_module->m_AttributeCount; i++)
            {
                dmHashUpdateBuffer64(&program_hash, &vertex_module->m_Attributes[i].m_Binding, sizeof(vertex_module->m_Attributes[i].m_Binding));
            }
        }

        dmHashUpdateBuffer64(&program_hash, &vertex_module->m_Hash, sizeof(vertex_module->m_Hash));
        dmHashUpdateBuffer64(&program_hash, &fragment_module->m_Hash, sizeof(fragment_module->m_Hash));
        program->m_Hash = dmHashFinal64(&program_hash);

        const uint32_t num_uniforms = vertex_module->m_UniformCount + fragment_module->m_UniformCount;
        if (num_uniforms > 0)
        {
            VkDescriptorSetLayoutBinding* vk_descriptor_set_bindings = new VkDescriptorSetLayoutBinding[num_uniforms];
            const uint32_t num_buffers  = vertex_module->m_UniformBufferCount + fragment_module->m_UniformBufferCount;

            if (num_buffers > 0)
            {
                program->m_UniformDataOffsets = new uint32_t[num_buffers];
            }

            uint32_t vs_last_offset   = 0;
            uint32_t fs_last_offset   = 0;

            CreateProgramUniforms(vertex_module, VK_SHADER_STAGE_VERTEX_BIT,
                0, program->m_UniformDataOffsets, num_buffers,
                &vs_last_offset, vk_descriptor_set_bindings);
            CreateProgramUniforms(fragment_module, VK_SHADER_STAGE_FRAGMENT_BIT,
                vs_last_offset, &program->m_UniformDataOffsets[vertex_module->m_UniformBufferCount], num_buffers,
                &fs_last_offset, &vk_descriptor_set_bindings[vertex_module->m_UniformCount]);

            program->m_UniformData = new uint8_t[vs_last_offset + fs_last_offset];
            memset(program->m_UniformData, 0, vs_last_offset + fs_last_offset);

            VkDescriptorSetLayoutCreateInfo vk_set_create_info[Program::MODULE_TYPE_COUNT];
            memset(&vk_set_create_info, 0, sizeof(vk_set_create_info));
            vk_set_create_info[Program::MODULE_TYPE_VERTEX].sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            vk_set_create_info[Program::MODULE_TYPE_VERTEX].pBindings      = vk_descriptor_set_bindings;
            vk_set_create_info[Program::MODULE_TYPE_VERTEX].bindingCount   = vertex_module->m_UniformCount;

            vk_set_create_info[Program::MODULE_TYPE_FRAGMENT].sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            vk_set_create_info[Program::MODULE_TYPE_FRAGMENT].pBindings    = &vk_descriptor_set_bindings[vertex_module->m_UniformCount];
            vk_set_create_info[Program::MODULE_TYPE_FRAGMENT].bindingCount = fragment_module->m_UniformCount;

            vkCreateDescriptorSetLayout(context->m_LogicalDevice.m_Device,
                &vk_set_create_info[Program::MODULE_TYPE_VERTEX],
                0, &program->m_Handle.m_DescriptorSetLayout[Program::MODULE_TYPE_VERTEX]);
            vkCreateDescriptorSetLayout(context->m_LogicalDevice.m_Device,
                &vk_set_create_info[Program::MODULE_TYPE_FRAGMENT],
                0, &program->m_Handle.m_DescriptorSetLayout[Program::MODULE_TYPE_FRAGMENT]);

            VkPipelineLayoutCreateInfo vk_layout_create_info;
            memset(&vk_layout_create_info, 0, sizeof(vk_layout_create_info));
            vk_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            vk_layout_create_info.setLayoutCount = Program::MODULE_TYPE_COUNT;
            vk_layout_create_info.pSetLayouts    = program->m_Handle.m_DescriptorSetLayout;

            vkCreatePipelineLayout(context->m_LogicalDevice.m_Device, &vk_layout_create_info, 0, &program->m_Handle.m_PipelineLayout);
            delete[] vk_descriptor_set_bindings;
        }
    }

    static HProgram VulkanNewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        Program* program = new Program;
        CreateProgram(context, program, (ShaderModule*) vertex_program, (ShaderModule*) fragment_program);
        return (HProgram) program;
    }

    static void DestroyProgram(HContext context, Program* program)
    {
        if (program->m_UniformData)
        {
            delete[] program->m_UniformData;
            delete[] program->m_UniformDataOffsets;
        }

        DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], program);
    }

    static void VulkanDeleteProgram(HContext context, HProgram program)
    {
        assert(program);
        Program* program_ptr = (Program*) program;
        DestroyProgram(context, program_ptr);
        delete program_ptr;
    }

    static void DestroyShader(ShaderModule* shader)
    {
        DestroyShaderModule(g_VulkanContext->m_LogicalDevice.m_Device, shader);

        for (uint32_t i=0; i < shader->m_UniformCount; i++)
        {
            free(shader->m_Uniforms[i].m_Name);
        }

        for (uint32_t i=0; i < shader->m_AttributeCount; i++)
        {
            free(shader->m_Attributes[i].m_Name);
        }

        if (shader->m_Attributes)
        {
            delete[] shader->m_Attributes;
        }

        if (shader->m_Uniforms)
        {
            delete[] shader->m_Uniforms;
        }
    }

    static bool ReloadShader(ShaderModule* shader, ShaderDesc::Shader* ddf)
    {
        ShaderModule tmp_shader;
        VkResult res = CreateShaderModule(g_VulkanContext->m_LogicalDevice.m_Device, ddf->m_Source.m_Data, ddf->m_Source.m_Count, &tmp_shader);
        if (res == VK_SUCCESS)
        {
            DestroyShader(shader);
            memset(shader, 0, sizeof(*shader));

            // Transfer created module to old pointer and recreate resource bindings
            shader->m_Hash    = tmp_shader.m_Hash;
            shader->m_Module  = tmp_shader.m_Module;
            CreateShaderResourceBindings(shader, ddf, (uint32_t) g_VulkanContext->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment);
            return true;
        }

        return false;
    }

    static bool VulkanReloadVertexProgram(HVertexProgram prog, ShaderDesc::Shader* ddf)
    {
        return ReloadShader((ShaderModule*) prog, ddf);
    }

    static bool VulkanReloadFragmentProgram(HFragmentProgram prog, ShaderDesc::Shader* ddf)
    {
        return ReloadShader((ShaderModule*) prog, ddf);
    }

    static void VulkanDeleteVertexProgram(HVertexProgram prog)
    {
        ShaderModule* shader = (ShaderModule*) prog;
        DestroyShader(shader);
        delete shader;
    }

    static void VulkanDeleteFragmentProgram(HFragmentProgram prog)
    {
        ShaderModule* shader = (ShaderModule*) prog;
        assert(shader->m_Attributes == 0);

        DestroyShaderModule(g_VulkanContext->m_LogicalDevice.m_Device, shader);

        for (uint32_t i=0; i < shader->m_UniformCount; i++)
        {
            free(shader->m_Uniforms[i].m_Name);
        }

        if (shader->m_Uniforms)
        {
            delete[] shader->m_Uniforms;
        }

        delete shader;
    }

    static ShaderDesc::Language VulkanGetShaderProgramLanguage(HContext context)
    {
        return ShaderDesc::LANGUAGE_SPIRV;
    }

    static void VulkanEnableProgram(HContext context, HProgram program)
    {
        context->m_CurrentProgram = (Program*) program;
    }

    static void VulkanDisableProgram(HContext context)
    {
        context->m_CurrentProgram = 0;
    }

    static bool VulkanReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        Program* program_ptr = (Program*) program;
        DestroyProgram(context, program_ptr);
        CreateProgram(context, program_ptr, (ShaderModule*) vert_program, (ShaderModule*) frag_program);
        return true;
    }

    static uint32_t VulkanGetUniformCount(HProgram prog)
    {
        assert(prog);
        Program* program_ptr = (Program*) prog;
        assert(program_ptr->m_VertexModule && program_ptr->m_FragmentModule);
        return program_ptr->m_VertexModule->m_UniformCount + program_ptr->m_FragmentModule->m_UniformCount;
    }

    static Type shaderDataTypeToGraphicsType(ShaderDesc::ShaderDataType shader_type)
    {
        switch(shader_type)
        {
            case ShaderDesc::SHADER_TYPE_INT:          return TYPE_INT;
            case ShaderDesc::SHADER_TYPE_UINT:         return TYPE_UNSIGNED_INT;
            case ShaderDesc::SHADER_TYPE_FLOAT:        return TYPE_FLOAT;
            case ShaderDesc::SHADER_TYPE_VEC4:         return TYPE_FLOAT_VEC4;
            case ShaderDesc::SHADER_TYPE_MAT4:         return TYPE_FLOAT_MAT4;
            case ShaderDesc::SHADER_TYPE_SAMPLER2D:    return TYPE_SAMPLER_2D;
            case ShaderDesc::SHADER_TYPE_SAMPLER_CUBE: return TYPE_SAMPLER_CUBE;
            default: break;
        }

        // Not supported
        return (Type) 0xffffffff;
    }

    static uint32_t VulkanGetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size)
    {
        assert(prog);
        Program* program_ptr = (Program*) prog;
        ShaderModule* module = program_ptr->m_VertexModule;

        if (index >= program_ptr->m_VertexModule->m_UniformCount)
        {
            module = program_ptr->m_FragmentModule;
            index -= program_ptr->m_VertexModule->m_UniformCount;
        }

        if (index >= module->m_UniformCount)
        {
            return 0;
        }

        ShaderResourceBinding* res = &module->m_Uniforms[index];
        *type = shaderDataTypeToGraphicsType(res->m_Type);
        *size = res->m_ElementCount;

        return (uint32_t)dmStrlCpy(buffer, res->m_Name, buffer_size);
    }

    // In OpenGL, there is a single global resource identifier between
    // fragment and vertex uniforms for a single program. In Vulkan,
    // a uniform can be present in both shaders so we have to keep track
    // of this ourselves. Because of this we pack resource locations
    // for uniforms in a single base register with 15 bits
    // per shader location. If uniform is not found, we return -1 as usual.
    #define UNIFORM_LOCATION_MAX         0x00007FFF
    #define UNIFORM_LOCATION_BIT_COUNT   15
    #define UNIFORM_LOCATION_GET_VS(loc) (loc  & UNIFORM_LOCATION_MAX)
    #define UNIFORM_LOCATION_GET_FS(loc) ((loc & (UNIFORM_LOCATION_MAX << UNIFORM_LOCATION_BIT_COUNT)) >> UNIFORM_LOCATION_BIT_COUNT)

    // TODO, comment from the PR (#4544):
    //   "These frequent lookups could be improved by sorting on the key beforehand,
    //   and during lookup, do a lower_bound, to find the item (or not).
    //   E.g see: engine/render/src/render/material.cpp#L446"
    static bool GetUniformIndex(ShaderResourceBinding* uniforms, uint32_t uniformCount, const char* name, uint32_t* index_out)
    {
        assert(uniformCount < UNIFORM_LOCATION_MAX);
        for (uint32_t i = 0; i < uniformCount; ++i)
        {
            if (dmStrCaseCmp(uniforms[i].m_Name, name) == 0)
            {
                *index_out = i;
                return true;
            }
        }

        return false;
    }

    static int32_t VulkanGetUniformLocation(HProgram prog, const char* name)
    {
        assert(prog);
        Program* program_ptr = (Program*) prog;
        ShaderModule* vs     = program_ptr->m_VertexModule;
        ShaderModule* fs     = program_ptr->m_FragmentModule;
        uint32_t vs_location = UNIFORM_LOCATION_MAX;
        uint32_t fs_location = UNIFORM_LOCATION_MAX;
        bool vs_found        = GetUniformIndex(vs->m_Uniforms, vs->m_UniformCount, name, &vs_location);
        bool fs_found        = GetUniformIndex(fs->m_Uniforms, fs->m_UniformCount, name, &fs_location);

        if (vs_found || fs_found)
        {
            return vs_location | (fs_location << UNIFORM_LOCATION_BIT_COUNT);
        }

        return -1;
    }

    static void VulkanSetConstantV4(HContext context, const dmVMath::Vector4* data, int count, int base_register)
    {
        assert(context->m_CurrentProgram);
        assert(base_register >= 0);
        Program* program_ptr = (Program*) context->m_CurrentProgram;

        uint32_t index_vs  = UNIFORM_LOCATION_GET_VS(base_register);
        uint32_t index_fs  = UNIFORM_LOCATION_GET_FS(base_register);
        assert(!(index_vs == UNIFORM_LOCATION_MAX && index_fs == UNIFORM_LOCATION_MAX));

        if (index_vs != UNIFORM_LOCATION_MAX)
        {
            ShaderResourceBinding& res = program_ptr->m_VertexModule->m_Uniforms[index_vs];
            assert(index_vs < program_ptr->m_VertexModule->m_UniformCount);
            assert(!IsUniformTextureSampler(res));
            uint32_t offset_index      = res.m_UniformDataIndex;
            uint32_t offset            = program_ptr->m_UniformDataOffsets[offset_index];
            memcpy(&program_ptr->m_UniformData[offset], data, sizeof(dmVMath::Vector4) * count);
        }

        if (index_fs != UNIFORM_LOCATION_MAX)
        {
            ShaderResourceBinding& res = program_ptr->m_FragmentModule->m_Uniforms[index_fs];
            assert(index_fs < program_ptr->m_FragmentModule->m_UniformCount);
            assert(!IsUniformTextureSampler(res));
            // Fragment uniforms are packed behind vertex uniforms hence the extra offset here
            uint32_t offset_index = program_ptr->m_VertexModule->m_UniformBufferCount + res.m_UniformDataIndex;
            uint32_t offset       = program_ptr->m_UniformDataOffsets[offset_index];
            memcpy(&program_ptr->m_UniformData[offset], data, sizeof(dmVMath::Vector4) * count);
        }
    }

    static void VulkanSetConstantM4(HContext context, const dmVMath::Vector4* data, int count, int base_register)
    {
        Program* program_ptr = (Program*) context->m_CurrentProgram;

        uint32_t index_vs  = UNIFORM_LOCATION_GET_VS(base_register);
        uint32_t index_fs  = UNIFORM_LOCATION_GET_FS(base_register);
        assert(!(index_vs == UNIFORM_LOCATION_MAX && index_fs == UNIFORM_LOCATION_MAX));

        if (index_vs != UNIFORM_LOCATION_MAX)
        {
            ShaderResourceBinding& res = program_ptr->m_VertexModule->m_Uniforms[index_vs];
            assert(index_vs < program_ptr->m_VertexModule->m_UniformCount);
            assert(!IsUniformTextureSampler(res));
            uint32_t offset_index      = res.m_UniformDataIndex;
            uint32_t offset            = program_ptr->m_UniformDataOffsets[offset_index];
            memcpy(&program_ptr->m_UniformData[offset], data, sizeof(dmVMath::Vector4) * 4 * count);
        }

        if (index_fs != UNIFORM_LOCATION_MAX)
        {
            ShaderResourceBinding& res = program_ptr->m_FragmentModule->m_Uniforms[index_fs];
            assert(index_fs < program_ptr->m_FragmentModule->m_UniformCount);
            assert(!IsUniformTextureSampler(res));
            // Fragment uniforms are packed behind vertex uniforms hence the extra offset here
            uint32_t offset_index = program_ptr->m_VertexModule->m_UniformBufferCount + res.m_UniformDataIndex;
            uint32_t offset       = program_ptr->m_UniformDataOffsets[offset_index];
            memcpy(&program_ptr->m_UniformData[offset], data, sizeof(dmVMath::Vector4) * 4 * count);
        }
    }

    static void VulkanSetSampler(HContext context, int32_t location, int32_t unit)
    {
        assert(context && context->m_CurrentProgram);
        Program* program_ptr = (Program*) context->m_CurrentProgram;

        uint32_t index_vs  = UNIFORM_LOCATION_GET_VS(location);
        uint32_t index_fs  = UNIFORM_LOCATION_GET_FS(location);
        assert(!(index_vs == UNIFORM_LOCATION_MAX && index_fs == UNIFORM_LOCATION_MAX));

        if (index_vs != UNIFORM_LOCATION_MAX)
        {
            ShaderResourceBinding& res = program_ptr->m_VertexModule->m_Uniforms[index_vs];
            assert(index_vs < program_ptr->m_VertexModule->m_UniformCount);
            assert(IsUniformTextureSampler(res));
            program_ptr->m_VertexModule->m_Uniforms[index_vs].m_TextureUnit = (uint16_t) unit;
        }

        if (index_fs != UNIFORM_LOCATION_MAX)
        {
            ShaderResourceBinding& res = program_ptr->m_FragmentModule->m_Uniforms[index_fs];
            assert(index_fs < program_ptr->m_FragmentModule->m_UniformCount);
            assert(IsUniformTextureSampler(res));
            program_ptr->m_FragmentModule->m_Uniforms[index_fs].m_TextureUnit = (uint16_t) unit;
        }
    }

    #undef UNIFORM_LOCATION_MAX
    #undef UNIFORM_LOCATION_BIT_COUNT
    #undef UNIFORM_LOCATION_GET_VS
    #undef UNIFORM_LOCATION_GET_FS

    static void VulkanSetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        // Defer the update to when we actually draw, since we *might* need to invert the viewport
        // depending on wether or not we have set a different rendertarget from when
        // this call was made.
        Viewport& viewport    = context->m_MainViewport;
        viewport.m_X          = (uint16_t) x;
        viewport.m_Y          = (uint16_t) y;
        viewport.m_W          = (uint16_t) width;
        viewport.m_H          = (uint16_t) height;

        context->m_ViewportChanged = 1;
    }

    static void VulkanEnableState(HContext context, State state)
    {
        assert(context);
        SetPipelineStateValue(context->m_PipelineState, state, 1);
    }

    static void VulkanDisableState(HContext context, State state)
    {
        assert(context);
        SetPipelineStateValue(context->m_PipelineState, state, 0);
    }

    static void VulkanSetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
        context->m_PipelineState.m_BlendSrcFactor = source_factor;
        context->m_PipelineState.m_BlendDstFactor = destinaton_factor;
    }

    static void VulkanSetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        uint8_t write_mask = red   ? DM_GRAPHICS_STATE_WRITE_R : 0;
        write_mask        |= green ? DM_GRAPHICS_STATE_WRITE_G : 0;
        write_mask        |= blue  ? DM_GRAPHICS_STATE_WRITE_B : 0;
        write_mask        |= alpha ? DM_GRAPHICS_STATE_WRITE_A : 0;

        context->m_PipelineState.m_WriteColorMask = write_mask;
    }

    static void VulkanSetDepthMask(HContext context, bool mask)
    {
        context->m_PipelineState.m_WriteDepth = mask;
    }

    static void VulkanSetDepthFunc(HContext context, CompareFunc func)
    {
        context->m_PipelineState.m_DepthTestFunc = func;
    }

    static void VulkanSetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        // While scissors are obviously supported in vulkan, we don't expose it
        // to the users via render scripts so it's a bit hard to test.
        // Leaving it unsupported for now.
        assert(0 && "Not supported");
    }

    static void VulkanSetStencilMask(HContext context, uint32_t mask)
    {
        context->m_PipelineState.m_StencilWriteMask = mask;
    }

    static void VulkanSetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void VulkanSetStencilFuncSeparate(HContext context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        if (face_type == FACE_TYPE_BACK)
        {
            context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        }
        else
        {
            context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        }
        context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void VulkanSetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(context);
        context->m_PipelineState.m_StencilFrontOpFail      = sfail;
        context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
        context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        context->m_PipelineState.m_StencilBackOpFail       = sfail;
        context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
        context->m_PipelineState.m_StencilBackOpPass       = dppass;
    }

    static void VulkanSetStencilOpSeparate(HContext context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        if (face_type == FACE_TYPE_BACK)
        {
            context->m_PipelineState.m_StencilBackOpFail       = sfail;
            context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
            context->m_PipelineState.m_StencilBackOpPass       = dppass;
        }
        else
        {
            context->m_PipelineState.m_StencilFrontOpFail      = sfail;
            context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
            context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        }
    }

    static void VulkanSetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
        context->m_PipelineState.m_CullFaceType = face_type;
        context->m_CullFaceChanged              = true;
    }

    static void VulkanSetFaceWinding(HContext, FaceWinding face_winding)
    {
        // TODO: Add this to the vulkan pipeline handle aswell, for now it's a NOP
    }

    static void VulkanSetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
        vkCmdSetDepthBias(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
            factor, 0.0, units);
    }

    static VkFormat GetVulkanFormatFromTextureFormat(TextureFormat format)
    {
        // Reference: https://github.com/KhronosGroup/Vulkan-Samples-Deprecated/blob/master/external/include/vulkan/vk_format.h
        assert(format <= TEXTURE_FORMAT_COUNT);
        switch (format)
        {
            case TEXTURE_FORMAT_LUMINANCE:          return VK_FORMAT_R8_UNORM;
            case TEXTURE_FORMAT_LUMINANCE_ALPHA:    return VK_FORMAT_R8G8_UNORM;
            case TEXTURE_FORMAT_RGB:                return VK_FORMAT_R8G8B8_UNORM;
            case TEXTURE_FORMAT_RGBA:               return VK_FORMAT_R8G8B8A8_UNORM;
            case TEXTURE_FORMAT_RGB_16BPP:          return VK_FORMAT_R5G6B5_UNORM_PACK16;
            case TEXTURE_FORMAT_RGBA_16BPP:         return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
            case TEXTURE_FORMAT_DEPTH:              return VK_FORMAT_UNDEFINED;
            case TEXTURE_FORMAT_STENCIL:            return VK_FORMAT_UNDEFINED;
            case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:   return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
            case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:   return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
            case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:  return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
            case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:  return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
            case TEXTURE_FORMAT_RGB_ETC1:           return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ETC2:          return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_4x4:      return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGB_BC1:            return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_BC3:           return VK_FORMAT_BC3_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_BC7:           return VK_FORMAT_BC7_UNORM_BLOCK;
            case TEXTURE_FORMAT_R_BC4:              return VK_FORMAT_BC4_UNORM_BLOCK;
            case TEXTURE_FORMAT_RG_BC5:             return VK_FORMAT_BC5_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGB16F:             return VK_FORMAT_R16G16B16_SFLOAT;
            case TEXTURE_FORMAT_RGB32F:             return VK_FORMAT_R32G32B32_SFLOAT;
            case TEXTURE_FORMAT_RGBA16F:            return VK_FORMAT_R16G16B16A16_SFLOAT;
            case TEXTURE_FORMAT_RGBA32F:            return VK_FORMAT_R32G32B32A32_SFLOAT;
            case TEXTURE_FORMAT_R16F:               return VK_FORMAT_R16_SFLOAT;
            case TEXTURE_FORMAT_RG16F:              return VK_FORMAT_R16G16_SFLOAT;
            case TEXTURE_FORMAT_R32F:               return VK_FORMAT_R32_SFLOAT;
            case TEXTURE_FORMAT_RG32F:              return VK_FORMAT_R32G32_SFLOAT;
            default:                                return VK_FORMAT_UNDEFINED;
        };
    }

    static VkResult CreateRenderTarget(VkDevice vk_device, Texture** color_textures, BufferType* buffer_types, uint8_t num_color_textures,  Texture* depthStencilTexture, RenderTarget* rtOut)
    {
        assert(rtOut->m_Framebuffer == VK_NULL_HANDLE && rtOut->m_RenderPass == VK_NULL_HANDLE);
        const uint8_t num_attachments = MAX_BUFFER_COLOR_ATTACHMENTS + 1;

        RenderPassAttachment  rp_attachments[num_attachments];
        RenderPassAttachment* rp_attachment_depth_stencil = 0;

        VkImageView fb_attachments[num_attachments];
        uint16_t    fb_attachment_count = 0;
        uint16_t    fb_width            = 0;
        uint16_t    fb_height           = 0;


        for (int i = 0; i < num_color_textures; ++i)
        {
            Texture* color_texture = color_textures[i];

            assert(!color_texture->m_Destroyed && color_texture->m_Handle.m_ImageView != VK_NULL_HANDLE && color_texture->m_Handle.m_Image != VK_NULL_HANDLE);
            uint8_t color_buffer_index = GetBufferTypeIndex(buffer_types[i]);
            fb_width                   = rtOut->m_BufferTextureParams[color_buffer_index].m_Width;
            fb_height                  = rtOut->m_BufferTextureParams[color_buffer_index].m_Height;

            RenderPassAttachment* rp_attachment_color = &rp_attachments[i];
            rp_attachment_color->m_ImageLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rp_attachment_color->m_Format             = color_texture->m_Format;
            fb_attachments[fb_attachment_count++]     = color_texture->m_Handle.m_ImageView;
        }

        if (depthStencilTexture)
        {
            uint8_t depth_buffer_index = GetBufferTypeIndex(BUFFER_TYPE_DEPTH_BIT);
            uint16_t depth_width       = rtOut->m_BufferTextureParams[depth_buffer_index].m_Width;
            uint16_t depth_height      = rtOut->m_BufferTextureParams[depth_buffer_index].m_Height;

            if (num_color_textures == 0)
            {
                fb_width  = depth_width;
                fb_height = depth_height;
            }

            rp_attachment_depth_stencil                = &rp_attachments[fb_attachment_count];
            rp_attachment_depth_stencil->m_ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            rp_attachment_depth_stencil->m_Format      = depthStencilTexture->m_Format;

            fb_attachments[fb_attachment_count++] = depthStencilTexture->m_Handle.m_ImageView;
        }

        VkResult res = CreateRenderPass(vk_device, VK_SAMPLE_COUNT_1_BIT, rp_attachments, num_color_textures, rp_attachment_depth_stencil, 0, &rtOut->m_RenderPass);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        res = CreateFramebuffer(vk_device, rtOut->m_RenderPass,
            fb_width, fb_height, fb_attachments, (uint8_t)fb_attachment_count, &rtOut->m_Framebuffer);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        for (int i = 0; i < num_color_textures; ++i)
        {
            rtOut->m_TextureColor[i] = color_textures[i];
            rtOut->m_ColorAttachmentBufferTypes[i] = buffer_types[i];
        }

        rtOut->m_ColorAttachmentCount = num_color_textures;
        rtOut->m_TextureDepthStencil  = depthStencilTexture;
        rtOut->m_Extent.width         = fb_width;
        rtOut->m_Extent.height        = fb_height;

        return VK_SUCCESS;
    }

    static void DestroyRenderTarget(LogicalDevice* logicalDevice, RenderTarget* renderTarget)
    {
        assert(logicalDevice);
        assert(renderTarget);
        DestroyFrameBuffer(logicalDevice->m_Device, renderTarget->m_Framebuffer);
        DestroyRenderPass(logicalDevice->m_Device, renderTarget->m_RenderPass);
        renderTarget->m_Framebuffer = VK_NULL_HANDLE;
        renderTarget->m_RenderPass = VK_NULL_HANDLE;
    }

    static HRenderTarget VulkanNewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        RenderTarget* rt = new RenderTarget(GetNextRenderTargetId());
        memcpy(rt->m_BufferTextureParams, params, sizeof(rt->m_BufferTextureParams));

        BufferType buffer_types[MAX_BUFFER_COLOR_ATTACHMENTS];
        Texture* texture_color[MAX_BUFFER_COLOR_ATTACHMENTS];
        Texture* texture_depth_stencil = 0; 

        uint8_t has_depth   = buffer_type_flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT;
        uint8_t has_stencil = buffer_type_flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT;
        uint8_t color_index = 0;

        // don't save the data
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            rt->m_BufferTextureParams[i].m_Data     = 0x0;
            rt->m_BufferTextureParams[i].m_DataSize = 0;
        }

        uint16_t fb_width  = 0;
        uint16_t fb_height = 0;

        BufferType color_buffer_flags[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            BufferType buffer_type = color_buffer_flags[i];

            if (buffer_type_flags & buffer_type)
            {
                uint8_t color_buffer_index         = GetBufferTypeIndex(buffer_type);
                TextureParams& color_buffer_params = rt->m_BufferTextureParams[color_buffer_index];
                fb_width                           = color_buffer_params.m_Width;
                fb_height                          = color_buffer_params.m_Height;

                VkFormat vk_color_format;

                // Promote format to RGBA if RGB, since it's not supported
                if (color_buffer_params.m_Format == TEXTURE_FORMAT_RGB)
                {
                    vk_color_format              = VK_FORMAT_R8G8B8A8_UNORM;
                    color_buffer_params.m_Format = TEXTURE_FORMAT_RGBA;
                }
                else
                {
                    vk_color_format = GetVulkanFormatFromTextureFormat(color_buffer_params.m_Format);
                }

                Texture* new_texture_color = NewTexture(context, creation_params[color_buffer_index]);
                VkResult res = CreateTexture2D(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device,
                    new_texture_color->m_Width, new_texture_color->m_Height, new_texture_color->m_MipMapCount,
                    VK_SAMPLE_COUNT_1_BIT, vk_color_format,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, new_texture_color);
                CHECK_VK_ERROR(res);

                SetTextureParams(new_texture_color, color_buffer_params.m_MinFilter, color_buffer_params.m_MagFilter, color_buffer_params.m_UWrap, color_buffer_params.m_VWrap, 1.0f);

                texture_color[color_index] = new_texture_color;
                buffer_types[color_index] = buffer_type;
                color_index++;
            }
        }

        if(has_depth || has_stencil)
        {
            VkFormat vk_depth_stencil_format = VK_FORMAT_UNDEFINED;
            VkImageTiling vk_depth_tiling    = VK_IMAGE_TILING_OPTIMAL;
            uint8_t depth_buffer_index       = GetBufferTypeIndex(BUFFER_TYPE_DEPTH_BIT);

            // Only try depth formats first
            if (has_depth && !has_stencil)
            {
                VkFormat vk_format_list[] = {
                    VK_FORMAT_D32_SFLOAT,
                    VK_FORMAT_D16_UNORM
                };

                uint8_t vk_format_list_size = sizeof(vk_format_list) / sizeof(vk_format_list[2]);
                GetDepthFormatAndTiling(context->m_PhysicalDevice.m_Device, vk_format_list, vk_format_list_size, &vk_depth_stencil_format, &vk_depth_tiling);
            }

            // If we request both depth & stencil OR test above failed,
            // try with default depth stencil formats
            if (vk_depth_stencil_format == VK_FORMAT_UNDEFINED)
            {
                GetDepthFormatAndTiling(context->m_PhysicalDevice.m_Device, 0, 0, &vk_depth_stencil_format, &vk_depth_tiling);
            }

            texture_depth_stencil = NewTexture(context, creation_params[depth_buffer_index]);
            VkResult res = CreateDepthStencilTexture(context,
                vk_depth_stencil_format, vk_depth_tiling,
                fb_width, fb_height, VK_SAMPLE_COUNT_1_BIT, // No support for multisampled FBOs
                texture_depth_stencil);
            CHECK_VK_ERROR(res);
        }

        VkResult res = CreateRenderTarget(context->m_LogicalDevice.m_Device, texture_color, buffer_types, color_index, texture_depth_stencil, rt);
        CHECK_VK_ERROR(res);

        return rt;
    }

    static void VulkanDeleteRenderTarget(HRenderTarget render_target)
    {
        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (render_target->m_TextureColor[i])
            {
                DeleteTexture(render_target->m_TextureColor[i]);
            }
        }

        if (render_target->m_TextureDepthStencil)
        {
            DeleteTexture(render_target->m_TextureDepthStencil);
        }

        DestroyRenderTarget(&g_VulkanContext->m_LogicalDevice, render_target);

        delete render_target;
    }

    static void VulkanSetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        context->m_ViewportChanged = 1;
        BeginRenderPass(context, render_target != 0x0 ? render_target : &context->m_MainRenderTarget);
    }

    static HTexture VulkanGetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
         if(!(buffer_type == BUFFER_TYPE_COLOR0_BIT ||
           buffer_type == BUFFER_TYPE_COLOR1_BIT ||
           buffer_type == BUFFER_TYPE_COLOR2_BIT ||
           buffer_type == BUFFER_TYPE_COLOR3_BIT))
        {
            return 0;
        }

        return (HTexture) render_target->m_TextureColor[GetBufferTypeIndex(buffer_type)];
    }

    static void VulkanGetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        uint32_t i = GetBufferTypeIndex(buffer_type);
        assert(i < MAX_BUFFER_TYPE_COUNT);
        width  = render_target->m_BufferTextureParams[i].m_Width;
        height = render_target->m_BufferTextureParams[i].m_Height;
    }

    static void VulkanSetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            render_target->m_BufferTextureParams[i].m_Width = width;
            render_target->m_BufferTextureParams[i].m_Height = height;

            if (i < MAX_BUFFER_COLOR_ATTACHMENTS && render_target->m_TextureColor[i])
            {
                Texture* texture_color = render_target->m_TextureColor[i];

                DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], texture_color);
                VkResult res = CreateTexture2D(g_VulkanContext->m_PhysicalDevice.m_Device, g_VulkanContext->m_LogicalDevice.m_Device,
                    width, height, texture_color->m_MipMapCount, VK_SAMPLE_COUNT_1_BIT, texture_color->m_Format,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, texture_color);
                CHECK_VK_ERROR(res);
            }
        }

        if (render_target->m_TextureDepthStencil)
        {
            DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], render_target->m_TextureDepthStencil);

            // Check tiling support for this format
            VkImageTiling vk_image_tiling    = VK_IMAGE_TILING_OPTIMAL;
            VkFormat vk_depth_stencil_format = render_target->m_TextureDepthStencil->m_Format;
            VkFormat vk_depth_format         = GetSupportedTilingFormat(g_VulkanContext->m_PhysicalDevice.m_Device, &vk_depth_stencil_format,
                1, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

            if (vk_depth_format == VK_FORMAT_UNDEFINED)
            {
                vk_image_tiling = VK_IMAGE_TILING_LINEAR;
            }

            VkResult res = CreateDepthStencilTexture(g_VulkanContext,
                vk_depth_stencil_format, vk_image_tiling,
                width, height, VK_SAMPLE_COUNT_1_BIT,
                render_target->m_TextureDepthStencil);
            CHECK_VK_ERROR(res);
        }

        DestroyRenderTarget(&g_VulkanContext->m_LogicalDevice, render_target);
        VkResult res = CreateRenderTarget(g_VulkanContext->m_LogicalDevice.m_Device,
            render_target->m_TextureColor,
            render_target->m_ColorAttachmentBufferTypes,
            render_target->m_ColorAttachmentCount,
            render_target->m_TextureDepthStencil, render_target);
        CHECK_VK_ERROR(res);
    }

    static bool VulkanIsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (context->m_TextureFormatSupport & (1 << format)) != 0;
    }

    static HTexture VulkanNewTexture(HContext context, const TextureCreationParams& params)
    {
        Texture* tex = new Texture;
        InitializeVulkanTexture(tex);

        tex->m_Type        = params.m_Type;
        tex->m_Width       = params.m_Width;
        tex->m_Height      = params.m_Height;
        tex->m_MipMapCount = params.m_MipMapCount;

        if (params.m_OriginalWidth == 0)
        {
            tex->m_OriginalWidth  = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
        }
        else
        {
            tex->m_OriginalWidth  = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
        }

        return (HTexture) tex;
    }

    static void VulkanDeleteTexture(HTexture t)
    {
        DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], t);
        delete t;
    }

    static inline uint32_t GetOffsetFromMipmap(Texture* texture, uint8_t mipmap)
    {
        uint8_t bitspp  = GetTextureFormatBitsPerPixel(texture->m_GraphicsFormat);
        uint32_t width  = texture->m_Width;
        uint32_t height = texture->m_Height;
        uint32_t offset = 0;

        for (uint32_t i = 0; i < mipmap; ++i)
        {
            offset += width * height * bitspp;
            width  /= 2;
            height /= 2;
        }

        offset /= 8;
        return offset;
    }

    static inline uint8_t GetLayerCount(Texture* texture)
    {
        return texture->m_Type == TEXTURE_TYPE_CUBE_MAP ? 6 : 1;
    }

    static void CopyToTexture(HContext context, const TextureParams& params,
        bool useStageBuffer, uint32_t texDataSize, void* texDataPtr, Texture* textureOut)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;
        uint8_t layer_count = GetLayerCount(textureOut);

        // TODO There is potentially a bunch of redundancy here.
        //      * Can we use a single command buffer for these updates,
        //        and not create a new one in every transition?
        //      * Should we batch upload all the mipmap levels instead?
        //        There's a lot of extra work doing all these transitions and image copies
        //        per mipmap instead of batching in one cmd
        if (useStageBuffer)
        {
            // Create one-time commandbuffer to carry the copy command
            VkCommandBuffer vk_command_buffer;
            CreateCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &vk_command_buffer);
            VkCommandBufferBeginInfo vk_command_buffer_begin_info;
            memset(&vk_command_buffer_begin_info, 0, sizeof(VkCommandBufferBeginInfo));

            vk_command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vk_command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VkResult res = vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info);
            CHECK_VK_ERROR(res);

            VkSubmitInfo vk_submit_info;
            memset(&vk_submit_info, 0, sizeof(vk_submit_info));
            vk_submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            vk_submit_info.commandBufferCount = 1;
            vk_submit_info.pCommandBuffers    = &vk_command_buffer;

            DeviceBuffer stage_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            res = CreateDeviceBuffer(context->m_PhysicalDevice.m_Device, vk_device, texDataSize,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stage_buffer);
            CHECK_VK_ERROR(res);

            res = WriteToDeviceBuffer(vk_device, texDataSize, 0, texDataPtr, &stage_buffer);
            CHECK_VK_ERROR(res);

            // Transition image to transfer dst for the mipmap level we are uploading
            res = TransitionImageLayout(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_LogicalDevice.m_GraphicsQueue,
                textureOut->m_Handle.m_Image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                params.m_MipMap, layer_count);
            CHECK_VK_ERROR(res);

            uint32_t slice_size = texDataSize / layer_count;

            VkBufferImageCopy vk_copy_regions[6];
            for (int i = 0; i < layer_count; ++i)
            {
                VkBufferImageCopy& vk_copy_region = vk_copy_regions[i];
                vk_copy_region.bufferOffset                    = i * slice_size;
                vk_copy_region.bufferRowLength                 = 0;
                vk_copy_region.bufferImageHeight               = 0;
                vk_copy_region.imageOffset.x                   = params.m_X;
                vk_copy_region.imageOffset.y                   = params.m_Y;
                vk_copy_region.imageOffset.z                   = 0;
                vk_copy_region.imageExtent.width               = params.m_Width;
                vk_copy_region.imageExtent.height              = params.m_Height;
                vk_copy_region.imageExtent.depth               = 1;
                vk_copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                vk_copy_region.imageSubresource.mipLevel       = params.m_MipMap;
                vk_copy_region.imageSubresource.baseArrayLayer = i;
                vk_copy_region.imageSubresource.layerCount     = 1;
            }

            vkCmdCopyBufferToImage(vk_command_buffer, stage_buffer.m_Handle.m_Buffer,
                textureOut->m_Handle.m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                layer_count, vk_copy_regions);

            res = vkEndCommandBuffer(vk_command_buffer);
            CHECK_VK_ERROR(res);

            res = vkQueueSubmit(context->m_LogicalDevice.m_GraphicsQueue, 1, &vk_submit_info, VK_NULL_HANDLE);
            CHECK_VK_ERROR(res);

            vkQueueWaitIdle(context->m_LogicalDevice.m_GraphicsQueue);

            res = TransitionImageLayout(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_LogicalDevice.m_GraphicsQueue,
                textureOut->m_Handle.m_Image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                params.m_MipMap, layer_count);
            CHECK_VK_ERROR(res);

            DestroyDeviceBuffer(vk_device, &stage_buffer.m_Handle);

            vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &vk_command_buffer);
        }
        else
        {
            uint32_t write_offset = GetOffsetFromMipmap(textureOut, (uint8_t) params.m_MipMap);

            VkResult res = WriteToDeviceBuffer(vk_device, texDataSize, write_offset, texDataPtr, &textureOut->m_DeviceBuffer);
            CHECK_VK_ERROR(res);

            res = TransitionImageLayout(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_LogicalDevice.m_GraphicsQueue, textureOut->m_Handle.m_Image,
                VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, params.m_MipMap, layer_count);
            CHECK_VK_ERROR(res);
        }
    }

    static void RepackRGBToRGBA(uint32_t num_pixels, uint8_t* rgb, uint8_t* rgba)
    {
        for(uint32_t px=0; px < num_pixels; px++)
        {
            rgba[0] = rgb[0];
            rgba[1] = rgb[1];
            rgba[2] = rgb[2];
            rgba[3] = 255;
            rgba+=4;
            rgb+=3;
        }
    }

    static void VulkanSetTexture(HTexture texture, const TextureParams& params)
    {
        // Same as graphics_opengl.cpp
        switch (params.m_Format)
        {
            case TEXTURE_FORMAT_DEPTH:
            case TEXTURE_FORMAT_STENCIL:
                dmLogError("Unable to upload texture data, unsupported type (%s).", TextureFormatToString(params.m_Format));
                return;
            default:break;
        }

        assert(params.m_Width  <= g_VulkanContext->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D);
        assert(params.m_Height <= g_VulkanContext->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D);

        if (texture->m_MipMapCount == 1 && params.m_MipMap > 0)
        {
            return;
        }

        TextureFormat format_orig   = params.m_Format;
        uint8_t tex_bpp             = GetTextureFormatBitsPerPixel(params.m_Format);
        uint8_t tex_layer_count     = GetLayerCount(texture);
        size_t tex_data_size        = 0;
        void*  tex_data_ptr         = (void*)params.m_Data;
        VkFormat vk_format          = GetVulkanFormatFromTextureFormat(params.m_Format);

        if (vk_format == VK_FORMAT_UNDEFINED)
        {
            dmLogError("Unable to upload texture data, unsupported type (%s).", TextureFormatToString(format_orig));
            return;
        }

        LogicalDevice& logical_device       = g_VulkanContext->m_LogicalDevice;
        VkPhysicalDevice vk_physical_device = g_VulkanContext->m_PhysicalDevice.m_Device;

        // Note: There's no RGB support in Vulkan. We have to expand this to four channels
        // TODO: Can we use R11G11B10 somehow?
        if (format_orig == TEXTURE_FORMAT_RGB)
        {
            uint32_t data_pixel_count = params.m_Width * params.m_Height * tex_layer_count;
            uint8_t bpp_new           = 32;
            uint8_t* data_new         = new uint8_t[data_pixel_count * bpp_new];

            RepackRGBToRGBA(data_pixel_count, (uint8_t*) tex_data_ptr, data_new);
            vk_format     = VK_FORMAT_R8G8B8A8_UNORM;
            tex_data_ptr  = data_new;
            tex_bpp       = bpp_new;
        }

        tex_data_size             = tex_bpp * params.m_Width * params.m_Height * tex_layer_count;
        texture->m_GraphicsFormat = params.m_Format;
        texture->m_MipMapCount    = dmMath::Max(texture->m_MipMapCount, (uint16_t)(params.m_MipMap+1));

        SetTextureParams(texture, params.m_MinFilter, params.m_MagFilter, params.m_UWrap, params.m_VWrap, 1.0f);

        if (params.m_SubUpdate)
        {
            // data size might be different if we have generated a new image
            tex_data_size = params.m_Width * params.m_Height * tex_bpp * tex_layer_count;
        }
        else if (params.m_MipMap == 0)
        {
            if (texture->m_Format != vk_format || texture->m_Width != params.m_Width || texture->m_Height != params.m_Height)
            {
                DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], texture);
                texture->m_Format = vk_format;
            }
        }

        bool use_stage_buffer = true;
#if defined(__MACH__) && (defined(__arm__) || defined(__arm64__) || defined(IOS_SIMULATOR))
        // Can't use a staging buffer for MoltenVK when we upload
        // PVRTC textures.
        if (vk_format == VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG ||
            vk_format == VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG)
        {
            use_stage_buffer = false;
        }
#endif

        // If texture hasn't been used yet or if it has been changed
        if (texture->m_Destroyed || texture->m_Handle.m_Image == VK_NULL_HANDLE)
        {
            assert(!params.m_SubUpdate);
            VkImageTiling vk_image_tiling           = VK_IMAGE_TILING_OPTIMAL;
            VkImageUsageFlags vk_usage_flags        = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            VkFormatFeatureFlags vk_format_features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            VkImageLayout vk_initial_layout         = VK_IMAGE_LAYOUT_UNDEFINED;
            VkMemoryPropertyFlags vk_memory_type    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            if (!use_stage_buffer)
            {
                vk_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT;
                vk_memory_type = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            }

            // Check this format for optimal layout support
            if (VK_FORMAT_UNDEFINED == GetSupportedTilingFormat(vk_physical_device, &vk_format,
                1, vk_image_tiling, vk_format_features))
            {
                // Linear doesn't support mipmapping (for MoltenVK only?)
                vk_image_tiling        = VK_IMAGE_TILING_LINEAR;
                texture->m_MipMapCount = 1;
            }

            VkResult res = CreateTexture2D(vk_physical_device, logical_device.m_Device,
                texture->m_Width, texture->m_Height, texture->m_MipMapCount, VK_SAMPLE_COUNT_1_BIT,
                vk_format, vk_image_tiling, vk_usage_flags,
                vk_memory_type, VK_IMAGE_ASPECT_COLOR_BIT, vk_initial_layout, texture);
            CHECK_VK_ERROR(res);
        }

        tex_data_size = (int) ceil((float) tex_data_size / 8.0f);

        CopyToTexture(g_VulkanContext, params, use_stage_buffer, tex_data_size, tex_data_ptr, texture);

        if (format_orig == TEXTURE_FORMAT_RGB)
        {
            delete[] (uint8_t*)tex_data_ptr;
        }
    }

    static void VulkanSetTextureAsync(HTexture texture, const TextureParams& params)
    {
        // Async texture loading is not supported in Vulkan, defaulting to syncronous loading until then
        VulkanSetTexture(texture, params);
    }

    static float GetMaxAnisotrophyClamped(float max_anisotropy_requested)
    {
        return dmMath::Min(max_anisotropy_requested, g_VulkanContext->m_PhysicalDevice.m_Properties.limits.maxSamplerAnisotropy);
    }

    static void VulkanSetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        TextureSampler sampler   = g_VulkanContext->m_TextureSamplers[texture->m_TextureSamplerIndex];
        float anisotropy_clamped = GetMaxAnisotrophyClamped(max_anisotropy);

        if (sampler.m_MinFilter     != minfilter              ||
            sampler.m_MagFilter     != magfilter              ||
            sampler.m_AddressModeU  != uwrap                  ||
            sampler.m_AddressModeV  != vwrap                  ||
            sampler.m_MaxLod        != texture->m_MipMapCount ||
            sampler.m_MaxAnisotropy != anisotropy_clamped)
        {
            int16_t sampler_index = GetTextureSamplerIndex(g_VulkanContext->m_TextureSamplers, minfilter, magfilter, uwrap, vwrap, texture->m_MipMapCount, anisotropy_clamped);
            if (sampler_index < 0)
            {
                sampler_index = CreateVulkanTextureSampler(g_VulkanContext->m_LogicalDevice.m_Device, g_VulkanContext->m_TextureSamplers, minfilter, magfilter, uwrap, vwrap, texture->m_MipMapCount, anisotropy_clamped);
            }

            texture->m_TextureSamplerIndex = sampler_index;
        }
    }

    // NOTE: Currently over estimates the resource usage for compressed formats!
    static uint32_t VulkanGetTextureResourceSize(HTexture texture)
    {
        uint32_t size_total = 0;
        uint32_t size = texture->m_Width * texture->m_Height * dmMath::Max(1U, GetTextureFormatBitsPerPixel(texture->m_GraphicsFormat)/8);
        for(uint32_t i = 0; i < texture->m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        if (texture->m_Type == TEXTURE_TYPE_CUBE_MAP)
        {
            size_total *= 6;
        }
        return size_total + sizeof(Texture);
    }

    static uint16_t VulkanGetTextureWidth(HTexture texture)
    {
        return texture->m_Width;
    }

    static uint16_t VulkanGetTextureHeight(HTexture texture)
    {
        return texture->m_Height;
    }

    static uint16_t VulkanGetOriginalTextureWidth(HTexture texture)
    {
        return texture->m_OriginalWidth;
    }

    static uint16_t VulkanGetOriginalTextureHeight(HTexture texture)
    {
        return texture->m_OriginalHeight;
    }

    static void VulkanEnableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(unit < DM_MAX_TEXTURE_UNITS);
        context->m_TextureUnits[unit] = texture;
    }

    static void VulkanDisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(unit < DM_MAX_TEXTURE_UNITS);
        context->m_TextureUnits[unit] = context->m_DefaultTexture;
    }

    static uint32_t VulkanGetMaxTextureSize(HContext context)
    {
        return context->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D;
    }

    static uint32_t VulkanGetTextureStatusFlags(HTexture texture)
    {
        return 0;
    }

    static void VulkanReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {}

    static void VulkanRunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }

    void DestroyPipelineCacheCb(HContext context, const uint64_t* key, Pipeline* value)
    {
        DestroyPipeline(context->m_LogicalDevice.m_Device, value);
    }

    static GraphicsAdapterFunctionTable VulkanRegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table;
        memset(&fn_table,0,sizeof(fn_table));
        fn_table.m_NewContext = VulkanNewContext;
        fn_table.m_DeleteContext = VulkanDeleteContext;
        fn_table.m_Initialize = VulkanInitialize;
        fn_table.m_Finalize = VulkanFinalize;
        fn_table.m_GetWindowRefreshRate = VulkanGetWindowRefreshRate;
        fn_table.m_OpenWindow = VulkanOpenWindow;
        fn_table.m_CloseWindow = VulkanCloseWindow;
        fn_table.m_IconifyWindow = VulkanIconifyWindow;
        fn_table.m_GetWindowState = VulkanGetWindowState;
        fn_table.m_GetDisplayDpi = VulkanGetDisplayDpi;
        fn_table.m_GetWidth = VulkanGetWidth;
        fn_table.m_GetHeight = VulkanGetHeight;
        fn_table.m_GetWindowWidth = VulkanGetWindowWidth;
        fn_table.m_GetWindowHeight = VulkanGetWindowHeight;
        fn_table.m_GetDisplayScaleFactor = VulkanGetDisplayScaleFactor;
        fn_table.m_SetWindowSize = VulkanSetWindowSize;
        fn_table.m_ResizeWindow = VulkanResizeWindow;
        fn_table.m_GetDefaultTextureFilters = VulkanGetDefaultTextureFilters;
        fn_table.m_BeginFrame = VulkanBeginFrame;
        fn_table.m_Flip = VulkanFlip;
        fn_table.m_SetSwapInterval = VulkanSetSwapInterval;
        fn_table.m_Clear = VulkanClear;
        fn_table.m_NewVertexBuffer = VulkanNewVertexBuffer;
        fn_table.m_DeleteVertexBuffer = VulkanDeleteVertexBuffer;
        fn_table.m_SetVertexBufferData = VulkanSetVertexBufferData;
        fn_table.m_SetVertexBufferSubData = VulkanSetVertexBufferSubData;
        fn_table.m_GetMaxElementsVertices = VulkanGetMaxElementsVertices;
        fn_table.m_NewIndexBuffer = VulkanNewIndexBuffer;
        fn_table.m_DeleteIndexBuffer = VulkanDeleteIndexBuffer;
        fn_table.m_SetIndexBufferData = VulkanSetIndexBufferData;
        fn_table.m_SetIndexBufferSubData = VulkanSetIndexBufferSubData;
        fn_table.m_IsIndexBufferFormatSupported = VulkanIsIndexBufferFormatSupported;
        fn_table.m_NewVertexDeclaration = VulkanNewVertexDeclaration;
        fn_table.m_NewVertexDeclarationStride = VulkanNewVertexDeclarationStride;
        fn_table.m_SetStreamOffset = VulkanSetStreamOffset;
        fn_table.m_DeleteVertexDeclaration = VulkanDeleteVertexDeclaration;
        fn_table.m_EnableVertexDeclaration = VulkanEnableVertexDeclaration;
        fn_table.m_EnableVertexDeclarationProgram = VulkanEnableVertexDeclarationProgram;
        fn_table.m_DisableVertexDeclaration = VulkanDisableVertexDeclaration;
        fn_table.m_HashVertexDeclaration = VulkanHashVertexDeclaration;
        fn_table.m_DrawElements = VulkanDrawElements;
        fn_table.m_Draw = VulkanDraw;
        fn_table.m_NewVertexProgram = VulkanNewVertexProgram;
        fn_table.m_NewFragmentProgram = VulkanNewFragmentProgram;
        fn_table.m_NewProgram = VulkanNewProgram;
        fn_table.m_DeleteProgram = VulkanDeleteProgram;
        fn_table.m_ReloadVertexProgram = VulkanReloadVertexProgram;
        fn_table.m_ReloadFragmentProgram = VulkanReloadFragmentProgram;
        fn_table.m_DeleteVertexProgram = VulkanDeleteVertexProgram;
        fn_table.m_DeleteFragmentProgram = VulkanDeleteFragmentProgram;
        fn_table.m_GetShaderProgramLanguage = VulkanGetShaderProgramLanguage;
        fn_table.m_EnableProgram = VulkanEnableProgram;
        fn_table.m_DisableProgram = VulkanDisableProgram;
        fn_table.m_ReloadProgram = VulkanReloadProgram;
        fn_table.m_GetUniformName = VulkanGetUniformName;
        fn_table.m_GetUniformCount = VulkanGetUniformCount;
        fn_table.m_GetUniformLocation = VulkanGetUniformLocation;
        fn_table.m_SetConstantV4 = VulkanSetConstantV4;
        fn_table.m_SetConstantM4 = VulkanSetConstantM4;
        fn_table.m_SetSampler = VulkanSetSampler;
        fn_table.m_SetViewport = VulkanSetViewport;
        fn_table.m_EnableState = VulkanEnableState;
        fn_table.m_DisableState = VulkanDisableState;
        fn_table.m_SetBlendFunc = VulkanSetBlendFunc;
        fn_table.m_SetColorMask = VulkanSetColorMask;
        fn_table.m_SetDepthMask = VulkanSetDepthMask;
        fn_table.m_SetDepthFunc = VulkanSetDepthFunc;
        fn_table.m_SetScissor = VulkanSetScissor;
        fn_table.m_SetStencilMask = VulkanSetStencilMask;
        fn_table.m_SetStencilFunc = VulkanSetStencilFunc;
        fn_table.m_SetStencilFuncSeparate = VulkanSetStencilFuncSeparate;
        fn_table.m_SetStencilOp = VulkanSetStencilOp;
        fn_table.m_SetStencilOpSeparate = VulkanSetStencilOpSeparate;
        fn_table.m_SetCullFace = VulkanSetCullFace;
        fn_table.m_SetFaceWinding = VulkanSetFaceWinding;
        fn_table.m_SetPolygonOffset = VulkanSetPolygonOffset;
        fn_table.m_NewRenderTarget = VulkanNewRenderTarget;
        fn_table.m_DeleteRenderTarget = VulkanDeleteRenderTarget;
        fn_table.m_SetRenderTarget = VulkanSetRenderTarget;
        fn_table.m_GetRenderTargetTexture = VulkanGetRenderTargetTexture;
        fn_table.m_GetRenderTargetSize = VulkanGetRenderTargetSize;
        fn_table.m_SetRenderTargetSize = VulkanSetRenderTargetSize;
        fn_table.m_IsTextureFormatSupported = VulkanIsTextureFormatSupported;
        fn_table.m_NewTexture = VulkanNewTexture;
        fn_table.m_DeleteTexture = VulkanDeleteTexture;
        fn_table.m_SetTexture = VulkanSetTexture;
        fn_table.m_SetTextureAsync = VulkanSetTextureAsync;
        fn_table.m_SetTextureParams = VulkanSetTextureParams;
        fn_table.m_GetTextureResourceSize = VulkanGetTextureResourceSize;
        fn_table.m_GetTextureWidth = VulkanGetTextureWidth;
        fn_table.m_GetTextureHeight = VulkanGetTextureHeight;
        fn_table.m_GetOriginalTextureWidth = VulkanGetOriginalTextureWidth;
        fn_table.m_GetOriginalTextureHeight = VulkanGetOriginalTextureHeight;
        fn_table.m_EnableTexture = VulkanEnableTexture;
        fn_table.m_DisableTexture = VulkanDisableTexture;
        fn_table.m_GetMaxTextureSize = VulkanGetMaxTextureSize;
        fn_table.m_GetTextureStatusFlags = VulkanGetTextureStatusFlags;
        fn_table.m_ReadPixels = VulkanReadPixels;
        fn_table.m_RunApplicationLoop = VulkanRunApplicationLoop;
        fn_table.m_IsExtensionSupported = VulkanIsExtensionSupported;
        fn_table.m_GetNumSupportedExtensions = VulkanGetNumSupportedExtensions;
        fn_table.m_GetSupportedExtension = VulkanGetSupportedExtension;
        fn_table.m_IsMultiTargetRenderingSupported = VulkanIsMultiTargetRenderingSupported;
        return fn_table;
    }
}
