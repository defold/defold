

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
