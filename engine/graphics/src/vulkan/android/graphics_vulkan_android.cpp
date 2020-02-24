#include <dlfcn.h>
#include "../graphics_vulkan_defines.h"

// Loader functions
PFN_vkCreateInstance vkCreateInstance;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;

// Device functions
PFN_vkCreateDevice vkCreateDevice;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
PFN_vkCreateShaderModule vkCreateShaderModule;
PFN_vkCreateBuffer vkCreateBuffer;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
PFN_vkMapMemory vkMapMemory;
PFN_vkUnmapMemory vkUnmapMemory;
PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
PFN_vkBindBufferMemory vkBindBufferMemory;
PFN_vkDestroyBuffer vkDestroyBuffer;
PFN_vkAllocateMemory vkAllocateMemory;
PFN_vkBindImageMemory vkBindImageMemory;
PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
PFN_vkCmdCopyImage vkCmdCopyImage;
PFN_vkCmdBlitImage vkCmdBlitImage;
PFN_vkCmdClearAttachments vkCmdClearAttachments;
PFN_vkCreateSampler vkCreateSampler;
PFN_vkDestroySampler vkDestroySampler;
PFN_vkDestroyImage vkDestroyImage;
PFN_vkFreeMemory vkFreeMemory;
PFN_vkCreateRenderPass vkCreateRenderPass;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
PFN_vkCmdNextSubpass vkCmdNextSubpass;
PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
PFN_vkCreateImage vkCreateImage;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
PFN_vkCreateImageView vkCreateImageView;
PFN_vkDestroyImageView vkDestroyImageView;
PFN_vkCreateSemaphore vkCreateSemaphore;
PFN_vkDestroySemaphore vkDestroySemaphore;
PFN_vkCreateFence vkCreateFence;
PFN_vkDestroyFence vkDestroyFence;
PFN_vkWaitForFences vkWaitForFences;
PFN_vkResetFences vkResetFences;
PFN_vkCreateCommandPool vkCreateCommandPool;
PFN_vkDestroyCommandPool vkDestroyCommandPool;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
PFN_vkEndCommandBuffer vkEndCommandBuffer;
PFN_vkGetDeviceQueue vkGetDeviceQueue;
PFN_vkQueueSubmit vkQueueSubmit;
PFN_vkQueueWaitIdle vkQueueWaitIdle;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
PFN_vkCreateFramebuffer vkCreateFramebuffer;
PFN_vkCreatePipelineCache vkCreatePipelineCache;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
PFN_vkCreateComputePipelines vkCreateComputePipelines;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
PFN_vkCmdBindPipeline vkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
PFN_vkCmdSetViewport vkCmdSetViewport;
PFN_vkCmdSetScissor vkCmdSetScissor;
PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
PFN_vkCmdPushConstants vkCmdPushConstants;
PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
PFN_vkCmdDraw vkCmdDraw;
PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
PFN_vkCmdDispatch vkCmdDispatch;
PFN_vkDestroyPipeline vkDestroyPipeline;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
PFN_vkDestroyDevice vkDestroyDevice;
PFN_vkDestroyInstance vkDestroyInstance;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
PFN_vkDestroyRenderPass vkDestroyRenderPass;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
PFN_vkDestroyShaderModule vkDestroyShaderModule;
PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
PFN_vkCreateQueryPool vkCreateQueryPool;
PFN_vkDestroyQueryPool vkDestroyQueryPool;
PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
PFN_vkCmdBeginQuery vkCmdBeginQuery;
PFN_vkCmdEndQuery vkCmdEndQuery;
PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkQueuePresentKHR vkQueuePresentKHR;
PFN_vkResetCommandBuffer vkResetCommandBuffer;

namespace dmGraphics
{
    void* g_lib_vulkan = 0;

    bool LoadVulkanLibrary()
    {
        if (g_lib_vulkan)
        {
            return true;
        }

        g_lib_vulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);

        if (!g_lib_vulkan)
        {
            return false;
        }

        // Load base function pointers
        vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties) dlsym(g_lib_vulkan, "vkEnumerateInstanceExtensionProperties");
        vkEnumerateInstanceLayerProperties     = (PFN_vkEnumerateInstanceLayerProperties) dlsym(g_lib_vulkan, "vkEnumerateInstanceLayerProperties");
        vkCreateInstance                       = (PFN_vkCreateInstance) dlsym(g_lib_vulkan, "vkCreateInstance");
        vkGetInstanceProcAddr                  = (PFN_vkGetInstanceProcAddr) dlsym(g_lib_vulkan, "vkGetInstanceProcAddr");
        vkGetDeviceProcAddr                    = (PFN_vkGetDeviceProcAddr) dlsym(g_lib_vulkan, "vkGetDeviceProcAddr");

        return true;
    }

    void LoadVulkanFunctions(VkInstance vk_instance)
    {
        #define DM_LOAD_FUNCTION(fn_name) \
            fn_name = (PFN_##fn_name) vkGetInstanceProcAddr(vk_instance, "#fn_name")

        DM_LOAD_FUNCTION(vkCreateDevice);
        DM_LOAD_FUNCTION(vkEnumeratePhysicalDevices);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceProperties);
        DM_LOAD_FUNCTION(vkEnumerateDeviceExtensionProperties);
        DM_LOAD_FUNCTION(vkEnumerateDeviceLayerProperties);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceFormatProperties);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceFeatures);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceMemoryProperties);
        DM_LOAD_FUNCTION(vkCmdPipelineBarrier);
        DM_LOAD_FUNCTION(vkCreateShaderModule);
        DM_LOAD_FUNCTION(vkCreateBuffer);
        DM_LOAD_FUNCTION(vkGetBufferMemoryRequirements);
        DM_LOAD_FUNCTION(vkMapMemory);
        DM_LOAD_FUNCTION(vkUnmapMemory);
        DM_LOAD_FUNCTION(vkFlushMappedMemoryRanges);
        DM_LOAD_FUNCTION(vkInvalidateMappedMemoryRanges);
        DM_LOAD_FUNCTION(vkBindBufferMemory);
        DM_LOAD_FUNCTION(vkDestroyBuffer);
        DM_LOAD_FUNCTION(vkAllocateMemory);
        DM_LOAD_FUNCTION(vkBindImageMemory);
        DM_LOAD_FUNCTION(vkGetImageSubresourceLayout);
        DM_LOAD_FUNCTION(vkCmdCopyBuffer);
        DM_LOAD_FUNCTION(vkCmdCopyBufferToImage);
        DM_LOAD_FUNCTION(vkCmdCopyImage);
        DM_LOAD_FUNCTION(vkCmdBlitImage);
        DM_LOAD_FUNCTION(vkCmdClearAttachments);
        DM_LOAD_FUNCTION(vkCreateSampler);
        DM_LOAD_FUNCTION(vkDestroySampler);
        DM_LOAD_FUNCTION(vkDestroyImage);
        DM_LOAD_FUNCTION(vkFreeMemory);
        DM_LOAD_FUNCTION(vkCreateRenderPass);
        DM_LOAD_FUNCTION(vkCmdBeginRenderPass);
        DM_LOAD_FUNCTION(vkCmdEndRenderPass);
        DM_LOAD_FUNCTION(vkCmdNextSubpass);
        DM_LOAD_FUNCTION(vkCmdExecuteCommands);
        DM_LOAD_FUNCTION(vkCreateImage);
        DM_LOAD_FUNCTION(vkGetImageMemoryRequirements);
        DM_LOAD_FUNCTION(vkCreateImageView);
        DM_LOAD_FUNCTION(vkDestroyImageView);
        DM_LOAD_FUNCTION(vkCreateSemaphore);
        DM_LOAD_FUNCTION(vkDestroySemaphore);
        DM_LOAD_FUNCTION(vkCreateFence);
        DM_LOAD_FUNCTION(vkDestroyFence);
        DM_LOAD_FUNCTION(vkWaitForFences);
        DM_LOAD_FUNCTION(vkResetFences);
        DM_LOAD_FUNCTION(vkCreateCommandPool);
        DM_LOAD_FUNCTION(vkDestroyCommandPool);
        DM_LOAD_FUNCTION(vkAllocateCommandBuffers);
        DM_LOAD_FUNCTION(vkBeginCommandBuffer);
        DM_LOAD_FUNCTION(vkEndCommandBuffer);
        DM_LOAD_FUNCTION(vkGetDeviceQueue);
        DM_LOAD_FUNCTION(vkQueueSubmit);
        DM_LOAD_FUNCTION(vkQueueWaitIdle);
        DM_LOAD_FUNCTION(vkDeviceWaitIdle);
        DM_LOAD_FUNCTION(vkCreateFramebuffer);
        DM_LOAD_FUNCTION(vkCreatePipelineCache);
        DM_LOAD_FUNCTION(vkCreatePipelineLayout);
        DM_LOAD_FUNCTION(vkCreateGraphicsPipelines);
        DM_LOAD_FUNCTION(vkCreateComputePipelines);
        DM_LOAD_FUNCTION(vkCreateDescriptorPool);
        DM_LOAD_FUNCTION(vkCreateDescriptorSetLayout);
        DM_LOAD_FUNCTION(vkAllocateDescriptorSets);
        DM_LOAD_FUNCTION(vkFreeDescriptorSets);
        DM_LOAD_FUNCTION(vkUpdateDescriptorSets);
        DM_LOAD_FUNCTION(vkCmdBindDescriptorSets);
        DM_LOAD_FUNCTION(vkCmdBindPipeline);
        DM_LOAD_FUNCTION(vkCmdBindVertexBuffers);
        DM_LOAD_FUNCTION(vkCmdBindIndexBuffer);
        DM_LOAD_FUNCTION(vkCmdSetViewport);
        DM_LOAD_FUNCTION(vkCmdSetScissor);
        DM_LOAD_FUNCTION(vkCmdSetLineWidth);
        DM_LOAD_FUNCTION(vkCmdSetDepthBias);
        DM_LOAD_FUNCTION(vkCmdPushConstants);
        DM_LOAD_FUNCTION(vkCmdDrawIndexed);
        DM_LOAD_FUNCTION(vkCmdDraw);
        DM_LOAD_FUNCTION(vkCmdDrawIndexedIndirect);
        DM_LOAD_FUNCTION(vkCmdDrawIndirect);
        DM_LOAD_FUNCTION(vkCmdDispatch);
        DM_LOAD_FUNCTION(vkDestroyPipeline);
        DM_LOAD_FUNCTION(vkDestroyPipelineLayout);
        DM_LOAD_FUNCTION(vkDestroyDescriptorSetLayout);
        DM_LOAD_FUNCTION(vkDestroyDevice);
        DM_LOAD_FUNCTION(vkDestroyInstance);
        DM_LOAD_FUNCTION(vkDestroyDescriptorPool);
        DM_LOAD_FUNCTION(vkFreeCommandBuffers);
        DM_LOAD_FUNCTION(vkDestroyRenderPass);
        DM_LOAD_FUNCTION(vkDestroyFramebuffer);
        DM_LOAD_FUNCTION(vkDestroyShaderModule);
        DM_LOAD_FUNCTION(vkDestroyPipelineCache);
        DM_LOAD_FUNCTION(vkCreateQueryPool);
        DM_LOAD_FUNCTION(vkDestroyQueryPool);
        DM_LOAD_FUNCTION(vkGetQueryPoolResults);
        DM_LOAD_FUNCTION(vkCmdBeginQuery);
        DM_LOAD_FUNCTION(vkCmdEndQuery);
        DM_LOAD_FUNCTION(vkCmdResetQueryPool);
        DM_LOAD_FUNCTION(vkCmdCopyQueryPoolResults);
        DM_LOAD_FUNCTION(vkCreateAndroidSurfaceKHR);
        DM_LOAD_FUNCTION(vkDestroySurfaceKHR);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);
        DM_LOAD_FUNCTION(vkDestroySwapchainKHR);
        DM_LOAD_FUNCTION(vkAcquireNextImageKHR);
        DM_LOAD_FUNCTION(vkCreateSwapchainKHR);
        DM_LOAD_FUNCTION(vkGetSwapchainImagesKHR);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);
        DM_LOAD_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR);
        DM_LOAD_FUNCTION(vkQueuePresentKHR);
        DM_LOAD_FUNCTION(vkResetCommandBuffer);

        #undef DM_LOAD_FUNCTION
    }
}
