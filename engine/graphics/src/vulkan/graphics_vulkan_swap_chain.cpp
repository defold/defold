#include "graphics_vulkan.h"

#include <dlib/math.h>

namespace dmGraphics
{
    static VkSurfaceFormatKHR SwapChainFindSurfaceFormat(const SwapChainCapabilities& capabilities)
    {
        VkSurfaceFormatKHR vk_swap_chain_format;
        vk_swap_chain_format.format     = VK_FORMAT_B8G8R8A8_UNORM;
        vk_swap_chain_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        // We expect to have one or more formats here, otherwise this function wouldn't be called.
        VkSurfaceFormatKHR vk_format_head = capabilities.m_SurfaceFormats[0];

        // If the first window surface format is undefined, we can use whatever we want.
        // Otherwise we must look through the available formats to see if any of them
        // is the one that we want. Otherwise, we just select the first one.
        if (vk_format_head.format != VK_FORMAT_UNDEFINED)
        {
            bool found_wanted_format = false;
            for (uint32_t i=0; i < capabilities.m_SurfaceFormats.Size(); ++i)
            {
                if (capabilities.m_SurfaceFormats[i].colorSpace == vk_swap_chain_format.colorSpace &&
                    capabilities.m_SurfaceFormats[i].format == vk_swap_chain_format.format)
                {
                    found_wanted_format = true;
                    break;
                }
            }

            if (!found_wanted_format)
            {
                vk_swap_chain_format = vk_format_head;
            }
        }

        return vk_swap_chain_format;
    }

    SwapChain::SwapChain(const LogicalDevice* logicalDevice, const VkSurfaceKHR surface, const SwapChainCapabilities& capabilities, const QueueFamily queueFamily)
        : m_LogicalDevice(logicalDevice)
        , m_Surface(surface)
        , m_QueueFamily(queueFamily)
        , m_SurfaceFormat(SwapChainFindSurfaceFormat(capabilities))
    {
    }

    VkResult UpdateSwapChain(SwapChain* swapChain, uint32_t* wantedWidth, uint32_t* wantedHeight,
        const bool wantVSync, SwapChainCapabilities& capabilities)
    {
        VkPresentModeKHR vk_present_mode = VK_PRESENT_MODE_FIFO_KHR;
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

         // The preTransform field can be used to rotate the swap chain when presenting
        vk_swap_chain_create_info.preTransform   = capabilities.m_SurfaceCapabilities.currentTransform;
        vk_swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        vk_swap_chain_create_info.presentMode    = vk_present_mode;
        vk_swap_chain_create_info.clipped        = VK_TRUE;
        vk_swap_chain_create_info.oldSwapchain   = VK_NULL_HANDLE;

        VkResult res = vkCreateSwapchainKHR(swapChain->m_LogicalDevice->m_Device, &vk_swap_chain_create_info, 0, &swapChain->m_SwapChain);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        vkGetSwapchainImagesKHR(swapChain->m_LogicalDevice->m_Device, swapChain->m_SwapChain, &swap_chain_image_count, 0);

        swapChain->m_Images.SetCapacity(swap_chain_image_count);
        swapChain->m_Images.SetSize(swap_chain_image_count);

        vkGetSwapchainImagesKHR(swapChain->m_LogicalDevice->m_Device, swapChain->m_SwapChain, &swap_chain_image_count, swapChain->m_Images.Begin());

        swapChain->m_ImageExtent = vk_extent;

        return VK_SUCCESS;
    }

    void ResetSwapChain(SwapChain* swapChain)
    {
        assert(swapChain);
        vkDestroySwapchainKHR(swapChain->m_LogicalDevice->m_Device, swapChain->m_SwapChain, 0);
    }

    void GetSwapChainCapabilities(PhysicalDevice* device, const VkSurfaceKHR surface, SwapChainCapabilities& capabilities)
    {
        assert(device);

        VkPhysicalDevice vk_device = device->m_Device;
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
