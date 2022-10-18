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
#include <dlib/array.h>

#include "graphics_vulkan_defines.h"
#include "graphics_vulkan_private.h"

namespace dmGraphics
{
    static VkSurfaceFormatKHR SwapChainFindSurfaceFormat(const SwapChainCapabilities& capabilities)
    {
        // We expect to have one or more formats here, otherwise this function wouldn't be called.
        VkSurfaceFormatKHR vk_format_head = capabilities.m_SurfaceFormats[0];

        // If the first window surface format is undefined, we can use whatever we want.
        // Otherwise we must look through the available formats to see if any of them
        // is the one that we want. Otherwise, we just select the first one.
        if (vk_format_head.format != VK_FORMAT_UNDEFINED)
        {
            for (uint32_t i=0; i < capabilities.m_SurfaceFormats.Size(); ++i)
            {
                const VkSurfaceFormatKHR& candidate = capabilities.m_SurfaceFormats[i];

                if (candidate.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                    continue;

                switch (candidate.format)
                {
                case VK_FORMAT_R8G8B8A8_UNORM:
                case VK_FORMAT_B8G8R8A8_UNORM:
                case VK_FORMAT_A8B8G8R8_UNORM_PACK32:   return candidate; break;
                default: break;
                }
            }
        }
        else
        {
            vk_format_head.format = VK_FORMAT_B8G8R8A8_UNORM;
        }

        return vk_format_head;
    }

    static void DestroyVkSwapChain(const VkDevice vk_device, const VkSwapchainKHR vk_swap_chain, const dmArray<VkImageView>& vk_image_views)
    {
        if (vk_swap_chain != VK_NULL_HANDLE)
        {
            for (uint32_t i=0; i < vk_image_views.Size(); i++)
            {
                vkDestroyImageView(vk_device, vk_image_views[i], 0);
            }
            vkDestroySwapchainKHR(vk_device, vk_swap_chain, 0);
        }
    }

    SwapChain::SwapChain(const VkSurfaceKHR surface, VkSampleCountFlagBits vk_sample_flag,
        const SwapChainCapabilities& capabilities, const QueueFamily queueFamily, Texture* resolveTexture)
        : m_ResolveTexture(resolveTexture)
        , m_Surface(surface)
        , m_QueueFamily(queueFamily)
        , m_SurfaceFormat(SwapChainFindSurfaceFormat(capabilities))
        , m_SwapChain(VK_NULL_HANDLE)
        , m_SampleCountFlag(vk_sample_flag)
    {
    }

    VkResult SwapChain::Advance(VkDevice vk_device, VkSemaphore vk_image_available)
    {
        uint32_t image_ix;
        VkResult res = vkAcquireNextImageKHR(vk_device, m_SwapChain, UINT64_MAX,
            vk_image_available, VK_NULL_HANDLE, &image_ix);
        m_ImageIndex = (uint8_t) image_ix;
        return res;
    }

    bool SwapChain::HasMultiSampling()
    {
        return m_SampleCountFlag > VK_SAMPLE_COUNT_1_BIT;
    }

    VkResult UpdateSwapChain(PhysicalDevice* physicalDevice, LogicalDevice* logicalDevice,
        uint32_t* wantedWidth, uint32_t* wantedHeight,
        bool wantVSync, SwapChainCapabilities& capabilities, SwapChain* swapChain)
    {
        VkSwapchainKHR vk_old_swap_chain    = swapChain->m_SwapChain;
        VkDevice vk_device                  = logicalDevice->m_Device;
        VkPhysicalDevice vk_physical_device = physicalDevice->m_Device;
        VkPresentModeKHR vk_present_mode    = VK_PRESENT_MODE_FIFO_KHR;

        if (!wantVSync)
        {
            for (uint32_t i=0; i < capabilities.m_PresentModes.Size(); i++)
            {
                VkPresentModeKHR vk_present_mode_candidate = capabilities.m_PresentModes[i];

                if (vk_present_mode_candidate == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    vk_present_mode = vk_present_mode_candidate;
                    break;
                }
                else if (vk_present_mode_candidate == VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    vk_present_mode = vk_present_mode_candidate;
                }
            }
        }

        VkExtent2D vk_current_extent = capabilities.m_SurfaceCapabilities.currentExtent;
        VkExtent2D vk_extent         = {};

        if (vk_current_extent.width == 0xFFFFFFFF || vk_current_extent.width == 0xFFFFFFFF)
        {
            // Clamp swap buffer extent to our wanted width / height.
            vk_extent.width  = *wantedWidth;
            vk_extent.height = *wantedHeight;
        }
        else
        {
            vk_extent     = vk_current_extent;
            *wantedWidth  = vk_extent.width;
            *wantedHeight = vk_extent.height;
        }

        // From the docs:
        // ==============
        // minImageCount: the minimum number of images the specified device supports for a swapchain created for the surface, and will be at least one.
        // maxImageCount: the maximum number of images the specified device supports for a swapchain created for the surface,
        //  and will be either 0, or greater than or equal to minImageCount. A value of 0 means that there is no limit on the number of images,
        //  though there may be limits related to the total amount of memory used by presentable images.

         // The +1 here is to add a little bit of headroom, from vulkan-tutorial.com:
        // "we may sometimes have to wait on the driver to complete internal operations
        // before we can acquire another image to render to"
        uint32_t swap_chain_image_count = capabilities.m_SurfaceCapabilities.minImageCount + 1;

        if (capabilities.m_SurfaceCapabilities.maxImageCount > 0)
        {
            swap_chain_image_count = dmMath::Clamp(swap_chain_image_count,
                capabilities.m_SurfaceCapabilities.minImageCount,
                capabilities.m_SurfaceCapabilities.maxImageCount);
        }

        VkSwapchainCreateInfoKHR vk_swap_chain_create_info;
        memset((void*)&vk_swap_chain_create_info, 0, sizeof(vk_swap_chain_create_info));

        vk_swap_chain_create_info.sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        vk_swap_chain_create_info.surface = swapChain->m_Surface;

        vk_swap_chain_create_info.minImageCount    = swap_chain_image_count;
        vk_swap_chain_create_info.imageFormat      = swapChain->m_SurfaceFormat.format;
        vk_swap_chain_create_info.imageColorSpace  = swapChain->m_SurfaceFormat.colorSpace;
        vk_swap_chain_create_info.imageExtent      = vk_extent;

        // imageArrayLayers: the number of views in a multiview/stereo surface.
        // For non-stereoscopic-3D applications, this value is 1
        vk_swap_chain_create_info.imageArrayLayers = 1;
        vk_swap_chain_create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

         // Move queue indices over to uint32_t array
        uint32_t queue_family_indices[2] = {(uint32_t) swapChain->m_QueueFamily.m_GraphicsQueueIx, (uint32_t) swapChain->m_QueueFamily.m_PresentQueueIx};

        // If we have different queues for the different types, we just stick with
        // concurrent mode. An option here would be to do ownership transfers:
        // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#synchronization-queue-transfers
        if (swapChain->m_QueueFamily.m_GraphicsQueueIx != swapChain->m_QueueFamily.m_PresentQueueIx)
        {
            vk_swap_chain_create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            vk_swap_chain_create_info.queueFamilyIndexCount = 2;
            vk_swap_chain_create_info.pQueueFamilyIndices   = queue_family_indices;
        }
        else
        {
            // This mode is best for performance and can be used with no penality
            // if we are using the same queue for everything.
            vk_swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        VkCompositeAlphaFlagBitsKHR vk_composite_alpha_flag_selected = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        VkCompositeAlphaFlagBitsKHR vk_composite_alpha_flags[4] = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };

        for (int i = 0; i < sizeof(vk_composite_alpha_flags)/sizeof(VkCompositeAlphaFlagBitsKHR); ++i)
        {
            if (capabilities.m_SurfaceCapabilities.supportedCompositeAlpha & vk_composite_alpha_flags[i])
            {
                vk_composite_alpha_flag_selected = vk_composite_alpha_flags[i];
                break;
            }
        }

         // The preTransform field can be used to rotate the swap chain when presenting
        vk_swap_chain_create_info.preTransform   = capabilities.m_SurfaceCapabilities.currentTransform;
        vk_swap_chain_create_info.compositeAlpha = vk_composite_alpha_flag_selected;
        vk_swap_chain_create_info.presentMode    = vk_present_mode;
        vk_swap_chain_create_info.clipped        = VK_TRUE;
        vk_swap_chain_create_info.oldSwapchain   = vk_old_swap_chain;

        VkResult res = vkCreateSwapchainKHR(vk_device, &vk_swap_chain_create_info, 0, &swapChain->m_SwapChain);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        if (vk_old_swap_chain != VK_NULL_HANDLE)
        {
            DestroyVkSwapChain(vk_device, vk_old_swap_chain, swapChain->m_ImageViews);
            DestroyTexture(vk_device, &swapChain->m_ResolveTexture->m_Handle);
        }

        vkGetSwapchainImagesKHR(vk_device, swapChain->m_SwapChain, &swap_chain_image_count, 0);

        swapChain->m_Images.SetCapacity(swap_chain_image_count);
        swapChain->m_Images.SetSize(swap_chain_image_count);

        vkGetSwapchainImagesKHR(vk_device, swapChain->m_SwapChain, &swap_chain_image_count, swapChain->m_Images.Begin());

        swapChain->m_ImageExtent = vk_extent;
        swapChain->m_ImageIndex  = 0;

        swapChain->m_ImageViews.SetCapacity(swap_chain_image_count);
        swapChain->m_ImageViews.SetSize(swap_chain_image_count);

        if (swapChain->HasMultiSampling())
        {
            VkResult res = CreateTexture2D(vk_physical_device, vk_device,
                vk_extent.width, vk_extent.height, 1,
                swapChain->m_SampleCountFlag, swapChain->m_SurfaceFormat.format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, swapChain->m_ResolveTexture);
            if (res != VK_SUCCESS)
            {
                return res;
            }

            res = TransitionImageLayout(vk_device, logicalDevice->m_CommandPool, logicalDevice->m_GraphicsQueue,
                swapChain->m_ResolveTexture->m_Handle.m_Image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            if (res != VK_SUCCESS)
            {
                return res;
            }
        }

        for (uint32_t i=0; i < swap_chain_image_count; i++)
        {
            VkImageViewCreateInfo vk_create_info_image_view;
            memset((void*)&vk_create_info_image_view, 0, sizeof(vk_create_info_image_view));

            vk_create_info_image_view.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vk_create_info_image_view.viewType     = VK_IMAGE_VIEW_TYPE_2D;
            vk_create_info_image_view.image        = swapChain->m_Images[i];
            vk_create_info_image_view.format       = swapChain->m_SurfaceFormat.format;
            vk_create_info_image_view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            vk_create_info_image_view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            vk_create_info_image_view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            vk_create_info_image_view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            vk_create_info_image_view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            vk_create_info_image_view.subresourceRange.baseMipLevel   = 0;
            vk_create_info_image_view.subresourceRange.levelCount     = 1;
            vk_create_info_image_view.subresourceRange.baseArrayLayer = 0;
            vk_create_info_image_view.subresourceRange.layerCount     = 1;

            res = vkCreateImageView(vk_device, &vk_create_info_image_view, 0, &swapChain->m_ImageViews[i]);
            if (res != VK_SUCCESS)
            {
                return res;
            }
        }

        return VK_SUCCESS;
    }

    void DestroySwapChain(VkDevice vk_device, SwapChain* swapChain)
    {
        assert(swapChain);
        DestroyVkSwapChain(vk_device, swapChain->m_SwapChain, swapChain->m_ImageViews);
        DestroyTexture(vk_device, &swapChain->m_ResolveTexture->m_Handle);

        swapChain->m_SwapChain = VK_NULL_HANDLE;
    }

    void GetSwapChainCapabilities(VkPhysicalDevice vk_device, const VkSurfaceKHR surface, SwapChainCapabilities& capabilities)
    {
        uint32_t format_count        = 0;
        uint32_t present_modes_count = 0;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_device, surface, &capabilities.m_SurfaceCapabilities);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk_device, surface, &format_count, 0);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk_device, surface, &present_modes_count, 0);

        if (format_count > 0)
        {
            capabilities.m_SurfaceFormats.SetCapacity(format_count);
            capabilities.m_SurfaceFormats.SetSize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(vk_device, surface, &format_count, capabilities.m_SurfaceFormats.Begin());
        }

        if (present_modes_count > 0)
        {
            capabilities.m_PresentModes.SetCapacity(present_modes_count);
            capabilities.m_PresentModes.SetSize(present_modes_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(vk_device, surface, &present_modes_count, capabilities.m_PresentModes.Begin());
        }
    }
}
