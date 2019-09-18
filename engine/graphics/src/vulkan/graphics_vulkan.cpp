#include "../graphics.h"
#include "graphics_vulkan.h"

#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <graphics/glfw/glfw.h>
#include <graphics/glfw/glfw_native.h>

#include <dmsdk/vectormath/cpp/vectormath_aos.h>

namespace dmGraphics
{
    #define CHECK_VK_ERROR(result, stmt) \
        result = stmt; \
        if(g_Context->m_VerifyGraphicsCalls && result != VK_SUCCESS) { \
            dmLogError("Vulkan Error (%s:%d): \n%s", __FILE__, __LINE__, #stmt); \
            assert(0); \
        }

    struct Context
    {
        Context(const ContextParams& params, const VkInstance vk_instance)
        : m_Instance(vk_instance)
        , m_MainRenderTarget(0)
        , m_DefaultTextureMinFilter(params.m_DefaultTextureMinFilter)
        , m_DefaultTextureMagFilter(params.m_DefaultTextureMagFilter)
        , m_VerifyGraphicsCalls(params.m_VerifyGraphicsCalls)
        {}

        SwapChain*            m_SwapChain;
        SwapChainCapabilities m_SwapChainCapabilities;
        PhysicalDevice        m_PhysicalDevice;
        LogicalDevice         m_LogicalDevice;
        VkInstance            m_Instance;
        VkSurfaceKHR          m_WindowSurface;

        // Main device rendering constructs
        dmArray<VkFramebuffer> m_MainFramebuffers;
        VkCommandPool          m_MainCommandPool;
        VkRenderPass           m_MainRenderPass;
        Texture                m_MainTextureDepthStencil;
        RenderTarget           m_MainRenderTarget;

        TextureFilter         m_DefaultTextureMinFilter;
        TextureFilter         m_DefaultTextureMagFilter;
        uint32_t              m_WindowOpened        : 1;
        uint32_t              m_VerifyGraphicsCalls : 1;
        uint32_t              : 30;
    };

    static const char* g_validation_layers[]      = { "VK_LAYER_LUNARG_standard_validation" };
    static const uint8_t g_validation_layer_count = 1;

    static Context* g_Context = 0;

    static const char* VkResultToStr(VkResult res)
    {
        switch(res)
        {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_EVENT_SET: return "VK_EVENT_SET";
            case VK_EVENT_RESET: return "VK_EVENT_RESET";
            case VK_INCOMPLETE: return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
            case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
            case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
            case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
            case VK_ERROR_FRAGMENTATION_EXT: return "VK_ERROR_FRAGMENTATION_EXT";
            case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
            case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT: return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
            case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM";
            default: break;
        }

        return "UNKNOWN_ERROR";
    }

    static inline void SynchronizeDevice(VkDevice vk_device)
    {
        vkDeviceWaitIdle(vk_device);
    }

    static inline uint32_t GetNextRenderTargetId()
    {
        static uint32_t next_id = 1;

        if (++next_id == 0)
        {
            next_id = 1;
        }

        return next_id;
    }

    static VkResult CreateCommandBuffers(VkDevice vk_device, uint32_t numBuffersToCreate, VkCommandPool vk_command_pool, VkCommandBuffer* vk_command_buffers_out)
    {
        VkCommandBufferAllocateInfo vk_buffers_allocate_info;
        memset(&vk_buffers_allocate_info, 0, sizeof(VkCommandBufferAllocateInfo));

        vk_buffers_allocate_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        vk_buffers_allocate_info.commandPool        = vk_command_pool;
        vk_buffers_allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        vk_buffers_allocate_info.commandBufferCount = numBuffersToCreate;

        return vkAllocateCommandBuffers(vk_device, &vk_buffers_allocate_info, vk_command_buffers_out);
    }

    static VkResult CreateFramebuffer(VkDevice vk_device, VkRenderPass vk_render_pass, uint32_t width, uint32_t height, VkImageView* vk_attachments, uint8_t attachmentCount, VkFramebuffer* vk_framebuffer_out)
    {
        VkFramebufferCreateInfo vk_framebuffer_create_info;
        memset(&vk_framebuffer_create_info, 0, sizeof(vk_framebuffer_create_info));

        vk_framebuffer_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        vk_framebuffer_create_info.renderPass      = vk_render_pass;
        vk_framebuffer_create_info.attachmentCount = attachmentCount;
        vk_framebuffer_create_info.pAttachments    = vk_attachments;
        vk_framebuffer_create_info.width           = width;
        vk_framebuffer_create_info.height          = height;
        vk_framebuffer_create_info.layers          = 1;

        return vkCreateFramebuffer(vk_device, &vk_framebuffer_create_info, 0, vk_framebuffer_out);
    }

    // Tiling is related to how an image is laid out in memory, either in 'optimal' or 'linear' fashion.
    // Optimal is always sought after since it should be the most performant (depending on hardware),
    // but is not always supported.
    static const VkFormat GetSupportedTilingFormat(VkPhysicalDevice vk_physical_device, VkFormat* vk_format_candidates,
        uint32_t vk_num_format_candidates, VkImageTiling vk_tiling_type, VkFormatFeatureFlags vk_format_flags)
    {
        for (uint32_t i=0; i < vk_num_format_candidates; i++)
        {
            VkFormatProperties format_properties;
            VkFormat formatCandidate = vk_format_candidates[i];

            vkGetPhysicalDeviceFormatProperties(vk_physical_device, formatCandidate, &format_properties);

            if ((vk_tiling_type == VK_IMAGE_TILING_LINEAR && (format_properties.linearTilingFeatures & vk_format_flags)) ||
                (vk_tiling_type == VK_IMAGE_TILING_OPTIMAL && (format_properties.optimalTilingFeatures & vk_format_flags)))
            {
                return formatCandidate;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }

    static VkResult TransitionImageLayout(VkDevice vk_device, VkCommandPool vk_command_pool, VkQueue vk_graphics_queue, VkImage vk_image,
        VkImageAspectFlags vk_image_aspect, VkImageLayout vk_from_layout, VkImageLayout vk_to_layout)
    {
        // Create a one-time-execute command buffer that will only be used for the transition
        VkCommandBuffer vk_command_buffer;
        CreateCommandBuffers(vk_device, 1, vk_command_pool, &vk_command_buffer);

        VkCommandBufferBeginInfo vk_command_buffer_begin_info;
        memset(&vk_command_buffer_begin_info, 0, sizeof(VkCommandBufferBeginInfo));

        vk_command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vk_command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info);

        VkImageMemoryBarrier vk_memory_barrier;
        memset(&vk_memory_barrier, 0, sizeof(vk_memory_barrier));

        vk_memory_barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        vk_memory_barrier.oldLayout                       = vk_from_layout;
        vk_memory_barrier.newLayout                       = vk_to_layout;
        vk_memory_barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        vk_memory_barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        vk_memory_barrier.image                           = vk_image;
        vk_memory_barrier.subresourceRange.aspectMask     = vk_image_aspect;
        vk_memory_barrier.subresourceRange.baseMipLevel   = 0;
        vk_memory_barrier.subresourceRange.levelCount     = 1;
        vk_memory_barrier.subresourceRange.baseArrayLayer = 0;
        vk_memory_barrier.subresourceRange.layerCount     = 1;

        VkPipelineStageFlags vk_source_stage      = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags vk_destination_stage = VK_IMAGE_LAYOUT_UNDEFINED;

        // These stage changes are explicit in our case:
        //   1) undefined -> transfer. This transition is used for staging buffers when uploading texture data
        //   2) transfer  -> shader read. This transition is used when the staging transfer is complete.
        //   3) undefined -> depth stencil. This transition is used when creating a depth buffer.
        if (vk_from_layout == VK_IMAGE_LAYOUT_UNDEFINED && vk_to_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            vk_memory_barrier.srcAccessMask = 0;
            vk_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            vk_source_stage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            vk_destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (vk_from_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && vk_to_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            vk_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vk_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vk_source_stage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
            vk_destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (vk_from_layout == VK_IMAGE_LAYOUT_UNDEFINED && vk_to_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            vk_memory_barrier.srcAccessMask = 0;
            vk_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            vk_source_stage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            vk_destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else
        {
            assert(0);
        }

        vkCmdPipelineBarrier(
            vk_command_buffer,
            vk_source_stage,
            vk_destination_stage,
            0,
            0, 0,
            0, 0,
            1, &vk_memory_barrier
        );

        vkEndCommandBuffer(vk_command_buffer);

        VkSubmitInfo vk_submit_info;
        memset(&vk_submit_info, 0, sizeof(VkSubmitInfo));

        vk_submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        vk_submit_info.commandBufferCount = 1;
        vk_submit_info.pCommandBuffers    = &vk_command_buffer;

        vkQueueSubmit(vk_graphics_queue, 1, &vk_submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(vk_graphics_queue);
        vkFreeCommandBuffers(vk_device, vk_command_pool, 1, &vk_command_buffer);

        return VK_SUCCESS;
    }

    static VkResult AllocateTexture2D(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        uint32_t imageWidth, uint32_t imageHeight, uint16_t imageMips,
        VkFormat vk_format, VkImageTiling vk_tiling,
        VkImageUsageFlags vk_usage, Texture* textureOut)
    {
        assert(textureOut);
        assert(textureOut->m_ImageView == VK_NULL_HANDLE);
        assert(textureOut->m_DeviceMemory.m_Memory == VK_NULL_HANDLE && textureOut->m_DeviceMemory.m_MemorySize == 0);

        VkImageFormatProperties vk_format_properties;

        VkResult res = vkGetPhysicalDeviceImageFormatProperties(
            vk_physical_device, vk_format, VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL, vk_usage, 0, &vk_format_properties);

        if (res != VK_SUCCESS)
        {
            return res;
        }

        VkImageCreateInfo vk_image_create_info;
        memset(&vk_image_create_info, 0, sizeof(vk_image_create_info));

        vk_image_create_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        vk_image_create_info.imageType     = VK_IMAGE_TYPE_2D;
        vk_image_create_info.extent.width  = imageWidth;
        vk_image_create_info.extent.height = imageHeight;
        vk_image_create_info.extent.depth  = 1;
        vk_image_create_info.mipLevels     = imageMips;
        vk_image_create_info.arrayLayers   = 1;
        vk_image_create_info.format        = vk_format;
        vk_image_create_info.tiling        = vk_tiling;
        vk_image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vk_image_create_info.usage         = vk_usage;
        vk_image_create_info.samples       = VK_SAMPLE_COUNT_1_BIT;
        vk_image_create_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        vk_image_create_info.flags         = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        res = vkCreateImage(vk_device, &vk_image_create_info, 0, &textureOut->m_Image);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        VkImageViewCreateInfo vk_view_create_info;
        memset(&vk_view_create_info, 0, sizeof(vk_view_create_info));

        vk_view_create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vk_view_create_info.image                           = textureOut->m_Image;
        vk_view_create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vk_view_create_info.format                          = vk_format;
        vk_view_create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        vk_view_create_info.subresourceRange.baseMipLevel   = 0;
        vk_view_create_info.subresourceRange.levelCount     = 1;
        vk_view_create_info.subresourceRange.baseArrayLayer = 0;
        vk_view_create_info.subresourceRange.layerCount     = 1;

        textureOut->m_Format = vk_format;

        return vkCreateImageView(vk_device, &vk_view_create_info, 0, &textureOut->m_ImageView);;
    }

    static void ResetTexture(VkDevice vk_device, Texture* textureOut)
    {
        vkDestroyImageView(vk_device, textureOut->m_ImageView, 0);
        vkDestroyImage(vk_device, textureOut->m_Image, 0);
        vkFreeMemory(vk_device, textureOut->m_DeviceMemory.m_Memory, 0);
        textureOut->m_ImageView             = VK_NULL_HANDLE;
        textureOut->m_Image                 = VK_NULL_HANDLE;
        textureOut->m_DeviceMemory.m_Memory = VK_NULL_HANDLE;
    }

    static VkResult AllocateDepthStencilTexture(HContext context, uint32_t width, uint32_t height, Texture* depthStencilTextureOut)
    {
        // Depth formats are optional, so we need to query
        // what available formats we have.
        VkFormat vk_format_list_default[] = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        const VkPhysicalDevice vk_physical_device = context->m_PhysicalDevice.m_Device;
        const VkDevice vk_device = context->m_LogicalDevice.m_Device;

        const size_t vk_format_list_size = sizeof(vk_format_list_default) / sizeof(vk_format_list_default[0]);

        VkImageTiling vk_image_tiling = VK_IMAGE_TILING_OPTIMAL;
        VkFormat vk_depth_format = GetSupportedTilingFormat(vk_physical_device, &vk_format_list_default[0],
            vk_format_list_size, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        if (vk_depth_format == VK_FORMAT_UNDEFINED)
        {
            vk_image_tiling = VK_IMAGE_TILING_LINEAR;
            vk_depth_format = GetSupportedTilingFormat(vk_physical_device, &vk_format_list_default[0],
                vk_format_list_size, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }

        VkResult res;
        CHECK_VK_ERROR(res, AllocateTexture2D(vk_physical_device, vk_device, width, height, 1,
            vk_depth_format, vk_image_tiling, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthStencilTextureOut));

        if (res == VK_SUCCESS)
        {
            VkImageAspectFlags vk_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (vk_depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                vk_depth_format == VK_FORMAT_D24_UNORM_S8_UINT  ||
                vk_depth_format == VK_FORMAT_D16_UNORM_S8_UINT)
            {
                vk_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            TransitionImageLayout(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_LogicalDevice.m_GraphicsQueue, depthStencilTextureOut->m_Image, vk_aspect,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }

        return res;
    }

    static VkResult CreateRenderPass(VkDevice vk_device, RenderPassAttachment* colorAttachments, uint8_t numColorAttachments, RenderPassAttachment* depthStencilAttachment, VkRenderPass* renderPassOut)
    {
        assert(*renderPassOut == VK_NULL_HANDLE);

        uint8_t num_attachments  = numColorAttachments + (depthStencilAttachment ? 1 : 0);
        VkAttachmentDescription* vk_attachment_desc    = new VkAttachmentDescription[num_attachments];
        VkAttachmentReference* vk_attachment_color_ref = new VkAttachmentReference[numColorAttachments];
        VkAttachmentReference vk_attachment_depth_ref;

        for (uint16_t i=0; i < numColorAttachments; i++)
        {
            VkAttachmentDescription& attachment_color = vk_attachment_desc[i];

            attachment_color.format         = colorAttachments[i].m_Format;
            attachment_color.samples        = VK_SAMPLE_COUNT_1_BIT;
            attachment_color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment_color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment_color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment_color.finalLayout    = colorAttachments[i].m_ImageLayout;

            VkAttachmentReference& ref = vk_attachment_color_ref[i];
            ref.attachment = i;
            ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        if (depthStencilAttachment)
        {
            VkAttachmentDescription& attachment_depth = vk_attachment_desc[numColorAttachments];

            attachment_depth.format         = depthStencilAttachment->m_Format;
            attachment_depth.samples        = VK_SAMPLE_COUNT_1_BIT;
            attachment_depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment_depth.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment_depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment_depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment_depth.finalLayout    = depthStencilAttachment->m_ImageLayout;

            vk_attachment_depth_ref.attachment = numColorAttachments;
            vk_attachment_depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        // Subpass dependencies describe access patterns between several 'sub-passes',
        // e.g for a post-processing stack you would likely have a dependency chain between
        // the results of various draw calls, which could then be specefied as subpass dependencies
        // which could yield optimal performance. We don't have any way of describing the access flow
        // yet so for now we just create a single subpass that connects an external source
        // (anything that has happend before this call) to the color output of the render pass,
        // which should be fine in most cases.
        VkSubpassDependency vk_sub_pass_dependency;
        memset(&vk_sub_pass_dependency, 0, sizeof(vk_sub_pass_dependency));
        vk_sub_pass_dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        vk_sub_pass_dependency.dstSubpass    = 0;
        vk_sub_pass_dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        vk_sub_pass_dependency.srcAccessMask = 0;
        vk_sub_pass_dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        vk_sub_pass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // The subpass description connects the input attachments to the render pass,
        // in a MRT situation writing to specific color outputs (gl_FragData[x]) match these numbers.
        VkSubpassDescription vk_sub_pass_description;
        memset(&vk_sub_pass_description, 0, sizeof(vk_sub_pass_description));

        vk_sub_pass_description.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vk_sub_pass_description.colorAttachmentCount    = numColorAttachments;
        vk_sub_pass_description.pColorAttachments       = vk_attachment_color_ref;
        vk_sub_pass_description.pDepthStencilAttachment = depthStencilAttachment ? &vk_attachment_depth_ref : 0;

        VkRenderPassCreateInfo render_pass_create_info;
        memset(&render_pass_create_info, 0, sizeof(render_pass_create_info));

        render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = num_attachments;
        render_pass_create_info.pAttachments    = vk_attachment_desc;
        render_pass_create_info.subpassCount    = 1;
        render_pass_create_info.pSubpasses      = &vk_sub_pass_description;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies   = &vk_sub_pass_dependency;

        VkResult res;
        CHECK_VK_ERROR(res,vkCreateRenderPass(vk_device, &render_pass_create_info, 0, renderPassOut))

        delete[] vk_attachment_desc;
        delete[] vk_attachment_color_ref;

        return res;
    }

    static VkResult CreateMainRenderTarget(HContext context)
    {
        assert(context);
        assert(context->m_SwapChain);

        // We need to create a framebuffer per swap chain image
        // so that they can be used in different states in the rendering pipeline
        context->m_MainFramebuffers.SetCapacity(context->m_SwapChain->m_Images.Size());
        context->m_MainFramebuffers.SetSize(context->m_SwapChain->m_Images.Size());

        SwapChain* swapChain = context->m_SwapChain;
        VkResult res;

        for (uint8_t i=0; i < context->m_MainFramebuffers.Size(); i++)
        {
            // All swap chain images can share the same depth buffer,
            // that's why we create a single depth buffer at the start and reuse it.
            VkImageView&   vk_image_view_color     = swapChain->m_ImageViews[i];
            VkImageView&   vk_image_view_depth     = context->m_MainTextureDepthStencil.m_ImageView;
            VkImageView    vk_image_attachments[2] = { vk_image_view_color, vk_image_view_depth };
            CHECK_VK_ERROR(res, CreateFramebuffer(context->m_LogicalDevice.m_Device, context->m_MainRenderPass,
                swapChain->m_ImageExtent.width, swapChain->m_ImageExtent.height,
                vk_image_attachments, sizeof(vk_image_attachments) / sizeof(vk_image_attachments[0]), &context->m_MainFramebuffers[i]))
        }

        // Initialize the dummy rendertarget for the main framebuffer
        // The m_Framebuffer construct will be rotated sequentially
        // with the framebuffer objects created per swap chain.
        RenderTarget& rt = context->m_MainRenderTarget;
        rt.m_RenderPass  = context->m_MainRenderPass;
        rt.m_Framebuffer = context->m_MainFramebuffers[0];

        return res;
    }

    static VkResult CreateMainRenderingResources(HContext context)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;
        VkResult res;

        // Create depth/stencil buffer
        CHECK_VK_ERROR(res, AllocateDepthStencilTexture(context,
            context->m_SwapChain->m_ImageExtent.width,
            context->m_SwapChain->m_ImageExtent.height,
            &context->m_MainTextureDepthStencil))

        // Create main render pass with two attachments
        RenderPassAttachment attachments[2];
        attachments[0].m_Format      = context->m_SwapChain->m_SurfaceFormat.format;;
        attachments[0].m_ImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[1].m_Format      = context->m_MainTextureDepthStencil.m_Format;
        attachments[1].m_ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        CHECK_VK_ERROR(res, CreateRenderPass(vk_device, attachments, 1, &attachments[1], &context->m_MainRenderPass))
        CHECK_VK_ERROR(res, CreateMainRenderTarget(context))

        return res;
    }

    static void SwapChainChanged(HContext context, uint32_t* width, uint32_t* height)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;
        VkResult res;
        // Flush all current commands
        SynchronizeDevice(vk_device);
        CHECK_VK_ERROR(res, UpdateSwapChain(context->m_SwapChain, width, height, true, context->m_SwapChainCapabilities))

        // Reset & create main Depth/Stencil buffer
        ResetTexture(vk_device, &context->m_MainTextureDepthStencil);
        CHECK_VK_ERROR(res, AllocateDepthStencilTexture(context,
            context->m_SwapChain->m_ImageExtent.width,
            context->m_SwapChain->m_ImageExtent.height,
            &context->m_MainTextureDepthStencil))

        // Reset main rendertarget (but not the render pass)
        RenderTarget* mainRenderTarget = &context->m_MainRenderTarget;
        mainRenderTarget->m_RenderPass = VK_NULL_HANDLE;
        for (uint8_t i=0; i < context->m_MainFramebuffers.Size(); i++)
        {
            mainRenderTarget->m_Framebuffer = context->m_MainFramebuffers[i];
            ResetRenderTarget(&context->m_LogicalDevice, mainRenderTarget);
        }

        CHECK_VK_ERROR(res, CreateMainRenderTarget(context))

        // Flush once again to make sure all transitions are complete
        SynchronizeDevice(vk_device);
    }

    HContext NewContext(const ContextParams& params)
    {
        if (g_Context == 0x0)
        {
            if (glfwInit() == 0)
            {
                dmLogError("Could not initialize glfw.");
                return 0x0;
            }

            bool enable_validation = false;

            const char* env_vulkan_validation = getenv("DM_VULKAN_VALIDATION");
            if (env_vulkan_validation != 0x0)
            {
                enable_validation = strtol(env_vulkan_validation, 0, 10);
            }

            VkInstance vk_instance;
            if (CreateInstance(&vk_instance, g_validation_layers, enable_validation) != VK_SUCCESS)
            {
                dmLogError("Could not create Vulkan instance");
                return 0x0;
            }

            g_Context = new Context(params, vk_instance);

            return g_Context;
        }
        return 0x0;
    }

    void DeleteContext(HContext context)
    {
        if (context != 0x0)
        {
            delete context;
            g_Context = 0x0;
        }
    }

    bool Initialize()
    {
        return glfwInit();
    }

    void Finalize()
    {
        glfwTerminate();
    }

    uint32_t GetWindowRefreshRate(HContext context)
    {
        return 0;
    }

    WindowResult OpenWindow(HContext context, WindowParams *params)
    {
        assert(context);
        assert(context->m_WindowSurface == VK_NULL_HANDLE);

        glfwOpenWindowHint(GLFW_CLIENT_API,   GLFW_NO_API);
        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, params->m_Samples);

        int mode = params->m_Fullscreen ? GLFW_FULLSCREEN : GLFW_WINDOW;

        if (!glfwOpenWindow(params->m_Width, params->m_Height, 8, 8, 8, 8, 32, 8, mode))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        VkResult res = CreateWindowSurface(context->m_Instance, &context->m_WindowSurface, params->m_HighDPI);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create window surface for Vulkan, reason: %s.", VkResultToStr(res));
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        uint32_t device_count = GetPhysicalDeviceCount(context->m_Instance);

        if (device_count == 0)
        {
            dmLogError("Could not get any Vulkan devices.");
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        PhysicalDevice* device_list     = new PhysicalDevice[device_count];
        PhysicalDevice* selected_device = NULL;
        GetPhysicalDevices(context->m_Instance, &device_list, device_count);

        const uint8_t required_device_extension_count = 2;
        const char* required_device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            // From spec:
            // "Allow negative height to be specified in the VkViewport::height field to
            // perform y-inversion of the clip-space to framebuffer-space transform.
            // This allows apps to avoid having to use gl_Position.y = -gl_Position.y
            // in shaders also targeting other APIs."
            "VK_KHR_maintenance1"
        };

        QueueFamily selected_queue_family;
        SwapChainCapabilities selected_swap_chain_capabilities;
        for (uint32_t i = 0; i < device_count; ++i)
        {
            #define RESET_AND_CONTINUE(d) \
                ResetPhysicalDevice(d); \
                continue;

            PhysicalDevice* device = &device_list[i];

            if (selected_device)
            {
                RESET_AND_CONTINUE(device)
            }

            // Make sure we have a graphics and present queue available
            QueueFamily queue_family = GetQueueFamily(device, context->m_WindowSurface);
            if (!queue_family.IsValid())
            {
                RESET_AND_CONTINUE(device)
            }

            // Make sure all device extensions are supported
            bool all_extensions_found = true;
            for (int32_t ext_i = 0; ext_i < required_device_extension_count; ++ext_i)
            {
                bool found = false;
                for (uint32_t j=0; j < device->m_DeviceExtensionCount; ++j)
                {
                    if (dmStrCaseCmp(device->m_DeviceExtensions[j].extensionName, required_device_extensions[ext_i]) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                all_extensions_found &= found;
            }

            if (!all_extensions_found)
            {
                RESET_AND_CONTINUE(device)
            }

            // Make sure device has swap chain support
            GetSwapChainCapabilities(device, context->m_WindowSurface, selected_swap_chain_capabilities);

            if (selected_swap_chain_capabilities.m_SurfaceFormats.Size() == 0 ||
                selected_swap_chain_capabilities.m_PresentModes.Size() == 0)
            {
                RESET_AND_CONTINUE(device)
            }

            selected_device = device;
            selected_queue_family = queue_family;

            #undef RESET_AND_CONTINUE
        }

        LogicalDevice logical_device;
        uint32_t created_width  = params->m_Width;
        uint32_t created_height = params->m_Height;
        const bool want_vsync   = true;

        if (selected_device == NULL)
        {
            dmLogError("Could not select a suitable Vulkan device.");
            goto bail;
        }

        res = CreateLogicalDevice(selected_device, context->m_WindowSurface, selected_queue_family,
            required_device_extensions, required_device_extension_count,
            g_validation_layers, g_validation_layer_count, &logical_device);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a logical Vulkan device, reason: %s", VkResultToStr(res));
            goto bail;
        }

        context->m_WindowOpened   = 1;
        context->m_PhysicalDevice = *selected_device;
        context->m_LogicalDevice  = logical_device;

        // Create swap chain
        context->m_SwapChainCapabilities.Swap(selected_swap_chain_capabilities);
        context->m_SwapChain = new SwapChain(&context->m_LogicalDevice, context->m_WindowSurface, context->m_SwapChainCapabilities, selected_queue_family);

        res = UpdateSwapChain(context->m_SwapChain, &created_width, &created_height, want_vsync, context->m_SwapChainCapabilities);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a swap chain for Vulkan, reason: %s", VkResultToStr(res));
            goto bail;
        }

        delete[] device_list;

        // Create framebuffers, default renderpass etc.
        res = CreateMainRenderingResources(context);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create main rendering resources for Vulkan, reason: %s", VkResultToStr(res));
            goto bail;
        }

        return WINDOW_RESULT_OK;
bail:
        if (context->m_SwapChain)
            delete context->m_SwapChain;
        if (device_list)
            delete[] device_list;

        return WINDOW_RESULT_WINDOW_OPEN_ERROR;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            VkDevice vk_device = context->m_LogicalDevice.m_Device;

            glfwCloseWindow();

            ResetTexture(vk_device, &context->m_MainTextureDepthStencil);

            vkDestroyRenderPass(vk_device, context->m_MainRenderPass, 0);

            for (uint8_t i=0; i < context->m_MainFramebuffers.Size(); i++)
            {
                vkDestroyFramebuffer(vk_device, context->m_MainFramebuffers[i], 0);
            }

            ResetSwapChain(context->m_SwapChain);

            ResetLogicalDevice(&context->m_LogicalDevice);
            ResetPhysicalDevice(&context->m_PhysicalDevice);

            vkDestroySurfaceKHR(context->m_Instance, context->m_WindowSurface, 0);

            DestroyInstance(&context->m_Instance);

            context->m_WindowOpened = 0;

            delete context->m_SwapChain;
        }
    }

    void IconifyWindow(HContext context)
    {}

    uint32_t GetWindowState(HContext context, WindowState state)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            return glfwGetWindowParam(state);
        }
        else
        {
            return 0;
        }
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        return 0;
    }

    uint32_t GetWidth(HContext context)
    {
        return 0;
    }

    uint32_t GetHeight(HContext context)
    {
        return 0;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        return 0;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        return 0;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {}

    void ResizeWindow(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            uint32_t wanted_width = width;
            uint32_t wanted_height = height;
            SwapChainChanged(context, &wanted_width, &wanted_height);
            glfwSetWindowSize((int)wanted_width, (int)wanted_height);
        }
    }

    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    void Flip(HContext context)
    {}

    void SetSwapInterval(HContext context, uint32_t swap_interval)
    {}

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {}

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return (HVertexBuffer) new uint32_t;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        delete (uint32_t*) buffer;
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {}

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {}

    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        return 0;
    }

    bool UnmapVertexBuffer(HVertexBuffer buffer)
    {
        return true;
    }

    uint32_t GetMaxElementsVertices(HContext context)
    {
        return 65536;
    }

    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return (HIndexBuffer) new uint32_t;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        delete (uint32_t*) buffer;
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {}

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {}

    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        return 0;
    }

    bool UnmapIndexBuffer(HIndexBuffer buffer)
    {
        return true;
    }

    bool IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        return true;
    }

    uint32_t GetMaxElementsIndices(HContext context)
    {
        return 65536;
    }


    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        return new VertexDeclaration;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        return new VertexDeclaration;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {}

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {}

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {}

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {}


    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {}

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {}

    HVertexProgram NewVertexProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return (HVertexProgram) new uint32_t;
    }

    HFragmentProgram NewFragmentProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return (HFragmentProgram) new uint32_t;
    }

    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        return (HProgram) new uint32_t;
    }

    void DeleteProgram(HContext context, HProgram program)
    {
        delete (uint32_t*) program;
    }

    bool ReloadVertexProgram(HVertexProgram prog, ShaderDesc::Shader* ddf)
    {
        return true;
    }

    bool ReloadFragmentProgram(HFragmentProgram prog, ShaderDesc::Shader* ddf)
    {
        return true;
    }

    void DeleteVertexProgram(HVertexProgram prog)
    {
        delete (uint32_t*) prog;
    }

    void DeleteFragmentProgram(HFragmentProgram prog)
    {
        delete (uint32_t*) prog;
    }

    ShaderDesc::Language GetShaderProgramLanguage(HContext context)
    {
        return ShaderDesc::LANGUAGE_SPIRV;
    }


    void EnableProgram(HContext context, HProgram program)
    {}

    void DisableProgram(HContext context)
    {}

    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        return true;
    }


    uint32_t GetUniformCount(HProgram prog)
    {
        return 0;
    }

    void GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type)
    {}

    int32_t GetUniformLocation(HProgram prog, const char* name)
    {
        return 0;
    }

    void SetConstantV4(HContext context, const Vectormath::Aos::Vector4* data, int base_register)
    {}

    void SetConstantM4(HContext context, const Vectormath::Aos::Vector4* data, int base_register)
    {}

    void SetSampler(HContext context, int32_t location, int32_t unit)
    {}


    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {}

    void EnableState(HContext context, State state)
    {}

    void DisableState(HContext context, State state)
    {}

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {}

    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {}

    void SetDepthMask(HContext context, bool mask)
    {}

    void SetDepthFunc(HContext context, CompareFunc func)
    {}

    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {}

    void SetStencilMask(HContext context, uint32_t mask)
    {}

    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {}

    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {}

    void SetCullFace(HContext context, FaceType face_type)
    {}

    void SetPolygonOffset(HContext context, float factor, float units)
    {}


    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        return new RenderTarget(GetNextRenderTargetId());
    }

    void DeleteRenderTarget(HRenderTarget render_target)
    {
        delete render_target;
    }

    void SetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {}

    HTexture GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        return 0;
    }

    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {}

    void SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {}

    bool IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return true;
    }

    HTexture NewTexture(HContext context, const TextureCreationParams& params)
    {
        return new Texture;
    }

    void DeleteTexture(HTexture t)
    {
        delete t;
    }

    void SetTexture(HTexture texture, const TextureParams& params)
    {}

    void SetTextureAsync(HTexture texture, const TextureParams& paramsa)
    {}

    uint8_t* GetTextureData(HTexture texture)
    {
        return 0;
    }

    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap)
    {}

    uint32_t GetTextureResourceSize(HTexture texture)
    {
        return 0;
    }

    uint16_t GetTextureWidth(HTexture texture)
    {
        return 0;
    }

    uint16_t GetTextureHeight(HTexture texture)
    {
        return 0;
    }

    uint16_t GetOriginalTextureWidth(HTexture texture)
    {
        return 0;
    }

    uint16_t GetOriginalTextureHeight(HTexture texture)
    {
        return 0;
    }

    void EnableTexture(HContext context, uint32_t unit, HTexture texture)
    {}

    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {}

    uint32_t GetMaxTextureSize(HContext context)
    {
        return context->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D;;
    }

    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        return 0;
    }

    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {}

    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }
}
