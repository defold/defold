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
PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
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
PFN_vkCmdClearColorImage vkCmdClearColorImage;
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
PFN_vkResetDescriptorPool vkResetDescriptorPool;
PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
PFN_vkGetFenceStatus vkGetFenceStatus;

namespace dmGraphics
{
    void* g_lib_vulkan = 0;
    uint8_t g_functions_loaded = 0;

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
        if (g_functions_loaded)
        {
            return;
        }

        vkCreateDevice = (PFN_vkCreateDevice) vkGetInstanceProcAddr(vk_instance, "vkCreateDevice");
        vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices) vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices");
        vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties");
        vkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties) vkGetInstanceProcAddr(vk_instance, "vkEnumerateDeviceExtensionProperties");
        vkEnumerateDeviceLayerProperties = (PFN_vkEnumerateDeviceLayerProperties) vkGetInstanceProcAddr(vk_instance, "vkEnumerateDeviceLayerProperties");
        vkGetPhysicalDeviceFormatProperties = (PFN_vkGetPhysicalDeviceFormatProperties) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFormatProperties");
        vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures");
        vkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures2");
        vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceQueueFamilyProperties");
        vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceMemoryProperties");
        vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier) vkGetInstanceProcAddr(vk_instance, "vkCmdPipelineBarrier");
        vkCreateShaderModule = (PFN_vkCreateShaderModule) vkGetInstanceProcAddr(vk_instance, "vkCreateShaderModule");
        vkCreateBuffer = (PFN_vkCreateBuffer) vkGetInstanceProcAddr(vk_instance, "vkCreateBuffer");
        vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements) vkGetInstanceProcAddr(vk_instance, "vkGetBufferMemoryRequirements");
        vkMapMemory = (PFN_vkMapMemory) vkGetInstanceProcAddr(vk_instance, "vkMapMemory");
        vkUnmapMemory = (PFN_vkUnmapMemory) vkGetInstanceProcAddr(vk_instance, "vkUnmapMemory");
        vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges) vkGetInstanceProcAddr(vk_instance, "vkFlushMappedMemoryRanges");
        vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges) vkGetInstanceProcAddr(vk_instance, "vkInvalidateMappedMemoryRanges");
        vkBindBufferMemory = (PFN_vkBindBufferMemory) vkGetInstanceProcAddr(vk_instance, "vkBindBufferMemory");
        vkDestroyBuffer = (PFN_vkDestroyBuffer) vkGetInstanceProcAddr(vk_instance, "vkDestroyBuffer");
        vkAllocateMemory = (PFN_vkAllocateMemory) vkGetInstanceProcAddr(vk_instance, "vkAllocateMemory");
        vkBindImageMemory = (PFN_vkBindImageMemory) vkGetInstanceProcAddr(vk_instance, "vkBindImageMemory");
        vkGetImageSubresourceLayout = (PFN_vkGetImageSubresourceLayout) vkGetInstanceProcAddr(vk_instance, "vkGetImageSubresourceLayout");
        vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyBuffer");
        vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyBufferToImage");
        vkCmdCopyImage = (PFN_vkCmdCopyImage) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyImage");
        vkCmdBlitImage = (PFN_vkCmdBlitImage) vkGetInstanceProcAddr(vk_instance, "vkCmdBlitImage");
        vkCmdClearAttachments = (PFN_vkCmdClearAttachments) vkGetInstanceProcAddr(vk_instance, "vkCmdClearAttachments");
        vkCmdClearColorImage = (PFN_vkCmdClearColorImage) vkGetInstanceProcAddr(vk_instance, "vkCmdClearColorImage");
        vkCreateSampler = (PFN_vkCreateSampler) vkGetInstanceProcAddr(vk_instance, "vkCreateSampler");
        vkDestroySampler = (PFN_vkDestroySampler) vkGetInstanceProcAddr(vk_instance, "vkDestroySampler");
        vkDestroyImage = (PFN_vkDestroyImage) vkGetInstanceProcAddr(vk_instance, "vkDestroyImage");
        vkFreeMemory = (PFN_vkFreeMemory) vkGetInstanceProcAddr(vk_instance, "vkFreeMemory");
        vkCreateRenderPass = (PFN_vkCreateRenderPass) vkGetInstanceProcAddr(vk_instance, "vkCreateRenderPass");
        vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass) vkGetInstanceProcAddr(vk_instance, "vkCmdBeginRenderPass");
        vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass) vkGetInstanceProcAddr(vk_instance, "vkCmdEndRenderPass");
        vkCmdNextSubpass = (PFN_vkCmdNextSubpass) vkGetInstanceProcAddr(vk_instance, "vkCmdNextSubpass");
        vkCmdExecuteCommands = (PFN_vkCmdExecuteCommands) vkGetInstanceProcAddr(vk_instance, "vkCmdExecuteCommands");
        vkCreateImage = (PFN_vkCreateImage) vkGetInstanceProcAddr(vk_instance, "vkCreateImage");
        vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements) vkGetInstanceProcAddr(vk_instance, "vkGetImageMemoryRequirements");
        vkCreateImageView = (PFN_vkCreateImageView) vkGetInstanceProcAddr(vk_instance, "vkCreateImageView");
        vkDestroyImageView = (PFN_vkDestroyImageView) vkGetInstanceProcAddr(vk_instance, "vkDestroyImageView");
        vkCreateSemaphore = (PFN_vkCreateSemaphore) vkGetInstanceProcAddr(vk_instance, "vkCreateSemaphore");
        vkDestroySemaphore = (PFN_vkDestroySemaphore) vkGetInstanceProcAddr(vk_instance, "vkDestroySemaphore");
        vkCreateFence = (PFN_vkCreateFence) vkGetInstanceProcAddr(vk_instance, "vkCreateFence");
        vkDestroyFence = (PFN_vkDestroyFence) vkGetInstanceProcAddr(vk_instance, "vkDestroyFence");
        vkWaitForFences = (PFN_vkWaitForFences) vkGetInstanceProcAddr(vk_instance, "vkWaitForFences");
        vkResetFences = (PFN_vkResetFences) vkGetInstanceProcAddr(vk_instance, "vkResetFences");
        vkCreateCommandPool = (PFN_vkCreateCommandPool) vkGetInstanceProcAddr(vk_instance, "vkCreateCommandPool");
        vkDestroyCommandPool = (PFN_vkDestroyCommandPool) vkGetInstanceProcAddr(vk_instance, "vkDestroyCommandPool");
        vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers) vkGetInstanceProcAddr(vk_instance, "vkAllocateCommandBuffers");
        vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer) vkGetInstanceProcAddr(vk_instance, "vkBeginCommandBuffer");
        vkEndCommandBuffer = (PFN_vkEndCommandBuffer) vkGetInstanceProcAddr(vk_instance, "vkEndCommandBuffer");
        vkGetDeviceQueue = (PFN_vkGetDeviceQueue) vkGetInstanceProcAddr(vk_instance, "vkGetDeviceQueue");
        vkQueueSubmit = (PFN_vkQueueSubmit) vkGetInstanceProcAddr(vk_instance, "vkQueueSubmit");
        vkQueueWaitIdle = (PFN_vkQueueWaitIdle) vkGetInstanceProcAddr(vk_instance, "vkQueueWaitIdle");
        vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle) vkGetInstanceProcAddr(vk_instance, "vkDeviceWaitIdle");
        vkCreateFramebuffer = (PFN_vkCreateFramebuffer) vkGetInstanceProcAddr(vk_instance, "vkCreateFramebuffer");
        vkCreatePipelineCache = (PFN_vkCreatePipelineCache) vkGetInstanceProcAddr(vk_instance, "vkCreatePipelineCache");
        vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout) vkGetInstanceProcAddr(vk_instance, "vkCreatePipelineLayout");
        vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines) vkGetInstanceProcAddr(vk_instance, "vkCreateGraphicsPipelines");
        vkCreateComputePipelines = (PFN_vkCreateComputePipelines) vkGetInstanceProcAddr(vk_instance, "vkCreateComputePipelines");
        vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool) vkGetInstanceProcAddr(vk_instance, "vkCreateDescriptorPool");
        vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout) vkGetInstanceProcAddr(vk_instance, "vkCreateDescriptorSetLayout");
        vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets) vkGetInstanceProcAddr(vk_instance, "vkAllocateDescriptorSets");
        vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets) vkGetInstanceProcAddr(vk_instance, "vkFreeDescriptorSets");
        vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets) vkGetInstanceProcAddr(vk_instance, "vkUpdateDescriptorSets");
        vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets) vkGetInstanceProcAddr(vk_instance, "vkCmdBindDescriptorSets");
        vkCmdBindPipeline = (PFN_vkCmdBindPipeline) vkGetInstanceProcAddr(vk_instance, "vkCmdBindPipeline");
        vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers) vkGetInstanceProcAddr(vk_instance, "vkCmdBindVertexBuffers");
        vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer) vkGetInstanceProcAddr(vk_instance, "vkCmdBindIndexBuffer");
        vkCmdSetViewport = (PFN_vkCmdSetViewport) vkGetInstanceProcAddr(vk_instance, "vkCmdSetViewport");
        vkCmdSetScissor = (PFN_vkCmdSetScissor) vkGetInstanceProcAddr(vk_instance, "vkCmdSetScissor");
        vkCmdSetLineWidth = (PFN_vkCmdSetLineWidth) vkGetInstanceProcAddr(vk_instance, "vkCmdSetLineWidth");
        vkCmdSetDepthBias = (PFN_vkCmdSetDepthBias) vkGetInstanceProcAddr(vk_instance, "vkCmdSetDepthBias");
        vkCmdPushConstants = (PFN_vkCmdPushConstants) vkGetInstanceProcAddr(vk_instance, "vkCmdPushConstants");
        vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed) vkGetInstanceProcAddr(vk_instance, "vkCmdDrawIndexed");
        vkCmdDraw = (PFN_vkCmdDraw) vkGetInstanceProcAddr(vk_instance, "vkCmdDraw");
        vkCmdDrawIndexedIndirect = (PFN_vkCmdDrawIndexedIndirect) vkGetInstanceProcAddr(vk_instance, "vkCmdDrawIndexedIndirect");
        vkCmdDrawIndirect = (PFN_vkCmdDrawIndirect) vkGetInstanceProcAddr(vk_instance, "vkCmdDrawIndirect");
        vkCmdDispatch = (PFN_vkCmdDispatch) vkGetInstanceProcAddr(vk_instance, "vkCmdDispatch");
        vkDestroyPipeline = (PFN_vkDestroyPipeline) vkGetInstanceProcAddr(vk_instance, "vkDestroyPipeline");
        vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout) vkGetInstanceProcAddr(vk_instance, "vkDestroyPipelineLayout");
        vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout) vkGetInstanceProcAddr(vk_instance, "vkDestroyDescriptorSetLayout");
        vkDestroyDevice = (PFN_vkDestroyDevice) vkGetInstanceProcAddr(vk_instance, "vkDestroyDevice");
        vkDestroyInstance = (PFN_vkDestroyInstance) vkGetInstanceProcAddr(vk_instance, "vkDestroyInstance");
        vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool) vkGetInstanceProcAddr(vk_instance, "vkDestroyDescriptorPool");
        vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers) vkGetInstanceProcAddr(vk_instance, "vkFreeCommandBuffers");
        vkDestroyRenderPass = (PFN_vkDestroyRenderPass) vkGetInstanceProcAddr(vk_instance, "vkDestroyRenderPass");
        vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer) vkGetInstanceProcAddr(vk_instance, "vkDestroyFramebuffer");
        vkDestroyShaderModule = (PFN_vkDestroyShaderModule) vkGetInstanceProcAddr(vk_instance, "vkDestroyShaderModule");
        vkDestroyPipelineCache = (PFN_vkDestroyPipelineCache) vkGetInstanceProcAddr(vk_instance, "vkDestroyPipelineCache");
        vkCreateQueryPool = (PFN_vkCreateQueryPool) vkGetInstanceProcAddr(vk_instance, "vkCreateQueryPool");
        vkDestroyQueryPool = (PFN_vkDestroyQueryPool) vkGetInstanceProcAddr(vk_instance, "vkDestroyQueryPool");
        vkGetQueryPoolResults = (PFN_vkGetQueryPoolResults) vkGetInstanceProcAddr(vk_instance, "vkGetQueryPoolResults");
        vkCmdBeginQuery = (PFN_vkCmdBeginQuery) vkGetInstanceProcAddr(vk_instance, "vkCmdBeginQuery");
        vkCmdEndQuery = (PFN_vkCmdEndQuery) vkGetInstanceProcAddr(vk_instance, "vkCmdEndQuery");
        vkCmdResetQueryPool = (PFN_vkCmdResetQueryPool) vkGetInstanceProcAddr(vk_instance, "vkCmdResetQueryPool");
        vkCmdCopyQueryPoolResults = (PFN_vkCmdCopyQueryPoolResults) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyQueryPoolResults");
        vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR) vkGetInstanceProcAddr(vk_instance, "vkCreateAndroidSurfaceKHR");
        vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR) vkGetInstanceProcAddr(vk_instance, "vkDestroySurfaceKHR");
        vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
        vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR) vkGetInstanceProcAddr(vk_instance, "vkDestroySwapchainKHR");
        vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR) vkGetInstanceProcAddr(vk_instance, "vkAcquireNextImageKHR");
        vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR) vkGetInstanceProcAddr(vk_instance, "vkCreateSwapchainKHR");
        vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR) vkGetInstanceProcAddr(vk_instance, "vkGetSwapchainImagesKHR");
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        vkQueuePresentKHR = (PFN_vkQueuePresentKHR) vkGetInstanceProcAddr(vk_instance, "vkQueuePresentKHR");
        vkResetCommandBuffer = (PFN_vkResetCommandBuffer) vkGetInstanceProcAddr(vk_instance, "vkResetCommandBuffer");
        vkResetDescriptorPool = (PFN_vkResetDescriptorPool) vkGetInstanceProcAddr(vk_instance, "vkResetDescriptorPool");
        vkCmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyImageToBuffer");
        vkGetFenceStatus = (PFN_vkGetFenceStatus) vkGetInstanceProcAddr(vk_instance, "vkGetFenceStatus");
        g_functions_loaded = 1;
    }
}
