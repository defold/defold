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
#include <dlib/log.h>

#include "graphics_vulkan_defines.h"
#include "graphics_vulkan_private.h"

namespace dmGraphics
{
    void InitializeVulkanTexture(VulkanTexture* t)
    {
        t->m_Type                = TEXTURE_TYPE_2D;
        t->m_GraphicsFormat      = TEXTURE_FORMAT_RGBA;
        t->m_DeviceBuffer        = 0;
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
        : m_SubPasses(0)
        , m_TextureDepthStencil(0)
        , m_Id(rtId)
        , m_IsBound(0)
        , m_SubPassCount(0)
        , m_SubPassIndex(0)
    {
        m_Extent.width  = 0;
        m_Extent.height = 0;
        memset(m_TextureColor, 0, sizeof(m_TextureColor));
        memset(&m_Handle, 0, sizeof(m_Handle));
    }

    static inline VkFormat GetVertexAttributeFormat(Type type, uint16_t size, bool normalized)
    {
        if (type == TYPE_FLOAT)
        {
            switch(size)
            {
                case 1:  return VK_FORMAT_R32_SFLOAT;
                case 2:  return VK_FORMAT_R32G32_SFLOAT;
                case 3:  return VK_FORMAT_R32G32B32_SFLOAT;
                case 4:  return VK_FORMAT_R32G32B32A32_SFLOAT;
                case 9:  return VK_FORMAT_R32G32B32_SFLOAT;
                case 16: return VK_FORMAT_R32G32B32A32_SFLOAT;
                default:break;
            }
        }
        else if (type == TYPE_INT)
        {
            switch(size)
            {
                case 1: return VK_FORMAT_R32_SINT;
                case 2: return VK_FORMAT_R32G32_SINT;
                case 3: return VK_FORMAT_R32G32B32_SINT;
                case 4: return VK_FORMAT_R32G32B32A32_SINT;
                case 9: return VK_FORMAT_R32G32B32_SINT;
                case 16: return VK_FORMAT_R32G32B32A32_SINT;
                default:break;
            }
        }
        else if (type == TYPE_UNSIGNED_INT)
        {
            switch(size)
            {
                case 1: return VK_FORMAT_R32_UINT;
                case 2: return VK_FORMAT_R32G32_UINT;
                case 3: return VK_FORMAT_R32G32B32_UINT;
                case 4: return VK_FORMAT_R32G32B32A32_UINT;
                case 9: return VK_FORMAT_R32G32B32_UINT;
                case 16: return VK_FORMAT_R32G32B32A32_UINT;
                default:break;
            }
        }
        else if (type == TYPE_BYTE)
        {
            switch(size)
            {
                case 1: return normalized ? VK_FORMAT_R8_SNORM : VK_FORMAT_R8_SINT;
                case 2: return normalized ? VK_FORMAT_R8G8_SNORM : VK_FORMAT_R8G8_SINT;
                case 3: return normalized ? VK_FORMAT_R8G8B8_SNORM : VK_FORMAT_R8G8B8_SINT;
                case 4: return normalized ? VK_FORMAT_R8G8B8A8_SNORM : VK_FORMAT_R8G8B8A8_SINT;
                case 9: return normalized ? VK_FORMAT_R8G8B8_SNORM : VK_FORMAT_R8G8B8_SINT;
                case 16: return normalized ? VK_FORMAT_R8G8B8A8_SNORM : VK_FORMAT_R8G8B8A8_SINT;
                default:break;
            }
        }
        else if (type == TYPE_UNSIGNED_BYTE)
        {
            switch(size)
            {
                case 1: return normalized ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8_UINT;
                case 2: return normalized ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8G8_UINT;
                case 3: return normalized ? VK_FORMAT_R8G8B8_UNORM : VK_FORMAT_R8G8B8_UINT;
                case 4: return normalized ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_UINT;
                case 9: return normalized ? VK_FORMAT_R8G8B8_UNORM : VK_FORMAT_R8G8B8_UINT;
                case 16: return normalized ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_UINT;
                default:break;
            }
        }
        else if (type == TYPE_SHORT)
        {
            switch(size)
            {
                case 1: return normalized ? VK_FORMAT_R16_SNORM : VK_FORMAT_R16_SINT;
                case 2: return normalized ? VK_FORMAT_R16G16_SNORM : VK_FORMAT_R16G16_SINT;
                case 3: return normalized ? VK_FORMAT_R16G16B16_SNORM : VK_FORMAT_R16G16B16_SINT;
                case 4: return normalized ? VK_FORMAT_R16G16B16A16_SNORM : VK_FORMAT_R16G16B16A16_SINT;
                case 9: return normalized ? VK_FORMAT_R16G16B16_SNORM : VK_FORMAT_R16G16B16_SINT;
                case 16: return normalized ? VK_FORMAT_R16G16B16A16_SNORM : VK_FORMAT_R16G16B16A16_SINT;
                default:break;
            }
        }
        else if (type == TYPE_UNSIGNED_SHORT)
        {
            switch(size)
            {
                case 1: return normalized ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16_UINT;
                case 2: return normalized ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R16G16_UINT;
                case 3: return normalized ? VK_FORMAT_R16G16B16_UNORM : VK_FORMAT_R16G16B16_UINT;
                case 4: return normalized ? VK_FORMAT_R16G16B16A16_UNORM : VK_FORMAT_R16G16B16A16_UINT;
                case 9: return normalized ? VK_FORMAT_R16G16B16_UNORM : VK_FORMAT_R16G16B16_UINT;
                case 16: return normalized ? VK_FORMAT_R16G16B16A16_UNORM : VK_FORMAT_R16G16B16A16_UINT;
                default:break;
            }
        }
        else if (type == TYPE_FLOAT_MAT4 || type == TYPE_FLOAT_MAT3 || type == TYPE_FLOAT_MAT2)
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

    static VkResult AllocateDescriptorPool(DescriptorAllocator* allocator, VkDevice vk_device)
    {
        VkDescriptorPool pool_handle = VK_NULL_HANDLE;
        VkResult res = CreateDescriptorPool(vk_device, allocator->m_DescriptorsPerPool, &pool_handle);

        DescriptorPool new_pool    = {};
        new_pool.m_DescriptorPool  = pool_handle;
        new_pool.m_DescriptorCount = 0;

        allocator->m_DescriptorPools.OffsetCapacity(1);
        allocator->m_DescriptorPools.Push(new_pool);

        return res;
    }

    static void AllocateDescriptorSets(DescriptorAllocator* allocator)
    {
        if (allocator->m_DescriptorSets == 0)
        {
            allocator->m_DescriptorSets = (VkDescriptorSet*) malloc(sizeof(VkDescriptorSet) * allocator->m_DescriptorSetMax);
        }
        else
        {
            allocator->m_DescriptorSets = (VkDescriptorSet*) realloc(allocator->m_DescriptorSets, sizeof(VkDescriptorSet) * allocator->m_DescriptorSetMax);
        }
    }

    static VkResult Prepare(DescriptorAllocator* allocator, VkDevice vk_device, uint32_t num_descriptors, uint32_t num_sets)
    {
        VkResult res = VK_SUCCESS;

        DescriptorPool& pool = allocator->m_DescriptorPools[allocator->m_DescriptorPoolIndex];

        if ((allocator->m_DescriptorsPerPool - pool.m_DescriptorCount) < num_descriptors)
        {
            allocator->m_DescriptorPoolIndex++;
            if (allocator->m_DescriptorPoolIndex >= allocator->m_DescriptorPools.Size())
            {
                res = AllocateDescriptorPool(allocator, vk_device);
            }
        }

        if ((allocator->m_DescriptorSetMax - allocator->m_DescriptorSetIndex) < num_sets)
        {
            allocator->m_DescriptorSetMax += allocator->m_DescriptorsPerPool;
            AllocateDescriptorSets(allocator);
        }

    #ifdef _DEBUG
        if (allocator->m_DescriptorsPerPool * allocator->m_DescriptorPools.Size() > 16384)
        {
            dmLogOnceWarning("Vulkan: There are more than 16768 descriptors (%d) in flight, this might be a performance issue.", m_DescriptorPools.Size());
        }
    #endif
        return res;
    }

    VkResult DescriptorAllocator::Allocate(VkDevice vk_device, VkDescriptorSetLayout* vk_descriptor_set_layout, uint8_t setCount, uint32_t descriptor_count, VkDescriptorSet** vk_descriptor_set_out)
    {
        Prepare(this, vk_device, descriptor_count, setCount);

        *vk_descriptor_set_out  = &m_DescriptorSets[m_DescriptorSetIndex];
        m_DescriptorSetIndex   += setCount;

        DescriptorPool& pool    = m_DescriptorPools[m_DescriptorPoolIndex];
        pool.m_DescriptorCount += descriptor_count;

        VkDescriptorSetAllocateInfo vk_descriptor_set_alloc;
        vk_descriptor_set_alloc.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        vk_descriptor_set_alloc.descriptorSetCount = setCount;
        vk_descriptor_set_alloc.pSetLayouts        = vk_descriptor_set_layout;
        vk_descriptor_set_alloc.descriptorPool     = pool.m_DescriptorPool;
        vk_descriptor_set_alloc.pNext              = 0;

        return vkAllocateDescriptorSets(vk_device, &vk_descriptor_set_alloc, *vk_descriptor_set_out);
    }

    void DescriptorAllocator::Reset(VkDevice vk_device)
    {
        if (m_DescriptorSetIndex > 0)
        {
            for (int i = 0; i < m_DescriptorPools.Size(); ++i)
            {
                vkResetDescriptorPool(vk_device, m_DescriptorPools[i].m_DescriptorPool, 0);
                m_DescriptorPools[i].m_DescriptorCount = 0;
            }
            m_DescriptorSetIndex  = 0;
            m_DescriptorPoolIndex = 0;
        }
    }

    VkResult DeviceBuffer::MapMemory(VkDevice vk_device, uint32_t offset, uint32_t size)
    {
        if (m_MappedDataPtr)
        {
            return VK_SUCCESS;
        }
        return vkMapMemory(vk_device, m_Handle.m_Memory, offset, size > 0 ? size : m_MemorySize, 0, &m_MappedDataPtr);
    }

    void DeviceBuffer::UnmapMemory(VkDevice vk_device)
    {
        if (m_MappedDataPtr == 0)
        {
            return;
        }
        vkUnmapMemory(vk_device, m_Handle.m_Memory);
        m_MappedDataPtr = 0;
    }

    const VulkanResourceType DeviceBuffer::GetType()
    {
        return RESOURCE_TYPE_DEVICE_BUFFER;
    }

    const VulkanResourceType VulkanTexture::GetType()
    {
        return RESOURCE_TYPE_TEXTURE;
    }

    const VulkanResourceType VulkanProgram::GetType()
    {
        return RESOURCE_TYPE_PROGRAM;
    }

    const VulkanResourceType RenderTarget::GetType()
    {
        return RESOURCE_TYPE_RENDER_TARGET;
    }

    uint32_t GetPhysicalDeviceCount(VkInstance vkInstance)
    {
        uint32_t vk_device_count = 0;
        vkEnumeratePhysicalDevices(vkInstance, &vk_device_count, 0);
        return vk_device_count;
    }

    void GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize, void* pNextFeatures)
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

            device_list[i].m_Features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            device_list[i].m_Features2.pNext = pNextFeatures;

            vkGetPhysicalDeviceProperties(vk_device, &device_list[i].m_Properties);
            vkGetPhysicalDeviceFeatures(vk_device, &device_list[i].m_Features);
            vkGetPhysicalDeviceFeatures2(vk_device, &device_list[i].m_Features2);
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

    struct LayoutTransitionInfo
    {
        VkAccessFlags        m_AccessMask;
        VkPipelineStageFlags m_StageMask;
    };

    static LayoutTransitionInfo GetAccessMaskAndStage(VkImageLayout layout)
    {
        switch (layout)
        {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                return { 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return { VK_ACCESS_SHADER_READ_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return { VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return { VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return { VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                return { VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT };

            case VK_IMAGE_LAYOUT_GENERAL:
                return { VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT }; // conservative

            default:
                assert(false && "Unsupported VkImageLayout in GetAccessMaskAndStage");
                // Fallback: allow everything, conservative but safe
                return { 0, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
        }
    }

    void TransitionImageLayoutWithCmdBuffer(
            VkCommandBuffer vk_command_buffer,
            VulkanTexture* texture,
            VkImageAspectFlags vk_image_aspect,
            VkImageLayout new_layout,
            uint32_t base_mip_level,
            uint32_t layer_count)
    {
        VkImageLayout old_layout = texture->m_ImageLayout[base_mip_level];
        if (old_layout == new_layout)
            return;

        LayoutTransitionInfo src = GetAccessMaskAndStage(old_layout);
        LayoutTransitionInfo dst = GetAccessMaskAndStage(new_layout);

        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = old_layout;
        barrier.newLayout                       = new_layout;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = texture->m_Handle.m_Image;
        barrier.subresourceRange.aspectMask     = vk_image_aspect;
        barrier.subresourceRange.baseMipLevel   = base_mip_level;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = layer_count;
        barrier.srcAccessMask                   = src.m_AccessMask;
        barrier.dstAccessMask                   = dst.m_AccessMask;

        vkCmdPipelineBarrier(
            vk_command_buffer,
            src.m_StageMask,
            dst.m_StageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        texture->m_ImageLayout[base_mip_level] = new_layout;
    }

    VkResult TransitionImageLayout(VkDevice vk_device,
        VkCommandPool vk_command_pool,
        VkQueue vk_queue,
        VulkanTexture* texture,
        VkImageAspectFlags vk_image_aspect,
        VkImageLayout vk_to_layout,
        uint32_t base_mip_level,
        uint32_t layer_count)
    {
        VkCommandBuffer vk_command_buffer = BeginSingleTimeCommands(vk_device, vk_command_pool);

        TransitionImageLayoutWithCmdBuffer(vk_command_buffer, texture, vk_image_aspect, vk_to_layout, base_mip_level, layer_count);

        VkFence fence;
        VkResult res = SubmitCommandBuffer(vk_device, vk_queue, vk_command_buffer, &fence);

        // Wait for the copy command to finish
        vkWaitForFences(vk_device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(vk_device, fence, NULL);
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

    VkResult CreateDescriptorPool(VkDevice vk_device, uint16_t max_descriptors, VkDescriptorPool* vk_descriptor_pool_out)
    {
        assert(vk_descriptor_pool_out && *vk_descriptor_pool_out == VK_NULL_HANDLE);

        VkDescriptorPoolSize vk_pool_size[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER,                max_descriptors},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       max_descriptors},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, max_descriptors},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_descriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          max_descriptors},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         max_descriptors},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          max_descriptors},
        };

        VkDescriptorPoolCreateInfo vk_pool_create_info;
        memset(&vk_pool_create_info, 0, sizeof(vk_pool_create_info));

        vk_pool_create_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        vk_pool_create_info.poolSizeCount = DM_ARRAY_SIZE(vk_pool_size);
        vk_pool_create_info.pPoolSizes    = vk_pool_size;
        vk_pool_create_info.maxSets       = max_descriptors;
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

    void DestroyFrameBuffer(VkDevice vk_device, VkFramebuffer vk_framebuffer)
    {
        if (vk_framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(vk_device, vk_framebuffer, 0);
        }
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

    VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool cmd_pool)
    {
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool                 = cmd_pool;
        alloc_info.commandBufferCount          = 1;

        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(device, &alloc_info, &cmd_buffer);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd_buffer, &begin_info);

        return cmd_buffer;
    }

    VkResult SubmitCommandBuffer(VkDevice vk_device, VkQueue queue, VkCommandBuffer cmd, VkFence* fence_out)
    {
        VkResult res = vkEndCommandBuffer(cmd);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkFence fence = VK_NULL_HANDLE;
        res = vkCreateFence(vk_device, &fence_info, NULL, &fence);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd;

        res = vkQueueSubmit(queue, 1, &submit_info, fence);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        *fence_out = fence;

        return res;
    }

    VkResult CreateShaderModule(VkDevice vk_device, const void* source, uint32_t sourceSize, VkShaderStageFlagBits stage_flag, ShaderModule* shaderModuleOut)
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

        VkResult res = vkCreateShaderModule( vk_device, &vk_create_info_shader, 0, &shaderModuleOut->m_Module);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        VkPipelineShaderStageCreateInfo shader_create_info = {};
        shader_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_create_info.stage  = stage_flag;
        shader_create_info.module = shaderModuleOut->m_Module;
        shader_create_info.pName  = "main";

        shaderModuleOut->m_PipelineStageInfo = shader_create_info;

        return VK_SUCCESS;
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
        descriptorAllocator->m_DescriptorSetMax   = descriptor_count;
        descriptorAllocator->m_DescriptorsPerPool = descriptor_count;
        AllocateDescriptorSets(descriptorAllocator);
        return AllocateDescriptorPool(descriptorAllocator, vk_device);
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

    VkResult CreateTexture(
        VkPhysicalDevice      vk_physical_device,
        VkDevice              vk_device,
        uint32_t              imageWidth,
        uint32_t              imageHeight,
        uint32_t              imageDepth,
        uint32_t              imageLayers,
        uint16_t              imageMips,
        VkSampleCountFlagBits vk_sample_count,
        VkFormat              vk_format,
        VkImageTiling         vk_tiling,
        VkImageUsageFlags     vk_usage,
        VkMemoryPropertyFlags vk_memory_flags,
        VkImageAspectFlags    vk_aspect,
        VulkanTexture*        textureOut)
    {
        DeviceBuffer& device_buffer = textureOut->m_DeviceBuffer;
        TextureType tex_type = textureOut->m_Type;

        VkImageViewType vk_view_type = VK_IMAGE_VIEW_TYPE_2D;

        VkImageCreateInfo vk_image_create_info = {};
        vk_image_create_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        vk_image_create_info.imageType     = VK_IMAGE_TYPE_2D;
        vk_image_create_info.extent.width  = imageWidth;
        vk_image_create_info.extent.height = imageHeight;
        vk_image_create_info.extent.depth  = imageDepth;
        vk_image_create_info.mipLevels     = imageMips;
        vk_image_create_info.arrayLayers   = imageLayers;
        vk_image_create_info.format        = vk_format;
        vk_image_create_info.tiling        = vk_tiling;
        vk_image_create_info.initialLayout = textureOut->m_ImageLayout[0];
        vk_image_create_info.usage         = vk_usage;
        vk_image_create_info.samples       = vk_sample_count;
        vk_image_create_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        vk_image_create_info.flags         = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        if (tex_type == TEXTURE_TYPE_CUBE_MAP)
        {
            assert(imageLayers == 6);
            vk_image_create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            vk_view_type   = VK_IMAGE_VIEW_TYPE_CUBE;
        }
        else if (tex_type == TEXTURE_TYPE_2D_ARRAY)
        {
            assert(imageLayers > 0);
            vk_view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }
        else if (tex_type == TEXTURE_TYPE_3D || tex_type == TEXTURE_TYPE_IMAGE_3D || tex_type == TEXTURE_TYPE_TEXTURE_3D)
        {
            vk_image_create_info.imageType = VK_IMAGE_TYPE_3D;
            vk_view_type = VK_IMAGE_VIEW_TYPE_3D;
        }

        VkResult res = vkCreateImage(vk_device, &vk_image_create_info, 0, &textureOut->m_Handle.m_Image);
        if (res != VK_SUCCESS)
        {
            return res;
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

        // Lazy / memorless might not be supported on this platform
        if (vk_memory_flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT && !GetMemoryTypeIndex(vk_physical_device, vk_memory_req.memoryTypeBits, vk_memory_flags, &memory_type_index))
        {
            vk_memory_flags ^= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
        }

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
        vk_view_create_info.subresourceRange.layerCount     = imageLayers;

        textureOut->m_Format                   = vk_format;
        textureOut->m_Destroyed                = 0;
        textureOut->m_DeviceBuffer.m_Destroyed = 0;

        if (imageMips == 0)
        {
            textureOut->m_Width  = imageWidth;
            textureOut->m_Height = imageHeight;
            textureOut->m_Depth  = imageDepth;
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
            attachment_color.loadOp         = colorAttachments[i].m_LoadOp;
            attachment_color.storeOp        = colorAttachments[i].m_StoreOp;
            attachment_color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment_color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment_color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment_color.finalLayout    = colorAttachments[i].m_ImageLayout;

            if (colorAttachments[i].m_LoadOp != VK_ATTACHMENT_LOAD_OP_DONT_CARE)
            {
                attachment_color.initialLayout = colorAttachments[i].m_ImageLayoutInitial;
            }

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

    static uint16_t FillVertexInputAttributeDesc(HVertexDeclaration vertexDeclaration, VkVertexInputAttributeDescription* vk_vertex_input_descs, uint32_t binding)
    {
        uint16_t num_attributes = 0;
        for (uint16_t i = 0; i < vertexDeclaration->m_StreamCount; ++i)
        {
            if (vertexDeclaration->m_Streams[i].m_Location == -1)
            {
                continue;
            }

            VertexDeclaration::Stream& stream = vertexDeclaration->m_Streams[i];
            VkFormat fmt = GetVertexAttributeFormat(stream.m_Type, stream.m_Size, stream.m_Normalize);

            #define PUT_ATTRIBUTE(ix, loc, ofs, fmt) \
                vk_vertex_input_descs[ix].binding = binding; \
                vk_vertex_input_descs[ix].location = loc; \
                vk_vertex_input_descs[ix].offset = ofs; \
                vk_vertex_input_descs[ix].format = fmt;

            uint32_t stream_data_size = GetGraphicsTypeDataSize(stream.m_Type);

            // TODO: This doesn't support 2x2 matrices - we can't distinguish between a vec4 and a 2x2 matrix here currently
            switch(stream.m_Size)
            {
            case 9: // 3x3 matrix
                PUT_ATTRIBUTE(num_attributes + 0, (stream.m_Location + 0), (stream.m_Offset + 0 * stream_data_size * 3), fmt);
                PUT_ATTRIBUTE(num_attributes + 1, (stream.m_Location + 1), (stream.m_Offset + 1 * stream_data_size * 3), fmt);
                PUT_ATTRIBUTE(num_attributes + 2, (stream.m_Location + 2), (stream.m_Offset + 2 * stream_data_size * 3), fmt);
                num_attributes += 3;
                break;
            case 16: // 4x4 matrix
                PUT_ATTRIBUTE(num_attributes + 0, (stream.m_Location + 0), (stream.m_Offset + 0 * stream_data_size * 4), fmt);
                PUT_ATTRIBUTE(num_attributes + 1, (stream.m_Location + 1), (stream.m_Offset + 1 * stream_data_size * 4), fmt);
                PUT_ATTRIBUTE(num_attributes + 2, (stream.m_Location + 2), (stream.m_Offset + 2 * stream_data_size * 4), fmt);
                PUT_ATTRIBUTE(num_attributes + 3, (stream.m_Location + 3), (stream.m_Offset + 3 * stream_data_size * 4), fmt);
                num_attributes += 4;
                break;
            default:
                PUT_ATTRIBUTE(num_attributes, stream.m_Location, stream.m_Offset, fmt);
                num_attributes++;
                break;
            }

            #undef PUT_ATTRIBUTE
        }

        return num_attributes;
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

    VkResult CreateComputePipeline(VkDevice vk_device, VulkanProgram* program, Pipeline* pipelineOut)
    {
        assert(pipelineOut && *pipelineOut == VK_NULL_HANDLE);

        VkComputePipelineCreateInfo vk_pipeline_create_info = {};
        vk_pipeline_create_info.sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        vk_pipeline_create_info.basePipelineHandle = 0;
        vk_pipeline_create_info.basePipelineIndex  = 0;
        vk_pipeline_create_info.flags              = 0;
        vk_pipeline_create_info.layout             = program->m_Handle.m_PipelineLayout;
        vk_pipeline_create_info.pNext              = 0;
        vk_pipeline_create_info.stage              = program->m_ComputeModule->m_PipelineStageInfo;
        return vkCreateComputePipelines(vk_device, 0, 1, &vk_pipeline_create_info, 0, pipelineOut);
    }

    VkResult CreateGraphicsPipeline(VkDevice vk_device, VkRect2D vk_scissor, VkSampleCountFlagBits vk_sample_count,
        PipelineState pipelineState, VulkanProgram* program, VertexDeclaration** vertexDeclarations, uint32_t vertexDeclarationCount,
        RenderTarget* render_target, Pipeline* pipelineOut)
    {
        assert(pipelineOut && *pipelineOut == VK_NULL_HANDLE);

        // This differs from MAX_VERTEX_STREAM_COUNT, since mat4 exhausts 4 desc slots
        const uint32_t MAX_VERTEX_INPUT_DESCS_COUNT = 32;

        uint16_t active_attributes = 0;
        VkVertexInputAttributeDescription vk_vertex_input_descs[MAX_VERTEX_INPUT_DESCS_COUNT] = {};
        VkVertexInputBindingDescription vk_vx_input_descriptions[MAX_VERTEX_BUFFERS] = {};

        for (int i = 0; i < vertexDeclarationCount; ++i)
        {
            active_attributes += FillVertexInputAttributeDesc(vertexDeclarations[i], &vk_vertex_input_descs[active_attributes], i);

            vk_vx_input_descriptions[i].binding   = i;
            vk_vx_input_descriptions[i].stride    = vertexDeclarations[i]->m_Stride;
            vk_vx_input_descriptions[i].inputRate = vertexDeclarations[i]->m_StepFunction == VERTEX_STEP_FUNCTION_VERTEX ?
                                                        VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
        }

        assert(active_attributes != 0);

        VkPipelineVertexInputStateCreateInfo vk_vertex_input_info = {};

        vk_vertex_input_info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vk_vertex_input_info.vertexBindingDescriptionCount   = vertexDeclarationCount;
        vk_vertex_input_info.pVertexBindingDescriptions      = vk_vx_input_descriptions;
        vk_vertex_input_info.vertexAttributeDescriptionCount = active_attributes;
        vk_vertex_input_info.pVertexAttributeDescriptions    = vk_vertex_input_descs;

        VkPipelineInputAssemblyStateCreateInfo vk_input_assembly;
        memset(&vk_input_assembly, 0, sizeof(vk_input_assembly));

        VkPrimitiveTopology vk_primitive_type    = g_vk_primitive_types[pipelineState.m_PrimtiveType];

        vk_input_assembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        vk_input_assembly.topology               = vk_primitive_type;
        // Only enable restart for strip topologies
        vk_input_assembly.primitiveRestartEnable =
            (vk_primitive_type == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ||
             vk_primitive_type == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP) ? VK_TRUE : VK_FALSE;

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

        uint8_t blend_attachment_count = render_target->m_ColorAttachmentCount;

        if (render_target->m_SubPasses)
        {
            SubPass& sub_pass_desc = render_target->m_SubPasses[render_target->m_SubPassIndex];
            blend_attachment_count = sub_pass_desc.m_ColorAttachments.Size();
        }

        for (int i = 0; i < blend_attachment_count; ++i)
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
        vk_color_blending.attachmentCount   = blend_attachment_count;
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

        VkPipelineShaderStageCreateInfo vk_stage_create_infos[2] = {};
        uint32_t stage_count = 0;
        if (program->m_ComputeModule)
        {
            vk_stage_create_infos[stage_count++] = program->m_ComputeModule->m_PipelineStageInfo;
        }
        if (program->m_VertexModule)
        {
            vk_stage_create_infos[stage_count++] = program->m_VertexModule->m_PipelineStageInfo;
        }
        if (program->m_FragmentModule)
        {
            vk_stage_create_infos[stage_count++] = program->m_FragmentModule->m_PipelineStageInfo;
        }

        VkGraphicsPipelineCreateInfo vk_pipeline_info;
        memset(&vk_pipeline_info, 0, sizeof(vk_pipeline_info));

        vk_pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        vk_pipeline_info.stageCount          = stage_count;
        vk_pipeline_info.pStages             = vk_stage_create_infos;
        vk_pipeline_info.pVertexInputState   = &vk_vertex_input_info;
        vk_pipeline_info.pInputAssemblyState = &vk_input_assembly;
        vk_pipeline_info.pViewportState      = &vk_viewport_state;
        vk_pipeline_info.pRasterizationState = &vk_rasterizer;
        vk_pipeline_info.pMultisampleState   = &vk_multisampling;
        vk_pipeline_info.pDepthStencilState  = &vk_depth_stencil_create_info;
        vk_pipeline_info.pColorBlendState    = &vk_color_blending;
        vk_pipeline_info.pDynamicState       = &vk_dynamic_state_create_info;
        vk_pipeline_info.layout              = program->m_Handle.m_PipelineLayout;
        vk_pipeline_info.renderPass          = render_target->m_Handle.m_RenderPass;
        vk_pipeline_info.subpass             = render_target->m_SubPassIndex;
        vk_pipeline_info.basePipelineHandle  = VK_NULL_HANDLE;
        vk_pipeline_info.basePipelineIndex   = -1;

        return vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &vk_pipeline_info, 0, pipelineOut);
    }

    void ResetScratchBuffer(VkDevice vk_device, ScratchBuffer* scratchBuffer)
    {
        assert(scratchBuffer);
        scratchBuffer->m_DescriptorAllocator->Reset(vk_device);
        scratchBuffer->m_MappedDataCursor = 0;
    }

    void DestroyProgram(VkDevice vk_device, VulkanProgram::VulkanHandle* handle)
    {
        assert(handle);
        if (handle->m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(vk_device, handle->m_PipelineLayout, 0);
            handle->m_PipelineLayout = VK_NULL_HANDLE;
        }

        for (int i = 0; i < handle->m_DescriptorSetLayoutsCount; ++i)
        {
            if (handle->m_DescriptorSetLayouts[i] != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(vk_device, handle->m_DescriptorSetLayouts[i], 0);
                handle->m_DescriptorSetLayouts[i] = VK_NULL_HANDLE;
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

    void DestroyDescriptorAllocator(VkDevice vk_device, DescriptorAllocator* allocator)
    {
        assert(allocator);
        delete[] allocator->m_DescriptorSets;
        allocator->m_DescriptorSets = 0x0;

        for (int i = 0; i < allocator->m_DescriptorPools.Size(); ++i)
        {
            vkDestroyDescriptorPool(vk_device, allocator->m_DescriptorPools[i].m_DescriptorPool, 0);
        }
    }

    void DestroyTexture(VkDevice vk_device, VulkanTexture::VulkanHandle* handle)
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

    void DestroyRenderTarget(VkDevice vk_device, RenderTarget::VulkanHandle* handle)
    {
        DestroyFrameBuffer(vk_device, handle->m_Framebuffer);
        DestroyRenderPass(vk_device, handle->m_RenderPass);
        handle->m_Framebuffer = VK_NULL_HANDLE;
        handle->m_RenderPass = VK_NULL_HANDLE;
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
        vkDestroyCommandPool(device->m_Device, device->m_CommandPoolWorker, 0);
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
        void* pNext, LogicalDevice* logicalDeviceOut)
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
        vk_device_create_info.pNext                   = pNext;
        vk_device_create_info.pQueueCreateInfos       = vk_device_queue_create_info;
        vk_device_create_info.queueCreateInfoCount    = queue_family_c;
        vk_device_create_info.pEnabledFeatures        = &device->m_Features;
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

            if (res == VK_SUCCESS)
            {
                memset(&vk_create_pool_info, 0, sizeof(vk_create_pool_info));
                vk_create_pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                vk_create_pool_info.queueFamilyIndex = (uint32_t) queueFamily.m_GraphicsQueueIx; // Use the same queue for now (use a transfer queue at some point)
                vk_create_pool_info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

                res = vkCreateCommandPool(logicalDeviceOut->m_Device, &vk_create_pool_info, 0, &logicalDeviceOut->m_CommandPoolWorker);
            }
        }

        return res;
    }

    #undef QUEUE_FAMILY_INVALID
}
