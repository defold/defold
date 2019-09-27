#include "graphics_vulkan_private.h"

namespace dmGraphics
{
    uint32_t GetPhysicalDeviceCount(VkInstance vkInstance)
    {
        uint32_t vk_device_count = 0;
        vkEnumeratePhysicalDevices(vkInstance, &vk_device_count, 0);
        return vk_device_count;
    }

    void GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize)
    {
        assert(deviceListOut);
        PhysicalDevice* device_list = *deviceListOut;
        uint32_t vk_device_count = deviceListSize;
        VkPhysicalDevice* vk_device_list = new VkPhysicalDevice[vk_device_count];
        vkEnumeratePhysicalDevices(vkInstance, &vk_device_count, vk_device_list);
        assert(vk_device_count == deviceListSize);

        const uint8_t vk_max_extension_count = 128;
        VkExtensionProperties vk_device_extensions[vk_max_extension_count];

        for (uint32_t i=0; i < vk_device_count; ++i)
        {
            VkPhysicalDevice vk_device = vk_device_list[i];
            uint32_t vk_device_extension_count, vk_queue_family_count;

            vkGetPhysicalDeviceProperties(vk_device, &device_list[i].m_Properties);
            vkGetPhysicalDeviceFeatures(vk_device, &device_list[i].m_Features);
            vkGetPhysicalDeviceMemoryProperties(vk_device, &device_list[i].m_MemoryProperties);

            vkGetPhysicalDeviceQueueFamilyProperties(vk_device, &vk_queue_family_count, 0);
            device_list[i].m_QueueFamilyProperties = new VkQueueFamilyProperties[vk_queue_family_count];
            vkGetPhysicalDeviceQueueFamilyProperties(vk_device, &vk_queue_family_count, device_list[i].m_QueueFamilyProperties);

            vkEnumerateDeviceExtensionProperties(vk_device, 0, &vk_device_extension_count, 0);

            // Note: If this is triggered, we need a higher limit for the max extension count
            //       or simply allocate the exact memory dynamically to hold the extensions data..
            assert(vk_device_extension_count < vk_max_extension_count);

            if (vkEnumerateDeviceExtensionProperties(vk_device, 0, &vk_device_extension_count, vk_device_extensions) == VK_SUCCESS)
            {
                device_list[i].m_DeviceExtensions = new VkExtensionProperties[vk_device_extension_count];

                for (uint8_t j = 0; j < vk_device_extension_count; ++j)
                {
                    device_list[i].m_DeviceExtensions[j] = vk_device_extensions[j];
                }
            }

            device_list[i].m_Device               = vk_device;
            device_list[i].m_QueueFamilyCount     = (uint16_t) vk_queue_family_count;
            device_list[i].m_DeviceExtensionCount = (uint16_t) vk_device_extension_count;
        }

        delete[] vk_device_list;
    }

    bool GetMemoryTypeIndex(VkPhysicalDevice vk_physical_device, uint32_t typeFilter, VkMemoryPropertyFlags vk_property_flags, uint32_t* memoryIndexOut)
    {
        VkPhysicalDeviceMemoryProperties vk_memory_props;
        vkGetPhysicalDeviceMemoryProperties(vk_physical_device, &vk_memory_props);

        for (uint32_t i = 0; i < vk_memory_props.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (vk_memory_props.memoryTypes[i].propertyFlags & vk_property_flags) == vk_property_flags)
            {
                *memoryIndexOut = i;
                return true;
            }
        }

        return false;
    }

    // Tiling is related to how an image is laid out in memory, either in 'optimal' or 'linear' fashion.
    // Optimal is always sought after since it should be the most performant (depending on hardware),
    // but is not always supported.
    const VkFormat GetSupportedTilingFormat(VkPhysicalDevice vk_physical_device, VkFormat* vk_format_candidates,
        uint32_t vk_num_format_candidates, VkImageTiling vk_tiling_type, VkFormatFeatureFlags vk_format_flags)
    {
        for (uint32_t i=0; i < vk_num_format_candidates; i++)
        {
            VkFormatProperties format_properties;
            VkFormat formatCandidate = vk_format_candidates[i];

            vkGetPhysicalDeviceFormatProperties(vk_physical_device, formatCandidate, &format_properties);

            if ((vk_tiling_type == VK_IMAGE_TILING_LINEAR && (format_properties.linearTilingFeatures & vk_format_flags) == vk_format_flags) ||
                (vk_tiling_type == VK_IMAGE_TILING_OPTIMAL && (format_properties.optimalTilingFeatures & vk_format_flags) == vk_format_flags))
            {
                return formatCandidate;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }

    VkResult TransitionImageLayout(VkDevice vk_device, VkCommandPool vk_command_pool, VkQueue vk_graphics_queue, VkImage vk_image,
        VkImageAspectFlags vk_image_aspect, VkImageLayout vk_from_layout, VkImageLayout vk_to_layout)
    {
        // Create a one-time-execute command buffer that will only be used for the transition
        VkCommandBuffer vk_command_buffer;
        CreateCommandBuffers(vk_device, vk_command_pool, 1, &vk_command_buffer);

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

    VkResult CreateFramebuffer(VkDevice vk_device, VkRenderPass vk_render_pass, uint32_t width, uint32_t height, VkImageView* vk_attachments, uint8_t attachmentCount, VkFramebuffer* vk_framebuffer_out)
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

    VkResult CreateCommandBuffers(VkDevice vk_device, VkCommandPool vk_command_pool, uint32_t numBuffersToCreate, VkCommandBuffer* vk_command_buffers_out)
    {
        VkCommandBufferAllocateInfo vk_buffers_allocate_info;
        memset(&vk_buffers_allocate_info, 0, sizeof(vk_buffers_allocate_info));

        vk_buffers_allocate_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        vk_buffers_allocate_info.commandPool        = vk_command_pool;
        vk_buffers_allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        vk_buffers_allocate_info.commandBufferCount = numBuffersToCreate;

        return vkAllocateCommandBuffers(vk_device, &vk_buffers_allocate_info, vk_command_buffers_out);
    }

    VkResult CreateTexture2D(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        uint32_t imageWidth, uint32_t imageHeight, uint16_t imageMips,
        VkFormat vk_format, VkImageTiling vk_tiling, VkImageUsageFlags vk_usage,
        VkMemoryPropertyFlags vk_memory_flags, VkImageAspectFlags vk_aspect, Texture* textureOut)
    {
        assert(textureOut);
        assert(textureOut->m_ImageView == VK_NULL_HANDLE);
        assert(textureOut->m_DeviceMemory.m_Memory == VK_NULL_HANDLE && textureOut->m_DeviceMemory.m_MemorySize == 0);

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

        VkResult res = vkCreateImage(vk_device, &vk_image_create_info, 0, &textureOut->m_Image);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        // Allocate GPU memory to hold texture
        VkMemoryRequirements vk_memory_req;
        vkGetImageMemoryRequirements(vk_device, textureOut->m_Image, &vk_memory_req);

        VkMemoryAllocateInfo vk_memory_alloc_info;
        memset(&vk_memory_alloc_info, 0, sizeof(vk_memory_alloc_info));

        vk_memory_alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vk_memory_alloc_info.allocationSize  = vk_memory_req.size;
        vk_memory_alloc_info.memoryTypeIndex = 0;

        uint32_t memory_type_index = 0;
        if (!GetMemoryTypeIndex(vk_physical_device, vk_memory_req.memoryTypeBits, vk_memory_flags, &memory_type_index))
        {
            res = VK_ERROR_INITIALIZATION_FAILED;
            goto bail;
        }

        vk_memory_alloc_info.memoryTypeIndex = memory_type_index;

        res = vkAllocateMemory(vk_device, &vk_memory_alloc_info, 0, &textureOut->m_DeviceMemory.m_Memory);
        if (res != VK_SUCCESS)
        {
            goto bail;
        }

        res = vkBindImageMemory(vk_device, textureOut->m_Image, textureOut->m_DeviceMemory.m_Memory, 0);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        textureOut->m_DeviceMemory.m_MemorySize = vk_memory_req.size;

        VkImageViewCreateInfo vk_view_create_info;
        memset(&vk_view_create_info, 0, sizeof(vk_view_create_info));

        vk_view_create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vk_view_create_info.image                           = textureOut->m_Image;
        vk_view_create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        vk_view_create_info.format                          = vk_format;
        vk_view_create_info.subresourceRange.aspectMask     = vk_aspect;
        vk_view_create_info.subresourceRange.baseMipLevel   = 0;
        vk_view_create_info.subresourceRange.levelCount     = 1;
        vk_view_create_info.subresourceRange.baseArrayLayer = 0;
        vk_view_create_info.subresourceRange.layerCount     = 1;

        textureOut->m_Format = vk_format;

        return vkCreateImageView(vk_device, &vk_view_create_info, 0, &textureOut->m_ImageView);
bail:
        ResetTexture(vk_device, textureOut);
        return res;
    }

    VkResult CreateRenderPass(VkDevice vk_device, RenderPassAttachment* colorAttachments, uint8_t numColorAttachments, RenderPassAttachment* depthStencilAttachment, VkRenderPass* renderPassOut)
    {
        assert(*renderPassOut == VK_NULL_HANDLE);

        uint8_t num_attachments  = numColorAttachments + (depthStencilAttachment ? 1 : 0);
        VkAttachmentDescription* vk_attachment_desc    = new VkAttachmentDescription[num_attachments];
        VkAttachmentReference* vk_attachment_color_ref = new VkAttachmentReference[numColorAttachments];
        VkAttachmentReference vk_attachment_depth_ref  = {};

        memset(vk_attachment_desc, 0, sizeof(VkAttachmentDescription) * num_attachments);
        memset(vk_attachment_color_ref, 0, sizeof(VkAttachmentReference) * numColorAttachments);

        for (uint16_t i=0; i < numColorAttachments; i++)
        {
            VkAttachmentDescription& attachment_color = vk_attachment_desc[i];

            attachment_color.format         = colorAttachments[i].m_Format;
            attachment_color.samples        = VK_SAMPLE_COUNT_1_BIT;
            attachment_color.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
            attachment_depth.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_depth.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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

        VkResult res = vkCreateRenderPass(vk_device, &render_pass_create_info, 0, renderPassOut);

        delete[] vk_attachment_desc;
        delete[] vk_attachment_color_ref;

        return res;
    }

    void ResetPhysicalDevice(PhysicalDevice* device)
    {
        assert(device);

        if (device->m_QueueFamilyProperties)
        {
            delete[] device->m_QueueFamilyProperties;
        }

        if (device->m_DeviceExtensions)
        {
            delete[] device->m_DeviceExtensions;
        }

        memset((void*)device, 0, sizeof(*device));
    }

    void ResetRenderTarget(LogicalDevice* logicalDevice, RenderTarget* renderTarget)
    {
        assert(logicalDevice);
        assert(renderTarget);
        if (renderTarget->m_Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(logicalDevice->m_Device, renderTarget->m_Framebuffer, 0);
            renderTarget->m_Framebuffer = VK_NULL_HANDLE;
        }

        if (renderTarget->m_RenderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(logicalDevice->m_Device, renderTarget->m_RenderPass, 0);
            renderTarget->m_RenderPass = VK_NULL_HANDLE;
        }
    }

    void ResetTexture(VkDevice vk_device, Texture* texture)
    {
        if (texture->m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(vk_device, texture->m_ImageView, 0);
            texture->m_ImageView = VK_NULL_HANDLE;
        }

        if (texture->m_Image != VK_NULL_HANDLE)
        {
            vkDestroyImage(vk_device, texture->m_Image, 0);
            texture->m_Image = VK_NULL_HANDLE;
        }

        if (texture->m_DeviceMemory.m_Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(vk_device, texture->m_DeviceMemory.m_Memory, 0);
            texture->m_DeviceMemory.m_Memory     = VK_NULL_HANDLE;
            texture->m_DeviceMemory.m_MemorySize = 0;
        }
    }

    void ResetLogicalDevice(LogicalDevice* device)
    {
        vkDestroyCommandPool(device->m_Device, device->m_CommandPool, 0);
        vkDestroyDevice(device->m_Device, 0);
        memset(device, 0, sizeof(*device));
    }

    #define QUEUE_FAMILY_INVALID 0xffff

    // All GPU operations are pushed to various queues. The physical device can have multiple
    // queues with different properties supported, so we need to find a combination of queues
    // that will work for our needs. Note that the present queue might not be the same queue as the
    // graphics queue. The graphics queue support is needed to do graphics operations at all.
    QueueFamily GetQueueFamily(PhysicalDevice* device, const VkSurfaceKHR surface)
    {
        assert(device);

        QueueFamily qf;

        if (device->m_QueueFamilyCount == 0)
        {
            return qf;
        }

        VkBool32* vk_present_queues = new VkBool32[device->m_QueueFamilyCount];

        // Try to find a queue that has both graphics and present capabilities,
        // and if we can't find one that has both, we take the first of each queue
        // that we can find.
        for (uint32_t i = 0; i < device->m_QueueFamilyCount; ++i)
        {
            QueueFamily candidate;
            vkGetPhysicalDeviceSurfaceSupportKHR(device->m_Device, i, surface, vk_present_queues+i);
            VkQueueFamilyProperties vk_properties = device->m_QueueFamilyProperties[i];

            if (vk_properties.queueCount > 0 && vk_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                candidate.m_GraphicsQueueIx = i;

                if (qf.m_GraphicsQueueIx == QUEUE_FAMILY_INVALID)
                {
                    qf.m_GraphicsQueueIx = i;
                }
            }

            if (vk_properties.queueCount > 0 && vk_present_queues[i])
            {
                candidate.m_PresentQueueIx = i;

                if (qf.m_PresentQueueIx == QUEUE_FAMILY_INVALID)
                {
                    qf.m_PresentQueueIx = i;
                }
            }

            if (candidate.IsValid() && candidate.m_GraphicsQueueIx == candidate.m_PresentQueueIx)
            {
                qf = candidate;
                break;
            }
        }

        delete[] vk_present_queues;
        return qf;
    }

    VkResult CreateLogicalDevice(PhysicalDevice* device, const VkSurfaceKHR surface, const QueueFamily queueFamily,
        const char** deviceExtensions, const uint8_t deviceExtensionCount,
        const char** validationLayers, const uint8_t validationLayerCount,
        LogicalDevice* logicalDeviceOut)
    {
        assert(device);

        // NOTE: Different queues can have different priority from [0..1], but
        //       we only have a single queue right now so set to 1.0f
        float queue_priority        = 1.0f;
        int32_t queue_family_set[2] = { queueFamily.m_PresentQueueIx, QUEUE_FAMILY_INVALID };
        int32_t queue_family_c      = 0;

        VkDeviceQueueCreateInfo vk_device_queue_create_info[2];
        memset(vk_device_queue_create_info, 0, sizeof(vk_device_queue_create_info));

        if (queueFamily.m_PresentQueueIx != queueFamily.m_GraphicsQueueIx)
        {
            queue_family_set[1] = queueFamily.m_GraphicsQueueIx;
        }

        while(queue_family_set[queue_family_c] != QUEUE_FAMILY_INVALID)
        {
            int queue_family_index = queue_family_set[queue_family_c];
            VkDeviceQueueCreateInfo& vk_queue_create_info = vk_device_queue_create_info[queue_family_c];

            vk_queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            vk_queue_create_info.queueFamilyIndex = queue_family_index;
            vk_queue_create_info.queueCount       = 1;
            vk_queue_create_info.pQueuePriorities = &queue_priority;

            queue_family_c++;
        }

        VkDeviceCreateInfo vk_device_create_info;
        memset(&vk_device_create_info, 0, sizeof(vk_device_create_info));

        vk_device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        vk_device_create_info.pQueueCreateInfos       = vk_device_queue_create_info;
        vk_device_create_info.queueCreateInfoCount    = queue_family_c;
        vk_device_create_info.pEnabledFeatures        = 0;
        vk_device_create_info.enabledExtensionCount   = deviceExtensionCount;
        vk_device_create_info.ppEnabledExtensionNames = deviceExtensions;
        vk_device_create_info.enabledLayerCount       = validationLayerCount;
        vk_device_create_info.ppEnabledLayerNames     = validationLayers;

        VkResult res = vkCreateDevice(device->m_Device, &vk_device_create_info, 0, &logicalDeviceOut->m_Device);

        if (res == VK_SUCCESS)
        {
            vkGetDeviceQueue(logicalDeviceOut->m_Device, queueFamily.m_GraphicsQueueIx, 0, &logicalDeviceOut->m_GraphicsQueue);
            vkGetDeviceQueue(logicalDeviceOut->m_Device, queueFamily.m_PresentQueueIx, 0, &logicalDeviceOut->m_PresentQueue);

            // Create command pool
            VkCommandPoolCreateInfo vk_create_pool_info;
            memset(&vk_create_pool_info, 0, sizeof(vk_create_pool_info));
            vk_create_pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            vk_create_pool_info.queueFamilyIndex = (uint32_t) queueFamily.m_GraphicsQueueIx;
            vk_create_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            res = vkCreateCommandPool(logicalDeviceOut->m_Device, &vk_create_pool_info, 0, &logicalDeviceOut->m_CommandPool);
        }

        return res;
    }

    #undef QUEUE_FAMILY_INVALID
}
