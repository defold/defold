// Copyright 2020-2025 The Defold Foundation
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

#ifndef DMSDK_GRAPHICS_VULKAN_H
#define DMSDK_GRAPHICS_VULKAN_H

#include <vulkan/vulkan_core.h>
#include <dmsdk/graphics/graphics.h>

/*# Graphics API documentation
 * [file:<dmsdk/graphics/graphics_vulkan.h>]
 *
 * Graphics Vulkan API
 *
 * @document
 * @name Graphics Vulkan
 * @namespace dmGraphics
 */

namespace dmGraphics
{
    /*#
     * Get the current swap chain texture
     * @name VulkanGetActiveSwapChainTexture
     * @param context [type: dmGraphics::HContext] the Vulkan context
     * @return swapchain [type: dmGraphics::HTexture] the swap chain texture for the current frame
     */
    HTexture VulkanGetActiveSwapChainTexture(HContext context);

    /*#
     * Get a native MTLTexture from a Vulkan HTexture. Only available when using Mac/iOS.
     * @name VulkanTextureToMetal
     * @param context [type: dmGraphics::HContext] the Vulkan context
     * @param texture [type: dmGraphics::HTexture] the texture
     * @return mtl_texture [type: id<MTLTexture>] the Metal texture wrapped with a (__bridge void*)
     */
    void* VulkanTextureToMetal(HContext context, const dmGraphics::HTexture& texture);

    /*#
     * Get the native MTLCommandQueue from the Vulkan context. Only available when using Mac/iOS.
     * @name VulkanGraphicsCommandQueueToMetal
     * @param context [type: dmGraphics::HContext] the Vulkan context
     * @return mtl_queue [type: id<MTLCommandQueue>] the Metal graphics queue wrapped with a (__bridge void*)
     */
    void* VulkanGraphicsCommandQueueToMetal(HContext context);

    /*#
     * Get Vulkan device handle. Only available when using Mac/iOS.
     * @name VulkanGetDevice
     * @param context [type:dmGraphics::HContext] the Vulkan context
     * @return device [type:VkDevice] the Vulkan device handle
     */
    VkDevice VulkanGetDevice(HContext context);

    /*#
     * Get Vulkan physical device handle. Only available when using Mac/iOS.
     * @name VulkanGetPhysicalDevice
     * @param context [type:dmGraphics::HContext] the Vulkan context
     * @return physical_device [type:VkPhysicalDevice] the Vulkan physical device handle
     */
    VkPhysicalDevice VulkanGetPhysicalDevice(HContext context);

    /*#
     * Get Vulkan instance handle. Only available when using Mac/iOS.
     * @name VulkanGetInstance
     * @param context [type:dmGraphics::HContext] the Vulkan context
     * @return instance [type:VkInstance] the Vulkan instance handle
     */
    VkInstance VulkanGetInstance(HContext context);

    /*#
     * Get Vulkan queue family. Only available when using Mac/iOS.
     * @name VulkanGetQueueFamily
     * @param context [type:dmGraphics::HContext] the Vulkan context
     * return family [type: uint16_t] ???????????
     */
    uint16_t VulkanGetQueueFamily(HContext context);

    /*#
     * Get Vulkan graphics queue handle. Only available when using Mac/iOS.
     * @name VulkanGetGraphicsQueue
     * @param context [type:dmGraphics::HContext] the Vulkan context
     * @return queue [type:VkQueue] the Vulkan graphics queue 
     */
    VkQueue VulkanGetGraphicsQueue(HContext context);

    /*#
     * Get Vulkan render pass handle. Only available when using Mac/iOS.
     * @name VulkanGetRenderPass
     * @param context [type:dmGraphics::HContext] the Vulkan context
     * @return render_pass [type:VkRenderPass] the Vulkan render pass handle
     */
    VkRenderPass VulkanGetRenderPass(HContext context);

    // bool VulkanCreateDescriptorPool(VkDevice vk_device, uint16_t max_descriptors, VkDescriptorPool* vk_descriptor_pool_out);
}

#endif // DMSDK_GRAPHICS_VULKAN_H
