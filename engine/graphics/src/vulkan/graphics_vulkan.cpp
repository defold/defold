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

#include <dlib/math.h>
#include <dlib/array.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/job_thread.h>
#include <dlib/thread.h>

#include <dmsdk/vectormath/cpp/vectormath_aos.h>

#include "../graphics_private.h"
#include "../graphics_native.h"
#include "../graphics_adapter.h"
#include "graphics_vulkan_defines.h"
#include "graphics_vulkan_private.h"

#include <platform/platform_window_vulkan.h>

#ifdef __MACH__
#include <vulkan/vulkan_metal.h>
#endif

DM_PROPERTY_EXTERN(rmtp_DrawCalls);
DM_PROPERTY_EXTERN(rmtp_DispatchCalls);

namespace dmGraphics
{
    static GraphicsAdapterFunctionTable VulkanRegisterFunctionTable();
    static bool                         VulkanIsSupported();
    static HContext                     VulkanGetContext();
    static GraphicsAdapter g_vulkan_adapter(ADAPTER_FAMILY_VULKAN);

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterVulkan, &g_vulkan_adapter, VulkanIsSupported, VulkanRegisterFunctionTable, VulkanGetContext, ADAPTER_FAMILY_PRIORITY_VULKAN);

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
    static void           VulkanSetTextureInternal(VulkanContext* context, VulkanTexture* texture, const TextureParams& params);
    static void           VulkanSetTextureParamsInternal(VulkanContext* context, VulkanTexture* texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy);
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
        m_JobThread               = params.m_JobThread;

        // We need to have some sort of valid default filtering
        if (m_DefaultTextureMinFilter == TEXTURE_FILTER_DEFAULT)
            m_DefaultTextureMinFilter = TEXTURE_FILTER_LINEAR;
        if (m_DefaultTextureMagFilter == TEXTURE_FILTER_DEFAULT)
            m_DefaultTextureMagFilter = TEXTURE_FILTER_LINEAR;

        assert(dmPlatform::GetWindowStateParam(m_Window, dmPlatform::WINDOW_STATE_OPENED));

        DM_STATIC_ASSERT(sizeof(m_TextureFormatSupport) * 8 >= TEXTURE_FORMAT_COUNT, Invalid_Struct_Size );
    }

    HContext VulkanGetContext()
    {
        return g_VulkanContext;
    }

    template <typename T>
    static inline void TouchResource(VulkanContext* context, T* resource)
    {
        resource->m_Handle.m_LastUsedFrame = context->m_CurrentFrameInFlight;

        if (resource->GetType() == RESOURCE_TYPE_TEXTURE)
        {
            VulkanTexture* texture = (VulkanTexture*) resource;
            TouchResource(context, &texture->m_DeviceBuffer);
        }
    }

    template <typename T>
    static void DestroyResourceDeferred(VulkanContext* context, T* resource)
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
            {
                VulkanTexture* tex = (VulkanTexture*) resource;
                resource_to_destroy.m_Texture = tex->m_Handle;
                DestroyResourceDeferred(context, &tex->m_DeviceBuffer);
                memset(tex->m_ImageLayout, 0, sizeof(tex->m_ImageLayout));
            } break;
            case RESOURCE_TYPE_DEVICE_BUFFER:
                resource_to_destroy.m_DeviceBuffer = ((DeviceBuffer*) resource)->m_Handle;
                ((DeviceBuffer*) resource)->UnmapMemory(context->m_LogicalDevice.m_Device);
                break;
            case RESOURCE_TYPE_PROGRAM:
                resource_to_destroy.m_Program = ((VulkanProgram*) resource)->m_Handle;
                break;
            case RESOURCE_TYPE_RENDER_TARGET:
                resource_to_destroy.m_RenderTarget = ((RenderTarget*) resource)->m_Handle;
                break;
            default:
                assert(0);
                break;
        }

        uint32_t frame_ix = resource->m_Handle.m_LastUsedFrame;
        if (context->m_MainResourcesToDestroy[frame_ix]->Full())
        {
            context->m_MainResourcesToDestroy[frame_ix]->OffsetCapacity(8);
        }

        context->m_MainResourcesToDestroy[frame_ix]->Push(resource_to_destroy);
        resource->m_Destroyed = 1;
    }

    static inline bool IsTextureMemoryless(VulkanTexture* texture)
    {
        return texture->m_UsageFlags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }

    static inline bool IsRenderTargetbound(VulkanContext* context, HRenderTarget rt)
    {
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
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

    static int16_t CreateVulkanTextureSampler(VulkanContext* context, VkDevice vk_device, dmArray<TextureSampler>& texture_samplers,
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
            magfilter = context->m_DefaultTextureMagFilter;
        if (minfilter == TEXTURE_FILTER_DEFAULT)
            minfilter = context->m_DefaultTextureMinFilter;

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

    static int16_t GetTextureSamplerIndex(VulkanContext* context, dmArray<TextureSampler>& texture_samplers, TextureFilter minfilter, TextureFilter magfilter,
        TextureWrap uwrap, TextureWrap vwrap, uint8_t maxLod, float max_anisotropy)
    {
        if (minfilter == TEXTURE_FILTER_DEFAULT)
            minfilter = context->m_DefaultTextureMinFilter;
        if (magfilter == TEXTURE_FILTER_DEFAULT)
            magfilter = context->m_DefaultTextureMagFilter;

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
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        RenderTarget* current_rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);

        if (!current_rt->m_IsBound)
        {
            return false;
        }

        vkCmdEndRenderPass(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight]);
        current_rt->m_IsBound = 0;
        return true;
    }

    static void BeginRenderPass(VulkanContext* context, HRenderTarget render_target)
    {
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
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

        vkCmdBeginRenderPass(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight], &vk_render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        rt->m_IsBound          = 1;
        rt->m_SubPassIndex     = 0;
        rt->m_Scissor.extent   = rt->m_Extent;
        rt->m_Scissor.offset.x = 0;
        rt->m_Scissor.offset.y = 0;

        context->m_CurrentRenderTarget = render_target;

        // We need to update the current frame stamp for all attachments and framebuffer, since they are part of the render pass
        TouchResource(context, rt);

        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt->m_TextureColor[i])
            {
                VulkanTexture* texture_color = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, rt->m_TextureColor[i]);
                TouchResource(context, texture_color);
            }
        }

        if (rt->m_TextureDepthStencil)
        {
            VulkanTexture* depth_stencil_texture = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, rt->m_TextureDepthStencil);
            TouchResource(context, depth_stencil_texture);
        }
    }

    static VkImageAspectFlags GetDefaultDepthAndStencilAspectFlags(VkFormat vk_format)
    {
        switch (vk_format)
        {
            // Depth-only formats
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM:
                return VK_IMAGE_ASPECT_DEPTH_BIT;

            // Stencil-only
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;

            // Depth+stencil formats
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

            default:
                // Fallback: assume depth only (safe but may be incomplete)
                return VK_IMAGE_ASPECT_DEPTH_BIT;
        }
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

        VkResult res = CreateTexture(
            vk_physical_device, vk_device,
            width, height, 1, 1, 1,
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
        assert(context->m_SwapChain);

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

        // Main color attachment
        attachments[0].m_Format             = context->m_SwapChain->m_SurfaceFormat.format;
        attachments[0].m_ImageLayoutInitial = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].m_ImageLayout        = context->m_SwapChain->HasMultiSampling() ?
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
                                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[0].m_LoadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].m_StoreOp            = VK_ATTACHMENT_STORE_OP_STORE;

        // Depth/stencil attachment
        attachments[1].m_Format             = depth_stencil_texture->m_Format;
        attachments[1].m_ImageLayoutInitial = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].m_ImageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[1].m_LoadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].m_StoreOp            = VK_ATTACHMENT_STORE_OP_STORE;

        // Optional resolve attachment (for MSAA)
        if (context->m_SwapChain->HasMultiSampling())
        {
            attachments[2].m_Format             = context->m_SwapChain->m_SurfaceFormat.format;
            attachments[2].m_ImageLayoutInitial = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[2].m_ImageLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            attachments[2].m_LoadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR;
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

        res = CreateCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, DM_ARRAY_SIZE(context->m_MainCommandBuffers), context->m_MainCommandBuffers);
        CHECK_VK_ERROR(res);

        // Create an additional single-time buffer for device uploading
        CreateCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &context->m_MainCommandBufferUploadHelper);

        // Create main resources-to-destroy lists, one for each command buffer
        for (uint32_t i = 0; i < DM_MAX_FRAMES_IN_FLIGHT; ++i)
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

        res = CreateMainScratchBuffers(context->m_PhysicalDevice.m_Device, vk_device, DM_MAX_FRAMES_IN_FLIGHT, buffer_size, descriptor_count_per_pool, context->m_MainDescriptorAllocators, context->m_MainScratchBuffers);
        CHECK_VK_ERROR(res);

        context->m_PipelineState = GetDefaultPipelineState();

        // Create default texture sampler
        CreateVulkanTextureSampler(context, vk_device, context->m_TextureSamplers, TEXTURE_FILTER_LINEAR, TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, 1, 1.0f);

        // Create default dummy texture
        TextureCreationParams default_texture_creation_params;
        default_texture_creation_params.m_Width          = 1;
        default_texture_creation_params.m_Height         = 1;
        default_texture_creation_params.m_LayerCount     = 1;
        default_texture_creation_params.m_OriginalWidth  = default_texture_creation_params.m_Width;
        default_texture_creation_params.m_OriginalHeight = default_texture_creation_params.m_Height;

        const uint8_t default_texture_data[4 * 6] = {}; // RGBA * 6 (for cubemap)

        TextureParams default_texture_params;
        default_texture_params.m_Width      = 1;
        default_texture_params.m_Height     = 1;
        default_texture_params.m_LayerCount = 1;
        default_texture_params.m_Data       = default_texture_data;
        default_texture_params.m_Format     = TEXTURE_FORMAT_RGBA;

        context->m_DefaultTexture2D = VulkanNewTextureInternal(default_texture_creation_params);
        VulkanSetTextureInternal(context, context->m_DefaultTexture2D, default_texture_params);

        default_texture_params.m_Format = TEXTURE_FORMAT_RGBA32UI;
        context->m_DefaultTexture2D32UI = VulkanNewTextureInternal(default_texture_creation_params);
        VulkanSetTextureInternal(context, context->m_DefaultTexture2D32UI, default_texture_params);

        default_texture_params.m_Format                 = TEXTURE_FORMAT_RGBA;
        default_texture_creation_params.m_LayerCount    = 1;
        default_texture_creation_params.m_Type          = TEXTURE_TYPE_IMAGE_2D;
        default_texture_creation_params.m_UsageHintBits = TEXTURE_USAGE_FLAG_STORAGE | TEXTURE_USAGE_FLAG_SAMPLE;
        context->m_DefaultStorageImage2D                = VulkanNewTextureInternal(default_texture_creation_params);
        VulkanSetTextureInternal(context, context->m_DefaultStorageImage2D, default_texture_params);

        default_texture_creation_params.m_UsageHintBits = TEXTURE_USAGE_FLAG_SAMPLE;
        default_texture_creation_params.m_Type          = TEXTURE_TYPE_2D_ARRAY;
        default_texture_creation_params.m_LayerCount    = 1;
        context->m_DefaultTexture2DArray                = VulkanNewTextureInternal(default_texture_creation_params);
        VulkanSetTextureInternal(context, context->m_DefaultTexture2DArray, default_texture_params);

        default_texture_creation_params.m_Type          = TEXTURE_TYPE_CUBE_MAP;
        default_texture_creation_params.m_Depth         = 1;
        default_texture_creation_params.m_LayerCount    = 6;
        context->m_DefaultTextureCubeMap = VulkanNewTextureInternal(default_texture_creation_params);
        VulkanSetTextureInternal(context, context->m_DefaultTextureCubeMap, default_texture_params);

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

        // Reset the image layout
        memset(depth_stencil_texture->m_ImageLayout, 0, sizeof(depth_stencil_texture->m_ImageLayout));

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

    static void DestroyResourceImmediatedly(VulkanContext* context, ResourceToDestroy* resource)
    {
        switch(resource->m_ResourceType)
        {
            case RESOURCE_TYPE_DEVICE_BUFFER:
                DestroyDeviceBuffer(context->m_LogicalDevice.m_Device, &resource->m_DeviceBuffer);
                break;
            case RESOURCE_TYPE_TEXTURE:
                DestroyTexture(context->m_LogicalDevice.m_Device, &resource->m_Texture);
                break;
            case RESOURCE_TYPE_PROGRAM:
                DestroyProgram(context->m_LogicalDevice.m_Device, &resource->m_Program);
                break;
            case RESOURCE_TYPE_RENDER_TARGET:
                DestroyRenderTarget(context->m_LogicalDevice.m_Device, &resource->m_RenderTarget);
                break;
            case RESOURCE_TYPE_COMMAND_BUFFER:
                vkFreeCommandBuffers(
                    context->m_LogicalDevice.m_Device,
                    resource->m_CommandBuffer.m_CommandPool,
                    1, &resource->m_CommandBuffer.m_CommandBuffer);
                break;
            default:
                assert(0);
                break;
        }
    }

    void FlushResourcesToDestroy(VulkanContext* context, ResourcesToDestroyList* resource_list)
    {
        if (resource_list->Size() > 0)
        {
            for (uint32_t i = 0; i < resource_list->Size(); ++i)
            {
                DestroyResourceImmediatedly(context, &resource_list->Begin()[i]);
            }

            resource_list->SetSize(0);
        }
    }

    // Note: fence_resources ptr is no longer valid after this call!
    static void DestroyFenceResourcesImmediately(VulkanContext* context, HOpaqueHandle fence_resource_handle, FenceResourcesToDestroy* fence_resources)
    {
        vkDestroyFence(context->m_LogicalDevice.m_Device, fence_resources->m_Fence, NULL);

        for (int j = 0; j < fence_resources->m_ResourcesCount; ++j)
        {
            DestroyResourceImmediatedly(context, &fence_resources->m_Resources[j]);
        }

        context->m_FenceResourcesToDestroy.Release(fence_resource_handle);

        delete fence_resources;
    }

    void FlushFenceResourcesToDestroy(VulkanContext* context)
    {
        uint32_t num_fence_resources = context->m_FenceResourcesToDestroy.Capacity();

        for (int i = 0; i < num_fence_resources; ++i)
        {
            FenceResourcesToDestroy* fence_resources = context->m_FenceResourcesToDestroy.GetByIndex(i);
            if (fence_resources && vkGetFenceStatus(context->m_LogicalDevice.m_Device, fence_resources->m_Fence) == VK_SUCCESS)
            {
                DestroyFenceResourcesImmediately(context, context->m_FenceResourcesToDestroy.IndexToHandle(i), fence_resources);
            }
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

    static bool VulkanIsExtensionSupported(HContext _context, const char* ext_name)
    {
        VulkanContext* context = (VulkanContext*)_context;
        return IsDeviceExtensionSupported(&context->m_PhysicalDevice, ext_name);
    }

    static uint32_t VulkanGetNumSupportedExtensions(HContext _context)
    {
        VulkanContext* context = (VulkanContext*)_context;
        return context->m_PhysicalDevice.m_DeviceExtensionCount;
    }

    static const char* VulkanGetSupportedExtension(HContext _context, uint32_t index)
    {
        VulkanContext* context = (VulkanContext*)_context;
        return context->m_PhysicalDevice.m_DeviceExtensions[index].extensionName;
    }

    static PipelineState VulkanGetPipelineState(HContext _context)
    {
        VulkanContext* context = (VulkanContext*)_context;
        return context->m_PipelineState;
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
            context->m_ASTCSupport = 1;
            context->m_ASTCArrayTextureSupport = 1;
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

        PhysicalDevice* device_list     = new PhysicalDevice[device_count];
        PhysicalDevice* selected_device = NULL;
        GetPhysicalDevices(context->m_Instance, &device_list, device_count, NULL);

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

        if (VulkanIsExtensionSupported((HContext) context, VK_EXT_METAL_OBJECTS_EXTENSION_NAME))
        {
            device_extensions.OffsetCapacity(1);
            device_extensions.Push(VK_EXT_METAL_OBJECTS_EXTENSION_NAME);
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
            context->m_FragmentShaderInterlockFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT;
            context->m_FragmentShaderInterlockFeatures.pNext = NULL;

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
        context->m_FenceResourcesToDestroy.Allocate(8);

        context->m_AsyncProcessingSupport = context->m_JobThread != 0x0 && dmThread::PlatformHasThreadSupport();
        if (context->m_AsyncProcessingSupport)
        {
            InitializeSetTextureAsyncState(context->m_SetTextureAsyncState);
            context->m_AssetHandleContainerMutex = dmMutex::New();
        }

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
            dmAtomicStore32(&context->m_DeleteContextRequested, 1);

            if (context->m_Instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(context->m_Instance, 0);
                context->m_Instance = VK_NULL_HANDLE;
            }

            if (context->m_AssetHandleContainerMutex)
            {
                dmMutex::Delete(context->m_AssetHandleContainerMutex);
            }

            ResetSetTextureAsyncState(context->m_SetTextureAsyncState);

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

        VkDevice vk_device = context->m_LogicalDevice.m_Device;
        uint32_t frameInFlight = context->m_CurrentFrameInFlight;
        FrameResource& currentFrame = context->m_FrameResources[frameInFlight];

        // Wait for GPU to finish work for this frame-in-flight
        vkWaitForFences(vk_device, 1, &currentFrame.m_SubmitFence, VK_TRUE, UINT64_MAX);
        vkResetFences(vk_device, 1, &currentFrame.m_SubmitFence);

        // Acquire next swap chain image
        VkResult res = context->m_SwapChain->Advance(vk_device, currentFrame.m_ImageAvailable);
        if (res != VK_SUCCESS)
        {
            if (res == VK_ERROR_OUT_OF_DATE_KHR)
            {
                context->m_WindowWidth  = dmPlatform::GetWindowWidth(context->m_Window);
                context->m_WindowHeight = dmPlatform::GetWindowHeight(context->m_Window);
                SwapChainChanged(context, &context->m_WindowWidth, &context->m_WindowHeight, 0, 0);
                res = context->m_SwapChain->Advance(vk_device, currentFrame.m_ImageAvailable);
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

        // Flush per-swapchain-image resources to destroy
        if (context->m_MainResourcesToDestroy[frameInFlight]->Size() > 0)
        {
            FlushResourcesToDestroy(context, context->m_MainResourcesToDestroy[frameInFlight]);
        }

        if (context->m_FenceResourcesToDestroy.Capacity() > 0)
        {
            FlushFenceResourcesToDestroy(context);
        }

        // Reset per-frame scratch buffer and descriptor pools
        ScratchBuffer* scratch = &context->m_MainScratchBuffers[frameInFlight];
        ResetScratchBuffer(vk_device, scratch);

        res = scratch->m_DeviceBuffer.MapMemory(vk_device);
        CHECK_VK_ERROR(res);

        // Begin command buffer for this frame-in-flight
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        VkCommandBuffer cmd = context->m_MainCommandBuffers[frameInFlight];
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Set framebuffer for the acquired swap chain image
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_MainRenderTarget);
        rt->m_Handle.m_Framebuffer = context->m_MainFrameBuffers[context->m_SwapChain->m_ImageIndex];

        context->m_FrameBegun      = 1;
        context->m_CurrentPipeline = 0;

        // Update current swapchain texture for rendering
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

        uint32_t frameInFlight = context->m_CurrentFrameInFlight;
        FrameResource& currentFrame = context->m_FrameResources[frameInFlight];

        // Determine the swapchain image we rendered to
        uint32_t swapchainImageIndex = context->m_SwapChain->m_ImageIndex;

        // End the current render pass
        EndRenderPass(context);

        // Finish recording the command buffer for this frame-in-flight
        VkCommandBuffer cmd = context->m_MainCommandBuffers[frameInFlight];
        VkResult res = vkEndCommandBuffer(cmd);
        CHECK_VK_ERROR(res);

        // Submit the command buffer
        VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &currentFrame.m_ImageAvailable;
        submitInfo.pWaitDstStageMask    = &waitStages;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &currentFrame.m_RenderFinished;

        res = vkQueueSubmit(context->m_LogicalDevice.m_GraphicsQueue, 1, &submitInfo, currentFrame.m_SubmitFence);
        CHECK_VK_ERROR(res);

        // Present the swapchain image
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &currentFrame.m_RenderFinished;
        presentInfo.swapchainCount     = 1;
        presentInfo.pSwapchains        = &context->m_SwapChain->m_SwapChain;
        presentInfo.pImageIndices      = &swapchainImageIndex;
        presentInfo.pResults           = nullptr;

        res = vkQueuePresentKHR(context->m_LogicalDevice.m_PresentQueue, &presentInfo);
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
        {
            CHECK_VK_ERROR(res);
        }

        // Advance frame-in-flight index
        context->m_CurrentFrameInFlight = (context->m_CurrentFrameInFlight + 1) % context->m_NumFramesInFlight;
        context->m_FrameBegun = 0;

    #if defined(ANDROID) || defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_VENDOR)
        dmPlatform::SwapBuffers(context->m_Window);
    #endif
    }

    static inline bool FormatHasDepth(VkFormat fmt)
    {
        switch (fmt) {
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return true;
            default:
                return false;
        }
    }

    static inline bool FormatHasStencil(VkFormat fmt) {
        switch (fmt) {
            case VK_FORMAT_S8_UINT:
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return true;
            default:
                return false;
        }
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
            VkFormat depth_stencil_format;

            if (current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
            {
                depth_stencil_format = context->m_MainTextureDepthStencil.m_Format;
            }
            else
            {
                depth_stencil_format = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, current_rt->m_TextureDepthStencil)->m_Format;
            }

            VkImageAspectFlags vk_aspect = 0;

            if ((flags & BUFFER_TYPE_DEPTH_BIT) && FormatHasDepth(depth_stencil_format))
            {
                vk_aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
            }

            if ((flags & BUFFER_TYPE_STENCIL_BIT) && FormatHasStencil(depth_stencil_format))
            {
                vk_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            if (vk_aspect != 0)
            {
                VkClearAttachment& vk_depth_attachment = vk_clear_attachments[attachment_count++];
                vk_depth_attachment.aspectMask = vk_aspect;
                vk_depth_attachment.clearValue.depthStencil.stencil = stencil;
                vk_depth_attachment.clearValue.depthStencil.depth   = depth;
            }
        }

        vkCmdClearAttachments(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight],
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
            vk_command_buffer = context->m_MainCommandBuffers[context->m_CurrentFrameInFlight];
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

    static Pipeline* GetOrCreateComputePipeline(VkDevice vk_device, PipelineCache& pipelineCache, VulkanProgram* program)
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
        VulkanProgram* program, RenderTarget* rt, VertexDeclaration** vertexDeclaration, uint32_t vertexDeclarationCount)
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
            if (res == VK_ERROR_INITIALIZATION_FAILED)
            {
                dmLogError("Failed to create VkPipeline");
                return 0;
            }
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

    static void SetDeviceBuffer(VulkanContext* context, DeviceBuffer* buffer, uint32_t size, uint32_t offset, const void* data)
    {
        if (size == 0)
        {
            return;
        }

        if (offset == 0)
        {
            // Coherent memory writes does not seem to be properly synced on MoltenVK,
            // so for now we always mark the old buffer for destruction when updating the data.
        #ifndef __MACH__
            if (size != buffer->m_MemorySize)
        #endif
            {
                DestroyResourceDeferred(context, buffer);
            }
        }

        DeviceBufferUploadHelper(context, data, size, offset, buffer);
    }

    static HUniformBuffer VulkanNewUniformBuffer(HContext _context, const UniformBufferLayout& layout)
    {
        VulkanUniformBuffer* ubo    = new VulkanUniformBuffer();
        ubo->m_DeviceBuffer.m_Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        ubo->m_BaseUniformBuffer.m_Layout       = layout;
        ubo->m_BaseUniformBuffer.m_BoundSet     = UNUSED_BINDING_OR_SET;
        ubo->m_BaseUniformBuffer.m_BoundBinding = UNUSED_BINDING_OR_SET;

        return (HUniformBuffer) ubo;
    }

    static void VulkanSetUniformBuffer(HContext _context, HUniformBuffer uniform_buffer, uint32_t offset, uint32_t size, const void* data)
    {
        VulkanContext* context   = (VulkanContext*)_context;
        VulkanUniformBuffer* ubo = (VulkanUniformBuffer*) uniform_buffer;
        assert(offset + size <= ubo->m_BaseUniformBuffer.m_Layout.m_Size);
        SetDeviceBuffer(context, &ubo->m_DeviceBuffer, size, offset, data);
    }

    static void VulkanDisableUniformBuffer(HContext _context, HUniformBuffer uniform_buffer)
    {
        VulkanContext* context = (VulkanContext*)_context;
        VulkanUniformBuffer* ubo = (VulkanUniformBuffer*) uniform_buffer;

        if (ubo->m_BaseUniformBuffer.m_BoundSet == UNUSED_BINDING_OR_SET || ubo->m_BaseUniformBuffer.m_BoundBinding == UNUSED_BINDING_OR_SET)
        {
            return;
        }

        if (context->m_CurrentUniformBuffers[ubo->m_BaseUniformBuffer.m_BoundSet][ubo->m_BaseUniformBuffer.m_BoundBinding] == ubo)
        {
            context->m_CurrentUniformBuffers[ubo->m_BaseUniformBuffer.m_BoundSet][ubo->m_BaseUniformBuffer.m_BoundBinding] = 0;
        }

        ubo->m_BaseUniformBuffer.m_BoundSet     = UNUSED_BINDING_OR_SET;
        ubo->m_BaseUniformBuffer.m_BoundBinding = UNUSED_BINDING_OR_SET;
    }

    static void VulkanEnableUniformBuffer(HContext _context, HUniformBuffer uniform_buffer, uint32_t binding, uint32_t set)
    {
        VulkanContext* context = (VulkanContext*)_context;
        VulkanUniformBuffer* ubo = (VulkanUniformBuffer*) uniform_buffer;

        ubo->m_BaseUniformBuffer.m_BoundBinding = binding;
        ubo->m_BaseUniformBuffer.m_BoundSet     = set;

        if (context->m_CurrentUniformBuffers[set][binding])
        {
            VulkanDisableUniformBuffer(context, (HUniformBuffer) context->m_CurrentUniformBuffers[set][binding]);
        }

        context->m_CurrentUniformBuffers[set][binding] = ubo;
    }

    static void VulkanDeleteUniformBuffer(HContext _context, HUniformBuffer uniform_buffer)
    {
        VulkanContext* context = (VulkanContext*)_context;
        VulkanUniformBuffer* ubo = (VulkanUniformBuffer*) uniform_buffer;

        VulkanDisableUniformBuffer(_context, uniform_buffer);

        if (!ubo->m_DeviceBuffer.m_Destroyed)
        {
            DestroyResourceDeferred(context, &ubo->m_DeviceBuffer);
        }

        delete ubo;
    }

    static HVertexBuffer VulkanNewVertexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VulkanContext* context = (VulkanContext*)_context;
        VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (buffer_usage & BUFFER_USAGE_TRANSFER)
        {
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        DeviceBuffer* buffer = new DeviceBuffer(usage_flags);

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
            DestroyResourceDeferred(g_VulkanContext, buffer_ptr);
        }
        delete buffer_ptr;
    }

    static void VulkanSetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);
        SetDeviceBuffer(g_VulkanContext, (DeviceBuffer*) buffer, size, 0, data);
    }

    static void VulkanSetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(__FUNCTION__);
        assert(size > 0);
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        assert(offset + size <= buffer_ptr->m_MemorySize);
        DeviceBufferUploadHelper(g_VulkanContext, data, size, offset, buffer_ptr);
    }

    static uint32_t VulkanGetVertexBufferSize(HVertexBuffer buffer)
    {
        if (!buffer)
        {
            return 0;
        }
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        return buffer_ptr->m_MemorySize;
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

    static HIndexBuffer VulkanNewIndexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VulkanContext* context = (VulkanContext*) _context;
        VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (buffer_usage & BUFFER_USAGE_TRANSFER)
        {
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        DeviceBuffer* buffer = new DeviceBuffer(usage_flags);

        if (size > 0)
        {
            DeviceBufferUploadHelper(context, data, size, 0, buffer);
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
            DestroyResourceDeferred(g_VulkanContext, buffer_ptr);
        }
        delete buffer_ptr;
    }

    static void VulkanSetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);
        SetDeviceBuffer(g_VulkanContext, (DeviceBuffer*) buffer, size, 0, data);
    }

    static void VulkanSetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(__FUNCTION__);
        assert(buffer);
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        assert(offset + size < buffer_ptr->m_MemorySize);
        DeviceBufferUploadHelper(g_VulkanContext, data, size, 0, buffer_ptr);
    }

    static uint32_t VulkanGetIndexBufferSize(HIndexBuffer buffer)
    {
        if (!buffer)
        {
            return 0;
        }
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        return buffer_ptr->m_MemorySize;
    }

    static bool VulkanIsIndexBufferFormatSupported(HContext _context, IndexBufferFormat format)
    {
        // From VkPhysicalDeviceFeatures spec:
        //   "fullDrawIndexUint32 - If this feature is supported, maxDrawIndexedIndexValue must be 2^32-1;
        //   otherwise it must be no smaller than 2^24-1."
        return true;
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
            vd->m_Stride                += stream.m_Size * GetTypeSize(stream.m_Type);

            dmHashUpdateBuffer64(hash, &stream.m_Size, sizeof(stream.m_Size));
            dmHashUpdateBuffer64(hash, &stream.m_Type, sizeof(stream.m_Type));
            dmHashUpdateBuffer64(hash, &vd->m_Streams[i].m_Type, sizeof(vd->m_Streams[i].m_Type));
        }

        vd->m_Stride = DM_ALIGN(vd->m_Stride, 4);

        return vd;
    }

    static HVertexDeclaration VulkanNewVertexDeclaration(HContext _context, HVertexStreamDeclaration stream_declaration)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, stream_declaration);
        dmHashUpdateBuffer64(&decl_hash_state, &vd->m_Stride, sizeof(vd->m_Stride));
        vd->m_PipelineHash = dmHashFinal64(&decl_hash_state);
        vd->m_StepFunction = stream_declaration->m_StepFunction;
        return vd;
    }

    static HVertexDeclaration VulkanNewVertexDeclarationStride(HContext _context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, stream_declaration);
        dmHashUpdateBuffer64(&decl_hash_state, &stride, sizeof(stride));
        vd->m_Stride       = stride;
        vd->m_PipelineHash = dmHashFinal64(&decl_hash_state);
        vd->m_StepFunction = stream_declaration->m_StepFunction;
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

    static void VulkanEnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program)
    {
        VulkanContext* context     = (VulkanContext*) _context;
        VulkanProgram* program_ptr = (VulkanProgram*) program;

        context->m_MainVertexDeclaration[binding_index]                = {};
        context->m_MainVertexDeclaration[binding_index].m_Stride       = vertex_declaration->m_Stride;
        context->m_MainVertexDeclaration[binding_index].m_StepFunction = vertex_declaration->m_StepFunction;
        context->m_MainVertexDeclaration[binding_index].m_PipelineHash = vertex_declaration->m_PipelineHash;

        context->m_CurrentVertexDeclaration[binding_index]             = &context->m_MainVertexDeclaration[binding_index];
        context->m_CurrentVertexBufferOffset[binding_index]            = base_offset;

        uint32_t stream_ix = 0;
        uint32_t num_inputs = program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs.Size();

        for (int i = 0; i < vertex_declaration->m_StreamCount; ++i)
        {
            for (int j = 0; j < num_inputs; ++j)
            {
                ShaderResourceBinding& input = program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs[j];

                if (input.m_StageFlags & SHADER_STAGE_FLAG_VERTEX && input.m_NameHash == vertex_declaration->m_Streams[i].m_NameHash)
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
            {
                context->m_CurrentVertexDeclaration[i]  = 0;
                context->m_CurrentVertexBufferOffset[i] = 0;
            }
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

    static inline VkDescriptorType TextureTypeToDescriptorType(ShaderDesc::ShaderDataType type)
    {
        switch(type)
        {
            // Sampler objects
            case ShaderDesc::SHADER_TYPE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_SAMPLER;

            // Combined samplers
            case ShaderDesc::SHADER_TYPE_SAMPLER2D:
            case ShaderDesc::SHADER_TYPE_SAMPLER3D:
            case ShaderDesc::SHADER_TYPE_SAMPLER_CUBE:
            case ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY:
            case ShaderDesc::SHADER_TYPE_SAMPLER3D_ARRAY:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

            case ShaderDesc::SHADER_TYPE_IMAGE2D:
            case ShaderDesc::SHADER_TYPE_UIMAGE2D:
            case ShaderDesc::SHADER_TYPE_IMAGE3D:
            case ShaderDesc::SHADER_TYPE_UIMAGE3D:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

            // Render pass inputs
            case ShaderDesc::SHADER_TYPE_RENDER_PASS_INPUT:
                return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

            // Texture objects
            case ShaderDesc::SHADER_TYPE_TEXTURE_CUBE:
            case ShaderDesc::SHADER_TYPE_TEXTURE2D:
            case ShaderDesc::SHADER_TYPE_TEXTURE2D_ARRAY:
            case ShaderDesc::SHADER_TYPE_UTEXTURE2D:
            case ShaderDesc::SHADER_TYPE_TEXTURE3D:
            case ShaderDesc::SHADER_TYPE_TEXTURE3D_ARRAY:
            case ShaderDesc::SHADER_TYPE_UTEXTURE3D:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

            // Storage buffers
            case ShaderDesc::SHADER_TYPE_STORAGE_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            default:break;
        }

        assert(0 && "Unsupported shader type");
        return (VkDescriptorType) -1;
    }

    static void UpdateImageDescriptor(VulkanContext* context, HTexture texture_handle, ShaderResourceBinding* binding, VkDescriptorImageInfo& vk_image_info, VkWriteDescriptorSet& vk_write_desc_info)
    {
        VulkanTexture* texture = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture_handle);

        if (texture == 0x0)
        {
            texture = GetDefaultTexture(context, binding->m_Type.m_ShaderType);
        }

        if (texture->m_PendingUpload != INVALID_OPAQUE_HANDLE)
        {
            FenceResourcesToDestroy* pending_upload = context->m_FenceResourcesToDestroy.Get(texture->m_PendingUpload);

            // We need this resource now, so we have to wait for the fence to trigger.
            // If pending_upload is NULL, the request has already been satisfied so we don't have to do anything.
            if (pending_upload)
            {
                vkWaitForFences(context->m_LogicalDevice.m_Device, 1, &pending_upload->m_Fence, VK_TRUE, UINT64_MAX);

                // It's now safe to release the resources held by the fence
                DestroyFenceResourcesImmediately(context, texture->m_PendingUpload, pending_upload);
            }

            texture->m_PendingUpload = INVALID_OPAQUE_HANDLE;
        }

        TouchResource(context, texture);

        VkImageLayout image_layout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkSampler image_sampler          = context->m_TextureSamplers[texture->m_TextureSamplerIndex].m_Sampler;
        VkImageView image_view           = texture->m_Handle.m_ImageView;
        VkDescriptorType descriptor_type = TextureTypeToDescriptorType(binding->m_Type.m_ShaderType);

        if (binding->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_RENDER_PASS_INPUT)
        {
            image_sampler = 0;
        }
        else if (binding->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_IMAGE2D || binding->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_UIMAGE2D)
        {
            image_layout  = VK_IMAGE_LAYOUT_GENERAL;
            image_sampler = 0;
        }
        else if (binding->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
        {
            image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_view   = VK_NULL_HANDLE;
        }

        // If the image layout is in the wrong state, we need to transition it to the new layout,
        // otherwise its memory might be getting wrriten to while we are reading from it.
        //
        // TODO: We cannot use the in-frame command buffer here since it's illegal to use barriers while inside a render pass
        //       So instead we have to use a temporary command buffer that only does the transition.
        //       It would be better if we could decouple this somehow. Perhaps we can solve it by buffering all render events (with a render graph?)
        if (texture->m_ImageLayout[0] != image_layout && image_layout != VK_IMAGE_LAYOUT_UNDEFINED)
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

    static void UpdateDescriptorSets(VulkanContext* context, VkDevice vk_device, VkDescriptorSet* vk_descriptor_sets, VulkanProgram* program, ScratchBuffer* scratch_buffer, uint32_t* dynamic_offsets, uint32_t dynamic_alignment)
    {
        const uint32_t max_write_descriptors = MAX_SET_COUNT * MAX_BINDINGS_PER_SET_COUNT;
        VkWriteDescriptorSet vk_write_descriptors[max_write_descriptors];
        VkDescriptorImageInfo vk_write_image_descriptors[max_write_descriptors];
        VkDescriptorBufferInfo vk_write_buffer_descriptors[max_write_descriptors];

        uint16_t uniform_to_write_index = 0;
        uint16_t image_to_write_index   = 0;
        uint16_t buffer_to_write_index  = 0;

        uint32_t dynamic_offset_index = 0;

        ProgramResourceBindingIterator it(&program->m_BaseProgram);
        const ProgramResourceBinding* next;
        while((next = it.Next()))
        {
            ShaderResourceBinding* res = next->m_Res;

            VkWriteDescriptorSet& vk_write_desc_info = vk_write_descriptors[uniform_to_write_index];
            vk_write_desc_info.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vk_write_desc_info.pNext                 = 0;
            vk_write_desc_info.dstSet                = vk_descriptor_sets[res->m_Set];
            vk_write_desc_info.dstBinding            = res->m_Binding;
            vk_write_desc_info.dstArrayElement       = 0;
            vk_write_desc_info.descriptorCount       = 1;
            vk_write_desc_info.pImageInfo            = 0;
            vk_write_desc_info.pBufferInfo           = 0;
            vk_write_desc_info.pTexelBufferView      = 0;

            switch(res->m_BindingFamily)
            {
                case BINDING_FAMILY_TEXTURE:
                    UpdateImageDescriptor(context,
                        context->m_TextureUnits[next->m_TextureUnit],
                        res,
                        vk_write_image_descriptors[image_to_write_index++],
                        vk_write_desc_info);
                    break;
                case BINDING_FAMILY_STORAGE_BUFFER:
                {
                    const StorageBufferBinding binding = context->m_CurrentStorageBuffers[next->m_StorageBufferUnit];

                    DeviceBuffer* ssbo_buffer = (DeviceBuffer*) binding.m_Buffer;
                    TouchResource(context, ssbo_buffer);
                    UpdateUniformBufferDescriptor(context,
                        ssbo_buffer->m_Handle.m_Buffer,
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        vk_write_buffer_descriptors[buffer_to_write_index++],
                        vk_write_desc_info,
                        binding.m_BufferOffset,
                        VK_WHOLE_SIZE);
                } break;
                case BINDING_FAMILY_UNIFORM_BUFFER:
                {
                    VulkanUniformBuffer* bound_ubo = context->m_CurrentUniformBuffers[res->m_Set][res->m_Binding];

                    if (bound_ubo)
                    {
                        UniformBufferLayout* pgm_layout = (UniformBufferLayout*) next->m_BindingUserData;
                        if (bound_ubo->m_BaseUniformBuffer.m_Layout.m_Hash != pgm_layout->m_Hash)
                        {
                            dmLogWarning("Uniform buffer with hash %d has an incompatible layout with the currently bound program at the shader binding '%s' (hash=%d)",
                                bound_ubo->m_BaseUniformBuffer.m_Layout.m_Hash,
                                res->m_Name,
                                pgm_layout->m_Hash);

                            // Fallback to the scratch buffer uniform setup
                            bound_ubo = 0;
                        }
                    }

                    if (bound_ubo)
                    {
                        // TODO: We shouldn't have to rebind this UBO every time it has to be used,
                        //       but in order for us to do that we need persistent descriptor sets.
                        //       To solve that, we should cache bindings. For now we simply update
                        //       the descriptor every frame, like animals..
                        UpdateUniformBufferDescriptor(context,
                            bound_ubo->m_DeviceBuffer.m_Handle.m_Buffer,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                            vk_write_buffer_descriptors[buffer_to_write_index++],
                            vk_write_desc_info,
                            0,
                            bound_ubo->m_BaseUniformBuffer.m_Layout.m_Size);
                        TouchResource(context, &bound_ubo->m_DeviceBuffer);
                    }
                    else
                    {
                        dynamic_offsets[dynamic_offset_index] = (uint32_t) scratch_buffer->m_MappedDataCursor;
                        const uint32_t uniform_size_nonalign  = res->m_BindingInfo.m_BlockSize;
                        const uint32_t uniform_size_align     = DM_ALIGN(uniform_size_nonalign, dynamic_alignment);

                        assert(uniform_size_nonalign > 0);

                        // Copy client data to aligned host memory
                        // The data_offset here is the offset into the programs uniform data,
                        // i.e the source buffer.
                        memcpy(&((uint8_t*)scratch_buffer->m_DeviceBuffer.m_MappedDataPtr)[scratch_buffer->m_MappedDataCursor],
                            &program->m_UniformData[next->m_UniformBufferOffset], uniform_size_nonalign);

                        UpdateUniformBufferDescriptor(context,
                            scratch_buffer->m_DeviceBuffer.m_Handle.m_Buffer,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                            vk_write_buffer_descriptors[buffer_to_write_index++],
                            vk_write_desc_info,
                            0,
                            uniform_size_align);

                        scratch_buffer->m_MappedDataCursor += uniform_size_align;
                        TouchResource(context, &scratch_buffer->m_DeviceBuffer);

                        dynamic_offset_index++;
                    }
                } break;
                case BINDING_FAMILY_GENERIC:
                default: continue;
            }

            uniform_to_write_index++;
        }

        vkUpdateDescriptorSets(vk_device, uniform_to_write_index, vk_write_descriptors, 0, 0);
    }

    static VkResult CommitUniforms(VulkanContext* context, VkCommandBuffer vk_command_buffer, VkDevice vk_device,
        VulkanProgram* program_ptr, VkPipelineBindPoint bind_point,
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
        DestroyResourceDeferred(context, &scratchBuffer->m_DeviceBuffer);

        VkResult res = CreateScratchBuffer(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device,
            newDataSize, false, scratchBuffer->m_DescriptorAllocator, scratchBuffer);
        scratchBuffer->m_DeviceBuffer.MapMemory(context->m_LogicalDevice.m_Device);

        return res;
    }

    static void PrepareScatchBuffer(VulkanContext* context, ScratchBuffer* scratchBuffer, VulkanProgram* program_ptr)
    {
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
        VulkanProgram* program_ptr = context->m_CurrentProgram;
        assert(program_ptr->m_ComputeModule);

        PrepareScatchBuffer(context, scratchBuffer, program_ptr);

        // Write the uniform data to the descriptors
        uint32_t dynamic_alignment = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment;
        VkResult res               = CommitUniforms(context, vk_command_buffer, vk_device, program_ptr, VK_PIPELINE_BIND_POINT_COMPUTE, scratchBuffer, context->m_DynamicOffsetBuffer, dynamic_alignment);
        CHECK_VK_ERROR(res);

        Pipeline* pipeline = GetOrCreateComputePipeline(vk_device, context->m_PipelineCache, program_ptr);
        vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    }

    static bool DrawSetup(VulkanContext* context, VkCommandBuffer vk_command_buffer, ScratchBuffer* scratchBuffer, DeviceBuffer* indexBuffer, Type indexBufferType)
    {
        RenderTarget* current_rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, context->m_CurrentRenderTarget);
        BeginRenderPass(context, context->m_CurrentRenderTarget);

        VulkanProgram* program_ptr = context->m_CurrentProgram;
        VkDevice vk_device   = context->m_LogicalDevice.m_Device;

        VkBuffer vk_buffers[MAX_VERTEX_BUFFERS]                = {};
        VkDeviceSize vk_buffer_offsets[MAX_VERTEX_BUFFERS]     = {};
        VertexDeclaration* vx_declarations[MAX_VERTEX_BUFFERS] = {};
        uint32_t num_vx_buffers                                = 0;

        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexBuffer[i] && context->m_CurrentVertexDeclaration[i])
            {
                TouchResource(context, context->m_CurrentVertexBuffer[i]);
                vx_declarations[num_vx_buffers]   = context->m_CurrentVertexDeclaration[i];
                vk_buffers[num_vx_buffers]        = context->m_CurrentVertexBuffer[i]->m_Handle.m_Buffer;
                vk_buffer_offsets[num_vx_buffers] = context->m_CurrentVertexBufferOffset[i];
                num_vx_buffers++;
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
                SetViewportHelper(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight],
                    vp.m_X, (context->m_WindowHeight - vp.m_Y), vp.m_W, -vp.m_H);
            }
            else
            {
                SetViewportHelper(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight],
                    vp.m_X, vp.m_Y, vp.m_W, vp.m_H);
            }

            vkCmdSetScissor(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight], 0, 1, &current_rt->m_Scissor);

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
            program_ptr, current_rt, vx_declarations, num_vx_buffers);

        if (!pipeline)
        {
            return false;
        }

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

            TouchResource(context, indexBuffer);

            vkCmdBindIndexBuffer(vk_command_buffer, indexBuffer->m_Handle.m_Buffer, 0, vk_index_type);
        }

        vkCmdBindVertexBuffers(vk_command_buffer, 0, num_vx_buffers, vk_buffers, vk_buffer_offsets);
        return true;
    }

    static void VulkanDrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);

        VulkanContext* context = (VulkanContext*) _context;

        assert(context->m_FrameBegun);
        const uint8_t ix = context->m_CurrentFrameInFlight;

        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[ix];
        context->m_PipelineState.m_PrimtiveType = prim_type;
        if (!DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[ix], (DeviceBuffer*) index_buffer, type))
        {
            dmLogError("Failed setup draw state");
            return;
        }

        // The 'first' value that comes in is intended to be a byte offset,
        // but vkCmdDrawIndexed only operates with actual offset values into the index buffer
        uint32_t index_offset = first / (type == TYPE_UNSIGNED_SHORT ? 2 : 4);
        vkCmdDrawIndexed(vk_command_buffer, count, dmMath::Max((uint32_t) 1, instance_count), index_offset, 0, 0);
    }

    static void VulkanDraw(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_FrameBegun);

        const uint8_t ix = context->m_CurrentFrameInFlight;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[ix];
        context->m_PipelineState.m_PrimtiveType = prim_type;
        DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[ix], 0, TYPE_BYTE);
        vkCmdDraw(vk_command_buffer, count, dmMath::Max((uint32_t) 1, instance_count), first, 0);
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

        const uint8_t ix = context->m_CurrentFrameInFlight;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[ix];
        DrawSetupCompute(context, vk_command_buffer, &context->m_MainScratchBuffers[ix]);
        vkCmdDispatch(vk_command_buffer, group_count_x, group_count_y, group_count_z);
    }

    static bool ValidateShaderModule(VulkanContext* context, ShaderMeta* meta, ShaderModule* shader, ShaderStageFlag stage_flags, char* error_buffer, uint32_t error_buffer_size)
    {
        uint32_t stage_uniform_buffers = 0;
        uint32_t stage_storage_buffers = 0;
        uint32_t stage_textures        = 0;

        for (int i = 0; i < meta->m_UniformBuffers.Size(); ++i)
        {
            if (meta->m_UniformBuffers[i].m_StageFlags & stage_flags)
                stage_uniform_buffers++;
        }

        for (int i = 0; i < meta->m_StorageBuffers.Size(); ++i)
        {
            if (meta->m_StorageBuffers[i].m_StageFlags & stage_flags)
                stage_storage_buffers++;
        }

        for (int i = 0; i < meta->m_Textures.Size(); ++i)
        {
            if (meta->m_Textures[i].m_StageFlags & stage_flags)
                stage_textures++;
        }

        if (stage_uniform_buffers > context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorUniformBuffers)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "Maximum number of uniform buffers exceeded: shader has %d buffers, but maximum is %d.",
                stage_uniform_buffers, context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorUniformBuffers);
            return false;
        }
        else if (stage_storage_buffers > context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorStorageBuffers)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "Maximum number of storage exceeded: shader has %d buffer, but maximum is %d.",
                stage_storage_buffers, context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorStorageBuffers);
            return false;
        }
        else if (stage_textures > context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorSamplers)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "Maximum number of texture samplers exceeded: shader has %d samplers, but maximum is %d.",
                stage_textures, context->m_PhysicalDevice.m_Properties.limits.maxPerStageDescriptorSamplers);
            return false;
        }
        return true;
    }

    static void CreatePipelineLayout(VulkanContext* context, VulkanProgram* program, VkDescriptorSetLayoutBinding bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT], uint32_t max_sets)
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

    static void ResolveSamplerTextureUnits(VulkanProgram* program, const dmArray<ShaderResourceBinding>&  texture_resources)
    {
        for (int i = 0; i < texture_resources.Size(); ++i)
        {
            const ShaderResourceBinding& shader_res = texture_resources[i];
            assert(shader_res.m_BindingFamily == BINDING_FAMILY_TEXTURE);

            ProgramResourceBinding& shader_pgm_res = program->m_BaseProgram.m_ResourceBindings[shader_res.m_Set][shader_res.m_Binding];

            if (shader_res.m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
            {
                const ShaderResourceBinding& texture_shader_res = texture_resources[shader_pgm_res.m_Res->m_BindingInfo.m_SamplerTextureIndex];
                const ProgramResourceBinding& texture_pgm_res   = program->m_BaseProgram.m_ResourceBindings[texture_shader_res.m_Set][texture_shader_res.m_Binding];
                shader_pgm_res.m_TextureUnit                    = texture_pgm_res.m_TextureUnit;
            #if 0 // Debug
                dmLogInfo("Resolving sampler at %d, %d to texture unit %d", shader_res.m_Set, shader_res.m_Binding, shader_pgm_res.m_TextureUnit);
            #endif
            }
        }
    }

    static inline VkFlags GetShaderStageFlags(uint8_t flag_bits)
    {
        VkFlags bits = 0;
        if (flag_bits & SHADER_STAGE_FLAG_VERTEX)
        {
            bits |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        if (flag_bits & SHADER_STAGE_FLAG_FRAGMENT)
        {
            bits |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (flag_bits & SHADER_STAGE_FLAG_COMPUTE)
        {
            bits |= VK_SHADER_STAGE_COMPUTE_BIT;
        }
        return bits;
    }

    static void VulkanFillProgramResourceBindings(
        VulkanProgram*                   program,
        dmArray<ShaderResourceBinding>&  resources,
        dmArray<ShaderResourceTypeInfo>& stage_type_infos,
        VkDescriptorSetLayoutBinding     bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT],
        uint32_t                         ubo_alignment,
        uint32_t                         ssbo_alignment,
        ProgramResourceBindingsInfo&     info)
    {
        for (int i = 0; i < resources.Size(); ++i)
        {
            ShaderResourceBinding& res            = resources[i];
            VkDescriptorSetLayoutBinding& binding = bindings[res.m_Set][res.m_Binding];
            ProgramResourceBinding& program_resource_binding = program->m_BaseProgram.m_ResourceBindings[res.m_Set][res.m_Binding];

        #if 0
            dmLogInfo("    name=%s, set=%d, binding=%d", res.m_Name, res.m_Set, res.m_Binding);
        #endif

            if (binding.descriptorCount == 0)
            {
                binding.binding            = res.m_Binding;
                binding.descriptorCount    = 1;
                binding.pImmutableSamplers = 0;

                program_resource_binding.m_Res       = &res;
                program_resource_binding.m_TypeInfos = &stage_type_infos;

                switch(res.m_BindingFamily)
                {
                    case BINDING_FAMILY_TEXTURE:
                        binding.descriptorType = TextureTypeToDescriptorType(res.m_Type.m_ShaderType);

                        if (res.m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
                        {
                            // Texture unit will be resolved in a second pass after.
                            // The sampler has indices to which texture it belongs to,
                            // but we cannot guarantee that texture units has been assigned to a texture
                            // until after this pass is done.
                            info.m_SamplerCount++;
                        }
                        else
                        {
                            program_resource_binding.m_TextureUnit = info.m_TextureCount;
                            info.m_TextureCount++;
                        }
                    #if 0
                        dmLogInfo("Texture: name=%s, set=%d, binding=%d, sampler-index=%d", res.m_Name, res.m_Set, res.m_Binding, res.m_BindingInfo.m_SamplerTextureIndex);
                    #endif
                        break;
                    case BINDING_FAMILY_STORAGE_BUFFER:
                        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                        program_resource_binding.m_StorageBufferUnit = info.m_StorageBufferCount;
                        info.m_StorageBufferCount++;
                    #if 0
                        dmLogInfo("SSBO: name=%s, set=%d, binding=%d, ssbo-unit=%d", res.m_Name, res.m_Set, res.m_Binding, program_resource_binding.m_StorageBufferUnit);
                    #endif
                        break;
                    case BINDING_FAMILY_UNIFORM_BUFFER:
                    {
                        assert(res.m_Type.m_UseTypeIndex);
                        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                        program_resource_binding.m_UniformBufferOffset = info.m_UniformDataSize;
                        program_resource_binding.m_BindingUserData     = AddUniformBufferLayout(&program->m_BaseProgram, &res, stage_type_infos.Begin(), stage_type_infos.Size());

                        info.m_UniformBufferCount++;
                        info.m_UniformDataSize        += res.m_BindingInfo.m_BlockSize;
                        info.m_UniformDataSizeAligned += DM_ALIGN(res.m_BindingInfo.m_BlockSize, ubo_alignment);
                    }
                    break;
                    case BINDING_FAMILY_GENERIC:
                    default:break;
                }

                info.m_MaxSet     = dmMath::Max(info.m_MaxSet, (uint32_t) (res.m_Set + 1));
                info.m_MaxBinding = dmMath::Max(info.m_MaxBinding, (uint32_t) (res.m_Binding + 1));
            #if 0
                dmLogInfo("    name=%s, set=%d, binding=%d, data_offset=%d", res.m_Name, res.m_Set, res.m_Binding, program_resource_binding.m_UniformBufferOffset);
            #endif
            }

            assert(res.m_StageFlags != 0);
            binding.stageFlags |= GetShaderStageFlags(res.m_StageFlags);
        }
    }

    static void VulkanFillProgramResourceBindings(
        VulkanProgram*               program,
        VkDescriptorSetLayoutBinding bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT],
        uint32_t                     ubo_alignment,
        uint32_t                     ssbo_alignment,
        ProgramResourceBindingsInfo& info)
    {
        // TODO: We should do this as a two-pass function, one that does the "base" program setup,
        //       and then one pass that does all the vulkan specific setup. So we can keep the two code paths aligned.
        program->m_BaseProgram.m_UniformBufferLayouts.SetCapacity(program->m_BaseProgram.m_ShaderMeta.m_UniformBuffers.Capacity());

        VulkanFillProgramResourceBindings(program, program->m_BaseProgram.m_ShaderMeta.m_UniformBuffers, program->m_BaseProgram.m_ShaderMeta.m_TypeInfos, bindings, ubo_alignment, ssbo_alignment, info);
        VulkanFillProgramResourceBindings(program, program->m_BaseProgram.m_ShaderMeta.m_StorageBuffers, program->m_BaseProgram.m_ShaderMeta.m_TypeInfos, bindings, ubo_alignment, ssbo_alignment, info);
        VulkanFillProgramResourceBindings(program, program->m_BaseProgram.m_ShaderMeta.m_Textures, program->m_BaseProgram.m_ShaderMeta.m_TypeInfos, bindings, ubo_alignment, ssbo_alignment, info);

        // Each module must resolve samplers individually since there is no contextual information across modules (currently)
        ResolveSamplerTextureUnits(program, program->m_BaseProgram.m_ShaderMeta.m_Textures);
    }

    static void CreateProgramResourceBindings(VulkanContext* context, VulkanProgram* program)
    {
        VkDescriptorSetLayoutBinding bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT] = {};

        uint32_t ubo_alignment  = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment;
        uint32_t ssbo_alignment = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minStorageBufferOffsetAlignment;

        ProgramResourceBindingsInfo binding_info = {};
        VulkanFillProgramResourceBindings(program, bindings, ubo_alignment, ssbo_alignment, binding_info);

        program->m_UniformData = new uint8_t[binding_info.m_UniformDataSize];
        memset(program->m_UniformData, 0, binding_info.m_UniformDataSize);

        program->m_UniformDataSizeAligned = binding_info.m_UniformDataSizeAligned;
        program->m_UniformBufferCount     = binding_info.m_UniformBufferCount;
        program->m_StorageBufferCount     = binding_info.m_StorageBufferCount;
        program->m_TextureSamplerCount    = binding_info.m_TextureCount;
        program->m_TotalResourcesCount    = binding_info.m_UniformBufferCount + binding_info.m_TextureCount + binding_info.m_SamplerCount + binding_info.m_StorageBufferCount; // num actual descriptors
        program->m_BaseProgram.m_MaxSet     = binding_info.m_MaxSet;
        program->m_BaseProgram.m_MaxBinding = binding_info.m_MaxBinding;

        CreatePipelineLayout(context, program, bindings, binding_info.m_MaxSet);

        BuildUniforms(&program->m_BaseProgram);
    }

    static void CreateComputeProgram(VulkanContext* context, VulkanProgram* program, ShaderModule* compute_module)
    {
        program->m_ComputeModule  = compute_module;
        program->m_Hash           = compute_module->m_Hash;
        CreateProgramResourceBindings(context, program);
    }

    static void CreateGraphicsProgram(VulkanContext* context, VulkanProgram* program, ShaderModule* vertex_module, ShaderModule* fragment_module)
    {
        program->m_Hash           = 0;
        program->m_UniformData    = 0;
        program->m_VertexModule   = vertex_module;
        program->m_FragmentModule = fragment_module;

        HashState64 program_hash;
        dmHashInit64(&program_hash, false);

        for (uint32_t i=0; i < program->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); i++)
        {
            if (program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_StageFlags & SHADER_STAGE_FLAG_VERTEX)
            {
                dmHashUpdateBuffer64(&program_hash, &program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_Binding, sizeof(program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_Binding));
            }
        }

        dmHashUpdateBuffer64(&program_hash, &vertex_module->m_Hash, sizeof(vertex_module->m_Hash));
        dmHashUpdateBuffer64(&program_hash, &fragment_module->m_Hash, sizeof(fragment_module->m_Hash));
        program->m_Hash = dmHashFinal64(&program_hash);

        CreateProgramResourceBindings(context, program);
    }

    static void DestroyProgram(HContext _context, VulkanProgram* program)
    {
        VulkanContext* context = (VulkanContext*) _context;
        if (program->m_UniformData)
        {
            delete[] program->m_UniformData;
        }

        DestroyResourceDeferred(context, program);
    }

    static void DestroyShader(VulkanContext* context, ShaderModule* shader)
    {
        if (!shader)
        {
            return;
        }

        DestroyShaderModule(context->m_LogicalDevice.m_Device, shader);
    }

    static void VulkanDeleteProgram(HContext _context, HProgram program)
    {
        assert(program);
        VulkanProgram* program_ptr = (VulkanProgram*) program;
        VulkanContext* context = (VulkanContext*) _context;

        DestroyProgram(context, program_ptr);

        DestroyShader(context, program_ptr->m_VertexModule);
        DestroyShader(context, program_ptr->m_FragmentModule);
        DestroyShader(context, program_ptr->m_ComputeModule);

        delete program_ptr;
    }

    static bool ReloadShader(VulkanContext* context, ShaderModule* shader, ShaderDesc::Shader* ddf, VkShaderStageFlagBits stage_flag)
    {
        ShaderModule tmp_shader;
        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf->m_Source.m_Data, ddf->m_Source.m_Count, stage_flag, &tmp_shader);
        if (res == VK_SUCCESS)
        {
            DestroyShader(context, shader);
            memset(shader, 0, sizeof(*shader));

            // Transfer created module to old pointer and recreate resource bindings
            shader->m_Hash    = tmp_shader.m_Hash;
            shader->m_Module  = tmp_shader.m_Module;
            return true;
        }

        return false;
    }

    static inline ShaderModule* NewShaderModule(VulkanContext* context, ShaderMeta* meta, ShaderDesc::Shader* ddf, VkShaderStageFlagBits stage, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderModule* module = new ShaderModule;
        memset(module, 0, sizeof(*module));

        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf->m_Source.m_Data, ddf->m_Source.m_Count, stage, module);
        CHECK_VK_ERROR(res);

        ShaderStageFlag stage_flag = (ShaderStageFlag) -1;
        if (stage == VK_SHADER_STAGE_VERTEX_BIT)
        {
            stage_flag = SHADER_STAGE_FLAG_VERTEX;
        }
        else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        {
            stage_flag = SHADER_STAGE_FLAG_FRAGMENT;
        }
        else if (stage == VK_SHADER_STAGE_COMPUTE_BIT)
        {
            stage_flag = SHADER_STAGE_FLAG_COMPUTE;
        }
        if (!ValidateShaderModule(context, meta, module, stage_flag, error_buffer, error_buffer_size))
        {
            DestroyShader(context, module);
            delete module;
            return 0;
        }
        return module;
    }

    static HProgram VulkanNewProgram(HContext _context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderDesc::Shader* ddf_vp = 0x0;
        ShaderDesc::Shader* ddf_fp = 0x0;
        ShaderDesc::Shader* ddf_cp = 0x0;

        if (!GetShaderProgram(_context, ddf, &ddf_vp, &ddf_fp, &ddf_cp))
        {
            return 0;
        }

        VulkanContext* context = (VulkanContext*) _context;
        VulkanProgram* program = new VulkanProgram;

        CreateShaderMeta(&ddf->m_Reflection, &program->m_BaseProgram.m_ShaderMeta);

        if (ddf_cp)
        {
            ShaderModule* module_cp = NewShaderModule(context, &program->m_BaseProgram.m_ShaderMeta, ddf_cp, VK_SHADER_STAGE_COMPUTE_BIT, error_buffer, error_buffer_size);
            CreateComputeProgram(context, program, module_cp);
        }
        else
        {
            ShaderModule* vertex_module = NewShaderModule(context, &program->m_BaseProgram.m_ShaderMeta, ddf_vp, VK_SHADER_STAGE_VERTEX_BIT, error_buffer, error_buffer_size);
            ShaderModule* fragment_module = NewShaderModule(context, &program->m_BaseProgram.m_ShaderMeta, ddf_fp, VK_SHADER_STAGE_FRAGMENT_BIT, error_buffer, error_buffer_size);
            CreateGraphicsProgram((VulkanContext*) context, program, vertex_module, fragment_module);
        }

        return (HProgram) program;
    }

    static bool VulkanReloadProgram(HContext _context, HProgram _program, ShaderDesc* ddf)
    {
        ShaderDesc::Shader* ddf_vp = 0x0;
        ShaderDesc::Shader* ddf_fp = 0x0;
        ShaderDesc::Shader* ddf_cp = 0x0;

        if (!GetShaderProgram(_context, ddf, &ddf_vp, &ddf_fp, &ddf_cp))
        {
            return 0;
        }

        VulkanContext* context = (VulkanContext*) _context;
        VulkanProgram* program = (VulkanProgram*) _program;

        if (ddf_cp)
        {
            if (!ReloadShader(context, program->m_ComputeModule, ddf_cp, VK_SHADER_STAGE_COMPUTE_BIT))
                return false;

            DestroyProgram(context, program);
            CreateShaderMeta(&ddf->m_Reflection, &program->m_BaseProgram.m_ShaderMeta);
            CreateComputeProgram(context, program, program->m_ComputeModule);
        }
        else
        {
            if (!ReloadShader(context, program->m_VertexModule, ddf_vp, VK_SHADER_STAGE_VERTEX_BIT))
                return false;
            if (!ReloadShader(context, program->m_FragmentModule, ddf_fp, VK_SHADER_STAGE_FRAGMENT_BIT))
                return false;

            DestroyProgram(context, program);
            CreateShaderMeta(&ddf->m_Reflection, &program->m_BaseProgram.m_ShaderMeta);
            CreateGraphicsProgram(context, program, program->m_VertexModule, program->m_FragmentModule);
        }

        return true;
    }

    static bool VulkanIsShaderLanguageSupported(HContext _context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
    {
        return language == ShaderDesc::LANGUAGE_SPIRV;
    }

    static ShaderDesc::Language VulkanGetProgramLanguage(HProgram program)
    {
        return ShaderDesc::LANGUAGE_SPIRV;
    }

    static void VulkanEnableProgram(HContext _context, HProgram program)
    {
        VulkanContext* context = (VulkanContext*)_context;
        context->m_CurrentProgram = (VulkanProgram*) program;
    }

    static void VulkanDisableProgram(HContext _context)
    {
        VulkanContext* context = (VulkanContext*)_context;
        context->m_CurrentProgram = 0;
    }

    static uint32_t VulkanGetAttributeCount(HProgram prog)
    {
        VulkanProgram* program_ptr = (VulkanProgram*) prog;
        uint32_t num_vx_inputs = 0;
        for (int i = 0; i < program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); ++i)
        {
            if (program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_StageFlags & SHADER_STAGE_FLAG_VERTEX)
            {
                num_vx_inputs++;
            }
        }
        return num_vx_inputs;
    }

    static void VulkanGetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        VulkanProgram* program = (VulkanProgram*) prog;
        uint32_t input_ix = 0;
        for (int i = 0; i < program->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); ++i)
        {
            if (program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_StageFlags & SHADER_STAGE_FLAG_VERTEX)
            {
                if (input_ix == index)
                {
                    ShaderResourceBinding& attr = program->m_BaseProgram.m_ShaderMeta.m_Inputs[i];
                    *name_hash                  = attr.m_NameHash;
                    *type                       = ShaderDataTypeToGraphicsType(attr.m_Type.m_ShaderType);
                    *num_values                 = 1;
                    *location                   = attr.m_Binding;
                    *element_count              = GetShaderTypeSize(attr.m_Type.m_ShaderType) / sizeof(float);
                }
                input_ix++;
            }
        }
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

        VulkanProgram* program_ptr = (VulkanProgram*) context->m_CurrentProgram;
        uint32_t set               = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding           = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset     = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        ProgramResourceBinding& pgm_res = program_ptr->m_BaseProgram.m_ResourceBindings[set][binding];

        uint32_t offset = pgm_res.m_UniformBufferOffset + buffer_offset;
        WriteConstantData(offset, program_ptr->m_UniformData, (uint8_t*) data, sizeof(dmVMath::Vector4) * count);
    }

    static void VulkanSetConstantM4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        VulkanProgram* program_ptr    = (VulkanProgram*) context->m_CurrentProgram;
        uint32_t set            = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding        = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset  = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        ProgramResourceBinding& pgm_res = program_ptr->m_BaseProgram.m_ResourceBindings[set][binding];

        uint32_t offset = pgm_res.m_UniformBufferOffset + buffer_offset;
        WriteConstantData(offset, program_ptr->m_UniformData, (uint8_t*) data, sizeof(dmVMath::Vector4) * 4 * count);
    }

    static void VulkanSetSampler(HContext _context, HUniformLocation location, int32_t unit)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_CurrentProgram);
        assert(location != INVALID_UNIFORM_LOCATION);

        VulkanProgram* program_ptr = (VulkanProgram*) context->m_CurrentProgram;
        uint32_t set         = UNIFORM_LOCATION_GET_OP0(location);
        uint32_t binding     = UNIFORM_LOCATION_GET_OP1(location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        assert(program_ptr->m_BaseProgram.m_ResourceBindings[set][binding].m_Res);
        program_ptr->m_BaseProgram.m_ResourceBindings[set][binding].m_TextureUnit = unit;
    }

    static void VulkanSetViewport(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        VulkanContext* context = (VulkanContext*)_context;
        // Defer the update to when we actually draw, since we *might* need to invert the viewport
        // depending on wether or not we have set a different rendertarget from when
        // this call was made.
        Viewport& viewport = context->m_MainViewport;
        viewport.m_X       = (uint16_t) x;
        viewport.m_Y       = (uint16_t) y;
        viewport.m_W       = (uint16_t) width;
        viewport.m_H       = (uint16_t) height;

        context->m_ViewportChanged = 1;
    }

    static void VulkanGetViewport(HContext _context, int32_t* x, int32_t* y, uint32_t* width, uint32_t* height)
    {
        VulkanContext* context = (VulkanContext*)_context;
        const Viewport& viewport = context->m_MainViewport;
        *x = viewport.m_X, *y = viewport.m_Y, *width = viewport.m_W, *height = viewport.m_H;
    }

    static void VulkanEnableState(HContext _context, State state)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(context);
        SetPipelineStateValue(context->m_PipelineState, state, 1);
    }

    static void VulkanDisableState(HContext _context, State state)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(context);
        SetPipelineStateValue(context->m_PipelineState, state, 0);
    }

    static void VulkanSetBlendFunc(HContext _context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(context);
        context->m_PipelineState.m_BlendSrcFactor = source_factor;
        context->m_PipelineState.m_BlendDstFactor = destinaton_factor;
    }

    static void VulkanSetColorMask(HContext _context, bool red, bool green, bool blue, bool alpha)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(context);
        uint8_t write_mask = red   ? DM_GRAPHICS_STATE_WRITE_R : 0;
        write_mask        |= green ? DM_GRAPHICS_STATE_WRITE_G : 0;
        write_mask        |= blue  ? DM_GRAPHICS_STATE_WRITE_B : 0;
        write_mask        |= alpha ? DM_GRAPHICS_STATE_WRITE_A : 0;

        context->m_PipelineState.m_WriteColorMask = write_mask;
    }

    static void VulkanSetDepthMask(HContext _context, bool enable_mask)
    {
        VulkanContext* context = (VulkanContext*)_context;
        context->m_PipelineState.m_WriteDepth = enable_mask;
    }

    static void VulkanSetDepthFunc(HContext _context, CompareFunc func)
    {
        VulkanContext* context = (VulkanContext*)_context;
        context->m_PipelineState.m_DepthTestFunc = func;
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

    static void VulkanSetStencilMask(HContext _context, uint32_t mask)
    {
        VulkanContext* context = (VulkanContext*)_context;
        context->m_PipelineState.m_StencilWriteMask = mask;
    }

    static void VulkanSetStencilFunc(HContext _context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(context);
        context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void VulkanSetStencilFuncSeparate(HContext _context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        VulkanContext* context = (VulkanContext*)_context;
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

    static void VulkanSetStencilOp(HContext _context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(context);
        context->m_PipelineState.m_StencilFrontOpFail      = sfail;
        context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
        context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        context->m_PipelineState.m_StencilBackOpFail       = sfail;
        context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
        context->m_PipelineState.m_StencilBackOpPass       = dppass;
    }

    static void VulkanSetStencilOpSeparate(HContext _context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        VulkanContext* context = (VulkanContext*)_context;
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

    static void VulkanSetCullFace(HContext _context, FaceType face_type)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(context);
        context->m_PipelineState.m_CullFaceType = face_type;
        context->m_CullFaceChanged              = true;
    }

    static void VulkanSetFaceWinding(HContext, FaceWinding face_winding)
    {
        // TODO: Add this to the vulkan pipeline handle aswell, for now it's a NOP
    }

    static void VulkanSetPolygonOffset(HContext _context, float factor, float units)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(context);
        vkCmdSetDepthBias(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight], factor, 0.0, units);
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
            // ASTC
            case TEXTURE_FORMAT_RGBA_ASTC_4X4:      return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_5X4:      return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_5X5:      return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_6X5:      return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_6X6:      return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_8X5:      return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_8X6:      return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_8X8:      return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_10X5:     return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_10X6:     return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_10X8:     return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_10X10:    return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_12X10:    return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
            case TEXTURE_FORMAT_RGBA_ASTC_12X12:    return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;

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
        DestroyResourceDeferred(context, renderTarget);
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

    static HRenderTarget VulkanNewRenderTarget(HContext _context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        VulkanContext* context = (VulkanContext*)_context;
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
                VulkanTexture* new_texture_color = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, new_texture_color_handle);

                VkImageUsageFlags vk_usage_flags     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | new_texture_color->m_UsageFlags;
                VkMemoryPropertyFlags vk_memory_type = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

                if (IsTextureMemoryless(new_texture_color))
                {
                    vk_memory_type |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
                }

                VkResult res = CreateTexture(
                    context->m_PhysicalDevice.m_Device,
                    context->m_LogicalDevice.m_Device,
                    new_texture_color->m_Width, new_texture_color->m_Height, 1, 1, new_texture_color->m_MipMapCount,
                    VK_SAMPLE_COUNT_1_BIT,
                    vk_color_format,
                    VK_IMAGE_TILING_OPTIMAL,
                    vk_usage_flags,
                    vk_memory_type,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    new_texture_color);
                CHECK_VK_ERROR(res);

                res = TransitionImageLayout(context->m_LogicalDevice.m_Device,
                    context->m_LogicalDevice.m_CommandPool,
                    context->m_LogicalDevice.m_GraphicsQueue,
                    new_texture_color,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                CHECK_VK_ERROR(res);

                VulkanSetTextureParamsInternal(context, new_texture_color, color_buffer_params.m_MinFilter, color_buffer_params.m_MagFilter, color_buffer_params.m_UWrap, color_buffer_params.m_VWrap, 1.0f);

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

                GetDepthFormatAndTiling(context->m_PhysicalDevice.m_Device, vk_format_list,
                    DM_ARRAY_SIZE(vk_format_list), &vk_depth_stencil_format, &vk_depth_tiling);
            }

            // If we request both depth & stencil OR test above failed,
            // try with default depth stencil formats
            if (vk_depth_stencil_format == VK_FORMAT_UNDEFINED)
            {
                GetDepthFormatAndTiling(context->m_PhysicalDevice.m_Device, 0, 0, &vk_depth_stencil_format, &vk_depth_tiling);
            }

            texture_depth_stencil                    = NewTexture(context, stencil_depth_create_params);
            VulkanTexture* texture_depth_stencil_ptr = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture_depth_stencil);

            // TODO: Right now we can only sample depth with this texture, if we want to support stencil texture reads we need to make a separate texture I think
            VkResult res = CreateDepthStencilTexture(context,
                vk_depth_stencil_format, vk_depth_tiling,
                fb_width, fb_height, VK_SAMPLE_COUNT_1_BIT, // No support for multisampled FBOs
                GetDefaultDepthAndStencilAspectFlags(vk_depth_stencil_format),
                texture_depth_stencil_ptr);
            CHECK_VK_ERROR(res);
        }

        if (color_index > 0 || has_depth || has_stencil)
        {
            VkResult res = CreateRenderTarget(context, texture_color, buffer_types, color_index, texture_depth_stencil, fb_width, fb_height, rt);
            CHECK_VK_ERROR(res);
        }

        return StoreAssetInContainer(context->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
    }

    static void VulkanDeleteRenderTarget(HContext _context, HRenderTarget render_target)
    {
        VulkanContext* context = (VulkanContext*)_context;
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);
        context->m_AssetHandleContainer.Release(render_target);

        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt->m_TextureColor[i])
            {
                DeleteTexture(context, rt->m_TextureColor[i]);
            }
        }

        if (rt->m_TextureDepthStencil)
        {
            DeleteTexture(context, rt->m_TextureDepthStencil);
        }

        DestroyRenderTarget(context, rt);

        delete rt;
    }

    static void VulkanSetRenderTarget(HContext _context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        VulkanContext* context = (VulkanContext*) _context;
        context->m_ViewportChanged = 1;
        BeginRenderPass(context, render_target != 0x0 ? render_target : context->m_MainRenderTarget);
    }

    static HTexture VulkanGetRenderTargetTexture(HContext _context, HRenderTarget render_target, BufferType buffer_type)
    {
        VulkanContext* context = (VulkanContext*)_context;
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);

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

    static void VulkanGetRenderTargetSize(HContext _context, HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        VulkanContext* context = (VulkanContext*)_context;
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);
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

    static void VulkanSetRenderTargetSize(HContext _context, HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        VulkanContext* context = (VulkanContext*)_context;
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);

        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            rt->m_ColorTextureParams[i].m_Width = width;
            rt->m_ColorTextureParams[i].m_Height = height;

            if (rt->m_TextureColor[i])
            {
                VulkanTexture* texture_color         = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, rt->m_TextureColor[i]);
                VkImageUsageFlags vk_usage_flags     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | texture_color->m_UsageFlags;
                VkMemoryPropertyFlags vk_memory_type = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

                if (IsTextureMemoryless(texture_color))
                {
                    vk_memory_type |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
                }

                texture_color->m_ImageLayout[0] = VK_IMAGE_LAYOUT_PREINITIALIZED;

                DestroyResourceDeferred(context, texture_color);
                VkResult res = CreateTexture(
                    context->m_PhysicalDevice.m_Device,
                    context->m_LogicalDevice.m_Device,
                    width, height, 1,
                    texture_color->m_MipMapCount, 1, VK_SAMPLE_COUNT_1_BIT,
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

            VulkanTexture* depth_stencil_texture = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, rt->m_TextureDepthStencil);
            DestroyResourceDeferred(context, depth_stencil_texture);

            // Check tiling support for this format
            VkImageTiling vk_image_tiling    = VK_IMAGE_TILING_OPTIMAL;
            VkFormat vk_depth_stencil_format = depth_stencil_texture->m_Format;
            VkFormat vk_depth_format         = GetSupportedTilingFormat(context->m_PhysicalDevice.m_Device, &vk_depth_stencil_format,
                1, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

            if (vk_depth_format == VK_FORMAT_UNDEFINED)
            {
                vk_image_tiling = VK_IMAGE_TILING_LINEAR;
            }

            VkResult res = CreateDepthStencilTexture(context,
                vk_depth_stencil_format, vk_image_tiling,
                width, height, VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                depth_stencil_texture);
            CHECK_VK_ERROR(res);

            depth_stencil_texture->m_Width  = width;
            depth_stencil_texture->m_Height = height;
        }

        DestroyRenderTarget(context, rt);
        VkResult res = CreateRenderTarget(context,
            rt->m_TextureColor,
            rt->m_ColorAttachmentBufferTypes,
            rt->m_ColorAttachmentCount,
            rt->m_TextureDepthStencil,
            width, height,
            rt);
        CHECK_VK_ERROR(res);
    }

    static bool VulkanIsTextureFormatSupported(HContext _context, TextureFormat format)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return (context->m_TextureFormatSupport & (1 << format)) != 0 || (context->m_ASTCSupport && IsTextureFormatASTC(format));
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
        tex->m_Depth          = dmMath::Max((uint16_t)1, params.m_Depth);
        tex->m_LayerCount     = dmMath::Max((uint8_t)1, params.m_LayerCount);
        tex->m_MipMapCount    = params.m_MipMapCount;
        tex->m_UsageFlags     = GetVulkanUsageFromHints(params.m_UsageHintBits);
        tex->m_UsageHintFlags = params.m_UsageHintBits;
        tex->m_PageCount      = params.m_LayerCount;
        tex->m_DataState      = 0;
        tex->m_PendingUpload  = INVALID_OPAQUE_HANDLE;
        tex->m_DataSize       = 0;

        for (int i = 0; i < DM_ARRAY_SIZE(tex->m_ImageLayout); ++i)
        {
            tex->m_ImageLayout[i] = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        if (params.m_OriginalWidth == 0)
        {
            tex->m_OriginalWidth  = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
            tex->m_OriginalDepth  = params.m_Depth;
        }
        else
        {
            tex->m_OriginalWidth  = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
            tex->m_OriginalDepth  = params.m_OriginalDepth;
        }
        return tex;
    }

    static HTexture VulkanNewTexture(HContext _context, const TextureCreationParams& params)
    {
        VulkanContext* context = (VulkanContext*) _context;
        VulkanTexture* texture = VulkanNewTextureInternal(params);
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        return StoreAssetInContainer(context->m_AssetHandleContainer, texture, ASSET_TYPE_TEXTURE);
    }

    static void VulkanDeleteTextureInternal(VulkanTexture* texture)
    {
        DestroyResourceDeferred(g_VulkanContext, texture);
        delete texture;
    }

    static void VulkanDeleteTexture(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanDeleteTextureInternal(GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture));
        context->m_AssetHandleContainer.Release(texture);
    }

    static void CopyToTextureLayerWithtStageBuffer(VulkanContext* context, VkCommandBuffer cmd_buffer, VkBufferImageCopy* copy_regions, DeviceBuffer* stage_buffer, VulkanTexture* texture, const TextureParams& params, uint32_t layer_count, uint32_t slice_size)
    {
        for (int i = 0; i < layer_count; ++i)
        {
            VkBufferImageCopy& vk_copy_region = copy_regions[i];
            vk_copy_region.bufferOffset                    = i * slice_size;
            vk_copy_region.bufferRowLength                 = 0;
            vk_copy_region.bufferImageHeight               = 0;
            vk_copy_region.imageOffset.x                   = params.m_X;
            vk_copy_region.imageOffset.y                   = params.m_Y;
            vk_copy_region.imageOffset.z                   = params.m_Z;
            vk_copy_region.imageExtent.width               = params.m_Width;
            vk_copy_region.imageExtent.height              = params.m_Height;
            vk_copy_region.imageExtent.depth               = dmMath::Max((uint32_t) 1, (uint32_t) params.m_Depth);
            vk_copy_region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            vk_copy_region.imageSubresource.mipLevel       = params.m_MipMap;
            vk_copy_region.imageSubresource.baseArrayLayer = i;
            vk_copy_region.imageSubresource.layerCount     = 1;
        }

        TouchResource(context, stage_buffer);
        TouchResource(context, texture);

        vkCmdCopyBufferToImage(cmd_buffer, stage_buffer->m_Handle.m_Buffer,
            texture->m_Handle.m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            layer_count, copy_regions);
    }

    static void CopyToTextureWithStageBuffer(VulkanContext* context, VkCommandBuffer cmd_buffer, DeviceBuffer* stage_buffer, VulkanTexture* texture, const TextureParams& params, uint32_t tex_data_size, const void* tex_data_ptr)
    {
        VkResult res = WriteToDeviceBuffer(context->m_LogicalDevice.m_Device, tex_data_size, 0, tex_data_ptr, stage_buffer);
        CHECK_VK_ERROR(res);

        uint32_t slice_size = tex_data_size / texture->m_LayerCount;
        uint32_t layer_count = texture->m_LayerCount;

        // NOTE: We should check max layer count in the device properties!
        VkBufferImageCopy* vk_copy_regions = new VkBufferImageCopy[layer_count];

        CopyToTextureLayerWithtStageBuffer(context, cmd_buffer, vk_copy_regions, stage_buffer, texture, params, layer_count, slice_size);

        delete[] vk_copy_regions;
    }

    static inline HOpaqueHandle DestroyFenceResourcesDeferred(VulkanContext* context, FenceResourcesToDestroy* fence_resources)
    {
        if (context->m_FenceResourcesToDestroy.Full())
        {
            context->m_FenceResourcesToDestroy.Allocate(8);
        }
        return context->m_FenceResourcesToDestroy.Put(fence_resources);
    }

    static void CopyToTexture(VulkanContext* context, const TextureParams& params,
        bool use_stage_buffer, uint32_t tex_data_size, void* tex_data_ptr, VulkanTexture* texture_out)
    {
        DM_PROFILE(__FUNCTION__);

        VkDevice vk_device = context->m_LogicalDevice.m_Device;
        uint32_t layer_count = texture_out->m_LayerCount;
        assert(layer_count > 0);

        uint32_t slice_size = tex_data_size / layer_count;

    #ifdef __MACH__
        // macOS quirk: stage buffer offsets for layered compressed data must be 8-byte aligned.
        // For small compressed mips (< 8 bytes per slice), skip upload to avoid validation errors.
        if (slice_size < 8 && layer_count > 1)
        {
            return;
        }
    #endif

        // Always allocate a temporary command buffer for the copy, even during a frame
        VkCommandBuffer vk_command_buffer = BeginSingleTimeCommands(
            context->m_LogicalDevice.m_Device,
            context->m_LogicalDevice.m_CommandPool);

        // Stage buffer path
        DeviceBuffer stage_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        VkResult res = CreateDeviceBuffer(
            context->m_PhysicalDevice.m_Device,
            vk_device,
            tex_data_size,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stage_buffer);
        CHECK_VK_ERROR(res);

        res = WriteToDeviceBuffer(vk_device, tex_data_size, 0, tex_data_ptr, &stage_buffer);
        CHECK_VK_ERROR(res);

        TransitionImageLayoutWithCmdBuffer(
            vk_command_buffer,
            texture_out,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            params.m_MipMap,
            layer_count);

        // Copy regions on the stack
        dmArray<VkBufferImageCopy> copy_regions;
        copy_regions.SetCapacity(layer_count);
        copy_regions.SetSize(layer_count);

        CopyToTextureLayerWithtStageBuffer(
            context,
            vk_command_buffer,
            copy_regions.Begin(),
            &stage_buffer,
            texture_out,
            params,
            layer_count,
            slice_size);

        TransitionImageLayoutWithCmdBuffer(
            vk_command_buffer,
            texture_out,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            params.m_MipMap,
            layer_count);

        VkFence submit_fence;
        res = SubmitCommandBuffer(
            context->m_LogicalDevice.m_Device,
            context->m_LogicalDevice.m_GraphicsQueue,
            vk_command_buffer,
            &submit_fence);
        CHECK_VK_ERROR(res);

        VulkanCommandBuffer cmd_buffer;
        cmd_buffer.m_Handle.m_CommandBuffer = vk_command_buffer;
        cmd_buffer.m_Handle.m_CommandPool   = context->m_LogicalDevice.m_CommandPool;

        FenceResourcesToDestroy* pending_upload        = new FenceResourcesToDestroy();
        pending_upload->m_Fence                        = submit_fence;
        pending_upload->m_Resources[0].m_DeviceBuffer  = stage_buffer.m_Handle;
        pending_upload->m_Resources[0].m_ResourceType  = RESOURCE_TYPE_DEVICE_BUFFER;
        pending_upload->m_Resources[1].m_CommandBuffer = cmd_buffer.m_Handle;
        pending_upload->m_Resources[1].m_ResourceType  = RESOURCE_TYPE_COMMAND_BUFFER;
        pending_upload->m_ResourcesCount               = 2;

        texture_out->m_PendingUpload = DestroyFenceResourcesDeferred(context, pending_upload);
    }

    static void VulkanSetTextureInternal(VulkanContext* context, VulkanTexture* texture, const TextureParams& params)
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

        assert(params.m_Width  <= context->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D);
        assert(params.m_Height <= context->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D);

        if (texture->m_MipMapCount == 1 && params.m_MipMap > 0)
        {
            return;
        }

        TextureFormat format_orig   = params.m_Format;
        uint8_t tex_layer_count     = dmMath::Max(texture->m_LayerCount, params.m_LayerCount);
        uint16_t tex_depth          = dmMath::Max(texture->m_Depth, params.m_Depth);
        uint8_t tex_bpp             = GetTextureFormatBitsPerPixel(params.m_Format);
        size_t tex_data_size_bpp    = params.m_DataSize * tex_layer_count * 8; // Convert into bits
        void*  tex_data_ptr         = (void*)params.m_Data;
        VkFormat vk_format          = GetVulkanFormatFromTextureFormat(params.m_Format);

        if (vk_format == VK_FORMAT_UNDEFINED)
        {
            dmLogError("Unable to upload texture data, unsupported type (%s).", GetTextureFormatLiteral(format_orig));
            return;
        }

        // For future reference, we could validate ASTC buffer sizes by using these calculations:
        // https://registry.khronos.org/webgl/extensions/WEBGL_compressed_texture_astc/

        LogicalDevice& logical_device       = context->m_LogicalDevice;
        VkPhysicalDevice vk_physical_device = context->m_PhysicalDevice.m_Device;

        // Note: There's no RGB support in Vulkan. We have to expand this to four channels
        // TODO: Can we use R11G11B10 somehow?
        uint8_t* temp_data = 0;
        if (format_orig == TEXTURE_FORMAT_RGB)
        {
            uint32_t data_pixel_count = params.m_Width * params.m_Height * tex_layer_count;
            temp_data                 = new uint8_t[data_pixel_count * 4]; // RGBA => 4 bytes per pixel

            RepackRGBToRGBA(data_pixel_count, (uint8_t*) tex_data_ptr, temp_data);
            vk_format     = VK_FORMAT_R8G8B8A8_UNORM;
            tex_data_ptr  = temp_data;
            tex_bpp       = 32;
        }

        // In cases where we just want to clear the texture we don't have a valid data or datasize, so we need to infer it.
        // This will NOT work for clearing compressed texture formats, but I don't think that is a case we can support anyway.
        tex_data_size_bpp         = tex_bpp * params.m_Width * params.m_Height * tex_depth * tex_layer_count;
        texture->m_GraphicsFormat = params.m_Format;
        texture->m_MipMapCount    = dmMath::Max(texture->m_MipMapCount, (uint16_t)(params.m_MipMap+1));
        texture->m_LayerCount     = tex_layer_count;

        VulkanSetTextureParamsInternal(context, texture, params.m_MinFilter, params.m_MagFilter, params.m_UWrap, params.m_VWrap, 1.0f);

        if (params.m_SubUpdate)
        {
            // TODO: Not sure this will work for compressed formats..
            // data size might be different if we have generated a new image
            tex_data_size_bpp = params.m_Width * params.m_Height * tex_bpp * tex_layer_count;
        }
        else if (params.m_MipMap == 0)
        {
            if (texture->m_Format != vk_format ||
                texture->m_Width != params.m_Width ||
                texture->m_Height != params.m_Height ||
                (IsTextureType3D(texture->m_Type) && (texture->m_Depth != params.m_Depth)))
            {
                DestroyResourceDeferred(context, texture);
                texture->m_Format = vk_format;
                texture->m_Width  = params.m_Width;
                texture->m_Height = params.m_Height;
                texture->m_Depth  = params.m_Depth;

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

            VkResult res = CreateTexture(
                vk_physical_device,
                logical_device.m_Device,
                texture->m_Width,
                texture->m_Height,
                tex_depth,
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
            uint32_t tex_data_size;
            if (IsTextureFormatASTC(params.m_Format))
                tex_data_size = params.m_DataSize;
            else
                tex_data_size = (int) ceil((float) tex_data_size_bpp / 8.0f);

            CopyToTexture(context, params, use_stage_buffer, tex_data_size, tex_data_ptr, texture);

            // update the resource size
            texture->m_DataSize = tex_data_size;
        }

        delete[] temp_data;
    }

    void VulkanDestroyResources(HContext _context)
    {
        VulkanContext* context = (VulkanContext*)_context;
        VkDevice vk_device = context->m_LogicalDevice.m_Device;

        context->m_PipelineCache.Iterate(DestroyPipelineCacheCb, context);

        DestroyDeviceBuffer(vk_device, &context->m_MainTextureDepthStencil.m_DeviceBuffer.m_Handle);
        DestroyTexture(vk_device, &context->m_MainTextureDepthStencil.m_Handle);
        DestroyDeviceBuffer(vk_device, &context->m_DefaultTexture2D->m_DeviceBuffer.m_Handle);
        DestroyTexture(vk_device, &context->m_DefaultTexture2D->m_Handle);
        DestroyDeviceBuffer(vk_device, &context->m_DefaultTexture2DArray->m_DeviceBuffer.m_Handle);
        DestroyTexture(vk_device, &context->m_DefaultTexture2DArray->m_Handle);
        DestroyDeviceBuffer(vk_device, &context->m_DefaultTextureCubeMap->m_DeviceBuffer.m_Handle);
        DestroyTexture(vk_device, &context->m_DefaultTextureCubeMap->m_Handle);
        DestroyDeviceBuffer(vk_device, &context->m_DefaultTexture2D32UI->m_DeviceBuffer.m_Handle);
        DestroyTexture(vk_device, &context->m_DefaultTexture2D32UI->m_Handle);
        DestroyDeviceBuffer(vk_device, &context->m_DefaultStorageImage2D->m_DeviceBuffer.m_Handle);
        DestroyTexture(vk_device, &context->m_DefaultStorageImage2D->m_Handle);

        vkDestroyRenderPass(vk_device, context->m_MainRenderPass, 0);

        vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, DM_ARRAY_SIZE(context->m_MainCommandBuffers), context->m_MainCommandBuffers);
        vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &context->m_MainCommandBufferUploadHelper);

        for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
        {
            vkDestroyFramebuffer(vk_device, context->m_MainFrameBuffers[i], 0);
        }

        for (uint8_t i=0; i < context->m_TextureSamplers.Size(); i++)
        {
            DestroyTextureSampler(vk_device, &context->m_TextureSamplers[i]);
        }

        for (uint8_t i=0; i < DM_ARRAY_SIZE(context->m_MainScratchBuffers); i++)
        {
            DestroyDeviceBuffer(vk_device, &context->m_MainScratchBuffers[i].m_DeviceBuffer.m_Handle);
        }

        for (uint8_t i=0; i < DM_ARRAY_SIZE(context->m_MainDescriptorAllocators); i++)
        {
            DestroyDescriptorAllocator(vk_device, &context->m_MainDescriptorAllocators[i]);
        }

        for (uint8_t i=0; i < DM_ARRAY_SIZE(context->m_MainCommandBuffers); i++)
        {
            FlushResourcesToDestroy(context, context->m_MainResourcesToDestroy[i]);
        }

        FlushFenceResourcesToDestroy(context);

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

    static void VulkanSetTexture(HContext _context, HTexture texture, const TextureParams& params)
    {
        DM_PROFILE(__FUNCTION__);
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        VulkanSetTextureInternal(context, tex, params);
    }

    static int AsyncProcessCallback(dmJobThread::HContext, dmJobThread::HJob job, void* _context, void* data)
    {
        VulkanContext* context     = (VulkanContext*) _context;
        uint16_t param_array_index = (uint16_t) (size_t) data;
        SetTextureAsyncParams ap   = GetSetTextureAsyncParams(context->m_SetTextureAsyncState, param_array_index);

        if (dmAtomicGet32(&context->m_DeleteContextRequested))
        {
            return 0;
        }

        VulkanTexture* tex;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
            tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, ap.m_Texture);
        }

        // Texture might have been deleted before the job thread has started processing this job.
        if (tex == NULL)
        {
            return 0;
        }

        // Async texture uploading
        {
            VkCommandBuffer cmd_buffer = BeginSingleTimeCommands(context->m_LogicalDevice.m_Device, context->m_LogicalDevice.m_CommandPoolWorker);

            uint8_t tex_layer_count   = dmMath::Max(tex->m_LayerCount, ap.m_Params.m_LayerCount);
            uint16_t tex_depth        = dmMath::Max((uint16_t) 1, dmMath::Max(tex->m_Depth, ap.m_Params.m_Depth));
            TextureFormat format_orig = ap.m_Params.m_Format;
            void*  tex_data_ptr       = (void*) ap.m_Params.m_Data;
            uint8_t* temp_data        = 0;
            bool is_memoryless        = IsTextureMemoryless(tex);

            uint32_t tex_data_size;
            if (IsTextureFormatASTC(ap.m_Params.m_Format))
            {
                tex_data_size = ap.m_Params.m_DataSize;
            }
            else
            {
                uint32_t tex_bpp           = GetTextureFormatBitsPerPixel(ap.m_Params.m_Format);
                uint32_t tex_data_size_bpp = tex_bpp * ap.m_Params.m_Width * ap.m_Params.m_Height * tex_depth * tex_layer_count;
                tex_data_size = (uint32_t) ceil((float) tex_data_size_bpp / 8.0f);
            }

            DeviceBuffer stage_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

            if (!is_memoryless)
            {
                // Note: There's no RGB support in Vulkan. We have to expand this to four channels
                // TODO: Can we use R11G11B10 somehow?
                if (format_orig == TEXTURE_FORMAT_RGB)
                {
                    uint32_t data_pixel_count = ap.m_Params.m_Width * ap.m_Params.m_Height * tex_layer_count;
                    temp_data                 = new uint8_t[data_pixel_count * 4]; // RGBA => 4 bytes per pixel

                    RepackRGBToRGBA(data_pixel_count, (uint8_t*) tex_data_ptr, temp_data);
                    tex_data_ptr  = temp_data;
                    uint32_t tex_bpp       = 32;
                    uint32_t tex_data_size_bpp = tex_bpp * ap.m_Params.m_Width * ap.m_Params.m_Height * tex_depth * tex_layer_count;
                    tex_data_size = (uint32_t) ceil((float) tex_data_size_bpp / 8.0f);
                }

                TransitionImageLayoutWithCmdBuffer(cmd_buffer, tex, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ap.m_Params.m_MipMap, tex_layer_count);

                VkResult res = CreateDeviceBuffer(
                    context->m_PhysicalDevice.m_Device,
                    context->m_LogicalDevice.m_Device, tex_data_size,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stage_buffer);
                CHECK_VK_ERROR(res);

                CopyToTextureWithStageBuffer(context, cmd_buffer, &stage_buffer, tex, ap.m_Params, tex_data_size, tex_data_ptr);

                // update the resource size
                tex->m_DataSize = tex_data_size;
            }

            TransitionImageLayoutWithCmdBuffer(cmd_buffer, tex, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, ap.m_Params.m_MipMap, tex_layer_count);

            VkFence fence;
            VkResult res = SubmitCommandBuffer(context->m_LogicalDevice.m_Device, context->m_LogicalDevice.m_GraphicsQueue, cmd_buffer, &fence);
            CHECK_VK_ERROR(res);

            if (!is_memoryless)
            {
                // We wait for the fence here so we don't keep a bunch of large buffers around.
                // Waiting for the fence should be fine here anyway, since we have commited the work on a different queue than the "main" graphics queue
                vkWaitForFences(context->m_LogicalDevice.m_Device, 1, &fence, VK_TRUE, UINT64_MAX);
                vkDestroyFence(context->m_LogicalDevice.m_Device, fence, NULL);
                DestroyDeviceBuffer(context->m_LogicalDevice.m_Device, &stage_buffer.m_Handle);
                vkFreeCommandBuffers(context->m_LogicalDevice.m_Device, context->m_LogicalDevice.m_CommandPoolWorker, 1, &cmd_buffer);
            }

            delete[] temp_data;
        }

        int32_t data_state = dmAtomicGet32(&tex->m_DataState);
        data_state &= ~(1<<ap.m_Params.m_MipMap);
        dmAtomicStore32(&tex->m_DataState, data_state);

        return 0;
    }

    // Called on thread where we update (which should be the main thread)
    static void AsyncCompleteCallback(dmJobThread::HContext, dmJobThread::HJob job, dmJobThread::JobStatus status, void* _context, void* data, int result)
    {
        VulkanContext* context     = (VulkanContext*) _context;
        uint16_t param_array_index = (uint16_t) (size_t) data;
        SetTextureAsyncParams ap   = GetSetTextureAsyncParams(context->m_SetTextureAsyncState, param_array_index);

        if (ap.m_Callback)
        {
            ap.m_Callback(ap.m_Texture, ap.m_UserData);
        }

        ReturnSetTextureAsyncIndex(context->m_SetTextureAsyncState, param_array_index);
    }

    static void PrepareTextureForUploading(VulkanContext* context, VulkanTexture* texture, const TextureParams& params)
    {
        VkFormat vk_format = GetVulkanFormatFromTextureFormat(params.m_Format);
        if (!params.m_SubUpdate && params.m_MipMap == 0)
        {
            if (texture->m_Format != vk_format ||
                texture->m_Width != params.m_Width ||
                texture->m_Height != params.m_Height ||
                (IsTextureType3D(texture->m_Type) && (texture->m_Depth != params.m_Depth)))
            {
                DestroyResourceDeferred(context, texture);
                texture->m_Format = vk_format;
                texture->m_Width  = params.m_Width;
                texture->m_Height = params.m_Height;
                texture->m_Depth  = params.m_Depth;

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
        // If texture hasn't been used yet or if it has been changed
        if (texture->m_Destroyed || texture->m_Handle.m_Image == VK_NULL_HANDLE)
        {
            VkImageTiling vk_image_tiling           = VK_IMAGE_TILING_OPTIMAL;
            VkImageUsageFlags vk_usage_flags        = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            VkFormatFeatureFlags vk_format_features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            VkMemoryPropertyFlags vk_memory_type    = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            uint16_t tex_depth                      = dmMath::Max(texture->m_Depth, params.m_Depth);
            uint8_t tex_layer_count                 = dmMath::Max(texture->m_LayerCount, params.m_LayerCount);
            TextureFormat format_orig               = params.m_Format;
            if (format_orig == TEXTURE_FORMAT_RGB)
            {
                vk_format = VK_FORMAT_R8G8B8A8_UNORM;
            }

            vk_usage_flags |= texture->m_UsageFlags;

            if (IsTextureMemoryless(texture))
            {
                vk_memory_type |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
            }

            // Check this format for optimal layout support
            if (GetSupportedTilingFormat(context->m_PhysicalDevice.m_Device, &vk_format, 1, vk_image_tiling, vk_format_features) == VK_FORMAT_UNDEFINED)
            {
                // Linear doesn't support mipmapping (for MoltenVK only?)
                vk_image_tiling        = VK_IMAGE_TILING_LINEAR;
                texture->m_MipMapCount = 1;
            }

            VkResult res = CreateTexture(
                context->m_PhysicalDevice.m_Device,
                context->m_LogicalDevice.m_Device,
                texture->m_Width,
                texture->m_Height,
                tex_depth,
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

            texture->m_GraphicsFormat = params.m_Format;
            texture->m_MipMapCount    = dmMath::Max(texture->m_MipMapCount, (uint16_t)(params.m_MipMap+1));
            texture->m_LayerCount     = tex_layer_count;

            // Not thread safe, but no-one should be touching the texture right now anyway
            VulkanSetTextureParamsInternal(context, texture, params.m_MinFilter, params.m_MagFilter, params.m_UWrap, params.m_VWrap, 1.0f);
        }
    }

    static void VulkanSetTextureAsync(HContext _context, HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        VulkanContext* context = (VulkanContext*)_context;
        if (context->m_AsyncProcessingSupport)
        {
            DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
            VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);

            PrepareTextureForUploading(context, tex, params);

            tex->m_DataState          |= 1<<params.m_MipMap;
            uint16_t param_array_index = PushSetTextureAsyncState(context->m_SetTextureAsyncState, texture, params, callback, user_data);

            dmJobThread::Job job = {0};
            job.m_Process = AsyncProcessCallback;
            job.m_Callback = AsyncCompleteCallback;
            job.m_Context = (void*) context;
            job.m_Data = (void*) (uintptr_t) param_array_index;

            dmJobThread::HJob hjob = dmJobThread::CreateJob(context->m_JobThread, &job);
            dmJobThread::PushJob(context->m_JobThread, hjob);
        } 
        else
        {
            VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
            VulkanSetTextureInternal(context, tex, params);
            if (callback)
            {
                callback(texture, user_data);
            }
        }
    }

    static float GetMaxAnisotrophyClamped(VulkanContext* context, float max_anisotropy_requested)
    {
        return dmMath::Min(max_anisotropy_requested, context->m_PhysicalDevice.m_Properties.limits.maxSamplerAnisotropy);
    }

    static void VulkanSetTextureParamsInternal(VulkanContext* context, VulkanTexture* texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        TextureSampler sampler   = context->m_TextureSamplers[texture->m_TextureSamplerIndex];
        float anisotropy_clamped = GetMaxAnisotrophyClamped(context, max_anisotropy);

        if (sampler.m_MinFilter     != minfilter              ||
            sampler.m_MagFilter     != magfilter              ||
            sampler.m_AddressModeU  != uwrap                  ||
            sampler.m_AddressModeV  != vwrap                  ||
            sampler.m_MaxLod        != texture->m_MipMapCount ||
            sampler.m_MaxAnisotropy != anisotropy_clamped)
        {
            int16_t sampler_index = GetTextureSamplerIndex(context, context->m_TextureSamplers, minfilter, magfilter, uwrap, vwrap, texture->m_MipMapCount, anisotropy_clamped);
            if (sampler_index < 0)
            {
                sampler_index = CreateVulkanTextureSampler(context, context->m_LogicalDevice.m_Device, context->m_TextureSamplers, minfilter, magfilter, uwrap, vwrap, texture->m_MipMapCount, anisotropy_clamped);
            }

            texture->m_TextureSamplerIndex = sampler_index;
        }
    }

    static void VulkanSetTextureParams(HContext _context, HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        VulkanSetTextureParamsInternal(context, tex, minfilter, magfilter, uwrap, vwrap, max_anisotropy);
    }

    // NOTE: Currently over estimates the resource usage for compressed formats!
    static uint32_t VulkanGetTextureResourceSize(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        if (!tex)
        {
            return 0;
        }

        if (tex->m_DataSize)
            return tex->m_DataSize + sizeof(VulkanTexture);

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

    static uint16_t VulkanGetTextureWidth(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_Width : 0;
    }

    static uint16_t VulkanGetTextureHeight(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_Height : 0;
    }

    static uint16_t VulkanGetOriginalTextureWidth(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_OriginalWidth : 0;
    }

    static uint16_t VulkanGetOriginalTextureHeight(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_OriginalHeight : 0;
    }

    static uint16_t VulkanGetTextureDepth(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_Depth : 0;
    }

    static uint8_t VulkanGetTextureMipmapCount(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_MipMapCount : 0;
    }

    static TextureType VulkanGetTextureType(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_Type : TEXTURE_TYPE_2D;
    }

    static uint32_t VulkanGetTextureUsageHintFlags(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_UsageHintFlags : 0;
    }

    static uint8_t VulkanGetTexturePageCount(HTexture texture)
    {
        DM_MUTEX_SCOPED_LOCK(g_VulkanContext->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(g_VulkanContext->m_AssetHandleContainer, texture);
        return tex ? tex->m_PageCount : 0;
    }

    static HandleResult VulkanGetTextureHandle(HTexture texture, void** out_handle)
    {
        assert(0 && "GetTextureHandle is not implemented on Vulkan.");
        return HANDLE_RESULT_NOT_AVAILABLE;
    }

    static uint8_t VulkanGetNumTextureHandles(HContext _context, HTexture texture)
    {
        return 1;
    }

    static void VulkanEnableTexture(HContext _context, uint32_t unit, uint8_t value_index, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(unit < DM_MAX_TEXTURE_UNITS);
        context->m_TextureUnits[unit] = texture;
    }

    static void VulkanDisableTexture(HContext _context, uint32_t unit, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        assert(unit < DM_MAX_TEXTURE_UNITS);
        context->m_TextureUnits[unit] = 0x0;
    }

    static uint32_t VulkanGetMaxTextureSize(HContext _context)
    {
        VulkanContext* context = (VulkanContext*)_context;
        return context->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D;
    }

    static uint32_t VulkanGetTextureStatusFlags(HContext _context, HTexture texture)
    {
        VulkanContext* context = (VulkanContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);
        uint32_t flags     = TEXTURE_STATUS_OK;
        if(tex && dmAtomicGet32(&tex->m_DataState))
        {
            flags |= TEXTURE_STATUS_DATA_PENDING;
        }
        return flags;
    }

    static void VulkanReadPixels(HContext _context, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t buffer_size)
    {
        VulkanContext* context = (VulkanContext*) _context;

        assert (buffer_size >= width * height * 4);

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

        VkCommandBuffer vk_command_buffer = BeginSingleTimeCommands(context->m_LogicalDevice.m_Device, context->m_LogicalDevice.m_CommandPool);

        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        VulkanTexture* tex_sc = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, context->m_CurrentSwapchainTexture);

        res = TransitionImageLayout(context->m_LogicalDevice.m_Device,
                context->m_LogicalDevice.m_CommandPool,
                context->m_LogicalDevice.m_GraphicsQueue,
                tex_sc,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        CHECK_VK_ERROR(res);

        VkBufferImageCopy vk_copy_region = {};
        vk_copy_region.imageOffset.x               = x;
        vk_copy_region.imageOffset.y               = y;
        vk_copy_region.imageExtent.width           = width;
        vk_copy_region.imageExtent.height          = height;
        vk_copy_region.imageExtent.depth           = 1;
        vk_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vk_copy_region.imageSubresource.layerCount = 1;

        TouchResource(context, tex_sc);
        TouchResource(context, &stage_buffer);

        vkCmdCopyImageToBuffer(
            vk_command_buffer,
            context->m_SwapChain->Image(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            stage_buffer.m_Handle.m_Buffer,
            1, &vk_copy_region);

        VkFence fence;
        res = SubmitCommandBuffer(context->m_LogicalDevice.m_Device, context->m_LogicalDevice.m_GraphicsQueue, vk_command_buffer, &fence);
        CHECK_VK_ERROR(res);

        // Wait for the copy command to finish
        vkWaitForFences(context->m_LogicalDevice.m_Device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(context->m_LogicalDevice.m_Device, fence, NULL);

        res = stage_buffer.MapMemory(context->m_LogicalDevice.m_Device);
        CHECK_VK_ERROR(res);

        memcpy(buffer, stage_buffer.m_MappedDataPtr, stage_buffer.m_MemorySize);

        stage_buffer.UnmapMemory(context->m_LogicalDevice.m_Device);

        DestroyResourceDeferred(context, &stage_buffer);

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
        DestroyPipeline(context->m_LogicalDevice.m_Device, value);
    }

    static bool VulkanIsContextFeatureSupported(HContext _context, ContextFeature feature)
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

        DM_MUTEX_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
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

    static void VulkanInvalidateGraphicsHandles(HContext context)
    {
        // NOP
    }

    ///////////////////////////////////
    // dmsdk / graphics_vulkan.h impls:
    ///////////////////////////////////

    HTexture VulkanGetActiveSwapChainTexture(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_CurrentSwapchainTexture;
    }

    VkDevice VulkanGetDevice(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_LogicalDevice.m_Device;
    }

    VkPhysicalDevice VulkanGetPhysicalDevice(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_PhysicalDevice.m_Device;
    }

    VkInstance VulkanGetInstance(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_Instance;
    }

    uint16_t VulkanGetGraphicsQueueFamily(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_SwapChain->m_QueueFamily.m_GraphicsQueueIx;
    }

    VkQueue VulkanGetGraphicsQueue(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_LogicalDevice.m_GraphicsQueue;

    }

    VkRenderPass VulkanGetRenderPass(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_MainRenderPass;
    }

    VkCommandBuffer VulkanGetCurrentFrameCommandBuffer(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        return context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex];
    }

    bool VulkanCreateDescriptorPool(VkDevice vk_device, uint16_t max_descriptors, VkDescriptorPool* vk_descriptor_pool_out)
    {
        VkResult res = CreateDescriptorPool(vk_device, max_descriptors, vk_descriptor_pool_out);
        return res == VK_SUCCESS;
    }


    static GraphicsAdapterFunctionTable VulkanRegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table = {};
        DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, Vulkan);
        return fn_table;
    }
}
