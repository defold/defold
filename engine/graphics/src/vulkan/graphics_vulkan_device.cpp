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

#include "graphics_vulkan_defines.h"
#include "graphics_vulkan_private.h"

namespace dmGraphics
{
    void InitializeVulkanTexture(Texture* t)
    {
        t->m_Type                = TEXTURE_TYPE_2D;
        t->m_GraphicsFormat      = TEXTURE_FORMAT_RGBA;
        t->m_DeviceBuffer        = VK_IMAGE_USAGE_SAMPLED_BIT;
        t->m_Format              = VK_FORMAT_UNDEFINED;
        t->m_Width               = 0;
        t->m_Height              = 0;
        t->m_OriginalWidth       = 0;
        t->m_OriginalHeight      = 0;
        t->m_MipMapCount         = 0;
        t->m_TextureSamplerIndex = 0;
        t->m_Destroyed           = 0;
        memset(&t->m_Handle, 0, sizeof(t->m_Handle));
    }

    RenderTarget::RenderTarget(const uint32_t rtId)
        : m_TextureDepthStencil(0)
        , m_RenderPass(VK_NULL_HANDLE)
        , m_Framebuffer(VK_NULL_HANDLE)
        , m_Id(rtId)
        , m_IsBound(0)
    {
        m_Extent.width  = 0;
        m_Extent.height = 0;
        memset(m_TextureColor, 0, sizeof(m_TextureColor));
    }

    Program::Program()
    {
        memset(this, 0, sizeof(*this));
    }

    static uint16_t FillVertexInputAttributeDesc(HVertexDeclaration vertexDeclaration, VkVertexInputAttributeDescription* vk_vertex_input_descs)
    {
        uint16_t num_attributes = 0;
        for (uint16_t i = 0; i < vertexDeclaration->m_StreamCount; ++i)
        {
            if (vertexDeclaration->m_Streams[i].m_Location == 0xffff)
            {
                continue;
            }

            vk_vertex_input_descs[num_attributes].binding  = 0;
            vk_vertex_input_descs[num_attributes].location = vertexDeclaration->m_Streams[i].m_Location;
            vk_vertex_input_descs[num_attributes].format   = vertexDeclaration->m_Streams[i].m_Format;
            vk_vertex_input_descs[num_attributes].offset   = vertexDeclaration->m_Streams[i].m_Offset;

            num_attributes++;
        }

        return num_attributes;
    }

    VkResult DescriptorAllocator::Allocate(VkDevice vk_device, VkDescriptorSetLayout* vk_descriptor_set_layout, uint8_t setCount, VkDescriptorSet** vk_descriptor_set_out)
    {
        assert(m_DescriptorMax >= (m_DescriptorIndex + setCount));
        *vk_descriptor_set_out  = &m_Handle.m_DescriptorSets[m_DescriptorIndex];
        m_DescriptorIndex      += setCount;

        VkDescriptorSetAllocateInfo vk_descriptor_set_alloc;
        vk_descriptor_set_alloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        vk_descriptor_set_alloc.descriptorSetCount = DM_MAX_SET_COUNT;
        vk_descriptor_set_alloc.pSetLayouts        = vk_descriptor_set_layout;
        vk_descriptor_set_alloc.descriptorPool     = m_Handle.m_DescriptorPool;
        vk_descriptor_set_alloc.pNext              = 0;

        return vkAllocateDescriptorSets(vk_device, &vk_descriptor_set_alloc, *vk_descriptor_set_out);
    }

    void DescriptorAllocator::Release(VkDevice vk_device)
    {
        if (m_DescriptorIndex > 0)
        {
            vkFreeDescriptorSets(vk_device, m_Handle.m_DescriptorPool, m_DescriptorIndex, m_Handle.m_DescriptorSets);
            m_DescriptorIndex = 0;
        }
    }

    const VulkanResourceType DescriptorAllocator::GetType()
    {
        return RESOURCE_TYPE_DESCRIPTOR_ALLOCATOR;
    }

    VkResult DeviceBuffer::MapMemory(VkDevice vk_device, uint32_t offset, uint32_t size)
    {
        return vkMapMemory(vk_device, m_Handle.m_Memory, offset, size > 0 ? size : m_MemorySize, 0, &m_MappedDataPtr);
    }

    void DeviceBuffer::UnmapMemory(VkDevice vk_device)
    {
        assert(m_MappedDataPtr);
        vkUnmapMemory(vk_device, m_Handle.m_Memory);
    }

    const VulkanResourceType DeviceBuffer::GetType()
    {
        return RESOURCE_TYPE_DEVICE_BUFFER;
    }

    const VulkanResourceType Texture::GetType()
    {
        return RESOURCE_TYPE_TEXTURE;
    }

    const VulkanResourceType Program::GetType()
    {
        return RESOURCE_TYPE_PROGRAM;
    }

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

            VkExtensionProperties* vk_device_extensions = new VkExtensionProperties[vk_device_extension_count];

            if (vkEnumerateDeviceExtensionProperties(vk_device, 0, &vk_device_extension_count, vk_device_extensions) == VK_SUCCESS)
            {
                device_list[i].m_DeviceExtensions = vk_device_extensions;
            }
            else if (vk_device_extensions != 0x0)
            {
                delete[] vk_device_extensions;
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
    const VkFormat GetSupportedTilingFormat(VkPhysicalDevice vk_physical_device, const VkFormat* vk_format_candidates,
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

    void GetFormatProperties(VkPhysicalDevice vk_physical_device, VkFormat vk_format, VkFormatProperties* properties)
    {
        vkGetPhysicalDeviceFormatProperties(vk_physical_device, vk_format, properties);
    }

    VkSampleCountFlagBits GetClosestSampleCountFlag(PhysicalDevice* physicalDevice, uint32_t bufferFlagBits, uint8_t sampleCount)
    {
        VkSampleCountFlags vk_sample_count = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

        if (bufferFlagBits & BUFFER_TYPE_COLOR0_BIT)
        {
            vk_sample_count = physicalDevice->m_Properties.limits.framebufferColorSampleCounts;
        }

        if (bufferFlagBits & BUFFER_TYPE_DEPTH_BIT)
        {
            vk_sample_count = dmMath::Min<VkSampleCountFlags>(vk_sample_count, physicalDevice->m_Properties.limits.framebufferColorSampleCounts);
        }

        if (bufferFlagBits & BUFFER_TYPE_STENCIL_BIT)
        {
            vk_sample_count = dmMath::Min<VkSampleCountFlags>(vk_sample_count, physicalDevice->m_Properties.limits.framebufferStencilSampleCounts);
        }

        const uint8_t sample_count_index_requested = (uint8_t) sampleCount == 0 ? 0 : (uint8_t) log2f((float) sampleCount);
        const uint8_t sample_count_index_max       = (uint8_t) log2f((float) vk_sample_count);
        const VkSampleCountFlagBits vk_count_bits[] = {
            VK_SAMPLE_COUNT_1_BIT,
            VK_SAMPLE_COUNT_2_BIT,
            VK_SAMPLE_COUNT_4_BIT,
            VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_16_BIT,
            VK_SAMPLE_COUNT_32_BIT,
            VK_SAMPLE_COUNT_64_BIT,
        };

        return vk_count_bits[dmMath::Min<uint8_t>(sample_count_index_requested, sample_count_index_max)];
    }

    VkResult TransitionImageLayout(VkDevice vk_device, VkCommandPool vk_command_pool, VkQueue vk_graphics_queue, VkImage vk_image,
        VkImageAspectFlags vk_image_aspect, VkImageLayout vk_from_layout, VkImageLayout vk_to_layout,
        uint32_t baseMipLevel, uint32_t layer_count)
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
        vk_memory_barrier.subresourceRange.baseMipLevel   = baseMipLevel;
        vk_memory_barrier.subresourceRange.levelCount     = 1;
        vk_memory_barrier.subresourceRange.baseArrayLayer = 0;
        vk_memory_barrier.subresourceRange.layerCount     = layer_count;

        VkPipelineStageFlags vk_source_stage      = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags vk_destination_stage = VK_IMAGE_LAYOUT_UNDEFINED;

        // These stage changes are explicit in our case:
        //   1) undefined -> shader read. This transition is used when uploading texture without a stage buffer.
        //   2) undefined -> transfer. This transition is used for staging buffers when uploading texture data
        //   3) transfer  -> shader read. This transition is used when the staging transfer is complete.
        //   4) undefined -> depth stencil. This transition is used when creating a depth buffer attachment.
        //   5) undefined -> color attachment. This transition is used when creating a color buffer attachment.
        if (vk_from_layout == VK_IMAGE_LAYOUT_UNDEFINED && vk_to_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            vk_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            vk_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vk_source_stage      = VK_PIPELINE_STAGE_HOST_BIT;
            vk_destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (vk_from_layout == VK_IMAGE_LAYOUT_UNDEFINED && vk_to_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
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
        else if (vk_from_layout == VK_IMAGE_LAYOUT_UNDEFINED && vk_to_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            vk_memory_barrier.srcAccessMask = 0;
            vk_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vk_source_stage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            vk_destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else
        {
            assert(0);
        }

        vkCmdPipelineBarrier(
            vk_command_buffer,
            vk_source_stage,
            vk_destination_stage,
            0, 0, 0, 0, 0, 1,
            &vk_memory_barrier);

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

    VkResult WriteToDeviceBuffer(VkDevice vk_device, VkDeviceSize size, VkDeviceSize offset, const void* data, DeviceBuffer* buffer)
    {
        assert(buffer);

        VkResult res = buffer->MapMemory(vk_device, (uint32_t) offset, (uint32_t) size);

        if (res != VK_SUCCESS)
        {
            return res;
        }

        if (data == 0)
        {
            memset(buffer->m_MappedDataPtr, 0, (size_t) size);
        }
        else
        {
            memcpy(buffer->m_MappedDataPtr, data, (size_t) size);
        }

        buffer->UnmapMemory(vk_device);

        return res;
    }

    VkResult CreateDescriptorPool(VkDevice vk_device, VkDescriptorPoolSize* vk_pool_sizes, uint8_t numPoolSizes, uint16_t maxDescriptors, VkDescriptorPool* vk_descriptor_pool_out)
    {
        assert(vk_descriptor_pool_out && *vk_descriptor_pool_out == VK_NULL_HANDLE);
        VkDescriptorPoolCreateInfo vk_pool_create_info;
        memset(&vk_pool_create_info, 0, sizeof(vk_pool_create_info));

        vk_pool_create_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        vk_pool_create_info.poolSizeCount = numPoolSizes;
        vk_pool_create_info.pPoolSizes    = vk_pool_sizes;
        vk_pool_create_info.maxSets       = maxDescriptors;
        vk_pool_create_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        return vkCreateDescriptorPool(vk_device, &vk_pool_create_info, 0, vk_descriptor_pool_out);
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

    VkResult DestroyFrameBuffer(VkDevice vk_device, VkFramebuffer vk_framebuffer)
    {
        if (vk_framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(vk_device, vk_framebuffer, 0);
        }
        return VK_SUCCESS;
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

    VkResult CreateShaderModule(VkDevice vk_device, const void* source, uint32_t sourceSize, ShaderModule* shaderModuleOut)
    {
        assert(shaderModuleOut);

        VkShaderModuleCreateInfo vk_create_info_shader;
        memset(&vk_create_info_shader, 0, sizeof(vk_create_info_shader));

        vk_create_info_shader.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vk_create_info_shader.codeSize = sourceSize;
        vk_create_info_shader.pCode    = (unsigned int*) source;

        HashState64 shader_hash_state;
        dmHashInit64(&shader_hash_state, false);
        dmHashUpdateBuffer64(&shader_hash_state, source, (uint32_t) sourceSize);
        shaderModuleOut->m_Hash = dmHashFinal64(&shader_hash_state);

        return vkCreateShaderModule( vk_device, &vk_create_info_shader, 0, &shaderModuleOut->m_Module);
    }

    VkResult CreateDeviceBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        VkDeviceSize vk_size, VkMemoryPropertyFlags vk_memory_flags, DeviceBuffer* bufferOut)
    {
        assert(vk_size < 0x80000000); // must match max bit count in graphics_vulkan_private.h

        VkBufferCreateInfo vk_buffer_create_info;
        memset(&vk_buffer_create_info, 0, sizeof(vk_buffer_create_info));

        vk_buffer_create_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vk_buffer_create_info.size        = vk_size;
        vk_buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vk_buffer_create_info.usage       = bufferOut->m_Usage;

        VkResult res = vkCreateBuffer(vk_device, &vk_buffer_create_info, 0, &bufferOut->m_Handle.m_Buffer);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        // Allocate GPU memory to hold buffer
        VkMemoryRequirements vk_buffer_memory_req;
        vkGetBufferMemoryRequirements(vk_device, bufferOut->m_Handle.m_Buffer, &vk_buffer_memory_req);

        VkMemoryAllocateInfo vk_memory_alloc_info;
        memset(&vk_memory_alloc_info, 0, sizeof(vk_memory_alloc_info));

        vk_memory_alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vk_memory_alloc_info.allocationSize  = vk_buffer_memory_req.size;
        vk_memory_alloc_info.memoryTypeIndex = 0;

        uint32_t memory_type_index = 0;
        if (!GetMemoryTypeIndex(vk_physical_device, vk_buffer_memory_req.memoryTypeBits, vk_memory_flags, &memory_type_index))
        {
            res = VK_ERROR_INITIALIZATION_FAILED;
            goto bail;
        }

        vk_memory_alloc_info.memoryTypeIndex = memory_type_index;

        res = vkAllocateMemory(vk_device, &vk_memory_alloc_info, 0, &bufferOut->m_Handle.m_Memory);
        if (res != VK_SUCCESS)
        {
            goto bail;
        }

        res = vkBindBufferMemory(vk_device, bufferOut->m_Handle.m_Buffer, bufferOut->m_Handle.m_Memory, 0);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        bufferOut->m_MemorySize = (size_t) vk_buffer_memory_req.size;
        bufferOut->m_Destroyed  = 0;

        return VK_SUCCESS;
bail:
        DestroyDeviceBuffer(vk_device, &bufferOut->m_Handle);
        return res;
    }

    VkResult CreateDescriptorAllocator(VkDevice vk_device, uint32_t descriptor_count, DescriptorAllocator* descriptorAllocator)
    {
        assert(descriptor_count < 0x8000); // Should match the bit count in graphics_vulkan_private.h

        VkDescriptorPoolSize vk_pool_size[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, descriptor_count},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptor_count}
        };

        descriptorAllocator->m_Handle.m_DescriptorPool = VK_NULL_HANDLE;
        descriptorAllocator->m_Handle.m_DescriptorSets = new VkDescriptorSet[descriptor_count];
        descriptorAllocator->m_DescriptorMax           = descriptor_count;
        descriptorAllocator->m_DescriptorIndex         = 0;
        descriptorAllocator->m_Destroyed               = 0;

        return CreateDescriptorPool(vk_device, vk_pool_size, sizeof(vk_pool_size) / sizeof(vk_pool_size[0]), descriptor_count, &descriptorAllocator->m_Handle.m_DescriptorPool);
    }

    VkResult CreateScratchBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        uint32_t bufferSize, bool clearData, DescriptorAllocator* descriptorAllocator, ScratchBuffer* scratchBufferOut)
    {
        scratchBufferOut->m_DeviceBuffer.m_Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        DeviceBuffer& device_buffer = scratchBufferOut->m_DeviceBuffer;

        // Note: While scratch buffer creates and initializes the device buffer for
        //       backing, it does not reset it or tear it down.
        VkResult res = CreateDeviceBuffer(vk_physical_device, vk_device, bufferSize,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &scratchBufferOut->m_DeviceBuffer);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        if (clearData)
        {
            res = device_buffer.MapMemory(vk_device);

            if (res != VK_SUCCESS)
            {
                DestroyDeviceBuffer(vk_device, &device_buffer.m_Handle);
                return res;
            }

            // Clear buffer data
            memset(device_buffer.m_MappedDataPtr, 0, bufferSize);
            device_buffer.UnmapMemory(vk_device);
        }

        scratchBufferOut->m_DescriptorAllocator = descriptorAllocator;

        return VK_SUCCESS;
    }

    VkResult CreateTexture2D(
        VkPhysicalDevice      vk_physical_device,
        VkDevice              vk_device,
        uint32_t              imageWidth,
        uint32_t              imageHeight,
        uint16_t              imageMips,
        VkSampleCountFlagBits vk_sample_count,
        VkFormat              vk_format,
        VkImageTiling         vk_tiling,
        VkImageUsageFlags     vk_usage,
        VkMemoryPropertyFlags vk_memory_flags,
        VkImageAspectFlags    vk_aspect,
        VkImageLayout         vk_initial_layout,
        Texture*              textureOut)
    {
        DeviceBuffer& device_buffer = textureOut->m_DeviceBuffer;
        TextureType tex_type = textureOut->m_Type;

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
        vk_image_create_info.initialLayout = vk_initial_layout;
        vk_image_create_info.usage         = vk_usage;
        vk_image_create_info.samples       = vk_sample_count;
        vk_image_create_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        vk_image_create_info.flags         = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        if (tex_type == TEXTURE_TYPE_CUBE_MAP)
        {
            vk_image_create_info.arrayLayers = 6;
            vk_image_create_info.flags      |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        VkResult res = vkCreateImage(vk_device, &vk_image_create_info, 0, &textureOut->m_Handle.m_Image);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        // Handle cubemap vs 2D
        VkImageViewType vk_view_type = VK_IMAGE_VIEW_TYPE_2D;
        uint8_t vk_layer_count       = 1;
        if (tex_type == TEXTURE_TYPE_CUBE_MAP)
        {
            vk_view_type   = VK_IMAGE_VIEW_TYPE_CUBE;
            vk_layer_count = 6;
        }

        // Allocate GPU memory to hold texture
        VkMemoryRequirements vk_memory_req;
        vkGetImageMemoryRequirements(vk_device, textureOut->m_Handle.m_Image, &vk_memory_req);

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

        res = vkAllocateMemory(vk_device, &vk_memory_alloc_info, 0, &device_buffer.m_Handle.m_Memory);
        if (res != VK_SUCCESS)
        {
            goto bail;
        }

        res = vkBindImageMemory(vk_device, textureOut->m_Handle.m_Image, device_buffer.m_Handle.m_Memory, 0);
        if (res != VK_SUCCESS)
        {
            goto bail;
        }

        device_buffer.m_MemorySize = vk_memory_req.size;

        VkImageViewCreateInfo vk_view_create_info;
        memset(&vk_view_create_info, 0, sizeof(vk_view_create_info));

        vk_view_create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vk_view_create_info.image                           = textureOut->m_Handle.m_Image;
        vk_view_create_info.viewType                        = vk_view_type;
        vk_view_create_info.format                          = vk_format;
        vk_view_create_info.subresourceRange.aspectMask     = vk_aspect;
        vk_view_create_info.subresourceRange.baseMipLevel   = 0;
        vk_view_create_info.subresourceRange.levelCount     = imageMips;
        vk_view_create_info.subresourceRange.baseArrayLayer = 0;
        vk_view_create_info.subresourceRange.layerCount     = vk_layer_count;

        textureOut->m_Format                   = vk_format;
        textureOut->m_Destroyed                = 0;
        textureOut->m_DeviceBuffer.m_Destroyed = 0;

        if (imageMips == 0)
        {
            textureOut->m_Width  = imageWidth;
            textureOut->m_Height = imageHeight;
        }

        return vkCreateImageView(vk_device, &vk_view_create_info, 0, &textureOut->m_Handle.m_ImageView);
bail:
        DestroyTexture(vk_device, &textureOut->m_Handle);
        return res;
    }

    VkResult CreateTextureSampler(VkDevice vk_device, VkFilter vk_min_filter, VkFilter vk_mag_filter, VkSamplerMipmapMode vk_mipmap_mode,
        VkSamplerAddressMode vk_wrap_u, VkSamplerAddressMode vk_wrap_v, float minLod, float maxLod, float max_anisotropy, VkSampler* vk_sampler_out)
    {
        VkSamplerCreateInfo vk_sampler_create_info;
        memset(&vk_sampler_create_info, 0, sizeof(vk_sampler_create_info));

        vk_sampler_create_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        vk_sampler_create_info.magFilter               = vk_mag_filter;
        vk_sampler_create_info.minFilter               = vk_min_filter;
        vk_sampler_create_info.addressModeU            = vk_wrap_u;
        vk_sampler_create_info.addressModeV            = vk_wrap_v;
        vk_sampler_create_info.addressModeW            = vk_wrap_u;
        vk_sampler_create_info.anisotropyEnable        = max_anisotropy > 1.0f;
        vk_sampler_create_info.maxAnisotropy           = max_anisotropy;
        vk_sampler_create_info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        vk_sampler_create_info.unnormalizedCoordinates = VK_FALSE;
        vk_sampler_create_info.compareEnable           = VK_FALSE;
        vk_sampler_create_info.compareOp               = VK_COMPARE_OP_ALWAYS;
        vk_sampler_create_info.mipmapMode              = vk_mipmap_mode;
        vk_sampler_create_info.mipLodBias              = 0.0f;
        vk_sampler_create_info.minLod                  = minLod;
        vk_sampler_create_info.maxLod                  = maxLod;

        return vkCreateSampler(vk_device, &vk_sampler_create_info, 0, vk_sampler_out);
    }

    VkResult CreateRenderPass(VkDevice vk_device, VkSampleCountFlagBits vk_sample_flags,
        RenderPassAttachment* colorAttachments, uint8_t numColorAttachments,
        RenderPassAttachment* depthStencilAttachment,
        RenderPassAttachment* resolveAttachment,
        VkRenderPass* renderPassOut)
    {
        assert(*renderPassOut == VK_NULL_HANDLE);

        const uint8_t num_depth_attachments = (depthStencilAttachment ? 1 : 0);
        const uint8_t num_attachments       = numColorAttachments + num_depth_attachments + (resolveAttachment ? 1 : 0);
        VkAttachmentDescription* vk_attachment_desc     = new VkAttachmentDescription[num_attachments];
        VkAttachmentReference* vk_attachment_color_ref  = new VkAttachmentReference[numColorAttachments];
        VkAttachmentReference vk_attachment_depth_ref   = {};
        VkAttachmentReference vk_attachment_resolve_ref = {};

        memset(vk_attachment_desc, 0, sizeof(VkAttachmentDescription) * num_attachments);
        memset(vk_attachment_color_ref, 0, sizeof(VkAttachmentReference) * numColorAttachments);

        for (uint16_t i=0; i < numColorAttachments; i++)
        {
            VkAttachmentDescription& attachment_color = vk_attachment_desc[i];

            attachment_color.format         = colorAttachments[i].m_Format;
            attachment_color.samples        = vk_sample_flags;
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
            attachment_depth.samples        = vk_sample_flags;
            attachment_depth.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_depth.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment_depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment_depth.finalLayout    = depthStencilAttachment->m_ImageLayout;

            vk_attachment_depth_ref.attachment = numColorAttachments;
            vk_attachment_depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        if (resolveAttachment)
        {
            const uint8_t resolve_index = numColorAttachments + num_depth_attachments;
            VkAttachmentDescription& attachment_resolve = vk_attachment_desc[resolve_index];
            attachment_resolve.format         = resolveAttachment->m_Format;
            attachment_resolve.samples        = VK_SAMPLE_COUNT_1_BIT;
            attachment_resolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_resolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_resolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment_resolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment_resolve.finalLayout    = resolveAttachment->m_ImageLayout;

            vk_attachment_resolve_ref.attachment = resolve_index;
            vk_attachment_resolve_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
        vk_sub_pass_description.pResolveAttachments     = resolveAttachment ? &vk_attachment_resolve_ref : 0;

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

    void DestroyRenderPass(VkDevice vk_device, VkRenderPass render_pass)
    {
        if (render_pass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(vk_device, render_pass, 0);
        }
    }

    // These lookup values should match the ones in graphics_vulkan_constants.cpp
    static const VkPrimitiveTopology g_vk_primitive_types[] = {
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
    };

    static const VkCullModeFlagBits g_vk_cull_modes[] = {
        VK_CULL_MODE_FRONT_BIT,
        VK_CULL_MODE_BACK_BIT,
        VK_CULL_MODE_FRONT_AND_BACK
    };

    static const VkBlendFactor g_vk_blend_factors[] = {
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_SRC_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
        VK_BLEND_FACTOR_DST_COLOR,
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
        VK_BLEND_FACTOR_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_FACTOR_DST_ALPHA,
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
        VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
    };

    static const VkStencilOp g_vk_stencil_ops[] = {
        VK_STENCIL_OP_KEEP,
        VK_STENCIL_OP_ZERO,
        VK_STENCIL_OP_REPLACE,
        VK_STENCIL_OP_INCREMENT_AND_CLAMP,
        VK_STENCIL_OP_INCREMENT_AND_WRAP,
        VK_STENCIL_OP_DECREMENT_AND_CLAMP,
        VK_STENCIL_OP_DECREMENT_AND_WRAP,
        VK_STENCIL_OP_INVERT
    };

    static const VkCompareOp g_vk_compare_funcs[] = {
        VK_COMPARE_OP_NEVER,
        VK_COMPARE_OP_LESS,
        VK_COMPARE_OP_LESS_OR_EQUAL,
        VK_COMPARE_OP_GREATER,
        VK_COMPARE_OP_GREATER_OR_EQUAL,
        VK_COMPARE_OP_EQUAL,
        VK_COMPARE_OP_NOT_EQUAL,
        VK_COMPARE_OP_ALWAYS
    };

    VkResult CreatePipeline(VkDevice vk_device, VkRect2D vk_scissor, VkSampleCountFlagBits vk_sample_count,
        PipelineState pipelineState, Program* program, DeviceBuffer* vertexBuffer,
        HVertexDeclaration vertexDeclaration, RenderTarget* render_target, Pipeline* pipelineOut)
    {
        assert(pipelineOut && *pipelineOut == VK_NULL_HANDLE);

        VkVertexInputAttributeDescription vk_vertex_input_descs[DM_MAX_VERTEX_STREAM_COUNT];
        uint16_t active_attributes = FillVertexInputAttributeDesc(vertexDeclaration, vk_vertex_input_descs);
        assert(active_attributes != 0);

        VkVertexInputBindingDescription vk_vx_input_description;
        memset(&vk_vx_input_description, 0, sizeof(vk_vx_input_description));

        vk_vx_input_description.binding   = 0;
        vk_vx_input_description.stride    = vertexDeclaration->m_Stride;
        vk_vx_input_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkPipelineVertexInputStateCreateInfo vk_vertex_input_info;
        memset(&vk_vertex_input_info, 0, sizeof(vk_vertex_input_info));

        vk_vertex_input_info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vk_vertex_input_info.vertexBindingDescriptionCount   = 1;
        vk_vertex_input_info.pVertexBindingDescriptions      = &vk_vx_input_description;
        vk_vertex_input_info.vertexAttributeDescriptionCount = active_attributes;
        vk_vertex_input_info.pVertexAttributeDescriptions    = vk_vertex_input_descs;

        VkPipelineInputAssemblyStateCreateInfo vk_input_assembly;
        memset(&vk_input_assembly, 0, sizeof(vk_input_assembly));

        VkPrimitiveTopology vk_primitive_type    = g_vk_primitive_types[pipelineState.m_PrimtiveType];

        vk_input_assembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        vk_input_assembly.topology               = vk_primitive_type;
        vk_input_assembly.primitiveRestartEnable = VK_FALSE;

        VkViewport vk_viewport;
        memset(&vk_viewport, 0, sizeof(vk_viewport));

        VkPipelineViewportStateCreateInfo vk_viewport_state;
        memset(&vk_viewport_state, 0, sizeof(vk_viewport_state));

        vk_viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vk_viewport_state.viewportCount = 1;
        vk_viewport_state.pViewports    = &vk_viewport;
        vk_viewport_state.scissorCount  = 1;
        vk_viewport_state.pScissors     = &vk_scissor;

        VkPipelineRasterizationStateCreateInfo vk_rasterizer;
        memset(&vk_rasterizer, 0, sizeof(vk_rasterizer));

        VkCullModeFlagBits vk_cull_mode = VK_CULL_MODE_NONE;

        if (pipelineState.m_CullFaceEnabled)
        {
            vk_cull_mode = g_vk_cull_modes[pipelineState.m_CullFaceType];
        }

        vk_rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        vk_rasterizer.depthClampEnable        = VK_FALSE;
        vk_rasterizer.rasterizerDiscardEnable = VK_FALSE;
        vk_rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
        vk_rasterizer.lineWidth               = 1.0f;
        vk_rasterizer.cullMode                = vk_cull_mode;
        vk_rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        vk_rasterizer.depthBiasEnable         = VK_FALSE;
        vk_rasterizer.depthBiasConstantFactor = 0.0f;
        vk_rasterizer.depthBiasClamp          = 0.0f;
        vk_rasterizer.depthBiasSlopeFactor    = 0.0f;

        VkPipelineMultisampleStateCreateInfo vk_multisampling;
        memset(&vk_multisampling, 0, sizeof(vk_multisampling));

        vk_multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        vk_multisampling.sampleShadingEnable   = VK_FALSE;
        vk_multisampling.rasterizationSamples  = vk_sample_count;
        vk_multisampling.minSampleShading      = 1.0f;
        vk_multisampling.pSampleMask           = 0;
        vk_multisampling.alphaToCoverageEnable = VK_FALSE;
        vk_multisampling.alphaToOneEnable      = VK_FALSE;

        VkPipelineColorBlendAttachmentState vk_color_blend_attachments[MAX_BUFFER_COLOR_ATTACHMENTS];
        memset(&vk_color_blend_attachments, 0, sizeof(vk_color_blend_attachments));

        uint8_t state_write_mask    = pipelineState.m_WriteColorMask;
        uint8_t vk_color_write_mask = 0;
        vk_color_write_mask        |= (state_write_mask & DM_GRAPHICS_STATE_WRITE_R) ? VK_COLOR_COMPONENT_R_BIT : 0;
        vk_color_write_mask        |= (state_write_mask & DM_GRAPHICS_STATE_WRITE_G) ? VK_COLOR_COMPONENT_G_BIT : 0;
        vk_color_write_mask        |= (state_write_mask & DM_GRAPHICS_STATE_WRITE_B) ? VK_COLOR_COMPONENT_B_BIT : 0;
        vk_color_write_mask        |= (state_write_mask & DM_GRAPHICS_STATE_WRITE_A) ? VK_COLOR_COMPONENT_A_BIT : 0;

        for (int i = 0; i < render_target->m_ColorAttachmentCount; ++i)
        {
            VkPipelineColorBlendAttachmentState& blend_attachment = vk_color_blend_attachments[i]; 
            blend_attachment.colorWriteMask      = vk_color_write_mask;
            blend_attachment.blendEnable         = pipelineState.m_BlendEnabled;
            blend_attachment.srcColorBlendFactor = g_vk_blend_factors[pipelineState.m_BlendSrcFactor];
            blend_attachment.dstColorBlendFactor = g_vk_blend_factors[pipelineState.m_BlendDstFactor];
            blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
            blend_attachment.srcAlphaBlendFactor = g_vk_blend_factors[pipelineState.m_BlendSrcFactor];
            blend_attachment.dstAlphaBlendFactor = g_vk_blend_factors[pipelineState.m_BlendDstFactor];
            blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo vk_color_blending;
        memset(&vk_color_blending, 0, sizeof(vk_color_blending));

        vk_color_blending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        vk_color_blending.logicOpEnable     = VK_FALSE;
        vk_color_blending.logicOp           = VK_LOGIC_OP_COPY;
        vk_color_blending.attachmentCount   = render_target->m_ColorAttachmentCount;
        vk_color_blending.pAttachments      = vk_color_blend_attachments;
        vk_color_blending.blendConstants[0] = 0.0f;
        vk_color_blending.blendConstants[1] = 0.0f;
        vk_color_blending.blendConstants[2] = 0.0f;
        vk_color_blending.blendConstants[3] = 0.0f;

        VkStencilOpState vk_stencil_op_state_front;
        memset(&vk_stencil_op_state_front, 0, sizeof(vk_stencil_op_state_front));

        vk_stencil_op_state_front.failOp      = g_vk_stencil_ops[pipelineState.m_StencilFrontOpFail];
        vk_stencil_op_state_front.depthFailOp = g_vk_stencil_ops[pipelineState.m_StencilFrontOpDepthFail];
        vk_stencil_op_state_front.passOp      = g_vk_stencil_ops[pipelineState.m_StencilFrontOpPass];
        vk_stencil_op_state_front.compareOp   = g_vk_compare_funcs[pipelineState.m_StencilFrontTestFunc];
        vk_stencil_op_state_front.compareMask = pipelineState.m_StencilCompareMask;
        vk_stencil_op_state_front.writeMask   = pipelineState.m_StencilWriteMask;
        vk_stencil_op_state_front.reference   = pipelineState.m_StencilReference;

        VkStencilOpState vk_stencil_op_state_back = vk_stencil_op_state_front;
        vk_stencil_op_state_back.failOp           = g_vk_stencil_ops[pipelineState.m_StencilBackOpFail];
        vk_stencil_op_state_back.depthFailOp      = g_vk_stencil_ops[pipelineState.m_StencilBackOpDepthFail];
        vk_stencil_op_state_back.passOp           = g_vk_stencil_ops[pipelineState.m_StencilBackOpPass];
        vk_stencil_op_state_back.compareOp        = g_vk_compare_funcs[pipelineState.m_StencilBackTestFunc];

        VkPipelineDepthStencilStateCreateInfo vk_depth_stencil_create_info;
        memset(&vk_depth_stencil_create_info, 0, sizeof(vk_depth_stencil_create_info));

        vk_depth_stencil_create_info.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        vk_depth_stencil_create_info.depthTestEnable       = pipelineState.m_DepthTestEnabled ? VK_TRUE : VK_FALSE;
        vk_depth_stencil_create_info.depthWriteEnable      = pipelineState.m_WriteDepth       ? VK_TRUE : VK_FALSE;
        vk_depth_stencil_create_info.depthCompareOp        = g_vk_compare_funcs[pipelineState.m_DepthTestFunc];
        vk_depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
        vk_depth_stencil_create_info.minDepthBounds        = 0.0f;
        vk_depth_stencil_create_info.maxDepthBounds        = 1.0f;
        vk_depth_stencil_create_info.stencilTestEnable     = pipelineState.m_StencilEnabled ? VK_TRUE : VK_FALSE;
        vk_depth_stencil_create_info.front                 = vk_stencil_op_state_front;
        vk_depth_stencil_create_info.back                  = vk_stencil_op_state_back;

        const VkDynamicState vk_dynamic_state[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo vk_dynamic_state_create_info;
        vk_dynamic_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        vk_dynamic_state_create_info.pNext             = NULL;
        vk_dynamic_state_create_info.flags             = 0;
        vk_dynamic_state_create_info.dynamicStateCount = sizeof(vk_dynamic_state) / sizeof(VkDynamicState);
        vk_dynamic_state_create_info.pDynamicStates    = vk_dynamic_state;

        VkGraphicsPipelineCreateInfo vk_pipeline_info;
        memset(&vk_pipeline_info, 0, sizeof(vk_pipeline_info));

        vk_pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        vk_pipeline_info.stageCount          = sizeof(program->m_PipelineStageInfo) / sizeof(VkPipelineShaderStageCreateInfo);
        vk_pipeline_info.pStages             = program->m_PipelineStageInfo;
        vk_pipeline_info.pVertexInputState   = &vk_vertex_input_info;
        vk_pipeline_info.pInputAssemblyState = &vk_input_assembly;
        vk_pipeline_info.pViewportState      = &vk_viewport_state;
        vk_pipeline_info.pRasterizationState = &vk_rasterizer;
        vk_pipeline_info.pMultisampleState   = &vk_multisampling;
        vk_pipeline_info.pDepthStencilState  = &vk_depth_stencil_create_info;
        vk_pipeline_info.pColorBlendState    = &vk_color_blending;
        vk_pipeline_info.pDynamicState       = &vk_dynamic_state_create_info;
        vk_pipeline_info.layout              = program->m_Handle.m_PipelineLayout;
        vk_pipeline_info.renderPass          = render_target->m_RenderPass;
        vk_pipeline_info.subpass             = 0;
        vk_pipeline_info.basePipelineHandle  = VK_NULL_HANDLE;
        vk_pipeline_info.basePipelineIndex   = -1;

        return vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &vk_pipeline_info, 0, pipelineOut);
    }

    void ResetScratchBuffer(VkDevice vk_device, ScratchBuffer* scratchBuffer)
    {
        assert(scratchBuffer);
        scratchBuffer->m_DescriptorAllocator->Release(vk_device);
        scratchBuffer->m_MappedDataCursor = 0;
    }

    void DestroyProgram(VkDevice vk_device, Program::VulkanHandle* handle)
    {
        assert(handle);
        if (handle->m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(vk_device, handle->m_PipelineLayout, 0);
            handle->m_PipelineLayout = VK_NULL_HANDLE;
        }

        for (int i = 0; i < Program::MODULE_TYPE_COUNT; ++i)
        {
            if (handle->m_DescriptorSetLayout[i] != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(vk_device, handle->m_DescriptorSetLayout[i], 0);
                handle->m_DescriptorSetLayout[i] = VK_NULL_HANDLE;
            }
        }
    }

    void DestroyPhysicalDevice(PhysicalDevice* device)
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

    void DestroyDescriptorAllocator(VkDevice vk_device, DescriptorAllocator::VulkanHandle* handle)
    {
        assert(handle);
        if (handle->m_DescriptorSets)
        {
            delete[] handle->m_DescriptorSets;
            handle->m_DescriptorSets = 0x0;
        }

        if (handle->m_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(vk_device, handle->m_DescriptorPool, 0);
            handle->m_DescriptorPool = VK_NULL_HANDLE;
        }
    }

    void DestroyTexture(VkDevice vk_device, Texture::VulkanHandle* handle)
    {
        assert(handle);
        if (handle->m_ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(vk_device, handle->m_ImageView, 0);
            handle->m_ImageView = VK_NULL_HANDLE;
        }

        if (handle->m_Image != VK_NULL_HANDLE)
        {
            vkDestroyImage(vk_device, handle->m_Image, 0);
            handle->m_Image = VK_NULL_HANDLE;
        }
    }

    void DestroyTextureSampler(VkDevice vk_device, TextureSampler* sampler)
    {
        assert(sampler);
        if (sampler->m_Sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(vk_device, sampler->m_Sampler, 0);
            sampler->m_Sampler = VK_NULL_HANDLE;
        }
    }

    void DestroyDeviceBuffer(VkDevice vk_device, DeviceBuffer::VulkanHandle* handle)
    {
        assert(handle);

        if (handle->m_Buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(vk_device, handle->m_Buffer, 0);
            handle->m_Buffer = VK_NULL_HANDLE;
        }

        if (handle->m_Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(vk_device, handle->m_Memory, 0);
            handle->m_Memory = VK_NULL_HANDLE;
        }
    }

    void DestroyShaderModule(VkDevice vk_device, ShaderModule* shaderModule)
    {
        assert(shaderModule);

        if (shaderModule->m_Module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(vk_device, shaderModule->m_Module, 0);
            shaderModule->m_Module = VK_NULL_HANDLE;
        }
    }

    void DestroyLogicalDevice(LogicalDevice* device)
    {
        vkDestroyCommandPool(device->m_Device, device->m_CommandPool, 0);
        vkDestroyDevice(device->m_Device, 0);
        memset(device, 0, sizeof(*device));
    }

    void DestroyPipeline(VkDevice vk_device, Pipeline* pipeline)
    {
        assert(pipeline);

        if (*pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(vk_device, *pipeline, 0);
            *pipeline = VK_NULL_HANDLE;
        }
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
