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



#include "../graphics_vulkan_defines.h"

#include "../graphics_native.h"
#include "../graphics_vulkan_private.h"

#include <vulkan/vulkan_metal.h>

namespace dmGraphics
{
    void* VulkanTextureToMetal(HContext _context, const HTexture& texture)
    {
        VulkanContext* context    = (VulkanContext*) _context;
        VulkanTexture* vk_texture = GetAssetFromContainer<VulkanTexture>(context->m_AssetHandleContainer, texture);

        VkExportMetalTextureInfoEXT textureInfo = {};
        textureInfo.sType     = VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT;
        textureInfo.image     = vk_texture->m_Handle.m_Image;
        textureInfo.imageView = vk_texture->m_Handle.m_ImageView;

        VkExportMetalObjectsInfoEXT exportInfo = {};
        exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT;
        exportInfo.pNext = &textureInfo;

        vkExportMetalObjectsEXT(context->m_LogicalDevice.m_Device, &exportInfo);
        return (__bridge void*) textureInfo.mtlTexture;
    }

    void* VulkanGraphicsCommandQueueToMetal(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;

        VkExportMetalCommandQueueInfoEXT queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_COMMAND_QUEUE_INFO_EXT;
        queueInfo.queue = context->m_LogicalDevice.m_GraphicsQueue;

        VkExportMetalObjectsInfoEXT exportInfo = {};
        exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT;
        exportInfo.pNext = &queueInfo;

        vkExportMetalObjectsEXT(context->m_LogicalDevice.m_Device, &exportInfo);
        return (__bridge void*) queueInfo.mtlCommandQueue;
    }
}
