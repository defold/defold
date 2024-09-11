// Copyright 2020-2024 The Defold Foundation
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

#include <platform/platform_window_vulkan.h>

DM_PROPERTY_EXTERN(rmtp_DrawCalls);
DM_PROPERTY_EXTERN(rmtp_DispatchCalls);

namespace dmGraphics
{
    static GraphicsAdapterFunctionTable VulkanRegisterFunctionTable();
    static bool                         VulkanIsSupported();
    static const int8_t    g_vulkan_adapter_priority = 0;
    static GraphicsAdapter g_vulkan_adapter(ADAPTER_FAMILY_VULKAN);

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterVulkan, &g_vulkan_adapter, VulkanIsSupported, VulkanRegisterFunctionTable, VulkanGetContext, g_vulkan_adapter_priority);

    static const char* VkResultToStr(VkResult res);
    #define CHECK_VK_ERROR(result) \
    { \
        if(g_VulkanContext->m_VerifyGraphicsCalls && result != VK_SUCCESS) { \
            dmLogError("Vulkan Error (%s:%d) %s", __FILE__, __LINE__, VkResultToStr(result)); \
            assert(0); \
        } \
    }

    VulkanContext* g_VulkanContext = 0;

    static VulkanTexture* VulkanNewTextureInternal(const TextureCreationParams& params);
    static void           VulkanDeleteTextureInternal(VulkanTexture* texture);
    static void           VulkanSetTextureInternal(VulkanTexture* texture, const TextureParams& params);
    static void           VulkanSetTextureParamsInternal(VulkanTexture* texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy);
    static void           CopyToTexture(VulkanContext* context, const TextureParams& params, bool useStageBuffer, uint32_t texDataSize, void* texDataPtr, VulkanTexture* textureOut);
    static VkFormat       GetVulkanFormatFromTextureFormat(TextureFormat format);

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

    VulkanContext::VulkanContext(const ContextParams& params, const VkInstance vk_instance)
    {
        memset(this, 0, sizeof(*this));
        m_Instance                = vk_instance;
        m_NumFramesInFlight       = DM_MAX_FRAMES_IN_FLIGHT;
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_VerifyGraphicsCalls     = params.m_VerifyGraphicsCalls;
        m_UseValidationLayers     = params.m_UseValidationLayers;
        m_RenderDocSupport        = params.m_RenderDocSupport;
        m_Window                  = params.m_Window;
        m_Width                   = params.m_Width;
        m_Height                  = params.m_Height;
        m_SwapInterval            = params.m_SwapInterval;

        // We need to have some sort of valid default filtering
        if (m_DefaultTextureMinFilter == TEXTURE_FILTER_DEFAULT)
            m_DefaultTextureMinFilter = TEXTURE_FILTER_LINEAR;
        if (m_DefaultTextureMagFilter == TEXTURE_FILTER_DEFAULT)
            m_DefaultTextureMagFilter = TEXTURE_FILTER_LINEAR;

        assert(dmPlatform::GetWindowStateParam(m_Window, dmPlatform::WINDOW_STATE_OPENED));

        DM_STATIC_ASSERT(sizeof(m_TextureFormatSupport) * 8 >= TEXTURE_FORMAT_COUNT, Invalid_Struct_Size );
    }

    static inline bool IsTextureMemoryless(VulkanTexture* texture)
    {
        return texture->m_UsageFlags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
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

    static inline bool IsRenderTargetbound(VulkanContext* context, HRenderTarget rt)
    {
        RenderTarget* current_rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, rt);
        return current_rt ? current_rt->m_IsBound : 0;
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

        if (magfilter == TEXTURE_FILTER_DEFAULT)
            magfilter = g_VulkanContext->m_DefaultTextureMagFilter;
        if (minfilter == TEXTURE_FILTER_DEFAULT)
            minfilter = g_VulkanContext->m_DefaultTextureMinFilter;

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
        if (minfilter == TEXTURE_FILTER_DEFAULT)
            minfilter = g_VulkanContext->m_DefaultTextureMinFilter;
        if (magfilter == TEXTURE_FILTER_DEFAULT)
            magfilter = g_VulkanContext->m_DefaultTextureMagFilter;

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

    static bool EndRenderPass(VulkanContext* context)
    {
        RenderTarget* current_rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);

        if (!current_rt->m_IsBound)
        {
            return false;
        }

        vkCmdEndRenderPass(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex]);
        current_rt->m_IsBound = 0;
        return true;
    }

    static void BeginRenderPass(VulkanContext* context, HRenderTarget render_target)
    {
        RenderTarget* current_rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);
        RenderTarget* rt         = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);

        if (current_rt->m_Id == rt->m_Id &&
            current_rt->m_IsBound)
        {
            return;
        }

        // If we bind a render pass without explicitly unbinding
        // the current render pass, we must first unbind it.
        if (current_rt->m_IsBound)
        {
            EndRenderPass(context);
        }

        VkClearValue vk_clear_values[MAX_BUFFER_COLOR_ATTACHMENTS + 1];
        memset(vk_clear_values, 0, sizeof(vk_clear_values));

        // Clear color
        for (int i = 0; i < rt->m_ColorAttachmentCount; ++i)
        {
            vk_clear_values[i].color.float32[3] = 1.0f;
            vk_clear_values[i].color.float32[0] = rt->m_ColorAttachmentClearValue[i][0];
            vk_clear_values[i].color.float32[1] = rt->m_ColorAttachmentClearValue[i][1];
            vk_clear_values[i].color.float32[2] = rt->m_ColorAttachmentClearValue[i][2];
            vk_clear_values[i].color.float32[3] = rt->m_ColorAttachmentClearValue[i][3];
        }

        // Clear depth
        vk_clear_values[1].depthStencil.depth   = 1.0f;
        vk_clear_values[1].depthStencil.stencil = 0;

        VkRenderPassBeginInfo vk_render_pass_begin_info;
        vk_render_pass_begin_info.sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vk_render_pass_begin_info.renderPass          = rt->m_Handle.m_RenderPass;
        vk_render_pass_begin_info.framebuffer         = rt->m_Handle.m_Framebuffer;
        vk_render_pass_begin_info.pNext               = 0;
        vk_render_pass_begin_info.renderArea.offset.x = 0;
        vk_render_pass_begin_info.renderArea.offset.y = 0;
        vk_render_pass_begin_info.renderArea.extent   = rt->m_Extent;
        vk_render_pass_begin_info.clearValueCount     = rt->m_ColorAttachmentCount + 1;
        vk_render_pass_begin_info.pClearValues        = vk_clear_values;

        vkCmdBeginRenderPass(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex], &vk_render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        rt->m_IsBound          = 1;
        rt->m_SubPassIndex     = 0;
        rt->m_Scissor.extent   = rt->m_Extent;
        rt->m_Scissor.offset.x = 0;
        rt->m_Scissor.offset.y = 0;

        context->m_CurrentRenderTarget = render_target;
    }

    static VkImageAspectFlags GetDefaultDepthAndStencilAspectFlags(VkFormat vk_format)
    {
        // The aspect flag indicates what the image should be used for,
        // it is usually color or stencil | depth.
        VkImageAspectFlags vk_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (vk_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            vk_format == VK_FORMAT_D24_UNORM_S8_UINT  ||
            vk_format == VK_FORMAT_D16_UNORM_S8_UINT)
        {
            vk_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        return vk_aspect;
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

    static VkResult CreateDepthStencilTexture(VulkanContext* context, VkFormat vk_depth_format, VkImageTiling vk_depth_tiling,
        uint32_t width, uint32_t height, VkSampleCountFlagBits vk_sample_count, VkImageAspectFlags vk_aspect_flags, VulkanTexture* depth_stencil_texture_out)
    {
        const VkPhysicalDevice vk_physical_device = context->m_PhysicalDevice.m_Device;
        const VkDevice vk_device                  = context->m_LogicalDevice.m_Device;
        VkImageUsageFlags vk_usage_flags          = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | depth_stencil_texture_out->m_UsageFlags;
        VkMemoryPropertyFlags vk_memory_type      = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (IsTextureMemoryless(depth_stencil_texture_out))
        {
            vk_memory_type |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
        }

        VkResult res = CreateTexture2D(
            vk_physical_device, vk_device, width, height, 1, 1,
            vk_sample_count, vk_depth_format, vk_depth_tiling,
            vk_usage_flags,
            vk_memory_type,
            vk_aspect_flags,
            depth_stencil_texture_out);
        CHECK_VK_ERROR(res);

        if (res == VK_SUCCESS)
        {
            res = TransitionImageLayout(vk_device,
                context->m_LogicalDevice.m_CommandPool,
                context->m_LogicalDevice.m_GraphicsQueue,
                depth_stencil_texture_out,
                vk_aspect_flags,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            CHECK_VK_ERROR(res);
        }

        return res;
    }

    VkResult CreateMainFrameBuffers(VulkanContext* context)
    {
        assert(g_VulkanContext->m_SwapChain);

        // We need to create a framebuffer per swap chain image
        // so that they can be used in different states in the rendering pipeline
        context->m_MainFrameBuffers.SetCapacity(context->m_SwapChain->m_Images.Size());
        context->m_MainFrameBuffers.SetSize(context->m_SwapChain->m_Images.Size());

        SwapChain* swapChain = context->m_SwapChain;
        VkResult res;

        VulkanTexture* depth_stencil_texture = &context->m_MainTextureDepthStencil;

        for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
        {
            // All swap chain images can share the same depth buffer,
            // that's why we create a single depth buffer at the start and reuse it.
            // Same thing goes for the resolve buffer if we are rendering with
            // multisampling enabled.
            VkImageView& vk_image_view_swap      = swapChain->m_ImageViews[i];
            VkImageView& vk_image_view_depth     = depth_stencil_texture->m_Handle.m_ImageView;
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

    VkResult DestroyMainFrameBuffers(VulkanContext* context)
    {
        for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
        {
            DestroyFrameBuffer(context->m_LogicalDevice.m_Device, context->m_MainFrameBuffers[i]);
        }
        context->m_MainFrameBuffers.SetCapacity(0);
        context->m_MainFrameBuffers.SetSize(0);
        return VK_SUCCESS;
    }

    static VkResult SetupMainRenderTarget(VulkanContext* context)
    {
        // Initialize the dummy rendertarget for the main framebuffer
        // The m_Framebuffer construct will be rotated sequentially
        // with the framebuffer objects created per swap chain.

        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_MainRenderTarget);
        if (rt == 0x0)
        {
            rt                          = new RenderTarget(DM_RENDERTARGET_BACKBUFFER_ID);
            context->m_MainRenderTarget = StoreAssetInContainer(context->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
        }

        rt->m_Handle.m_RenderPass  = context->m_MainRenderPass;
        rt->m_Handle.m_Framebuffer = context->m_MainFrameBuffers[0];
        rt->m_Extent               = context->m_SwapChain->m_ImageExtent;
        rt->m_ColorAttachmentCount = 1;

        return VK_SUCCESS;
    }

    static VkResult CreateMainRenderingResources(VulkanContext* context)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;

        // Create depth/stencil buffer
        VkFormat vk_depth_format;
        VkImageTiling vk_depth_tiling;
        GetDepthFormatAndTiling(context->m_PhysicalDevice.m_Device, 0, 0, &vk_depth_format, &vk_depth_tiling);

        VulkanTexture* depth_stencil_texture = &context->m_MainTextureDepthStencil;

        VkResult res = CreateDepthStencilTexture(context,
            vk_depth_format, vk_depth_tiling,
            context->m_SwapChain->m_ImageExtent.width,
            context->m_SwapChain->m_ImageExtent.height,
            context->m_SwapChain->m_SampleCountFlag,
            GetDefaultDepthAndStencilAspectFlags(vk_depth_format),
            depth_stencil_texture);
        CHECK_VK_ERROR(res);

        // Create main render pass with two attachments
        RenderPassAttachment  attachments[3];
        RenderPassAttachment* attachment_resolve = 0;
        attachments[0].m_Format             = context->m_SwapChain->m_SurfaceFormat.format;
        attachments[0].m_ImageLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[0].m_ImageLayoutInitial = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].m_LoadOp             = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].m_StoreOp            = VK_ATTACHMENT_STORE_OP_STORE;

        if (context->m_SwapChain->HasMultiSampling())
        {
            attachments[0].m_LoadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[0].m_ImageLayoutInitial = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[0].m_ImageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        attachments[1].m_Format      = depth_stencil_texture->m_Format;
        attachments[1].m_ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[1].m_LoadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].m_StoreOp     = VK_ATTACHMENT_STORE_OP_STORE;

        // Set third attachment as framebuffer resolve attachment
        if (context->m_SwapChain->HasMultiSampling())
        {
            attachments[2].m_Format             = context->m_SwapChain->m_SurfaceFormat.format;
            attachments[2].m_ImageLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            attachments[2].m_ImageLayoutInitial = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[2].m_LoadOp             = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachments[2].m_StoreOp            = VK_ATTACHMENT_STORE_OP_STORE;

            attachment_resolve = &attachments[2];
        }

        res = CreateRenderPass(vk_device, context->m_SwapChain->m_SampleCountFlag, attachments, 1, &attachments[1], attachment_resolve, &context->m_MainRenderPass);
        CHECK_VK_ERROR(res);

        res = CreateMainFrameBuffers(context);
        CHECK_VK_ERROR(res);

        res = SetupMainRenderTarget(context);
        CHECK_VK_ERROR(res);
        context->m_CurrentRenderTarget = context->m_MainRenderTarget;

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
        res = CreateMainFrameSyncObjects(vk_device, DM_MAX_FRAMES_IN_FLIGHT, context->m_FrameResources);
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
        default_texture_creation_params.m_Depth          = 1;
        default_texture_creation_params.m_OriginalWidth  = default_texture_creation_params.m_Width;
        default_texture_creation_params.m_OriginalHeight = default_texture_creation_params.m_Height;

        const uint8_t default_texture_data[4 * 6] = {}; // RGBA * 6 (for cubemap)

        TextureParams default_texture_params;
        default_texture_params.m_Width  = 1;
        default_texture_params.m_Height = 1;
        default_texture_params.m_Depth  = 1;
        default_texture_params.m_Data   = default_texture_data;
        default_texture_params.m_Format = TEXTURE_FORMAT_RGBA;

        context->m_DefaultTexture2D = VulkanNewTextureInternal(default_texture_creation_params);
        VulkanSetTextureInternal(context->m_DefaultTexture2D, default_texture_params);

        default_texture_params.m_Format = TEXTURE_FORMAT_RGBA32UI;
        context->m_DefaultTexture2D32UI = VulkanNewTextureInternal(default_texture_creation_params);
        VulkanSetTextureInternal(context->m_DefaultTexture2D32UI, default_texture_params);

        default_texture_params.m_Format                 = TEXTURE_FORMAT_RGBA;
        default_texture_creation_params.m_Depth         = 1;
        default_texture_creation_params.m_Type          = TEXTURE_TYPE_IMAGE_2D;
        default_texture_creation_params.m_UsageHintBits = TEXTURE_USAGE_FLAG_STORAGE | TEXTURE_USAGE_FLAG_SAMPLE;
        context->m_DefaultStorageImage2D                = VulkanNewTextureInternal(default_texture_creation_params);
        VulkanSetTextureInternal(context->m_DefaultStorageImage2D, default_texture_params);

        default_texture_creation_params.m_UsageHintBits = TEXTURE_USAGE_FLAG_SAMPLE;
        default_texture_creation_params.m_Type          = TEXTURE_TYPE_2D_ARRAY;
        default_texture_creation_params.m_Depth         = 1;
        context->m_DefaultTexture2DArray                = VulkanNewTextureInternal(default_texture_creation_params);

        default_texture_creation_params.m_Type  = TEXTURE_TYPE_CUBE_MAP;
        default_texture_creation_params.m_Depth = 6;
        context->m_DefaultTextureCubeMap = VulkanNewTextureInternal(default_texture_creation_params);

        memset(context->m_TextureUnits, 0x0, sizeof(context->m_TextureUnits));
        context->m_CurrentSwapchainTexture  = StoreAssetInContainer(context->m_AssetHandleContainer, VulkanNewTextureInternal({}), ASSET_TYPE_TEXTURE);

        return res;
    }

    void SwapChainChanged(VulkanContext* context, uint32_t* width, uint32_t* height, VkResult (*cb)(void* ctx), void* cb_ctx)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;

        if (vk_device == VK_NULL_HANDLE)
        {
            return;
        }

        // Flush all current commands
        SynchronizeDevice(vk_device);

        DestroyMainFrameBuffers(context);

        // Destroy main Depth/Stencil buffer
        VulkanTexture* depth_stencil_texture = &context->m_MainTextureDepthStencil;
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

        const bool want_vsync = context->m_SwapInterval != 0;

        // Create the swap chain
        VkResult res = UpdateSwapChain(&context->m_PhysicalDevice, &context->m_LogicalDevice, width, height, want_vsync, context->m_SwapChainCapabilities, context->m_SwapChain);
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
            GetDefaultDepthAndStencilAspectFlags(vk_depth_format),
            depth_stencil_texture);
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
                    case RESOURCE_TYPE_PROGRAM:
                        DestroyProgram(vk_device, &resource.m_Program);
                        break;
                    case RESOURCE_TYPE_RENDER_TARGET:
                        DestroyRenderTarget(vk_device, &resource.m_RenderTarget);
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
        return IsDeviceExtensionSupported(&((VulkanContext*) context)->m_PhysicalDevice, ext_name);
    }

    static uint32_t VulkanGetNumSupportedExtensions(HContext context)
    {
        return ((VulkanContext*) context)->m_PhysicalDevice.m_DeviceExtensionCount;
    }

    static const char* VulkanGetSupportedExtension(HContext context, uint32_t index)
    {
        return ((VulkanContext*) context)->m_PhysicalDevice.m_DeviceExtensions[index].extensionName;
    }

    static PipelineState VulkanGetPipelineState(HContext context)
    {
        return ((VulkanContext*) context)->m_PipelineState;
    }

    static void SetupSupportedTextureFormats(VulkanContext* context)
    {
    #if defined(__MACH__)
        // Check for optional extensions so that we can enable them if they exist
        if (VulkanIsExtensionSupported((HContext) context, VK_IMG_FORMAT_PVRTC_EXTENSION_NAME))
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

                                            // Float formats
                                            TEXTURE_FORMAT_RGB16F,
                                            TEXTURE_FORMAT_RGB32F,
                                            TEXTURE_FORMAT_RGBA16F,
                                            TEXTURE_FORMAT_RGBA32F,
                                            TEXTURE_FORMAT_R16F,
                                            TEXTURE_FORMAT_RG16F,
                                            TEXTURE_FORMAT_R32F,
                                            TEXTURE_FORMAT_RG32F,

                                            // Misc formats
                                            TEXTURE_FORMAT_RGBA32UI,
                                            TEXTURE_FORMAT_R32UI,

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

    bool InitializeVulkan(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        VkResult res = CreateWindowSurface(context->m_Window, context->m_Instance, &context->m_WindowSurface, dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_HIGH_DPI));
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

        context->m_FragmentShaderInterlockFeatures       = {};
        context->m_FragmentShaderInterlockFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;

        PhysicalDevice* device_list     = new PhysicalDevice[device_count];
        PhysicalDevice* selected_device = NULL;
        GetPhysicalDevices(context->m_Instance, &device_list, device_count, &context->m_FragmentShaderInterlockFeatures);

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
                dmLogError("Device selection failed for device %s (%d/%d): Could not get a valid queue family.", device->m_Properties.deviceName, i, device_count);
                DESTROY_AND_CONTINUE(device)
            }

            // Make sure all device extensions are supported
            bool all_extensions_found = true;
            for (uint32_t ext_i = 0; ext_i < device_extensions.Size(); ++ext_i)
            {
                if (!IsDeviceExtensionSupported(device, device_extensions[ext_i]))
                {
                    dmLogError("Required device extension '%s' is missing for device %s (%d/%d).", device_extensions[ext_i], device->m_Properties.deviceName, i, device_count);
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
        uint32_t created_width  = context->m_Width;
        uint32_t created_height = context->m_Height;
        const bool want_vsync   = context->m_SwapInterval != 0;
        VkSampleCountFlagBits vk_closest_multisample_flag;

        void* device_pNext_chain = 0;

        uint16_t validation_layers_count;
        const char** validation_layers = GetValidationLayers(&validation_layers_count, context->m_UseValidationLayers, context->m_RenderDocSupport);

        if (selected_device == NULL)
        {
            dmLogError("Could not select a suitable Vulkan device.");
            goto bail;
        }

        context->m_PhysicalDevice = *selected_device;

        SetupSupportedTextureFormats(context);

    #if defined(__MACH__)
        // Check for optional extensions so that we can enable them if they exist
        if (VulkanIsExtensionSupported((HContext) context, VK_IMG_FORMAT_PVRTC_EXTENSION_NAME))
        {
            device_extensions.OffsetCapacity(1);
            device_extensions.Push(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME);
        }

        #ifdef DM_VULKAN_VALIDATION
        if (context->m_UseValidationLayers)
        {
            device_extensions.OffsetCapacity(1);
            device_extensions.Push("VK_KHR_portability_subset");
        }
        #endif
    #endif

        if (VulkanIsExtensionSupported((HContext) context, VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME))
        {
            device_extensions.OffsetCapacity(1);
            device_extensions.Push(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
            device_pNext_chain = &context->m_FragmentShaderInterlockFeatures;
        }

        res = CreateLogicalDevice(selected_device, context->m_WindowSurface, selected_queue_family,
            device_extensions.Begin(), (uint8_t)device_extensions.Size(),
            validation_layers, (uint8_t)validation_layers_count, device_pNext_chain, &logical_device);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a logical Vulkan device, reason: %s", VkResultToStr(res));
            goto bail;
        }

        context->m_LogicalDevice    = logical_device;
        vk_closest_multisample_flag = GetClosestSampleCountFlag(selected_device, BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT, dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_SAMPLE_COUNT));

        // Create swap chain
        InitializeVulkanTexture(&context->m_ResolveTexture);
        context->m_SwapChainCapabilities.Swap(selected_swap_chain_capabilities);
        context->m_SwapChain = new SwapChain(context->m_WindowSurface, vk_closest_multisample_flag, context->m_SwapChainCapabilities, selected_queue_family, &context->m_ResolveTexture);

        res = UpdateSwapChain(&context->m_PhysicalDevice, &context->m_LogicalDevice, &created_width, &created_height, want_vsync, context->m_SwapChainCapabilities, context->m_SwapChain);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a swap chain for Vulkan, reason: %s", VkResultToStr(res));
            goto bail;
        }

        delete[] device_list;

        // GLFW3 handles window size changes differently, so we need to cater for that.
    #ifndef __MACH__
        if (created_width != context->m_Width || created_height != context->m_Height)
        {
            dmPlatform::SetWindowSize(context->m_Window, created_width, created_height);
        }
    #endif

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
#if defined(DM_PLATFORM_VENDOR)
        // If we are on a private platform, and this driver is registered, then it's already supported.
        // We do this skip to avoid calling NativeInit(), or creating an extra api to support creating a vendor specific device twice with
        // memory etc.
        // It might make more sense if this function was queried with some actual parameters, but as it is now
        // it is merely a question of "is Vulkan supported at all on this platform" /MAWE
        return true;
#else

    #if ANDROID
        if (!LoadVulkanLibrary())
        {
            dmLogError("Could not load Vulkan functions.");
            return 0x0;
        }
    #endif


        // GLFW 3.4 doesn't support static linking with vulkan,
        // instead we need to pass in the function that loads symbol for any
        // platform that is using GLFW.
    #if defined(__MACH__) && !defined(DM_PLATFORM_IOS)
        dmPlatform::VulkanSetLoader();
    #endif

        uint16_t extensionNameCount = 0;
        const char** extensionNames = 0;

    #ifdef DM_VULKAN_VALIDATION
        const char* portabilityExt = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        extensionNames             = &portabilityExt;
        extensionNameCount         = 1;
    #endif

        VkInstance inst;
        VkResult res = CreateInstance(&inst, extensionNames, extensionNameCount, 0, 0, 0, 0);

        if (res == VK_SUCCESS)
        {
        #if ANDROID
            LoadVulkanFunctions(inst);
        #endif
            DestroyInstance(&inst);
        }

        return res == VK_SUCCESS;
#endif
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

            g_VulkanContext = new VulkanContext(params, vk_instance);

            if (NativeInitializeContext(g_VulkanContext))
            {
                return (HContext) g_VulkanContext;
            }
        }
        return 0x0;
    }

    static void VulkanDeleteContext(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
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

    static void VulkanFinalize()
    {
        NativeExit();
    }

    static void VulkanGetDefaultTextureFilters(HContext _context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        VulkanContext* context = (VulkanContext*) _context;
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    static void VulkanBeginFrame(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
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
                context->m_WindowWidth  = dmPlatform::GetWindowWidth(context->m_Window);
                context->m_WindowHeight = dmPlatform::GetWindowHeight(context->m_Window);
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

        RenderTarget* rt           = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_MainRenderTarget);
        rt->m_Handle.m_Framebuffer = context->m_MainFrameBuffers[frame_ix];

        context->m_FrameBegun      = 1;
        context->m_CurrentPipeline = 0;

        VulkanTexture* tex_sc = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, context->m_CurrentSwapchainTexture);
        assert(tex_sc);

        memset(tex_sc, 0, sizeof(VulkanTexture));

        tex_sc->m_Handle.m_Image     = context->m_SwapChain->Image();
        tex_sc->m_Handle.m_ImageView = context->m_SwapChain->ImageView();
        tex_sc->m_Width              = context->m_SwapChain->m_ImageExtent.width;
        tex_sc->m_Height             = context->m_SwapChain->m_ImageExtent.height;
        tex_sc->m_Format             = context->m_SwapChain->m_SurfaceFormat.format;
        tex_sc->m_OriginalWidth      = tex_sc->m_Width;
        tex_sc->m_OriginalHeight     = tex_sc->m_Height;
        tex_sc->m_Type               = TEXTURE_TYPE_2D;
        tex_sc->m_GraphicsFormat     = TEXTURE_FORMAT_BGRA8U;
    }

    static void VulkanFlip(HContext _context)
    {
        DM_PROFILE(__FUNCTION__);
        VulkanContext* context = (VulkanContext*) _context;
        uint32_t frame_ix = context->m_SwapChain->m_ImageIndex;
        FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameInFlight];

        EndRenderPass(context);

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

        // This is a fix / workaround for android where the swap chain capabilities can have a presentation transform.
        // For now we skip the transform completely by setting the currentTransform = identity,
        // but that causes the presentation function to return a suboptimal result, which we don't care about right now.
        // A more "proper" way of doing this would be to actually use the preTransform values, but I don't know how it works.
        res = vkQueuePresentKHR(context->m_LogicalDevice.m_PresentQueue, &vk_present_info);
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
        {
            CHECK_VK_ERROR(res);
        }

        // Advance frame index
        context->m_CurrentFrameInFlight = (context->m_CurrentFrameInFlight + 1) % context->m_NumFramesInFlight;
        context->m_FrameBegun           = 0;

#if defined(ANDROID) || defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_VENDOR)
        dmPlatform::SwapBuffers(context->m_Window);
#endif
    }

    static void VulkanClear(HContext _context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        DM_PROFILE(__FUNCTION__);

        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_CurrentRenderTarget);

        RenderTarget* current_rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);

        BeginRenderPass(context, context->m_CurrentRenderTarget);

        uint32_t attachment_count = 0;
        VkClearAttachment vk_clear_attachments[MAX_BUFFER_COLOR_ATTACHMENTS + 1];
        memset(vk_clear_attachments, 0, sizeof(vk_clear_attachments));

        VkClearRect vk_clear_rect;
        vk_clear_rect.rect.offset.x      = 0;
        vk_clear_rect.rect.offset.y      = 0;
        vk_clear_rect.rect.extent.width  = current_rt->m_Extent.width;
        vk_clear_rect.rect.extent.height = current_rt->m_Extent.height;
        vk_clear_rect.baseArrayLayer     = 0;
        vk_clear_rect.layerCount         = 1;

        bool has_depth_stencil_texture = current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID || current_rt->m_TextureDepthStencil;
        bool clear_depth_stencil       = flags & (BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT);

        float r = ((float)red)/255.0f;
        float g = ((float)green)/255.0f;
        float b = ((float)blue)/255.0f;
        float a = ((float)alpha)/255.0f;

        const BufferType color_buffers[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        for (int i = 0; i < current_rt->m_ColorAttachmentCount; ++i)
        {
            if (flags & color_buffers[i])
            {
                VkClearAttachment& vk_color_attachment          = vk_clear_attachments[attachment_count++];
                vk_color_attachment.aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
                vk_color_attachment.colorAttachment             = i;
                vk_color_attachment.clearValue.color.float32[0] = r;
                vk_color_attachment.clearValue.color.float32[1] = g;
                vk_color_attachment.clearValue.color.float32[2] = b;
                vk_color_attachment.clearValue.color.float32[3] = a;
            }
        }

        // Clear depth / stencil
        if (has_depth_stencil_texture && clear_depth_stencil)
        {
            VkImageAspectFlags vk_aspect = 0;
            if (flags & BUFFER_TYPE_DEPTH_BIT)
            {
                vk_aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
            }

            if (flags & BUFFER_TYPE_STENCIL_BIT)
            {
                vk_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            VkClearAttachment& vk_depth_attachment              = vk_clear_attachments[attachment_count++];
            vk_depth_attachment.aspectMask                      = vk_aspect;
            vk_depth_attachment.clearValue.depthStencil.stencil = stencil;
            vk_depth_attachment.clearValue.depthStencil.depth   = depth;
        }

        vkCmdClearAttachments(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
            attachment_count, vk_clear_attachments, 1, &vk_clear_rect);

    }

    static void DeviceBufferUploadHelper(VulkanContext* context, const void* data, uint32_t size, uint32_t offset, DeviceBuffer* bufferOut)
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
        if (resource == 0x0 || resource->m_Destroyed)
        {
            return;
        }

        ResourceToDestroy resource_to_destroy;
        resource_to_destroy.m_ResourceType = resource->GetType();

        switch(resource_to_destroy.m_ResourceType)
        {
            case RESOURCE_TYPE_TEXTURE:
                resource_to_destroy.m_Texture = ((VulkanTexture*) resource)->m_Handle;
                DestroyResourceDeferred(resource_list, &((VulkanTexture*) resource)->m_DeviceBuffer);
                break;
            case RESOURCE_TYPE_DEVICE_BUFFER:
                resource_to_destroy.m_DeviceBuffer = ((DeviceBuffer*) resource)->m_Handle;
                ((DeviceBuffer*) resource)->UnmapMemory(g_VulkanContext->m_LogicalDevice.m_Device);
                break;
            case RESOURCE_TYPE_PROGRAM:
                resource_to_destroy.m_Program = ((Program*) resource)->m_Handle;
                break;
            case RESOURCE_TYPE_RENDER_TARGET:
                resource_to_destroy.m_RenderTarget = ((RenderTarget*) resource)->m_Handle;
                break;
            default:
                assert(0);
                break;
        }

        if (resource_list->Full())
        {
            resource_list->OffsetCapacity(8);
        }

        resource_list->Push(resource_to_destroy);
        resource->m_Destroyed = 1;
    }

    static Pipeline* GetOrCreateComputePipeline(VkDevice vk_device, PipelineCache& pipelineCache, Program* program)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &program->m_Hash, sizeof(program->m_Hash));

        uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);
        Pipeline* cached_pipeline = pipelineCache.Get(pipeline_hash);

        if (!cached_pipeline)
        {
            Pipeline new_pipeline = {};

            VkResult res = CreateComputePipeline(vk_device, program, &new_pipeline);
            CHECK_VK_ERROR(res);

            if (pipelineCache.Full())
            {
                pipelineCache.SetCapacity(32, pipelineCache.Capacity() + 4);
            }

            pipelineCache.Put(pipeline_hash, new_pipeline);
            cached_pipeline = pipelineCache.Get(pipeline_hash);

            dmLogDebug("Created new VK Compute Pipeline with hash %llu", (unsigned long long) pipeline_hash);
        }

        return cached_pipeline;
    }

    static Pipeline* GetOrCreatePipeline(VkDevice vk_device, VkSampleCountFlagBits vk_sample_count,
        const PipelineState pipelineState, PipelineCache& pipelineCache,
        Program* program, RenderTarget* rt, VertexDeclaration** vertexDeclaration, uint32_t vertexDeclarationCount)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &program->m_Hash, sizeof(program->m_Hash));
        dmHashUpdateBuffer64(&pipeline_hash_state, &pipelineState, sizeof(pipelineState));
        dmHashUpdateBuffer64(&pipeline_hash_state, &rt->m_Id, sizeof(rt->m_Id));
        dmHashUpdateBuffer64(&pipeline_hash_state, &vk_sample_count, sizeof(vk_sample_count));

        for (int i = 0; i < vertexDeclarationCount; ++i)
        {
            dmHashUpdateBuffer64(&pipeline_hash_state, &vertexDeclaration[i]->m_PipelineHash, sizeof(vertexDeclaration[i]->m_PipelineHash));
            dmHashUpdateBuffer64(&pipeline_hash_state, &vertexDeclaration[i]->m_StepFunction, sizeof(vertexDeclaration[i]->m_StepFunction));
        }

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

            VkResult res = CreateGraphicsPipeline(vk_device, vk_scissor, vk_sample_count, pipelineState, program, vertexDeclaration, vertexDeclarationCount, rt, &new_pipeline);
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

    static void SetDeviceBuffer(VulkanContext* context, DeviceBuffer* buffer, uint32_t size, const void* data)
    {
        if (size == 0)
        {
            return;
        }

        // Coherent memory writes does not seem to be properly synced on MoltenVK,
        // so for now we always mark the old buffer for destruction when updating the data.
    #ifndef __MACH__
        if (size != buffer->m_MemorySize)
    #endif
        {
            DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], buffer);
        }

        DeviceBufferUploadHelper(g_VulkanContext, data, size, 0, buffer);
    }

    static HVertexBuffer VulkanNewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (buffer_usage & BUFFER_USAGE_TRANSFER)
        {
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        DeviceBuffer* buffer = new DeviceBuffer(usage_flags);

        if (size > 0)
        {
            DeviceBufferUploadHelper(g_VulkanContext, data, size, 0, buffer);
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
        SetDeviceBuffer(g_VulkanContext, (DeviceBuffer*) buffer, size, data);
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
        return ((VulkanContext*) context)->m_PhysicalDevice.m_Properties.limits.maxDrawIndexedIndexValue;
    }

    // NOTE: This function doesn't seem to be used anywhere?
    static uint32_t VulkanGetMaxElementsIndices(HContext context)
    {
        return -1;
    }

    static HIndexBuffer VulkanNewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (buffer_usage & BUFFER_USAGE_TRANSFER)
        {
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        DeviceBuffer* buffer = new DeviceBuffer(usage_flags);

        if (size > 0)
        {
            DeviceBufferUploadHelper((VulkanContext*) context, data, size, 0, buffer);
        }

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
        SetDeviceBuffer(g_VulkanContext, (DeviceBuffer*) buffer, size, data);
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

    static VertexDeclaration* CreateAndFillVertexDeclaration(HashState64* hash, HVertexStreamDeclaration stream_declaration)
    {
        VertexDeclaration* vd = new VertexDeclaration();
        memset(vd, 0, sizeof(VertexDeclaration));

        vd->m_StreamCount = stream_declaration->m_StreamCount;

        for (uint32_t i = 0; i < stream_declaration->m_StreamCount; ++i)
        {
            VertexStream& stream = stream_declaration->m_Streams[i];

        #if __MACH__
            if (stream.m_Type == TYPE_UNSIGNED_BYTE && !stream.m_Normalize)
            {
                dmLogWarning("Using the type '%s' for stream '%s' with normalize: false is not supported for vertex declarations. Defaulting to TYPE_BYTE.", GetGraphicsTypeLiteral(stream.m_Type), dmHashReverseSafe64(stream.m_NameHash));
                stream.m_Type = TYPE_BYTE;
            }
            else if (stream.m_Type == TYPE_UNSIGNED_SHORT && !stream.m_Normalize)
            {
                dmLogWarning("Using the type '%s' for stream '%s' with normalize: false is not supported for vertex declarations. Defaulting to TYPE_SHORT.", GetGraphicsTypeLiteral(stream.m_Type), dmHashReverseSafe64(stream.m_NameHash));
                stream.m_Type = TYPE_SHORT;
            }
        #else

            // JG: Not sure this is what we want to do, OpenGL performs automatic conversion to float regardless of type, but Vulkan doesn't seem to do that
            //     unless we use the normalized format variants of these types.. which in turn means that we will have different looks between the adapters if we use the narrower formats
            //     Alternatively, we could force OpenGL to behave like vulkan?
            if ((stream.m_Type == TYPE_BYTE           ||
                 stream.m_Type == TYPE_UNSIGNED_BYTE  ||
                 stream.m_Type == TYPE_SHORT          ||
                 stream.m_Type == TYPE_UNSIGNED_SHORT) && !stream.m_Normalize)
            {
                dmLogWarning("Using the type '%s' for stream '%s' with normalize: false is not supported for vertex declarations. Defaulting to normalize:true.", GetGraphicsTypeLiteral(stream.m_Type), dmHashReverseSafe64(stream.m_NameHash));
                stream.m_Normalize = 1;
            }
        #endif

            vd->m_Streams[i].m_NameHash  = stream.m_NameHash;
            vd->m_Streams[i].m_Type      = stream.m_Type;
            vd->m_Streams[i].m_Size      = stream.m_Size;
            vd->m_Streams[i].m_Normalize = stream.m_Normalize;
            vd->m_Streams[i].m_Offset    = vd->m_Stride;
            vd->m_Streams[i].m_Location  = -1;
            vd->m_Stride                += stream.m_Size * GetGraphicsTypeSize(stream.m_Type);

            dmHashUpdateBuffer64(hash, &stream.m_Size, sizeof(stream.m_Size));
            dmHashUpdateBuffer64(hash, &stream.m_Type, sizeof(stream.m_Type));
            dmHashUpdateBuffer64(hash, &vd->m_Streams[i].m_Type, sizeof(vd->m_Streams[i].m_Type));
        }

        vd->m_Stride = DM_ALIGN(vd->m_Stride, 4);

        return vd;
    }

    static HVertexDeclaration VulkanNewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, stream_declaration);
        dmHashUpdateBuffer64(&decl_hash_state, &vd->m_Stride, sizeof(vd->m_Stride));
        vd->m_PipelineHash = dmHashFinal64(&decl_hash_state);
        vd->m_StepFunction = VERTEX_STEP_FUNCTION_VERTEX;
        return vd;
    }

    static HVertexDeclaration VulkanNewVertexDeclarationStride(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, stream_declaration);
        dmHashUpdateBuffer64(&decl_hash_state, &stride, sizeof(stride));
        vd->m_Stride       = stride;
        vd->m_PipelineHash = dmHashFinal64(&decl_hash_state);
        vd->m_StepFunction = VERTEX_STEP_FUNCTION_VERTEX;
        return vd;
    }

    static void VulkanEnableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer, uint32_t binding_index)
    {
        VulkanContext* context                        = (VulkanContext*) _context;
        context->m_CurrentVertexBuffer[binding_index] = (DeviceBuffer*) vertex_buffer;
    }

    static void VulkanDisableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer)
    {
        VulkanContext* context = (VulkanContext*) _context;
        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexBuffer[i] == (DeviceBuffer*) vertex_buffer)
                context->m_CurrentVertexBuffer[i] = 0;
        }
    }

    static void VulkanEnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index, HProgram program)
    {
        VulkanContext* context      = (VulkanContext*) _context;
        Program* program_ptr        = (Program*) program;
        ShaderModule* vertex_shader = program_ptr->m_VertexModule;

        context->m_MainVertexDeclaration[binding_index]                = {};
        context->m_MainVertexDeclaration[binding_index].m_Stride       = vertex_declaration->m_Stride;
        context->m_MainVertexDeclaration[binding_index].m_StepFunction = vertex_declaration->m_StepFunction;
        context->m_MainVertexDeclaration[binding_index].m_PipelineHash = vertex_declaration->m_PipelineHash;

        context->m_CurrentVertexDeclaration[binding_index]             = &context->m_MainVertexDeclaration[binding_index];

        uint32_t stream_ix = 0;
        uint32_t num_inputs = vertex_shader->m_ShaderMeta.m_Inputs.Size();

        for (int i = 0; i < vertex_declaration->m_StreamCount; ++i)
        {
            for (int j = 0; j < num_inputs; ++j)
            {
                ShaderResourceBinding& input = vertex_shader->m_ShaderMeta.m_Inputs[j];

                if (input.m_NameHash == vertex_declaration->m_Streams[i].m_NameHash)
                {
                    VertexDeclaration::Stream& stream = context->m_MainVertexDeclaration[binding_index].m_Streams[stream_ix];
                    stream.m_NameHash  = input.m_NameHash;
                    stream.m_Location  = input.m_Binding;
                    stream.m_Type      = vertex_declaration->m_Streams[i].m_Type;
                    stream.m_Offset    = vertex_declaration->m_Streams[i].m_Offset;
                    stream.m_Size      = vertex_declaration->m_Streams[i].m_Size;
                    stream.m_Normalize = vertex_declaration->m_Streams[i].m_Normalize;
                    stream_ix++;

                    context->m_MainVertexDeclaration[binding_index].m_StreamCount++;
                    break;
                }
            }
        }
    }

    static void VulkanDisableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration)
    {
        VulkanContext* context = (VulkanContext*) _context;
        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexDeclaration[i] == vertex_declaration)
                context->m_CurrentVertexDeclaration[i] = 0;
        }
    }

    static inline VulkanTexture* GetDefaultTexture(VulkanContext* context, ShaderDesc::ShaderDataType type)
    {
        switch(type)
        {
            case ShaderDesc::SHADER_TYPE_RENDER_PASS_INPUT:
            case ShaderDesc::SHADER_TYPE_TEXTURE2D:
            case ShaderDesc::SHADER_TYPE_SAMPLER:
            case ShaderDesc::SHADER_TYPE_SAMPLER2D:       return context->m_DefaultTexture2D;
            case ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY: return context->m_DefaultTexture2DArray;
            case ShaderDesc::SHADER_TYPE_SAMPLER_CUBE:    return context->m_DefaultTextureCubeMap;
            case ShaderDesc::SHADER_TYPE_UTEXTURE2D:      return context->m_DefaultTexture2D32UI;
            case ShaderDesc::SHADER_TYPE_IMAGE2D:
            case ShaderDesc::SHADER_TYPE_UIMAGE2D:        return context->m_DefaultStorageImage2D;
            default:break;
        }
        return 0x0;
    }

    static void UpdateImageDescriptor(VulkanContext* context, HTexture texture_handle, ShaderResourceBinding* binding, VkDescriptorImageInfo& vk_image_info, VkWriteDescriptorSet& vk_write_desc_info)
    {
        VulkanTexture* texture  = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture_handle);

        if (texture == 0x0)
        {
            texture = GetDefaultTexture(context, binding->m_Type.m_ShaderType);
        }

        VkImageLayout image_layout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkSampler image_sampler          = context->m_TextureSamplers[texture->m_TextureSamplerIndex].m_Sampler;
        VkImageView image_view           = texture->m_Handle.m_ImageView;
        VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        if (binding->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_RENDER_PASS_INPUT)
        {
            image_sampler   = 0;
            descriptor_type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        }
        else if (binding->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_IMAGE2D || binding->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_UIMAGE2D)
        {
            image_layout    = VK_IMAGE_LAYOUT_GENERAL;
            image_sampler   = 0;
            descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        else if (binding->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
        {
            descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER;
            image_layout    = VK_IMAGE_LAYOUT_UNDEFINED;
            image_view      = VK_NULL_HANDLE;
        }

        // If the image layout is in the wrong state, we need to transition it to the new layout,
        // otherwise its memory might be getting wrriten to while we are reading from it.
        if (texture->m_ImageLayout[0] != VK_IMAGE_LAYOUT_GENERAL && image_layout != texture->m_ImageLayout[0])
        {
            VkResult res = TransitionImageLayout(context->m_LogicalDevice.m_Device,
                context->m_LogicalDevice.m_CommandPool,
                context->m_LogicalDevice.m_GraphicsQueue,
                texture,
                VK_IMAGE_ASPECT_COLOR_BIT,
                image_layout);
            CHECK_VK_ERROR(res);
        }

        vk_image_info.sampler     = image_sampler;
        vk_image_info.imageView   = image_view;
        vk_image_info.imageLayout = texture->m_ImageLayout[0];

        vk_write_desc_info.descriptorType = descriptor_type;
        vk_write_desc_info.pImageInfo     = &vk_image_info;
    }

    static void UpdateUniformBufferDescriptor(VulkanContext* context, VkBuffer vk_buffer, VkDescriptorType descriptor_type, VkDescriptorBufferInfo& vk_buffer_info, VkWriteDescriptorSet& vk_write_desc_info, size_t offset, size_t buffer_size)
    {
        // Note in the spec about the offset being zero:
        //   "For VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC and VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC descriptor types,
        //    offset is the base offset from which the dynamic offset is applied and range is the static size
        //    used for all dynamic offsets."
        vk_buffer_info.buffer = vk_buffer;
        vk_buffer_info.offset = offset;
        vk_buffer_info.range  = buffer_size;

        vk_write_desc_info.descriptorType = descriptor_type;
        vk_write_desc_info.pBufferInfo    = &vk_buffer_info;
    }

    static void UpdateDescriptorSets(VulkanContext* context, VkDevice vk_device, VkDescriptorSet* vk_descriptor_sets, Program* program, ScratchBuffer* scratch_buffer, uint32_t* dynamic_offsets, uint32_t dynamic_alignment)
    {
        const uint32_t max_write_descriptors = MAX_SET_COUNT * MAX_BINDINGS_PER_SET_COUNT;
        VkWriteDescriptorSet vk_write_descriptors[max_write_descriptors];
        VkDescriptorImageInfo vk_write_image_descriptors[max_write_descriptors];
        VkDescriptorBufferInfo vk_write_buffer_descriptors[max_write_descriptors];

        uint16_t uniform_to_write_index = 0;
        uint16_t image_to_write_index   = 0;
        uint16_t buffer_to_write_index  = 0;

        for (int set = 0; set < program->m_MaxSet; ++set)
        {
            for (int binding = 0; binding < program->m_MaxBinding; ++binding)
            {
                ProgramResourceBinding& pgm_res = program->m_ResourceBindings[set][binding];

                if (pgm_res.m_Res == 0x0)
                    continue;

                VkWriteDescriptorSet& vk_write_desc_info = vk_write_descriptors[uniform_to_write_index++];
                vk_write_desc_info.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                vk_write_desc_info.pNext                 = 0;
                vk_write_desc_info.dstSet                = vk_descriptor_sets[set];
                vk_write_desc_info.dstBinding            = binding;
                vk_write_desc_info.dstArrayElement       = 0;
                vk_write_desc_info.descriptorCount       = 1;
                vk_write_desc_info.pImageInfo            = 0;
                vk_write_desc_info.pBufferInfo           = 0;
                vk_write_desc_info.pTexelBufferView      = 0;

                switch(pgm_res.m_Res->m_BindingFamily)
                {
                    case ShaderResourceBinding::BINDING_FAMILY_TEXTURE:
                        UpdateImageDescriptor(context,
                            context->m_TextureUnits[pgm_res.m_TextureUnit],
                            pgm_res.m_Res,
                            vk_write_image_descriptors[image_to_write_index++],
                            vk_write_desc_info);
                        break;
                    case ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER:
                    {
                        const StorageBufferBinding binding = context->m_CurrentStorageBuffers[pgm_res.m_StorageBufferUnit];
                        UpdateUniformBufferDescriptor(context,
                            ((DeviceBuffer*) binding.m_Buffer)->m_Handle.m_Buffer,
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            vk_write_buffer_descriptors[buffer_to_write_index++],
                            vk_write_desc_info,
                            binding.m_BufferOffset,
                            VK_WHOLE_SIZE);
                    } break;
                    case ShaderResourceBinding::BINDING_FAMILY_UNIFORM_BUFFER:
                    {
                        dynamic_offsets[pgm_res.m_DynamicOffsetIndex] = (uint32_t) scratch_buffer->m_MappedDataCursor;
                        const uint32_t uniform_size_nonalign          = pgm_res.m_Res->m_BlockSize;
                        const uint32_t uniform_size_align             = DM_ALIGN(uniform_size_nonalign, dynamic_alignment);

                        assert(uniform_size_nonalign > 0);

                        // Copy client data to aligned host memory
                        // The data_offset here is the offset into the programs uniform data,
                        // i.e the source buffer.
                        memcpy(&((uint8_t*)scratch_buffer->m_DeviceBuffer.m_MappedDataPtr)[scratch_buffer->m_MappedDataCursor],
                            &program->m_UniformData[pgm_res.m_DataOffset], uniform_size_nonalign);

                        UpdateUniformBufferDescriptor(context,
                            scratch_buffer->m_DeviceBuffer.m_Handle.m_Buffer,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                            vk_write_buffer_descriptors[buffer_to_write_index++],
                            vk_write_desc_info,
                            0,
                            uniform_size_align);

                        scratch_buffer->m_MappedDataCursor += uniform_size_align;
                    } break;
                    case ShaderResourceBinding::BINDING_FAMILY_GENERIC:
                    default: continue;
                }
            }
        }

        vkUpdateDescriptorSets(vk_device, uniform_to_write_index, vk_write_descriptors, 0, 0);
    }

    static VkResult CommitUniforms(VulkanContext* context, VkCommandBuffer vk_command_buffer, VkDevice vk_device,
        Program* program_ptr, VkPipelineBindPoint bind_point,
        ScratchBuffer* scratch_buffer, uint32_t* dynamic_offsets, const uint32_t alignment)
    {
        const uint32_t num_descriptors     = program_ptr->m_TotalResourcesCount;
        const uint32_t num_dynamic_offsets = program_ptr->m_UniformBufferCount;

        if (num_descriptors == 0)
        {
            return VK_SUCCESS;
        }

        VkDescriptorSet* vk_descriptor_set_list = 0x0;
        VkResult res = scratch_buffer->m_DescriptorAllocator->Allocate(vk_device, program_ptr->m_Handle.m_DescriptorSetLayouts, program_ptr->m_Handle.m_DescriptorSetLayoutsCount, num_descriptors, &vk_descriptor_set_list);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        UpdateDescriptorSets(context, vk_device, vk_descriptor_set_list, program_ptr, scratch_buffer, dynamic_offsets, alignment);

        vkCmdBindDescriptorSets(vk_command_buffer,
            bind_point,
            program_ptr->m_Handle.m_PipelineLayout,
            0,
            program_ptr->m_Handle.m_DescriptorSetLayoutsCount,
            vk_descriptor_set_list,
            num_dynamic_offsets,
            dynamic_offsets);

        return VK_SUCCESS;
    }

    static VkResult ResizeScratchBuffer(VulkanContext* context, uint32_t newDataSize, ScratchBuffer* scratchBuffer)
    {
        // Put old buffer on the delete queue so we don't mess the descriptors already in-use
        DestroyResourceDeferred(context->m_MainResourcesToDestroy[context->m_SwapChain->m_ImageIndex], &scratchBuffer->m_DeviceBuffer);

        VkResult res = CreateScratchBuffer(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device,
            newDataSize, false, scratchBuffer->m_DescriptorAllocator, scratchBuffer);
        scratchBuffer->m_DeviceBuffer.MapMemory(context->m_LogicalDevice.m_Device);

        return res;
    }

    static void PrepareScatchBuffer(VulkanContext* context, ScratchBuffer* scratchBuffer, Program* program_ptr)
    {
        // Ensure there is room in the descriptor allocator to support this dispatch call
        const uint32_t num_uniform_buffers = program_ptr->m_UniformBufferCount;
        const bool resize_scratch_buffer   = program_ptr->m_UniformDataSizeAligned > (scratchBuffer->m_DeviceBuffer.m_MemorySize - scratchBuffer->m_MappedDataCursor);

        if (resize_scratch_buffer)
        {
            const uint8_t descriptor_increase = 32;
            const uint32_t bytes_increase = 256 * descriptor_increase;
            VkResult res = ResizeScratchBuffer(context, scratchBuffer->m_DeviceBuffer.m_MemorySize + bytes_increase, scratchBuffer);
            CHECK_VK_ERROR(res);
        }

        // Ensure we have enough room in the dynamic offset buffer to support the uniforms for this dispatch call
        if (context->m_DynamicOffsetBufferSize < num_uniform_buffers)
        {
            const size_t offset_buffer_size = sizeof(uint32_t) * num_uniform_buffers;

            if (context->m_DynamicOffsetBuffer == 0x0)
            {
                context->m_DynamicOffsetBuffer = (uint32_t*) malloc(offset_buffer_size);
            }
            else
            {
                context->m_DynamicOffsetBuffer = (uint32_t*) realloc(context->m_DynamicOffsetBuffer, offset_buffer_size);
            }

            memset(context->m_DynamicOffsetBuffer, 0, offset_buffer_size);

            context->m_DynamicOffsetBufferSize = num_uniform_buffers;
        }
    }

    static void DrawSetupCompute(VulkanContext* context, VkCommandBuffer vk_command_buffer, ScratchBuffer* scratchBuffer)
    {
        VkDevice vk_device   = context->m_LogicalDevice.m_Device;
        Program* program_ptr = context->m_CurrentProgram;
        assert(program_ptr->m_ComputeModule);

        PrepareScatchBuffer(context, scratchBuffer, program_ptr);

        // Write the uniform data to the descriptors
        uint32_t dynamic_alignment = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment;
        VkResult res               = CommitUniforms(context, vk_command_buffer, vk_device, program_ptr, VK_PIPELINE_BIND_POINT_COMPUTE, scratchBuffer, context->m_DynamicOffsetBuffer, dynamic_alignment);
        CHECK_VK_ERROR(res);

        Pipeline* pipeline = GetOrCreateComputePipeline(vk_device, context->m_PipelineCache, program_ptr);
        vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    }

    static void DrawSetup(VulkanContext* context, VkCommandBuffer vk_command_buffer, ScratchBuffer* scratchBuffer, DeviceBuffer* indexBuffer, Type indexBufferType)
    {
        RenderTarget* current_rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);
        BeginRenderPass(context, context->m_CurrentRenderTarget);

        Program* program_ptr = context->m_CurrentProgram;
        VkDevice vk_device   = context->m_LogicalDevice.m_Device;

        VkBuffer vk_buffers[MAX_VERTEX_BUFFERS]            = {};
        VkDeviceSize vk_buffer_offsets[MAX_VERTEX_BUFFERS] = {};
        uint32_t num_vx_buffers                            = 0;

        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexBuffer[i])
            {
                vk_buffers[num_vx_buffers++] = context->m_CurrentVertexBuffer[i]->m_Handle.m_Buffer;
            }
        }

        PrepareScatchBuffer(context, scratchBuffer, program_ptr);

        // Write the uniform data to the descriptors
        uint32_t dynamic_alignment = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment;
        VkResult res = CommitUniforms(context, vk_command_buffer, vk_device, program_ptr, VK_PIPELINE_BIND_POINT_GRAPHICS, scratchBuffer, context->m_DynamicOffsetBuffer, dynamic_alignment);
        CHECK_VK_ERROR(res);

        PipelineState pipeline_state_draw = context->m_PipelineState;

        // If the culling, or viewport has changed, make sure to flip the
        // culling flag if we are rendering to the backbuffer.
        // This is needed because we are rendering with a negative viewport
        // which means that the face direction is inverted.
        if (current_rt->m_Id != DM_RENDERTARGET_BACKBUFFER_ID)
        {
            if (pipeline_state_draw.m_CullFaceType == FACE_TYPE_BACK)
            {
                pipeline_state_draw.m_CullFaceType = FACE_TYPE_FRONT;
            }
            else if (pipeline_state_draw.m_CullFaceType == FACE_TYPE_FRONT)
            {
                pipeline_state_draw.m_CullFaceType = FACE_TYPE_BACK;
            }
        }

        // Update the viewport
        if (context->m_ViewportChanged)
        {
            Viewport& vp = context->m_MainViewport;

            // If we are rendering to the backbuffer, we must invert the viewport on
            // the y axis. Otherwise we just use the values as-is.
            // If we don't, all FBO rendering will be upside down.
            if (current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
            {
                SetViewportHelper(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
                    vp.m_X, (context->m_WindowHeight - vp.m_Y), vp.m_W, -vp.m_H);
            }
            else
            {
                SetViewportHelper(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
                    vp.m_X, vp.m_Y, vp.m_W, vp.m_H);
            }

            vkCmdSetScissor(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex], 0, 1, &current_rt->m_Scissor);

            context->m_ViewportChanged = 0;
        }

        // Get the pipeline for the active draw state
        VkSampleCountFlagBits vk_sample_count = VK_SAMPLE_COUNT_1_BIT;
        if (current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            vk_sample_count = context->m_SwapChain->m_SampleCountFlag;
        }

        Pipeline* pipeline = GetOrCreatePipeline(vk_device, vk_sample_count,
            pipeline_state_draw, context->m_PipelineCache,
            program_ptr, current_rt, context->m_CurrentVertexDeclaration, num_vx_buffers);

        if (pipeline != context->m_CurrentPipeline)
        {
            vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
            context->m_CurrentPipeline = pipeline;
        }

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

        vkCmdBindVertexBuffers(vk_command_buffer, 0, num_vx_buffers, vk_buffers, vk_buffer_offsets);
    }

    static void VulkanDrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);

        VulkanContext* context = (VulkanContext*) _context;

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

    static void VulkanDraw(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_FrameBegun);
        const uint8_t image_ix = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
        context->m_PipelineState.m_PrimtiveType = prim_type;
        DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[image_ix], 0, TYPE_BYTE);
        vkCmdDraw(vk_command_buffer, count, 1, first, 0);
    }

    static void VulkanDispatchCompute(HContext _context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DispatchCalls, 1);
        VulkanContext* context = (VulkanContext*) _context;

        // We can't run compute if we have started a render pass
        // Perhaps it would work if we could run it in a separate command buffer or a dedicated compute queue?
        if (IsRenderTargetbound(context, context->m_CurrentRenderTarget))
        {
            EndRenderPass(context);
        }

        const uint8_t image_ix = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
        DrawSetupCompute(context, vk_command_buffer, &context->m_MainScratchBuffers[image_ix]);
        vkCmdDispatch(vk_command_buffer, group_count_x, group_count_y, group_count_z);
    }

    static bool ValidateShaderModule(VulkanContext* context, ShaderModule* shader, char* error_buffer, uint32_t error_buffer_size)
    {
        if (shader->m_ShaderMeta.m_UniformBuffers.Size() > context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorUniformBuffers)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "Maximum number of uniform buffers exceeded: shader has %d buffers, but maximum is %d.",
                shader->m_ShaderMeta.m_UniformBuffers.Size(), context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorUniformBuffers);
            return false;
        }
        else if (shader->m_ShaderMeta.m_StorageBuffers.Size() > context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorStorageBuffers)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "Maximum number of storage exceeded: shader has %d buffer, but maximum is %d.",
                shader->m_ShaderMeta.m_StorageBuffers.Size(), context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorStorageBuffers);
            return false;
        }
        else if (shader->m_ShaderMeta.m_Textures.Size() > context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorSamplers)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "Maximum number of texture samplers exceeded: shader has %d samplers, but maximum is %d.",
                shader->m_ShaderMeta.m_Textures.Size(), context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorSamplers);
            return false;
        }
        return true;
    }

    static HVertexProgram VulkanNewVertexProgram(HContext _context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderDesc::Shader* ddf_shader = GetShaderProgram(_context, ddf);
        if (ddf_shader == 0x0)
        {
            return 0x0;
        }

        ShaderModule* shader = new ShaderModule;
        memset(shader, 0, sizeof(*shader));
        VulkanContext* context = (VulkanContext*) _context;

        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf_shader->m_Source.m_Data, ddf_shader->m_Source.m_Count, VK_SHADER_STAGE_VERTEX_BIT, shader);
        CHECK_VK_ERROR(res);

        CreateShaderMeta(&ddf->m_Reflection, &shader->m_ShaderMeta);

        if (!ValidateShaderModule(context, shader, error_buffer, error_buffer_size))
        {
            DeleteVertexProgram((HVertexProgram) shader);
            return 0;
        }

        return (HVertexProgram) shader;
    }

    static HFragmentProgram VulkanNewFragmentProgram(HContext _context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderDesc::Shader* ddf_shader = GetShaderProgram(_context, ddf);
        if (ddf_shader == 0x0)
        {
            return 0x0;
        }

        ShaderModule* shader = new ShaderModule;
        memset(shader, 0, sizeof(*shader));
        VulkanContext* context = (VulkanContext*) _context;

        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf_shader->m_Source.m_Data, ddf_shader->m_Source.m_Count, VK_SHADER_STAGE_FRAGMENT_BIT, shader);
        CHECK_VK_ERROR(res);

        CreateShaderMeta(&ddf->m_Reflection, &shader->m_ShaderMeta);

        if (!ValidateShaderModule(context, shader, error_buffer, error_buffer_size))
        {
            DeleteFragmentProgram((HFragmentProgram) shader);
            return 0;
        }

        return (HFragmentProgram) shader;
    }

    static inline VkDescriptorType GetDescriptorType(const ShaderResourceBinding& res)
    {
        if (ShaderResourceBinding::BINDING_FAMILY_GENERIC)
            return (VkDescriptorType) -1;

        if (res.m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_TEXTURE)
        {
            switch(res.m_Type.m_ShaderType)
            {
                case ShaderDesc::SHADER_TYPE_RENDER_PASS_INPUT:
                    return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                case ShaderDesc::SHADER_TYPE_UIMAGE2D:
                case ShaderDesc::SHADER_TYPE_IMAGE2D:
                    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                case ShaderDesc::SHADER_TYPE_SAMPLER:
                    return VK_DESCRIPTOR_TYPE_SAMPLER;
                default:
                    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
        }
        else if (res.m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER)
        {
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    }

    static void CreatePipelineLayout(VulkanContext* context, Program* program, VkDescriptorSetLayoutBinding bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT], uint32_t max_sets)
    {
        program->m_Handle.m_DescriptorSetLayoutsCount = max_sets;

        for (int i = 0; i < max_sets; ++i)
        {
            VkDescriptorSetLayoutBinding set_bindings[MAX_BINDINGS_PER_SET_COUNT] = {};
            uint32_t bindings_count = 0;

            for (int j = 0; j < MAX_BINDINGS_PER_SET_COUNT; ++j)
            {
                if (bindings[i][j].stageFlags != 0)
                {
                    set_bindings[bindings_count++] = bindings[i][j];
                }
            }

            VkDescriptorSetLayoutCreateInfo set_create_info = {};
            set_create_info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            set_create_info.pBindings                       = set_bindings;
            set_create_info.bindingCount                    = bindings_count;

            vkCreateDescriptorSetLayout(context->m_LogicalDevice.m_Device, &set_create_info, 0, &program->m_Handle.m_DescriptorSetLayouts[i]);
        }

        VkPipelineLayoutCreateInfo vk_layout_create_info;
        memset(&vk_layout_create_info, 0, sizeof(vk_layout_create_info));
        vk_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        vk_layout_create_info.setLayoutCount = program->m_Handle.m_DescriptorSetLayoutsCount;
        vk_layout_create_info.pSetLayouts    = program->m_Handle.m_DescriptorSetLayouts;

        vkCreatePipelineLayout(context->m_LogicalDevice.m_Device, &vk_layout_create_info, 0, &program->m_Handle.m_PipelineLayout);
    }

    static void FillProgramResourceBindings(
        Program*                         program,
        dmArray<ShaderResourceBinding>&  resources,
        dmArray<ShaderResourceTypeInfo>& stage_type_infos,
        VkDescriptorSetLayoutBinding     bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT],
        uint32_t                         ubo_alignment,
        uint32_t                         ssbo_alignment,
        VkShaderStageFlagBits            stage_flag,
        ProgramResourceBindingsInfo&     info)
    {
        for (int i = 0; i < resources.Size(); ++i)
        {
            ShaderResourceBinding& res            = resources[i];
            VkDescriptorSetLayoutBinding& binding = bindings[res.m_Set][res.m_Binding];
            ProgramResourceBinding& program_resource_binding = program->m_ResourceBindings[res.m_Set][res.m_Binding];

        #if 0
            dmLogInfo("    name=%s, set=%d, binding=%d", res.m_Name, res.m_Set, res.m_Binding);
        #endif

            if (binding.descriptorCount == 0)
            {
                binding.binding            = res.m_Binding;
                binding.descriptorType     = GetDescriptorType(res);
                binding.descriptorCount    = 1;
                binding.pImmutableSamplers = 0;

                program_resource_binding.m_Res       = &res;
                program_resource_binding.m_TypeInfos = &stage_type_infos;

                switch(res.m_BindingFamily)
                {
                    case ShaderResourceBinding::BINDING_FAMILY_TEXTURE:
                        program_resource_binding.m_TextureUnit = info.m_TextureCount;
                        info.m_TextureCount++;
                        info.m_TotalUniformCount++;
                        break;
                    case ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER:
                        program_resource_binding.m_StorageBufferUnit = info.m_StorageBufferCount;
                        info.m_StorageBufferCount++;
                        info.m_TotalUniformCount++;

                    #if 0
                        dmLogInfo("SSBO: name=%s, set=%d, binding=%d, ssbo-unit=%d", res.m_Name, res.m_Set, res.m_Binding, program_resource_binding.m_StorageBufferUnit);
                    #endif

                        break;
                    case ShaderResourceBinding::BINDING_FAMILY_UNIFORM_BUFFER:
                    {
                        assert(res.m_Type.m_UseTypeIndex);
                        const ShaderResourceTypeInfo& type_info       = stage_type_infos[res.m_Type.m_TypeIndex];
                        program_resource_binding.m_DataOffset         = info.m_UniformDataSize;
                        program_resource_binding.m_DynamicOffsetIndex = info.m_UniformBufferCount;

                        info.m_UniformBufferCount++;
                        info.m_UniformDataSize        += res.m_BlockSize;
                        info.m_UniformDataSizeAligned += DM_ALIGN(res.m_BlockSize, ubo_alignment);
                        info.m_TotalUniformCount      += type_info.m_Members.Size();
                    }
                    break;
                    case ShaderResourceBinding::BINDING_FAMILY_GENERIC:
                    default:break;
                }

                info.m_MaxSet     = dmMath::Max(info.m_MaxSet, (uint32_t) (res.m_Set + 1));
                info.m_MaxBinding = dmMath::Max(info.m_MaxBinding, (uint32_t) (res.m_Binding + 1));
            #if 0
                dmLogInfo("    name=%s, set=%d, binding=%d, data_offset=%d", res.m_Name, res.m_Set, res.m_Binding, program_resource_binding.m_DataOffset);
            #endif
            }

            binding.stageFlags |= stage_flag;
        }
    }

    static void FillProgramResourceBindings(
        Program*                     program,
        ShaderModule*                module,
        VkDescriptorSetLayoutBinding bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT],
        uint32_t                     ubo_alignment,
        uint32_t                     ssbo_alignment,
        VkShaderStageFlagBits        stage_flag,
        ProgramResourceBindingsInfo& info)
    {
        FillProgramResourceBindings(program, module->m_ShaderMeta.m_UniformBuffers, module->m_ShaderMeta.m_TypeInfos, bindings, ubo_alignment, ssbo_alignment, stage_flag, info);
        FillProgramResourceBindings(program, module->m_ShaderMeta.m_StorageBuffers, module->m_ShaderMeta.m_TypeInfos, bindings, ubo_alignment, ssbo_alignment, stage_flag, info);
        FillProgramResourceBindings(program, module->m_ShaderMeta.m_Textures, module->m_ShaderMeta.m_TypeInfos, bindings, ubo_alignment, ssbo_alignment, stage_flag, info);
    }

    static void CreateProgramResourceBindings(VulkanContext* context, Program* program)
    {
        VkDescriptorSetLayoutBinding bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT] = {};

        uint32_t ubo_alignment  = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment;
        uint32_t ssbo_alignment = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minStorageBufferOffsetAlignment;

        ProgramResourceBindingsInfo binding_info = {};

        if (program->m_ComputeModule)
        {
            FillProgramResourceBindings(program, program->m_ComputeModule, bindings, ubo_alignment, ssbo_alignment, VK_SHADER_STAGE_COMPUTE_BIT, binding_info);
        }
        else
        {
            assert(program->m_VertexModule && program->m_FragmentModule);
            FillProgramResourceBindings(program, program->m_VertexModule, bindings, ubo_alignment, ssbo_alignment, VK_SHADER_STAGE_VERTEX_BIT, binding_info);
            FillProgramResourceBindings(program, program->m_FragmentModule, bindings, ubo_alignment, ssbo_alignment, VK_SHADER_STAGE_FRAGMENT_BIT, binding_info);
        }

        program->m_UniformData = new uint8_t[binding_info.m_UniformDataSize];
        memset(program->m_UniformData, 0, binding_info.m_UniformDataSize);

        program->m_UniformDataSizeAligned = binding_info.m_UniformDataSizeAligned;
        program->m_UniformBufferCount     = binding_info.m_UniformBufferCount;
        program->m_StorageBufferCount     = binding_info.m_StorageBufferCount;
        program->m_TextureSamplerCount    = binding_info.m_TextureCount;
        program->m_TotalUniformCount      = binding_info.m_TotalUniformCount;
        program->m_TotalResourcesCount    = binding_info.m_UniformBufferCount + binding_info.m_TextureCount + binding_info.m_StorageBufferCount; // num actual descriptors
        program->m_MaxSet                 = binding_info.m_MaxSet;
        program->m_MaxBinding             = binding_info.m_MaxBinding;
        CreatePipelineLayout(context, program, bindings, binding_info.m_MaxSet);
    }

    static void CreateComputeProgram(VulkanContext* context, Program* program, ShaderModule* compute_module)
    {
        program->m_ComputeModule  = compute_module;
        program->m_Hash           = compute_module->m_Hash;
        CreateProgramResourceBindings(context, program);
    }

    static void CreateGraphicsProgram(VulkanContext* context, Program* program, ShaderModule* vertex_module, ShaderModule* fragment_module)
    {
        program->m_Hash           = 0;
        program->m_UniformData    = 0;
        program->m_VertexModule   = vertex_module;
        program->m_FragmentModule = fragment_module;

        HashState64 program_hash;
        dmHashInit64(&program_hash, false);

        for (uint32_t i=0; i < vertex_module->m_ShaderMeta.m_Inputs.Size(); i++)
        {
            dmHashUpdateBuffer64(&program_hash, &vertex_module->m_ShaderMeta.m_Inputs[i].m_Binding, sizeof(vertex_module->m_ShaderMeta.m_Inputs[i].m_Binding));
        }

        dmHashUpdateBuffer64(&program_hash, &vertex_module->m_Hash, sizeof(vertex_module->m_Hash));
        dmHashUpdateBuffer64(&program_hash, &fragment_module->m_Hash, sizeof(fragment_module->m_Hash));
        program->m_Hash = dmHashFinal64(&program_hash);

        CreateProgramResourceBindings(context, program);
    }

    static HProgram VulkanNewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        Program* program = new Program;
        CreateGraphicsProgram((VulkanContext*) context, program, (ShaderModule*) vertex_program, (ShaderModule*) fragment_program);
        return (HProgram) program;
    }

    static void DestroyProgram(HContext context, Program* program)
    {
        if (program->m_UniformData)
        {
            delete[] program->m_UniformData;
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
        if (!shader)
        {
            return;
        }

        DestroyShaderModule(g_VulkanContext->m_LogicalDevice.m_Device, shader);
        DestroyShaderMeta(shader->m_ShaderMeta);
    }

    static bool ReloadShader(ShaderModule* shader, ShaderDesc* ddf, VkShaderStageFlagBits stage_flag)
    {
        ShaderDesc::Shader* ddf_shader = GetShaderProgram((HContext) g_VulkanContext, ddf);
        if (ddf_shader == 0x0)
        {
            return false;
        }
        ShaderModule tmp_shader;
        VkResult res = CreateShaderModule(g_VulkanContext->m_LogicalDevice.m_Device, ddf_shader->m_Source.m_Data, ddf_shader->m_Source.m_Count, stage_flag, &tmp_shader);
        if (res == VK_SUCCESS)
        {
            DestroyShader(shader);
            memset(shader, 0, sizeof(*shader));

            // Transfer created module to old pointer and recreate resource bindings
            shader->m_Hash    = tmp_shader.m_Hash;
            shader->m_Module  = tmp_shader.m_Module;

            CreateShaderMeta(&ddf->m_Reflection, &shader->m_ShaderMeta);
            return true;
        }

        return false;
    }

    static bool VulkanReloadVertexProgram(HVertexProgram prog, ShaderDesc* ddf)
    {
        return ReloadShader((ShaderModule*) prog, ddf, VK_SHADER_STAGE_VERTEX_BIT);
    }

    static bool VulkanReloadFragmentProgram(HFragmentProgram prog, ShaderDesc* ddf)
    {
        return ReloadShader((ShaderModule*) prog, ddf, VK_SHADER_STAGE_FRAGMENT_BIT);
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
        DestroyShader(shader);
        delete shader;
    }

    static bool VulkanIsShaderLanguageSupported(HContext context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
    {
        return language == ShaderDesc::LANGUAGE_SPIRV;
    }

    static ShaderDesc::Language VulkanGetProgramLanguage(HProgram program)
    {
        return ShaderDesc::LANGUAGE_SPIRV;
    }

    static void VulkanEnableProgram(HContext context, HProgram program)
    {
        g_VulkanContext->m_CurrentProgram = (Program*) program;
    }

    static void VulkanDisableProgram(HContext context)
    {
        g_VulkanContext->m_CurrentProgram = 0;
    }

    static bool VulkanReloadProgramGraphics(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        Program* program_ptr = (Program*) program;
        DestroyProgram(context, program_ptr);
        CreateGraphicsProgram((VulkanContext*) context, program_ptr, (ShaderModule*) vert_program, (ShaderModule*) frag_program);
        return true;
    }

    static bool VulkanReloadProgramCompute(HContext context, HProgram program, HComputeProgram compute_program)
    {
        Program* program_ptr = (Program*) program;
        DestroyProgram(context, program_ptr);
        CreateComputeProgram((VulkanContext*) context, program_ptr, (ShaderModule*) compute_program);
        return true;
    }

    static bool VulkanReloadComputeProgram(HComputeProgram prog, ShaderDesc* ddf)
    {
        return ReloadShader((ShaderModule*) prog, ddf, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    static uint32_t VulkanGetAttributeCount(HProgram prog)
    {
        Program* program_ptr = (Program*) prog;
        return program_ptr->m_VertexModule->m_ShaderMeta.m_Inputs.Size();
    }

    static void VulkanGetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        Program* program_ptr = (Program*) prog;
        assert(index < program_ptr->m_VertexModule->m_ShaderMeta.m_Inputs.Size());
        ShaderResourceBinding& attr = program_ptr->m_VertexModule->m_ShaderMeta.m_Inputs[index];

        *name_hash     = attr.m_NameHash;
        *type          = ShaderDataTypeToGraphicsType(attr.m_Type.m_ShaderType);
        *num_values    = 1;
        *location      = attr.m_Binding;
        *element_count = GetShaderTypeSize(attr.m_Type.m_ShaderType) / sizeof(float);
    }

    static uint32_t VulkanGetUniformCount(HProgram prog)
    {
        assert(prog);
        Program* program_ptr = (Program*) prog;
        return program_ptr->m_TotalUniformCount;
    }

    static uint32_t VulkanGetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size)
    {
        assert(prog);
        Program* program      = (Program*) prog;
        uint32_t search_index = 0;

        for (int set = 0; set < program->m_MaxSet; ++set)
        {
            for (int binding = 0; binding < program->m_MaxBinding; ++binding)
            {
                ProgramResourceBinding& pgm_res = program->m_ResourceBindings[set][binding];

                if (pgm_res.m_Res == 0x0)
                    continue;

                if (pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_TEXTURE ||
                    pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER)
                {
                    if (search_index == index)
                    {
                        ShaderResourceBinding* res = pgm_res.m_Res;
                        *type = ShaderDataTypeToGraphicsType(res->m_Type.m_ShaderType);
                        *size = 1;
                        return (uint32_t)dmStrlCpy(buffer, res->m_Name, buffer_size);
                    }
                    search_index++;
                }
                else if (pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_UNIFORM_BUFFER)
                {
                    // TODO: Generic type lookup is not supported yet!
                    // We can only support one level of indirection here right now
                    assert(pgm_res.m_Res->m_Type.m_UseTypeIndex);
                    const dmArray<ShaderResourceTypeInfo>& type_infos = *pgm_res.m_TypeInfos;
                    const ShaderResourceTypeInfo& type_info = type_infos[pgm_res.m_Res->m_Type.m_TypeIndex];

                    const uint32_t num_members = type_info.m_Members.Size();
                    for (int i = 0; i < num_members; ++i)
                    {
                        if (search_index == index)
                        {
                            const ShaderResourceMember& member = type_info.m_Members[i];
                            *type = ShaderDataTypeToGraphicsType(member.m_Type.m_ShaderType);
                            *size = dmMath::Max((uint32_t) 1, member.m_ElementCount);
                            return (uint32_t)dmStrlCpy(buffer, member.m_Name, buffer_size);
                        }
                        search_index++;
                    }
                }
            }
        }
        return 0;
    }

    static HUniformLocation VulkanGetUniformLocation(HProgram prog, const char* name)
    {
        assert(prog);
        Program* program_ptr = (Program*) prog;
        dmhash_t name_hash   = dmHashString64(name);

        for (int set = 0; set < program_ptr->m_MaxSet; ++set)
        {
            for (int binding = 0; binding < program_ptr->m_MaxBinding; ++binding)
            {
                ProgramResourceBinding& pgm_res = program_ptr->m_ResourceBindings[set][binding];

                if (pgm_res.m_Res == 0x0)
                    continue;

                if (pgm_res.m_Res->m_NameHash == name_hash)
                {
                    return set | binding << 16;
                }
                else if (pgm_res.m_Res->m_Type.m_UseTypeIndex)
                {
                    // TODO: Generic type lookup is not supported yet!
                    // We can only support one level of indirection here right now
                    const dmArray<ShaderResourceTypeInfo>& type_infos = *pgm_res.m_TypeInfos;
                    const ShaderResourceTypeInfo& type_info = type_infos[pgm_res.m_Res->m_Type.m_TypeIndex];

                    const uint32_t num_members = type_info.m_Members.Size();
                    for (int i = 0; i < num_members; ++i)
                    {
                        const ShaderResourceMember& member = type_info.m_Members[i];

                        if (member.m_NameHash == name_hash)
                        {
                            return set | binding << 16 | ((uint64_t) i) << 32;
                        }
                    }
                }
            }
        }

        return INVALID_UNIFORM_LOCATION;
    }

    static inline void WriteConstantData(uint32_t offset, uint8_t* uniform_data_ptr, uint8_t* data_ptr, uint32_t data_size)
    {
        memcpy(&uniform_data_ptr[offset], data_ptr, data_size);
    }

    static void VulkanSetConstantV4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        Program* program_ptr = (Program*) context->m_CurrentProgram;
        uint32_t set         = UNIFORM_LOCATION_GET_VS(base_location);
        uint32_t binding     = UNIFORM_LOCATION_GET_VS_MEMBER(base_location);
        uint32_t member      = UNIFORM_LOCATION_GET_FS(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        ProgramResourceBinding& pgm_res                   = program_ptr->m_ResourceBindings[set][binding];
        const dmArray<ShaderResourceTypeInfo>& type_infos = *pgm_res.m_TypeInfos;
        const ShaderResourceTypeInfo&           type_info = type_infos[pgm_res.m_Res->m_Type.m_TypeIndex];

        uint32_t offset = pgm_res.m_DataOffset + type_info.m_Members[member].m_Offset;
        WriteConstantData(offset, program_ptr->m_UniformData, (uint8_t*) data, sizeof(dmVMath::Vector4) * count);
    }

    static void VulkanSetConstantM4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        Program* program_ptr = (Program*) context->m_CurrentProgram;
        uint32_t set         = UNIFORM_LOCATION_GET_VS(base_location);
        uint32_t binding     = UNIFORM_LOCATION_GET_VS_MEMBER(base_location);
        uint32_t member      = UNIFORM_LOCATION_GET_FS(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        ProgramResourceBinding& pgm_res                   = program_ptr->m_ResourceBindings[set][binding];
        const dmArray<ShaderResourceTypeInfo>& type_infos = *pgm_res.m_TypeInfos;
        const ShaderResourceTypeInfo&           type_info = type_infos[pgm_res.m_Res->m_Type.m_TypeIndex];

        uint32_t offset = pgm_res.m_DataOffset + type_info.m_Members[member].m_Offset;
        WriteConstantData(offset, program_ptr->m_UniformData, (uint8_t*) data, sizeof(dmVMath::Vector4) * 4 * count);
    }

    static void VulkanSetSampler(HContext _context, HUniformLocation location, int32_t unit)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_CurrentProgram);
        assert(location != INVALID_UNIFORM_LOCATION);

        Program* program_ptr = (Program*) context->m_CurrentProgram;
        uint32_t set         = UNIFORM_LOCATION_GET_VS(location);
        uint32_t binding     = UNIFORM_LOCATION_GET_VS_MEMBER(location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        assert(program_ptr->m_ResourceBindings[set][binding].m_Res);
        program_ptr->m_ResourceBindings[set][binding].m_TextureUnit = unit;
    }

    static void VulkanSetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        // Defer the update to when we actually draw, since we *might* need to invert the viewport
        // depending on wether or not we have set a different rendertarget from when
        // this call was made.
        Viewport& viewport = g_VulkanContext->m_MainViewport;
        viewport.m_X       = (uint16_t) x;
        viewport.m_Y       = (uint16_t) y;
        viewport.m_W       = (uint16_t) width;
        viewport.m_H       = (uint16_t) height;

        g_VulkanContext->m_ViewportChanged = 1;
    }

    static void VulkanEnableState(HContext context, State state)
    {
        assert(context);
        SetPipelineStateValue(g_VulkanContext->m_PipelineState, state, 1);
    }

    static void VulkanDisableState(HContext context, State state)
    {
        assert(context);
        SetPipelineStateValue(g_VulkanContext->m_PipelineState, state, 0);
    }

    static void VulkanSetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
        g_VulkanContext->m_PipelineState.m_BlendSrcFactor = source_factor;
        g_VulkanContext->m_PipelineState.m_BlendDstFactor = destinaton_factor;
    }

    static void VulkanSetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        uint8_t write_mask = red   ? DM_GRAPHICS_STATE_WRITE_R : 0;
        write_mask        |= green ? DM_GRAPHICS_STATE_WRITE_G : 0;
        write_mask        |= blue  ? DM_GRAPHICS_STATE_WRITE_B : 0;
        write_mask        |= alpha ? DM_GRAPHICS_STATE_WRITE_A : 0;

        g_VulkanContext->m_PipelineState.m_WriteColorMask = write_mask;
    }

    static void VulkanSetDepthMask(HContext context, bool mask)
    {
        g_VulkanContext->m_PipelineState.m_WriteDepth = mask;
    }

    static void VulkanSetDepthFunc(HContext context, CompareFunc func)
    {
        g_VulkanContext->m_PipelineState.m_DepthTestFunc = func;
    }

    static void VulkanSetScissor(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        VulkanContext* context              = (VulkanContext*) _context;
        context->m_ViewportChanged          = 1;
        RenderTarget* current_rt            = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);
        current_rt->m_Scissor.extent.width  = width;
        current_rt->m_Scissor.extent.height = height;
        current_rt->m_Scissor.offset.x      = x;
        current_rt->m_Scissor.offset.y      = y;
    }

    static void VulkanSetStencilMask(HContext context, uint32_t mask)
    {
        g_VulkanContext->m_PipelineState.m_StencilWriteMask = mask;
    }

    static void VulkanSetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        g_VulkanContext->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        g_VulkanContext->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        g_VulkanContext->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        g_VulkanContext->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void VulkanSetStencilFuncSeparate(HContext context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        if (face_type == FACE_TYPE_BACK)
        {
            g_VulkanContext->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        }
        else
        {
            g_VulkanContext->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        }
        g_VulkanContext->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        g_VulkanContext->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void VulkanSetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(context);
        g_VulkanContext->m_PipelineState.m_StencilFrontOpFail      = sfail;
        g_VulkanContext->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
        g_VulkanContext->m_PipelineState.m_StencilFrontOpPass      = dppass;
        g_VulkanContext->m_PipelineState.m_StencilBackOpFail       = sfail;
        g_VulkanContext->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
        g_VulkanContext->m_PipelineState.m_StencilBackOpPass       = dppass;
    }

    static void VulkanSetStencilOpSeparate(HContext context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        if (face_type == FACE_TYPE_BACK)
        {
            g_VulkanContext->m_PipelineState.m_StencilBackOpFail       = sfail;
            g_VulkanContext->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
            g_VulkanContext->m_PipelineState.m_StencilBackOpPass       = dppass;
        }
        else
        {
            g_VulkanContext->m_PipelineState.m_StencilFrontOpFail      = sfail;
            g_VulkanContext->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
            g_VulkanContext->m_PipelineState.m_StencilFrontOpPass      = dppass;
        }
    }

    static void VulkanSetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
        g_VulkanContext->m_PipelineState.m_CullFaceType = face_type;
        g_VulkanContext->m_CullFaceChanged              = true;
    }

    static void VulkanSetFaceWinding(HContext, FaceWinding face_winding)
    {
        // TODO: Add this to the vulkan pipeline handle aswell, for now it's a NOP
    }

    static void VulkanSetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
        vkCmdSetDepthBias(g_VulkanContext->m_MainCommandBuffers[g_VulkanContext->m_SwapChain->m_ImageIndex], factor, 0.0, units);
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
            case TEXTURE_FORMAT_RGBA32UI:           return VK_FORMAT_R32G32B32A32_UINT;
            case TEXTURE_FORMAT_BGRA8U:             return VK_FORMAT_B8G8R8A8_UNORM;
            case TEXTURE_FORMAT_R32UI:              return VK_FORMAT_R32_UINT;
            default:                                return VK_FORMAT_UNDEFINED;
        };
    }

    static inline VkAttachmentStoreOp VulkanStoreOp(AttachmentOp op)
    {
        switch(op)
        {
            case ATTACHMENT_OP_STORE:     return VK_ATTACHMENT_STORE_OP_STORE;
            case ATTACHMENT_OP_DONT_CARE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            default:break;
        }
        assert(0);
        return (VkAttachmentStoreOp) -1;
    }

    static inline VkAttachmentLoadOp VulkanLoadOp(AttachmentOp op)
    {
        switch(op)
        {
            case ATTACHMENT_OP_LOAD:      return VK_ATTACHMENT_LOAD_OP_LOAD;
            case ATTACHMENT_OP_CLEAR:     return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case ATTACHMENT_OP_DONT_CARE: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            default:break;
        }
        assert(0);
        return (VkAttachmentLoadOp) -1;
    }

    static VkResult CreateRenderTarget(VulkanContext* context, HTexture* color_textures, BufferType* buffer_types, uint8_t num_color_textures,  HTexture depth_stencil_texture, uint32_t width, uint32_t height, RenderTarget* rtOut)
    {
        assert(rtOut->m_Handle.m_Framebuffer == VK_NULL_HANDLE && rtOut->m_Handle.m_RenderPass == VK_NULL_HANDLE);
        const uint8_t num_attachments = MAX_BUFFER_COLOR_ATTACHMENTS + 1;

        RenderPassAttachment  rp_attachments[num_attachments];
        RenderPassAttachment* rp_attachment_depth_stencil = 0;

        VkImageView fb_attachments[num_attachments];
        uint16_t    fb_attachment_count = 0;
        uint16_t    fb_width            = width;
        uint16_t    fb_height           = height;

        for (int i = 0; i < num_color_textures; ++i)
        {
            VulkanTexture* color_texture_ptr = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, color_textures[i]);

            assert(!color_texture_ptr->m_Destroyed && color_texture_ptr->m_Handle.m_ImageView != VK_NULL_HANDLE && color_texture_ptr->m_Handle.m_Image != VK_NULL_HANDLE);
            uint8_t color_buffer_index = GetBufferTypeIndex(buffer_types[i]);
            fb_width                   = rtOut->m_ColorTextureParams[color_buffer_index].m_Width;
            fb_height                  = rtOut->m_ColorTextureParams[color_buffer_index].m_Height;

            RenderPassAttachment* rp_attachment_color = &rp_attachments[i];
            rp_attachment_color->m_ImageLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rp_attachment_color->m_ImageLayoutInitial = VK_IMAGE_LAYOUT_UNDEFINED;
            rp_attachment_color->m_Format             = color_texture_ptr->m_Format;
            rp_attachment_color->m_LoadOp             = VulkanLoadOp(rtOut->m_ColorBufferLoadOps[color_buffer_index]);
            rp_attachment_color->m_StoreOp            = VulkanStoreOp(rtOut->m_ColorBufferStoreOps[color_buffer_index]);

            if (rp_attachment_color->m_LoadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
            {
                rp_attachment_color->m_ImageLayoutInitial = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            fb_attachments[fb_attachment_count++] = color_texture_ptr->m_Handle.m_ImageView;
        }

        if (depth_stencil_texture)
        {
            VulkanTexture* depth_stencil_texture_ptr = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, depth_stencil_texture);
            if (num_color_textures == 0)
            {
                fb_width  = rtOut->m_DepthStencilTextureParams.m_Height;
                fb_height = rtOut->m_DepthStencilTextureParams.m_Height;
            }

            rp_attachment_depth_stencil                = &rp_attachments[fb_attachment_count];
            rp_attachment_depth_stencil->m_ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            rp_attachment_depth_stencil->m_Format      = depth_stencil_texture_ptr->m_Format;

            fb_attachments[fb_attachment_count++] = depth_stencil_texture_ptr->m_Handle.m_ImageView;
        }

        VkResult res = CreateRenderPass(context->m_LogicalDevice.m_Device, VK_SAMPLE_COUNT_1_BIT, rp_attachments, num_color_textures, rp_attachment_depth_stencil, 0, &rtOut->m_Handle.m_RenderPass);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        res = CreateFramebuffer(context->m_LogicalDevice.m_Device, rtOut->m_Handle.m_RenderPass,
            fb_width, fb_height, fb_attachments, (uint8_t)fb_attachment_count, &rtOut->m_Handle.m_Framebuffer);
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
        rtOut->m_TextureDepthStencil  = depth_stencil_texture;
        rtOut->m_Extent.width         = fb_width;
        rtOut->m_Extent.height        = fb_height;

        return VK_SUCCESS;
    }

    static void DestroyRenderTarget(VulkanContext* context, RenderTarget* renderTarget)
    {
        DestroyResourceDeferred(context->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], renderTarget);
        renderTarget->m_Handle.m_Framebuffer = VK_NULL_HANDLE;
        renderTarget->m_Handle.m_RenderPass = VK_NULL_HANDLE;
    }

    static inline VkImageUsageFlags GetVulkanUsageFromHints(uint8_t hint_bits)
    {
        VkImageUsageFlags vk_flags = 0;

    #define APPEND_IF_SET(usage_hint, vk_enum) \
        if (hint_bits & usage_hint) vk_flags |= vk_enum;

        APPEND_IF_SET(TEXTURE_USAGE_FLAG_SAMPLE,     VK_IMAGE_USAGE_SAMPLED_BIT);
        APPEND_IF_SET(TEXTURE_USAGE_FLAG_MEMORYLESS, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT);
        APPEND_IF_SET(TEXTURE_USAGE_FLAG_INPUT,      VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
        APPEND_IF_SET(TEXTURE_USAGE_FLAG_COLOR,      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        APPEND_IF_SET(TEXTURE_USAGE_FLAG_STORAGE,    VK_IMAGE_USAGE_STORAGE_BIT);
    #undef APPEND_IF_SET

        return vk_flags;
    }

    static HRenderTarget VulkanNewRenderTarget(HContext context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        RenderTarget* rt = new RenderTarget(GetNextRenderTargetId());

        memcpy(rt->m_ColorTextureParams, params.m_ColorBufferParams, sizeof(TextureParams) * MAX_BUFFER_COLOR_ATTACHMENTS);
        memcpy(rt->m_ColorBufferLoadOps, params.m_ColorBufferLoadOps, sizeof(AttachmentOp) * MAX_BUFFER_COLOR_ATTACHMENTS);
        memcpy(rt->m_ColorBufferStoreOps, params.m_ColorBufferStoreOps, sizeof(AttachmentOp) * MAX_BUFFER_COLOR_ATTACHMENTS);

        rt->m_DepthStencilTextureParams = (buffer_type_flags & BUFFER_TYPE_DEPTH_BIT) ?
            params.m_DepthBufferParams :
            params.m_StencilBufferParams;

        // don't save the data
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            ClearTextureParamsData(rt->m_ColorTextureParams[i]);
        }
        ClearTextureParamsData(rt->m_DepthStencilTextureParams);

        BufferType buffer_types[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture texture_color[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture texture_depth_stencil = 0;

        uint8_t has_depth   = buffer_type_flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT;
        uint8_t has_stencil = buffer_type_flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT;
        uint8_t color_index = 0;

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
                TextureParams& color_buffer_params = rt->m_ColorTextureParams[i];
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

                HTexture new_texture_color_handle = NewTexture(context, params.m_ColorBufferCreationParams[i]);
                VulkanTexture* new_texture_color = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, new_texture_color_handle);

                VkImageUsageFlags vk_usage_flags     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | new_texture_color->m_UsageFlags;
                VkMemoryPropertyFlags vk_memory_type = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

                if (IsTextureMemoryless(new_texture_color))
                {
                    vk_memory_type |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
                }

                VkResult res = CreateTexture2D(
                    g_VulkanContext->m_PhysicalDevice.m_Device,
                    g_VulkanContext->m_LogicalDevice.m_Device,
                    new_texture_color->m_Width, new_texture_color->m_Height, 1, new_texture_color->m_MipMapCount,
                    VK_SAMPLE_COUNT_1_BIT,
                    vk_color_format,
                    VK_IMAGE_TILING_OPTIMAL,
                    vk_usage_flags,
                    vk_memory_type,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    new_texture_color);
                CHECK_VK_ERROR(res);

                res = TransitionImageLayout(g_VulkanContext->m_LogicalDevice.m_Device,
                    g_VulkanContext->m_LogicalDevice.m_CommandPool,
                    g_VulkanContext->m_LogicalDevice.m_GraphicsQueue,
                    new_texture_color,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                CHECK_VK_ERROR(res);

                VulkanSetTextureParamsInternal(new_texture_color, color_buffer_params.m_MinFilter, color_buffer_params.m_MagFilter, color_buffer_params.m_UWrap, color_buffer_params.m_VWrap, 1.0f);

                texture_color[color_index] = new_texture_color_handle;
                buffer_types[color_index] = buffer_type;
                color_index++;
            }
        }

        if(has_depth || has_stencil)
        {
            VkFormat vk_depth_stencil_format = VK_FORMAT_UNDEFINED;
            VkImageTiling vk_depth_tiling    = VK_IMAGE_TILING_OPTIMAL;

            const TextureCreationParams& stencil_depth_create_params = has_depth ? params.m_DepthBufferCreationParams : params.m_StencilBufferCreationParams;

            // Only try depth formats first
            if (has_depth && !has_stencil)
            {
                VkFormat vk_format_list[] = {
                    VK_FORMAT_D32_SFLOAT,
                    VK_FORMAT_D16_UNORM
                };

                GetDepthFormatAndTiling(g_VulkanContext->m_PhysicalDevice.m_Device, vk_format_list,
                    DM_ARRAY_SIZE(vk_format_list), &vk_depth_stencil_format, &vk_depth_tiling);
            }

            // If we request both depth & stencil OR test above failed,
            // try with default depth stencil formats
            if (vk_depth_stencil_format == VK_FORMAT_UNDEFINED)
            {
                GetDepthFormatAndTiling(g_VulkanContext->m_PhysicalDevice.m_Device, 0, 0, &vk_depth_stencil_format, &vk_depth_tiling);
            }

            texture_depth_stencil                    = NewTexture(context, stencil_depth_create_params);
            VulkanTexture* texture_depth_stencil_ptr = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture_depth_stencil);

            // TODO: Right now we can only sample depth with this texture, if we want to support stencil texture reads we need to make a separate texture I think
            VkResult res = CreateDepthStencilTexture(g_VulkanContext,
                vk_depth_stencil_format, vk_depth_tiling,
                fb_width, fb_height, VK_SAMPLE_COUNT_1_BIT, // No support for multisampled FBOs
                VK_IMAGE_ASPECT_DEPTH_BIT, // JG: This limits us to sampling depth only afaik
                texture_depth_stencil_ptr);
            CHECK_VK_ERROR(res);
        }

        if (color_index > 0 || has_depth || has_stencil)
        {
            VkResult res = CreateRenderTarget(g_VulkanContext, texture_color, buffer_types, color_index, texture_depth_stencil, fb_width, fb_height, rt);
            CHECK_VK_ERROR(res);
        }

        return StoreAssetInContainer(g_VulkanContext->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
    }

    static void VulkanDeleteRenderTarget(HRenderTarget render_target)
    {
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(g_VulkanContext->m_AssetHandleContainer, render_target);
        g_VulkanContext->m_AssetHandleContainer.Release(render_target);

        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt->m_TextureColor[i])
            {
                DeleteTexture(rt->m_TextureColor[i]);
            }
        }

        if (rt->m_TextureDepthStencil)
        {
            DeleteTexture(rt->m_TextureDepthStencil);
        }

        DestroyRenderTarget(g_VulkanContext, rt);

        delete rt;
    }

    static void VulkanSetRenderTarget(HContext _context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        VulkanContext* context = (VulkanContext*) _context;
        context->m_ViewportChanged = 1;
        BeginRenderPass(context, render_target != 0x0 ? render_target : context->m_MainRenderTarget);
    }

    static HTexture VulkanGetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(g_VulkanContext->m_AssetHandleContainer, render_target);

        if (IsColorBufferType(buffer_type))
        {
            return rt->m_TextureColor[GetBufferTypeIndex(buffer_type)];
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT || buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            return rt->m_TextureDepthStencil;
        }
        return 0;
    }

    static void VulkanGetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(g_VulkanContext->m_AssetHandleContainer, render_target);
        TextureParams* params = 0;

        if (IsColorBufferType(buffer_type))
        {
            uint32_t i = GetBufferTypeIndex(buffer_type);
            assert(i < MAX_BUFFER_COLOR_ATTACHMENTS);
            params = &rt->m_ColorTextureParams[i];
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT || buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            params = &rt->m_DepthStencilTextureParams;
        }
        else
        {
            assert(0);
        }

        width  = params->m_Width;
        height = params->m_Height;
    }

    static void VulkanSetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(g_VulkanContext->m_AssetHandleContainer, render_target);

        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            rt->m_ColorTextureParams[i].m_Width = width;
            rt->m_ColorTextureParams[i].m_Height = height;

            if (rt->m_TextureColor[i])
            {
                VulkanTexture* texture_color         = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, rt->m_TextureColor[i]);
                VkImageUsageFlags vk_usage_flags     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | texture_color->m_UsageFlags;
                VkMemoryPropertyFlags vk_memory_type = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

                if (IsTextureMemoryless(texture_color))
                {
                    vk_memory_type |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
                }

                texture_color->m_ImageLayout[0] = VK_IMAGE_LAYOUT_PREINITIALIZED;

                DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], texture_color);
                VkResult res = CreateTexture2D(
                    g_VulkanContext->m_PhysicalDevice.m_Device,
                    g_VulkanContext->m_LogicalDevice.m_Device,
                    width, height, texture_color->m_MipMapCount, 1, VK_SAMPLE_COUNT_1_BIT,
                    texture_color->m_Format,
                    VK_IMAGE_TILING_OPTIMAL,
                    vk_usage_flags,
                    vk_memory_type,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    texture_color);
                CHECK_VK_ERROR(res);

                texture_color->m_Width  = width;
                texture_color->m_Height = height;
            }
        }

        if (rt->m_TextureDepthStencil)
        {
            rt->m_DepthStencilTextureParams.m_Width = width;
            rt->m_DepthStencilTextureParams.m_Height = height;

            VulkanTexture* depth_stencil_texture = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, rt->m_TextureDepthStencil);
            DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], depth_stencil_texture);

            // Check tiling support for this format
            VkImageTiling vk_image_tiling    = VK_IMAGE_TILING_OPTIMAL;
            VkFormat vk_depth_stencil_format = depth_stencil_texture->m_Format;
            VkFormat vk_depth_format         = GetSupportedTilingFormat(g_VulkanContext->m_PhysicalDevice.m_Device, &vk_depth_stencil_format,
                1, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

            if (vk_depth_format == VK_FORMAT_UNDEFINED)
            {
                vk_image_tiling = VK_IMAGE_TILING_LINEAR;
            }

            VkResult res = CreateDepthStencilTexture(g_VulkanContext,
                vk_depth_stencil_format, vk_image_tiling,
                width, height, VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                depth_stencil_texture);
            CHECK_VK_ERROR(res);

            depth_stencil_texture->m_Width  = width;
            depth_stencil_texture->m_Height = height;
        }

        DestroyRenderTarget(g_VulkanContext, rt);
        VkResult res = CreateRenderTarget(g_VulkanContext,
            rt->m_TextureColor,
            rt->m_ColorAttachmentBufferTypes,
            rt->m_ColorAttachmentCount,
            rt->m_TextureDepthStencil,
            width, height,
            rt);
        CHECK_VK_ERROR(res);
    }

    static bool VulkanIsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (g_VulkanContext->m_TextureFormatSupport & (1 << format)) != 0;
    }

    static VulkanTexture* VulkanNewTextureInternal(const TextureCreationParams& params)
    {
        VulkanTexture* tex = new VulkanTexture;
        InitializeVulkanTexture(tex);

    #ifdef __MACH__
        if (params.m_UsageHintBits & dmGraphics::TEXTURE_USAGE_FLAG_INPUT &&
            params.m_UsageHintBits & dmGraphics::TEXTURE_USAGE_FLAG_MEMORYLESS)
        {
            dmLogWarning("Using both usage hints 'TEXTURE_USAGE_FLAG_INPUT' and 'TEXTURE_USAGE_FLAG_MEMORYLESS' when creating a texture on MoltenVK is not supported. The texture will not be created as memoryless.");
        }
    #endif

        tex->m_Type           = params.m_Type;
        tex->m_Width          = params.m_Width;
        tex->m_Height         = params.m_Height;
        tex->m_Depth          = params.m_Depth;
        tex->m_MipMapCount    = params.m_MipMapCount;
        tex->m_UsageFlags     = GetVulkanUsageFromHints(params.m_UsageHintBits);
        tex->m_UsageHintFlags = params.m_UsageHintBits;

        for (int i = 0; i < DM_ARRAY_SIZE(tex->m_ImageLayout); ++i)
        {
            tex->m_ImageLayout[i] = VK_IMAGE_LAYOUT_UNDEFINED;
        }

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
        return tex;
    }

    static HTexture VulkanNewTexture(HContext context, const TextureCreationParams& params)
    {
        return StoreAssetInContainer(g_VulkanContext->m_AssetHandleContainer, VulkanNewTextureInternal(params), ASSET_TYPE_TEXTURE);
    }

    static void VulkanDeleteTextureInternal(VulkanTexture* texture)
    {
        DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], texture);
        delete texture;
    }

    static void VulkanDeleteTexture(HTexture texture)
    {
        VulkanDeleteTextureInternal(GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture));
        g_VulkanContext->m_AssetHandleContainer.Release(texture);
    }

    static inline uint32_t GetOffsetFromMipmap(VulkanTexture* texture, uint8_t mipmap)
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

    static void CopyToTexture(VulkanContext* context, const TextureParams& params,
        bool useStageBuffer, uint32_t texDataSize, void* texDataPtr, VulkanTexture* textureOut)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;
        uint8_t layer_count = textureOut->m_Depth;
        assert(layer_count > 0);

        // TODO There is potentially a bunch of redundancy here.
        //      * Can we use a single command buffer for these updates,
        //        and not create a new one in every transition?
        //      * Should we batch upload all the mipmap levels instead?
        //        There's a lot of extra work doing all these transitions and image copies
        //        per mipmap instead of batching in one cmd
        if (useStageBuffer)
        {
            uint32_t slice_size = texDataSize / layer_count;

        #ifdef __MACH__
            // Note: There is an annoying validation issue on osx for layered compressed data that causes a validation error
            //       due to misalignment of the data when using a stage buffer. The offsets in the stage buffer needs to be
            //       8 byte aligned but for compressed data that is not the case for the lowest mipmaps.
            //       This might need some more investigation, but for now we don't want a crash at least...
            if (slice_size < 8 && layer_count > 1)
            {
                return;
            }
        #endif

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
            res = TransitionImageLayout(vk_device,
                context->m_LogicalDevice.m_CommandPool,
                context->m_LogicalDevice.m_GraphicsQueue,
                textureOut,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                params.m_MipMap,
                layer_count);
            CHECK_VK_ERROR(res);

            // NOTE: We should check max layer count in the device properties!
            VkBufferImageCopy* vk_copy_regions = new VkBufferImageCopy[layer_count];
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

            res = TransitionImageLayout(vk_device,
                context->m_LogicalDevice.m_CommandPool,
                context->m_LogicalDevice.m_GraphicsQueue,
                textureOut,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                params.m_MipMap,
                layer_count);
            CHECK_VK_ERROR(res);

            DestroyDeviceBuffer(vk_device, &stage_buffer.m_Handle);

            vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &vk_command_buffer);

            delete[] vk_copy_regions;
        }
        else
        {
            uint32_t write_offset = GetOffsetFromMipmap(textureOut, (uint8_t) params.m_MipMap);

            VkResult res = WriteToDeviceBuffer(vk_device, texDataSize, write_offset, texDataPtr, &textureOut->m_DeviceBuffer);
            CHECK_VK_ERROR(res);

            res = TransitionImageLayout(vk_device,
                context->m_LogicalDevice.m_CommandPool,
                context->m_LogicalDevice.m_GraphicsQueue,
                textureOut,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                params.m_MipMap,
                layer_count);
            CHECK_VK_ERROR(res);
        }
    }

    static void VulkanSetTextureInternal(VulkanTexture* texture, const TextureParams& params)
    {
        // Same as graphics_opengl.cpp
        switch (params.m_Format)
        {
            case TEXTURE_FORMAT_DEPTH:
            case TEXTURE_FORMAT_STENCIL:
                dmLogError("Unable to upload texture data, unsupported type (%s).", GetTextureFormatLiteral(params.m_Format));
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
        uint16_t tex_layer_count    = dmMath::Max(texture->m_Depth, params.m_Depth);
        uint8_t tex_bpp             = GetTextureFormatBitsPerPixel(params.m_Format);
        size_t tex_data_size        = 0;
        void*  tex_data_ptr         = (void*)params.m_Data;
        VkFormat vk_format          = GetVulkanFormatFromTextureFormat(params.m_Format);

        if (vk_format == VK_FORMAT_UNDEFINED)
        {
            dmLogError("Unable to upload texture data, unsupported type (%s).", GetTextureFormatLiteral(format_orig));
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
        texture->m_Depth          = tex_layer_count;

        VulkanSetTextureParamsInternal(texture, params.m_MinFilter, params.m_MagFilter, params.m_UWrap, params.m_VWrap, 1.0f);

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
                texture->m_Format      = vk_format;
                texture->m_Width       = params.m_Width;
                texture->m_Height      = params.m_Height;

                // Note:
                // If the texture has requested mipmaps and we need to recreate the texture, make sure to allocate enough mipmaps.
                // For vulkan this means that we can't cap a texture to a specific mipmap count since the engine expects
                // that setting texture data works like the OpenGL backend where we set the mipmap count to zero and then
                // update the mipmap count based on the params. If we recreate the texture when that is detected (i.e we have too few mipmaps in the texture)
                // we will lose all the data that was previously uploaded. We could copy that data, but for now this is the easiest way of dealing with this..

                if (texture->m_MipMapCount > 1)
                {
                    texture->m_MipMapCount = (uint16_t) GetMipmapCount(dmMath::Max(texture->m_Width, texture->m_Height));
                }
            }
        }

        bool use_stage_buffer = true;
        bool memoryless = IsTextureMemoryless(texture);

#if defined(DM_PLATFORM_IOS)
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
            VkImageUsageFlags vk_usage_flags        = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            VkFormatFeatureFlags vk_format_features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            VkMemoryPropertyFlags vk_memory_type    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            if (!use_stage_buffer)
            {
                vk_usage_flags = 0;
                vk_memory_type = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            }

            vk_usage_flags |= texture->m_UsageFlags;

            if (memoryless)
            {
                vk_memory_type |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
            }

            // Check this format for optimal layout support
            if (GetSupportedTilingFormat(vk_physical_device, &vk_format, 1, vk_image_tiling, vk_format_features) == VK_FORMAT_UNDEFINED)
            {
                // Linear doesn't support mipmapping (for MoltenVK only?)
                vk_image_tiling        = VK_IMAGE_TILING_LINEAR;
                texture->m_MipMapCount = 1;
            }

            VkResult res = CreateTexture2D(vk_physical_device,
                logical_device.m_Device,
                texture->m_Width,
                texture->m_Height,
                tex_layer_count,
                texture->m_MipMapCount,
                VK_SAMPLE_COUNT_1_BIT,
                vk_format,
                vk_image_tiling,
                vk_usage_flags,
                vk_memory_type,
                VK_IMAGE_ASPECT_COLOR_BIT,
                texture);
            CHECK_VK_ERROR(res);
        }

        if (!memoryless)
        {
            tex_data_size = (int) ceil((float) tex_data_size / 8.0f);
            CopyToTexture(g_VulkanContext, params, use_stage_buffer, tex_data_size, tex_data_ptr, texture);
        }

        if (format_orig == TEXTURE_FORMAT_RGB)
        {
            delete[] (uint8_t*)tex_data_ptr;
        }
    }

    void VulkanDestroyResources(HContext _context)
    {
        VulkanContext* context = (VulkanContext*)_context;
        VkDevice vk_device = context->m_LogicalDevice.m_Device;

        context->m_PipelineCache.Iterate(DestroyPipelineCacheCb, context);

        DestroyDeviceBuffer(vk_device, &context->m_MainTextureDepthStencil.m_DeviceBuffer.m_Handle);
        DestroyTexture(vk_device, &context->m_MainTextureDepthStencil.m_Handle);
        DestroyTexture(vk_device, &context->m_DefaultTexture2D->m_Handle);
        DestroyTexture(vk_device, &context->m_DefaultTexture2DArray->m_Handle);
        DestroyTexture(vk_device, &context->m_DefaultTextureCubeMap->m_Handle);

        vkDestroyRenderPass(vk_device, context->m_MainRenderPass, 0);

        vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_MainCommandBuffers.Size(), context->m_MainCommandBuffers.Begin());
        vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &context->m_MainCommandBufferUploadHelper);

        for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
        {
            vkDestroyFramebuffer(vk_device, context->m_MainFrameBuffers[i], 0);
        }

        for (uint8_t i=0; i < context->m_TextureSamplers.Size(); i++)
        {
            DestroyTextureSampler(vk_device, &context->m_TextureSamplers[i]);
        }

        for (uint8_t i=0; i < context->m_MainScratchBuffers.Size(); i++)
        {
            DestroyDeviceBuffer(vk_device, &context->m_MainScratchBuffers[i].m_DeviceBuffer.m_Handle);
        }

        for (uint8_t i=0; i < context->m_MainDescriptorAllocators.Size(); i++)
        {
            DestroyDescriptorAllocator(vk_device, &context->m_MainDescriptorAllocators[i]);
        }

        for (uint8_t i=0; i < context->m_MainCommandBuffers.Size(); i++)
        {
            FlushResourcesToDestroy(vk_device, context->m_MainResourcesToDestroy[i]);
        }

        for (size_t i = 0; i < DM_MAX_FRAMES_IN_FLIGHT; i++) {
            FrameResource& frame_resource = context->m_FrameResources[i];
            vkDestroySemaphore(vk_device, frame_resource.m_RenderFinished, 0);
            vkDestroySemaphore(vk_device, frame_resource.m_ImageAvailable, 0);
            vkDestroyFence(vk_device, frame_resource.m_SubmitFence, 0);
        }

        DestroySwapChain(vk_device, context->m_SwapChain);
        DestroyLogicalDevice(&context->m_LogicalDevice);
        DestroyPhysicalDevice(&context->m_PhysicalDevice);
    }

    static void VulkanSetTexture(HTexture texture, const TextureParams& params)
    {
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture);
        VulkanSetTextureInternal(tex, params);
    }

    static void VulkanSetTextureAsync(HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        // Async texture loading is not supported in Vulkan, defaulting to syncronous loading until then
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture);
        VulkanSetTextureInternal(tex, params);
        if (callback)
        {
            callback(texture, user_data);
        }
    }

    static float GetMaxAnisotrophyClamped(float max_anisotropy_requested)
    {
        return dmMath::Min(max_anisotropy_requested, g_VulkanContext->m_PhysicalDevice.m_Properties.limits.maxSamplerAnisotropy);
    }

    static void VulkanSetTextureParamsInternal(VulkanTexture* texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
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

    static void VulkanSetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture);
        VulkanSetTextureParamsInternal(tex, minfilter, magfilter, uwrap, vwrap, max_anisotropy);
    }

    // NOTE: Currently over estimates the resource usage for compressed formats!
    static uint32_t VulkanGetTextureResourceSize(HTexture texture)
    {
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture);
        if (!tex)
        {
            return 0;
        }
        uint32_t size_total = 0;
        uint32_t size = tex->m_Width * tex->m_Height * dmMath::Max(1U, GetTextureFormatBitsPerPixel(tex->m_GraphicsFormat)/8);
        for(uint32_t i = 0; i < tex->m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        if (tex->m_Type == TEXTURE_TYPE_CUBE_MAP)
        {
            size_total *= 6;
        }
        return size_total + sizeof(VulkanTexture);
    }

    static uint16_t VulkanGetTextureWidth(HTexture texture)
    {
        return GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture)->m_Width;
    }

    static uint16_t VulkanGetTextureHeight(HTexture texture)
    {
        return GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture)->m_Height;
    }

    static uint16_t VulkanGetOriginalTextureWidth(HTexture texture)
    {
        return GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture)->m_OriginalWidth;
    }

    static uint16_t VulkanGetOriginalTextureHeight(HTexture texture)
    {
        return GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture)->m_OriginalHeight;
    }

    static uint16_t VulkanGetTextureDepth(HTexture texture)
    {
        return GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture)->m_Depth;
    }

    static uint8_t VulkanGetTextureMipmapCount(HTexture texture)
    {
        return GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture)->m_MipMapCount;
    }

    static TextureType VulkanGetTextureType(HTexture texture)
    {
        return GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture)->m_Type;
    }

    static uint32_t VulkanGetTextureUsageHintFlags(HTexture texture)
    {
        return GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture)->m_UsageHintFlags;
    }

    static HandleResult VulkanGetTextureHandle(HTexture texture, void** out_handle)
    {
        assert(0 && "GetTextureHandle is not implemented on Vulkan.");
        return HANDLE_RESULT_NOT_AVAILABLE;
    }

    static uint8_t VulkanGetNumTextureHandles(HTexture texture)
    {
        return 1;
    }

    static void VulkanEnableTexture(HContext context, uint32_t unit, uint8_t value_index, HTexture texture)
    {
        assert(unit < DM_MAX_TEXTURE_UNITS);
        g_VulkanContext->m_TextureUnits[unit] = texture;
    }

    static void VulkanDisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(unit < DM_MAX_TEXTURE_UNITS);
        g_VulkanContext->m_TextureUnits[unit] = 0x0;
    }

    static uint32_t VulkanGetMaxTextureSize(HContext context)
    {
        return g_VulkanContext->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D;
    }

    static uint32_t VulkanGetTextureStatusFlags(HTexture texture)
    {
        return 0;
    }

    static void VulkanReadPixels(HContext _context, void* buffer, uint32_t buffer_size)
    {
        VulkanContext* context = (VulkanContext*) _context;

        uint32_t w = context->m_WindowWidth;
        uint32_t h = context->m_WindowHeight;
        assert (buffer_size >= w * h * 4);

        HRenderTarget currentt_rt_h = context->m_CurrentRenderTarget;
        bool in_render_pass = IsRenderTargetbound(context, currentt_rt_h);

        // We can't copy an image if we are inside a render pass.
        // This is considered an expensive operation, but unless we have user facing functionality to begin/end render passes,
        // this is as good as it gets currently.
        if (in_render_pass)
        {
            EndRenderPass(context);
        }

        // Create a temporary stage buffer that we can map to
        DeviceBuffer stage_buffer(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        VkResult res = CreateDeviceBuffer(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device, buffer_size,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stage_buffer);
        CHECK_VK_ERROR(res);

        // input image must be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL, so we pick the optimal layout
        // otherwise, the validation layers will complain that this is a performance issue and spam errors..

        OneTimeCommandBuffer cmd_buffer(context);
        res = cmd_buffer.Begin();
        CHECK_VK_ERROR(res);

        VulkanTexture* tex_sc = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, context->m_CurrentSwapchainTexture);

        res = TransitionImageLayout(context->m_LogicalDevice.m_Device,
                context->m_LogicalDevice.m_CommandPool,
                context->m_LogicalDevice.m_GraphicsQueue,
                tex_sc,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        CHECK_VK_ERROR(res);

        VkBufferImageCopy vk_copy_region = {};
        vk_copy_region.imageExtent.width           = w;
        vk_copy_region.imageExtent.height          = h;
        vk_copy_region.imageExtent.depth           = 1;
        vk_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vk_copy_region.imageSubresource.layerCount = 1;

        vkCmdCopyImageToBuffer(
            cmd_buffer.m_CmdBuffer,
            context->m_SwapChain->Image(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            stage_buffer.m_Handle.m_Buffer,
            1, &vk_copy_region);

        res = cmd_buffer.End();
        CHECK_VK_ERROR(res);

        res = stage_buffer.MapMemory(context->m_LogicalDevice.m_Device);
        CHECK_VK_ERROR(res);

        memcpy(buffer, stage_buffer.m_MappedDataPtr, stage_buffer.m_MemorySize);

        stage_buffer.UnmapMemory(context->m_LogicalDevice.m_Device);

        DestroyResourceDeferred(context->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], &stage_buffer);

        if (in_render_pass)
        {
            BeginRenderPass(context, currentt_rt_h);
        }
    }

    static dmPlatform::HWindow VulkanGetWindow(HContext context)
    {
        return ((VulkanContext*) context)->m_Window;
    }

    static uint32_t VulkanGetWidth(HContext context)
    {
        return ((VulkanContext*) context)->m_Width;
    }

    static uint32_t VulkanGetHeight(HContext context)
    {
        return ((VulkanContext*) context)->m_Height;
    }

    static uint32_t VulkanGetDisplayDpi(HContext context)
    {
        return 0;
    }

    static void VulkanRunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }

    void DestroyPipelineCacheCb(VulkanContext* context, const uint64_t* key, Pipeline* value)
    {
        DestroyPipeline(g_VulkanContext->m_LogicalDevice.m_Device, value);
    }

    static bool VulkanIsContextFeatureSupported(HContext context, ContextFeature feature)
    {
        return true;
    }

    static bool VulkanIsAssetHandleValid(HContext _context, HAssetHandle asset_handle)
    {
        if (asset_handle == 0)
        {
            return false;
        }

        VulkanContext* context = (VulkanContext*) _context;
        AssetType type         = GetAssetType(asset_handle);

        if (type == ASSET_TYPE_TEXTURE)
        {
            return GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        else if (type == ASSET_TYPE_RENDER_TARGET)
        {
            return GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        return false;
    }

    static HComputeProgram VulkanNewComputeProgram(HContext _context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderDesc::Shader* ddf_shader = GetShaderProgram(_context, ddf);
        if (ddf_shader == 0x0)
        {
            return 0x0;
        }

        VulkanContext* context = (VulkanContext*) _context;
        ShaderModule* shader   = new ShaderModule;
        memset(shader, 0, sizeof(*shader));

        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf_shader->m_Source.m_Data, ddf_shader->m_Source.m_Count, VK_SHADER_STAGE_COMPUTE_BIT, shader);
        CHECK_VK_ERROR(res);
        CreateShaderMeta(&ddf->m_Reflection, &shader->m_ShaderMeta);

        if (!ValidateShaderModule(context, shader, error_buffer, error_buffer_size))
        {
            DeleteComputeProgram((HComputeProgram) shader);
            return 0;
        }

        return (HComputeProgram) shader;
    }

    static HProgram VulkanNewProgramFromCompute(HContext context, HComputeProgram compute_program)
    {
        Program* program = new Program;
        CreateComputeProgram((VulkanContext*) context, program, (ShaderModule*) compute_program);
        return (HProgram) program;
    }

    static void VulkanDeleteComputeProgram(HComputeProgram prog)
    {
        ShaderModule* shader = (ShaderModule*) prog;
        DestroyShader(shader);
        delete shader;
    }

    ///////////////////////////////////
    // dmsdk / graphics_vulkan.h impls:
    ///////////////////////////////////

    HStorageBuffer VulkanNewStorageBuffer(HContext _context, uint32_t buffer_size)
    {
        VulkanContext* context       = (VulkanContext*) _context;
        DeviceBuffer* storage_buffer = new DeviceBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        if (buffer_size > 0)
        {
            DeviceBufferUploadHelper(context, 0, buffer_size, 0, storage_buffer);
        }
        return (HStorageBuffer) storage_buffer;
    }

    void VulkanDeleteStorageBuffer(HContext context, HStorageBuffer storage_buffer)
    {
        if (!storage_buffer)
            return;

        DeviceBuffer* buffer_ptr = (DeviceBuffer*) storage_buffer;
        if (!buffer_ptr->m_Destroyed)
        {
            DestroyResourceDeferred(g_VulkanContext->m_MainResourcesToDestroy[g_VulkanContext->m_SwapChain->m_ImageIndex], buffer_ptr);
        }
        delete buffer_ptr;
    }

    void VulkanSetStorageBuffer(HContext _context, HStorageBuffer storage_buffer, uint32_t binding_index, uint32_t data_offset, HUniformLocation base_location)
    {
        VulkanContext* context = (VulkanContext*) _context;
        context->m_CurrentStorageBuffers[binding_index].m_Buffer       = storage_buffer;
        context->m_CurrentStorageBuffers[binding_index].m_BufferOffset = data_offset;

        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        Program* program_ptr = (Program*) context->m_CurrentProgram;
        uint32_t set         = UNIFORM_LOCATION_GET_VS(base_location);
        uint32_t binding     = UNIFORM_LOCATION_GET_VS_MEMBER(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        // TODO!
        assert(program_ptr->m_ComputeModule == 0x0);
        assert(program_ptr->m_ResourceBindings[set][binding].m_Res);
        program_ptr->m_ResourceBindings[set][binding].m_StorageBufferUnit = binding_index;
    }

    void VulkanSetStorageBufferData(HContext _context, HStorageBuffer storage_buffer, uint32_t size, const void* data)
    {
        DM_PROFILE(__FUNCTION__);
        if (size == 0)
        {
            return;
        }

        VulkanContext* context = (VulkanContext*) _context;
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) storage_buffer;
        if (size != buffer_ptr->m_MemorySize)
        {
            DestroyResourceDeferred(context->m_MainResourcesToDestroy[context->m_SwapChain->m_ImageIndex], buffer_ptr);
        }

        DeviceBufferUploadHelper(context, data, size, 0, buffer_ptr);
    }

    void* VulkanMapVertexBuffer(HContext context, HVertexBuffer buffer, BufferAccess access)
    {
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        VkResult res = buffer_ptr->MapMemory(g_VulkanContext->m_LogicalDevice.m_Device);
        CHECK_VK_ERROR(res);
        return buffer_ptr->m_MappedDataPtr;
    }

    bool VulkanUnmapVertexBuffer(HContext context, HVertexBuffer buffer)
    {
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        buffer_ptr->UnmapMemory(g_VulkanContext->m_LogicalDevice.m_Device);
        return true;
    }

    void* VulkanMapIndexBuffer(HContext context, HIndexBuffer buffer, BufferAccess access)
    {
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        VkResult res = buffer_ptr->MapMemory(g_VulkanContext->m_LogicalDevice.m_Device);
        CHECK_VK_ERROR(res);
        return buffer_ptr->m_MappedDataPtr;
    }

    bool VulkanUnmapIndexBuffer(HContext context, HIndexBuffer buffer)
    {
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        buffer_ptr->UnmapMemory(g_VulkanContext->m_LogicalDevice.m_Device);
        return true;
    }

    HContext VulkanGetContext()
    {
        return g_VulkanContext;
    }

    void VulkanCopyBufferToTexture(HContext _context, HVertexBuffer _buffer, HTexture _texture, uint32_t width, uint32_t height)
    {
        VulkanContext* context = (VulkanContext*) _context;
        DeviceBuffer* buffer   = (DeviceBuffer*) _buffer;
        VulkanTexture* texture = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, _texture);

        OneTimeCommandBuffer cmd_buffer(context);
        VkResult res = cmd_buffer.Begin();
        CHECK_VK_ERROR(res);

        res = TransitionImageLayout(context->m_LogicalDevice.m_Device,
                context->m_LogicalDevice.m_CommandPool,
                context->m_LogicalDevice.m_GraphicsQueue,
                texture,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CHECK_VK_ERROR(res);

        uint8_t layer_count = texture->m_Depth;
        uint32_t slice_size = buffer->m_MemorySize / layer_count;

        VkBufferImageCopy* vk_copy_regions = new VkBufferImageCopy[layer_count];
        for (int i = 0; i < layer_count; ++i)
        {
            VkBufferImageCopy& vk_copy_region = vk_copy_regions[i];
            vk_copy_region.bufferOffset                    = i * slice_size;
            vk_copy_region.bufferRowLength                 = 0;
            vk_copy_region.bufferImageHeight               = 0;
            vk_copy_region.imageOffset.x                   = 0; // x;
            vk_copy_region.imageOffset.y                   = 0; // y;
            vk_copy_region.imageOffset.z                   = 0;
            vk_copy_region.imageExtent.width               = width;
            vk_copy_region.imageExtent.height              = height;
            vk_copy_region.imageExtent.depth               = 1;
            vk_copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            vk_copy_region.imageSubresource.mipLevel       = 0; // mipmap;
            vk_copy_region.imageSubresource.baseArrayLayer = i;
            vk_copy_region.imageSubresource.layerCount     = 1;
        }

        vkCmdCopyBufferToImage(cmd_buffer.m_CmdBuffer, buffer->m_Handle.m_Buffer,
            texture->m_Handle.m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            layer_count, vk_copy_regions);

        res = cmd_buffer.End();
        CHECK_VK_ERROR(res);

        res = TransitionImageLayout(context->m_LogicalDevice.m_Device,
            context->m_LogicalDevice.m_CommandPool,
            context->m_LogicalDevice.m_GraphicsQueue,
            texture,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0, // mipmap,
            layer_count);
        CHECK_VK_ERROR(res);

        delete[] vk_copy_regions;
    }

    void VulkanNextRenderPass(HContext _context, HRenderTarget render_target)
    {
        VulkanContext* context = (VulkanContext*) _context;
        RenderTarget* rt       = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);
        if (rt->m_SubPasses != 0)
        {
            const uint8_t image_ix            = context->m_SwapChain->m_ImageIndex;
            VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
            vkCmdNextSubpass(vk_command_buffer, VK_SUBPASS_CONTENTS_INLINE);
            rt->m_SubPassIndex++;
        }
    }

    void VulkanCreateRenderPass(HContext _context, HRenderTarget render_target, const CreateRenderPassParams& params)
    {
        VulkanContext* context = (VulkanContext*) _context;
        RenderTarget* rt       = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);

        assert(rt->m_TextureDepthStencil == 0); // TODO

        DestroyFrameBuffer(context->m_LogicalDevice.m_Device, rt->m_Handle.m_Framebuffer);

        delete[] rt->m_SubPasses;
        rt->m_SubPasses    = new SubPass[params.m_SubPassCount];
        rt->m_SubPassCount = params.m_SubPassCount;

        VkSubpassDescription vk_sub_passes[MAX_SUBPASSES];
        memset(vk_sub_passes, 0, sizeof(vk_sub_passes));

        for (int i = 0; i < params.m_SubPassCount; ++i)
        {
            const RenderPassDescriptor& rp_desc = params.m_SubPasses[i];

            VkAttachmentReference* color_attachment_ref         = 0;
            VkAttachmentReference* depth_stencil_attachment_ref = 0;
            VkAttachmentReference* input_attachment_ref         = 0;

            rt->m_SubPasses[i].m_ColorAttachments.SetCapacity(rp_desc.m_ColorAttachmentIndicesCount);
            rt->m_SubPasses[i].m_ColorAttachments.SetSize(rp_desc.m_ColorAttachmentIndicesCount);
            memcpy(rt->m_SubPasses[i].m_ColorAttachments.Begin(), rp_desc.m_ColorAttachmentIndices, sizeof(uint8_t) * rp_desc.m_ColorAttachmentIndicesCount);

            rt->m_SubPasses[i].m_InputAttachments.SetCapacity(rp_desc.m_InputAttachmentIndicesCount);
            rt->m_SubPasses[i].m_InputAttachments.SetSize(rp_desc.m_InputAttachmentIndicesCount);
            memcpy(rt->m_SubPasses[i].m_InputAttachments.Begin(), rp_desc.m_InputAttachmentIndices, sizeof(uint8_t) * rp_desc.m_InputAttachmentIndicesCount);

            if (rp_desc.m_ColorAttachmentIndicesCount > 0)
            {
                color_attachment_ref = new VkAttachmentReference[rp_desc.m_ColorAttachmentIndicesCount];

                for (int j = 0; j < rp_desc.m_ColorAttachmentIndicesCount; ++j)
                {
                    color_attachment_ref[j].attachment = rp_desc.m_ColorAttachmentIndices[j] == SUBPASS_ATTACHMENT_UNUSED ? VK_ATTACHMENT_UNUSED : rp_desc.m_ColorAttachmentIndices[j];
                    color_attachment_ref[j].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
            }

            if (rp_desc.m_DepthStencilAttachmentIndex)
            {
                depth_stencil_attachment_ref             = new VkAttachmentReference;
                depth_stencil_attachment_ref->attachment = rp_desc.m_DepthStencilAttachmentIndex[0] == SUBPASS_ATTACHMENT_UNUSED ? VK_ATTACHMENT_UNUSED : rp_desc.m_DepthStencilAttachmentIndex[0];
                depth_stencil_attachment_ref->layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            if (rp_desc.m_InputAttachmentIndicesCount > 0)
            {
                input_attachment_ref = new VkAttachmentReference[rp_desc.m_InputAttachmentIndicesCount];
                for (int j = 0; j < rp_desc.m_InputAttachmentIndicesCount; ++j)
                {
                    input_attachment_ref[j].attachment = rp_desc.m_InputAttachmentIndices[j] == SUBPASS_ATTACHMENT_UNUSED ? VK_ATTACHMENT_UNUSED : rp_desc.m_InputAttachmentIndices[j];
                    input_attachment_ref[j].layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }

            vk_sub_passes[i].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            vk_sub_passes[i].colorAttachmentCount    = rp_desc.m_ColorAttachmentIndicesCount;
            vk_sub_passes[i].pColorAttachments       = color_attachment_ref;
            vk_sub_passes[i].pDepthStencilAttachment = depth_stencil_attachment_ref;
            vk_sub_passes[i].inputAttachmentCount    = rp_desc.m_InputAttachmentIndicesCount;
            vk_sub_passes[i].pInputAttachments       = input_attachment_ref;
        }

        uint32_t num_attachments = rt->m_ColorAttachmentCount + (rt->m_TextureDepthStencil ? 1 : 0);

        VkAttachmentDescription* vk_attachments = new VkAttachmentDescription[num_attachments];
        memset(vk_attachments, 0, sizeof(VkAttachmentDescription) * num_attachments);

        for (int i = 0; i < rt->m_ColorAttachmentCount; ++i)
        {
            VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, rt->m_TextureColor[i]);

            VkAttachmentDescription& attachment_color = vk_attachments[i];
            attachment_color.format         = tex->m_Format;
            attachment_color.samples        = VK_SAMPLE_COUNT_1_BIT;
            attachment_color.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment_color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment_color.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        VkSubpassDependency* vk_sub_pass_dependencies = new VkSubpassDependency[params.m_DependencyCount];
        memset(vk_sub_pass_dependencies, 0, sizeof(VkSubpassDependency) * params.m_DependencyCount);

        for (int i = 0; i < params.m_DependencyCount; ++i)
        {
            const RenderPassDependency& dep_desc = params.m_Dependencies[i];
            vk_sub_pass_dependencies[i].srcSubpass      = dep_desc.m_Src == SUBPASS_EXTERNAL ? VK_SUBPASS_EXTERNAL : dep_desc.m_Src;
            vk_sub_pass_dependencies[i].dstSubpass      = dep_desc.m_Dst == SUBPASS_EXTERNAL ? VK_SUBPASS_EXTERNAL : dep_desc.m_Dst;
            vk_sub_pass_dependencies[i].srcStageMask    = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT ; // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            vk_sub_pass_dependencies[i].srcAccessMask   = 0;
            vk_sub_pass_dependencies[i].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT ; // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            vk_sub_pass_dependencies[i].dstAccessMask   = VK_ACCESS_MEMORY_WRITE_BIT; // | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            vk_sub_pass_dependencies[i].dependencyFlags = 0; // VK_DEPENDENCY_BY_REGION_BIT;
        }

        VkRenderPassCreateInfo render_pass_create_info = {};

        render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = num_attachments;
        render_pass_create_info.pAttachments    = vk_attachments;
        render_pass_create_info.subpassCount    = params.m_SubPassCount;
        render_pass_create_info.pSubpasses      = vk_sub_passes;
        render_pass_create_info.dependencyCount = params.m_DependencyCount;
        render_pass_create_info.pDependencies   = vk_sub_pass_dependencies;

        VkResult res = vkCreateRenderPass(context->m_LogicalDevice.m_Device, &render_pass_create_info, 0, &rt->m_Handle.m_RenderPass);
        CHECK_VK_ERROR(res);

        VkImageView fb_attachments[MAX_BUFFER_COLOR_ATTACHMENTS + 1];
        memset(fb_attachments, 0, sizeof(fb_attachments));

        uint16_t    fb_attachment_count = 0;
        uint16_t    fb_width            = rt->m_Extent.width;
        uint16_t    fb_height           = rt->m_Extent.height;

        for (int i = 0; i < rt->m_ColorAttachmentCount; ++i)
        {
            VulkanTexture* color_texture_ptr      = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, rt->m_TextureColor[i]);
            fb_attachments[fb_attachment_count++] = color_texture_ptr->m_Handle.m_ImageView;
        }

        res = CreateFramebuffer(context->m_LogicalDevice.m_Device, rt->m_Handle.m_RenderPass, fb_width, fb_height, fb_attachments, (uint8_t) fb_attachment_count, &rt->m_Handle.m_Framebuffer);
        CHECK_VK_ERROR(res);

        delete[] vk_sub_pass_dependencies;
        delete[] vk_attachments;
    }

    void VulkanSetRenderTargetAttachments(HContext _context, HRenderTarget render_target, const SetRenderTargetAttachmentsParams& params)
    {
        VulkanContext* context = (VulkanContext*) _context;

        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);

        if (rt->m_Handle.m_Framebuffer != VK_NULL_HANDLE)
        {
            DestroyRenderTarget(context, rt);
        }

        BufferType color_buffer_flags[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        uint32_t rt_width  = 0;
        uint32_t rt_height = 0;

        if (params.m_SetDimensions)
        {
            rt_width  = params.m_Width;
            rt_height = params.m_Height;
        }

        BufferType buffer_types[MAX_BUFFER_COLOR_ATTACHMENTS] = {};
        for (int i = 0; i < params.m_ColorAttachmentsCount; ++i)
        {
            buffer_types[i]                      = color_buffer_flags[i];
            VulkanTexture* attachment            = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, params.m_ColorAttachments[i]);
            rt->m_ColorTextureParams[i].m_Width  = attachment->m_Width;
            rt->m_ColorTextureParams[i].m_Height = attachment->m_Height;
            rt->m_ColorBufferLoadOps[i]          = params.m_ColorAttachmentLoadOps[i];
            rt->m_ColorBufferStoreOps[i]         = params.m_ColorAttachmentStoreOps[i];

            if (params.m_SetDimensions)
            {
                rt->m_ColorTextureParams[i].m_Width  = params.m_Width;
                rt->m_ColorTextureParams[i].m_Height = params.m_Height;
            }

            rt_width  = rt->m_ColorTextureParams[i].m_Width;
            rt_height = rt->m_ColorTextureParams[i].m_Height;

            if (params.m_ColorAttachmentLoadOps[i] == ATTACHMENT_OP_CLEAR)
            {
                memcpy(rt->m_ColorAttachmentClearValue, params.m_ColorAttachmentClearValues[i], sizeof(float) * 4);
            }
        }

        VkResult res = CreateRenderTarget(context,
            (HTexture*) params.m_ColorAttachments,
            buffer_types,
            params.m_ColorAttachmentsCount,
            0, rt_width, rt_height, rt);
        CHECK_VK_ERROR(res);

        rt->m_ColorAttachmentCount = params.m_ColorAttachmentsCount;

    }

    void VulkanSetConstantBuffer(HContext _context, dmGraphics::HVertexBuffer _buffer, uint32_t buffer_offset, HUniformLocation base_location)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        Program* program_ptr = (Program*) context->m_CurrentProgram;
        uint32_t set         = UNIFORM_LOCATION_GET_VS(base_location);
        uint32_t binding     = UNIFORM_LOCATION_GET_VS_MEMBER(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        DeviceBuffer* buffer   = (DeviceBuffer*) _buffer;
        VkResult res = buffer->MapMemory(context->m_LogicalDevice.m_Device);
        CHECK_VK_ERROR(res);

        uint8_t* buffer_ptr = (uint8_t*) buffer->m_MappedDataPtr + buffer_offset;

        WriteConstantData(
            program_ptr->m_ResourceBindings[set][binding].m_DataOffset,
            program_ptr->m_UniformData,
            buffer_ptr,
            program_ptr->m_ResourceBindings[set][binding].m_Res->m_BlockSize);

        buffer->UnmapMemory(context->m_LogicalDevice.m_Device);
    }

    HTexture VulkanGetActiveSwapChainTexture(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_CurrentSwapchainTexture;
    }

    void VulkanDrawElementsInstanced(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count, uint32_t base_instance, Type type, HIndexBuffer index_buffer)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);

        VulkanContext* context = (VulkanContext*) _context;

        assert(context->m_FrameBegun);
        const uint8_t image_ix = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
        context->m_PipelineState.m_PrimtiveType = prim_type;
        DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[image_ix], (DeviceBuffer*) index_buffer, type);

        // The 'first' value that comes in is intended to be a byte offset,
        // but vkCmdDrawIndexed only operates with actual offset values into the index buffer
        uint32_t index_offset = first / (type == TYPE_UNSIGNED_SHORT ? 2 : 4);
        vkCmdDrawIndexed(vk_command_buffer, count, instance_count, index_offset, 0, base_instance);
    }

    void VulkanDrawBaseInstance(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count, uint32_t base_instance)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_FrameBegun);
        const uint8_t image_ix = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
        context->m_PipelineState.m_PrimtiveType = prim_type;
        DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[image_ix], 0, TYPE_BYTE);
        vkCmdDraw(vk_command_buffer, count, instance_count, first, base_instance);
    }

    void VulkanSetVertexDeclarationStepFunction(HContext, HVertexDeclaration vertex_declaration, VertexStepFunction step_function)
    {
        vertex_declaration->m_StepFunction = step_function;
    }

    void VulkanSetFrameInFlightCount(HContext _context, uint8_t num_frames_in_flight)
    {
        if (num_frames_in_flight > DM_MAX_FRAMES_IN_FLIGHT)
        {
            dmLogWarning("Max number of frames in flight cannot be more than %d.", DM_MAX_FRAMES_IN_FLIGHT);
            num_frames_in_flight = DM_MAX_FRAMES_IN_FLIGHT;
        }

        VulkanContext* context = (VulkanContext*) _context;
        context->m_NumFramesInFlight = num_frames_in_flight;
    }

    void VulkanEnableVertexDeclarationProgram(HContext _context, HVertexDeclaration _vertex_declaration, uint32_t binding, HVertexBuffer _vertex_buffer, HProgram program)
    {
        VulkanContext* context                       = (VulkanContext*) _context;
        Program* program_ptr                         = (Program*) program;
        ShaderModule* vertex_shader                  = program_ptr->m_VertexModule;
        DeviceBuffer* vertex_buffer                  = (DeviceBuffer*) _vertex_buffer;
        VertexDeclaration* vertex_declaration        = (VertexDeclaration*) _vertex_declaration;

        context->m_MainVertexDeclaration[binding]                = {};
        context->m_MainVertexDeclaration[binding].m_StreamCount  = vertex_declaration->m_StreamCount;
        context->m_MainVertexDeclaration[binding].m_Stride       = vertex_declaration->m_Stride;
        context->m_MainVertexDeclaration[binding].m_StepFunction = vertex_declaration->m_StepFunction;
        context->m_MainVertexDeclaration[binding].m_PipelineHash = vertex_declaration->m_PipelineHash;

        context->m_CurrentVertexBuffer[binding]                  = vertex_buffer;
        context->m_CurrentVertexDeclaration[binding]             = &context->m_MainVertexDeclaration[binding];

        uint32_t stream_ix = 0;

        for (int i = 0; i < vertex_declaration->m_StreamCount; ++i)
        {
            for (int j = 0; j < vertex_shader->m_ShaderMeta.m_Inputs.Size(); ++j)
            {
                ShaderResourceBinding& input = vertex_shader->m_ShaderMeta.m_Inputs[j];

                if (input.m_NameHash == vertex_declaration->m_Streams[i].m_NameHash)
                {
                    VertexDeclaration::Stream& stream = context->m_MainVertexDeclaration[binding].m_Streams[stream_ix];
                    stream.m_NameHash = input.m_NameHash;
                    stream.m_Location = input.m_Binding;
                    stream.m_Type     = vertex_declaration->m_Streams[i].m_Type;
                    stream.m_Offset   = vertex_declaration->m_Streams[i].m_Offset;
                    stream_ix++;
                    break;
                }
            }
        }
    }

    void VulkanDisableVertexDeclaration(HContext _context, uint32_t binding)
    {
        VulkanContext* context                       = (VulkanContext*) _context;
        context->m_CurrentVertexBuffer[binding]      = 0;
        context->m_CurrentVertexDeclaration[binding] = 0;
    }


    void VulkanSetPipelineState(HContext _context, HPipelineState ps)
    {
        VulkanContext* context     = (VulkanContext*) _context;
        context->m_PipelineState   = *ps;
        context->m_ViewportChanged = 1;
    }

    void VulkanClearTexture(HContext _context, HTexture _texture, float values[4])
    {
        VulkanContext* context = (VulkanContext*) _context;
        VulkanTexture* texture = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, _texture);

        VkClearColorValue clear_value = {};
        for (int i = 0; i < 4; ++i)
        {
            clear_value.float32[i] = values[i];
            clear_value.int32[i]   = (int32_t) values[i];
            clear_value.uint32[i]  = (uint32_t) values[i];
        }

        VkResult res = TransitionImageLayout(
            context->m_LogicalDevice.m_Device,
            context->m_LogicalDevice.m_CommandPool,
            context->m_LogicalDevice.m_GraphicsQueue,
            texture,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL);
        CHECK_VK_ERROR(res);

        OneTimeCommandBuffer cmd_buffer(context);
        res  = cmd_buffer.Begin();
        CHECK_VK_ERROR(res);

        VkImageSubresourceRange range = {};
        range.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount              = 1;
        range.layerCount              = 1;

        vkCmdClearColorImage(
            cmd_buffer.m_CmdBuffer,
            texture->m_Handle.m_Image,
            VK_IMAGE_LAYOUT_GENERAL,
            &clear_value,
            1, &range);

        res = cmd_buffer.End();
        CHECK_VK_ERROR(res);
    }

    static inline VkPipelineStageFlags GetPipelineStageFlags(uint32_t bits)
    {
        VkPipelineStageFlags flags = 0;
        if (bits & STAGE_FLAG_QUEUE_BEGIN)                flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if (bits & STAGE_FLAG_QUEUE_END)                  flags |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        if (bits & STAGE_FLAG_FRAGMENT_SHADER)            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        if (bits & STAGE_FLAG_EARLY_FRAGMENT_SHADER_TEST) flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        return flags;
    }

    static inline VkAccessFlags GetAccessFlags(uint32_t bits)
    {
        VkAccessFlags flags = 0;
        if (bits & ACCESS_FLAG_SHADER)
        {
            if (bits & ACCESS_FLAG_READ)  flags |= VK_ACCESS_SHADER_READ_BIT;
            if (bits & ACCESS_FLAG_WRITE) flags |= VK_ACCESS_SHADER_WRITE_BIT;
        }
        return flags;
    }

    void VulkanMemorybarrier(HContext _context, HTexture _texture, uint32_t src_stage_flags, uint32_t dst_stage_flags, uint32_t src_access_flags, uint32_t dst_access_flags)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_FrameBegun);

        const uint8_t image_ix            = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];

        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType           = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.pNext           = 0;
        memoryBarrier.srcAccessMask   = GetAccessFlags(src_access_flags);
        memoryBarrier.dstAccessMask   = GetAccessFlags(dst_access_flags);

        vkCmdPipelineBarrier(
            vk_command_buffer,
            GetPipelineStageFlags(src_stage_flags),
            GetPipelineStageFlags(dst_stage_flags),
            0, 1, &memoryBarrier, 0, 0, 0, 0);
    }

    void VulkanGetUniformBinding(HContext context, HProgram prog, uint32_t index, uint32_t* set_out, uint32_t* binding_out, uint32_t* member_index_out)
    {
        assert(prog);
        Program* program = (Program*) prog;
        uint32_t search_index = 0;

        for (int set = 0; set < program->m_MaxSet; ++set)
        {
            for (int binding = 0; binding < program->m_MaxBinding; ++binding)
            {
                ProgramResourceBinding& pgm_res = program->m_ResourceBindings[set][binding];

                if (pgm_res.m_Res == 0x0)
                    continue;

                if (pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_TEXTURE ||
                    pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER)
                {
                    if (search_index == index)
                    {
                        *set_out = set;
                        *binding_out = binding;
                        *member_index_out = 0;
                        return;
                    }
                    search_index++;
                }
                else if (pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_UNIFORM_BUFFER)
                {
                    // TODO: Generic type lookup is not supported yet!
                    // We can only support one level of indirection here right now
                    assert(pgm_res.m_Res->m_Type.m_UseTypeIndex);
                    const dmArray<ShaderResourceTypeInfo>& type_infos = *pgm_res.m_TypeInfos;
                    const ShaderResourceTypeInfo& type_info = type_infos[pgm_res.m_Res->m_Type.m_TypeIndex];

                    const uint32_t num_members = type_info.m_Members.Size();
                    for (int i = 0; i < num_members; ++i)
                    {
                        if (search_index == index)
                        {
                            *set_out = set;
                            *binding_out = binding;
                            *member_index_out = i;
                            return;
                        }
                        search_index++;
                    }
                }
            }
        }
        assert(0); // Should not happen
    }

    static GraphicsAdapterFunctionTable VulkanRegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table = {};
        DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, Vulkan);
        return fn_table;
    }
}
