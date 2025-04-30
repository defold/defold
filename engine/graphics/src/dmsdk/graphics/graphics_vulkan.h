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
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @return swapchain [type: dmGraphics::HTexture] the swap chain texture for the current frame
     */
    HTexture VulkanGetActiveSwapChainTexture(HContext context);

    /*#
     * Get a native MTLTexture from a Vulkan HTexture. Only available when using Mac/iOS.
     * @name VulkanTextureToMetal
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param texture [type: dmGraphics::HTexture] the texture
     * @return mtl_texture [type: id<MTLTexture>] the Metal texture wrapped with a (__bridge void*)
     */
    void* VulkanTextureToMetal(HContext context, const dmGraphics::HTexture& texture);

    /*#
     * Get the native MTLCommandQueue from the Vulkan context. Only available when using Mac/iOS.
     * @name VulkanGraphicsCommandQueueToMetal
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @return mtl_queue [type: id<MTLCommandQueue>] the Metal graphics queue wrapped with a (__bridge void*)
     */
    void* VulkanGraphicsCommandQueueToMetal(HContext context);

    VkDevice VulkanGetDevice(HContext context);
    VkPhysicalDevice VulkanGetPhysicalDevice(HContext context);
    VkInstance VulkanGetInstance(HContext context);
    uint16_t VulkanGetQueueFamily(HContext context);
    VkQueue VulkanGetQueue(HContext context);
    VkRenderPass VulkanGetRenderPass(HContext context);
}

#endif // DMSDK_GRAPHICS_VULKAN_H
