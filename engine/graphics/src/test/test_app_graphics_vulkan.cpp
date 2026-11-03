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

#include "vulkan/graphics_vulkan_private.h"
#include "test_app_graphics_texture_upload.h"

namespace dmGraphicsTest
{
    void PrepareVulkanTextureUploadReadback(dmGraphics::HContext context, dmGraphics::HTexture handle)
    {
        dmGraphics::VulkanTexture* texture = dmGraphics::GetAssetFromContainer<dmGraphics::VulkanTexture>(context->m_AssetHandleContainer, handle);
        texture->m_UsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    bool ReadVulkanTextureUpload(dmGraphics::HContext _context, dmGraphics::HTexture handle, const TextureUploadReadback& params, dmArray<uint8_t>& pixels)
    {
        using namespace dmGraphics;
        VulkanContext* context = (VulkanContext*)_context;
        VulkanTexture* texture = GetAssetFromContainer<VulkanTexture>(_context->m_AssetHandleContainer, handle);
        uint32_t slice_size = params.m_RowBytes * params.m_Rows * params.m_Depth;
        TextureFormatCompressedBlockSize block;
        uint32_t texel_bytes = GetTextureFormatCompressedBlockSize(params.m_Format, &block) ? block.m_ByteSize : GetTextureFormatBitsPerPixel(params.m_Format) / 8;
        uint32_t alignment = texel_bytes;
        while (alignment % 4)
            alignment += texel_bytes;
        uint32_t layer_stride = ((slice_size + alignment - 1) / alignment) * alignment;
        uint32_t size = layer_stride * params.m_Layers;
        DeviceBuffer buffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        VkDevice device = context->m_LogicalDevice.m_Device;
        if (CreateDeviceBuffer(context->m_PhysicalDevice.m_Device, device, size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer) != VK_SUCCESS)
            return false;
        VkCommandBuffer command = BeginSingleTimeCommands(device, context->m_LogicalDevice.m_CommandPool);
        VkImageLayout previous = texture->m_ImageLayout[params.m_Mip];
        TransitionImageLayoutWithCmdBuffer(command, texture, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, params.m_Mip, texture->m_LayerCount);
        dmArray<VkBufferImageCopy> regions;
        regions.SetCapacity(params.m_Layers);
        regions.SetSize(params.m_Layers);
        for (uint32_t layer = 0; layer < params.m_Layers; ++layer)
        {
            VkBufferImageCopy& region = regions[layer];
            memset(&region, 0, sizeof(region));
            region.bufferOffset = layer * layer_stride;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = params.m_Mip;
            region.imageSubresource.baseArrayLayer = layer;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = params.m_Width;
            region.imageExtent.height = params.m_Height;
            region.imageExtent.depth = params.m_Depth;
        }
        vkCmdCopyImageToBuffer(command, texture->m_Handle.m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer.m_Handle.m_Buffer, regions.Size(), regions.Begin());
        TransitionImageLayoutWithCmdBuffer(command, texture, VK_IMAGE_ASPECT_COLOR_BIT, previous, params.m_Mip, texture->m_LayerCount);
        VkFence fence;
        VkResult result = SubmitCommandBuffer(&context->m_LogicalDevice, command, &fence);
        bool success = result == VK_SUCCESS && vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
        if (success && buffer.MapMemory(device) == VK_SUCCESS)
            for (uint32_t layer = 0; layer < params.m_Layers; ++layer)
                memcpy(pixels.Begin() + layer * slice_size, (uint8_t*)buffer.m_MappedDataPtr + layer * layer_stride, slice_size);
        else
            success = false;
        buffer.UnmapMemory(device);
        DestroyDeviceBuffer(device, &buffer.m_Handle);
        vkDestroyFence(device, fence, 0);
        vkFreeCommandBuffers(device, context->m_LogicalDevice.m_CommandPool, 1, &command);
        return success;
    }
} // namespace dmGraphicsTest
