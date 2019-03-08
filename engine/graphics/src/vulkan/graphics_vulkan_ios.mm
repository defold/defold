#import <Foundation/Foundation.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_ios.h>
#include "graphics_vulkan_platform.h"

namespace glfwWrapper
{
    VkResult glfwCreateWindowSurface(VkInstance instance,
                                     GLFWWindow* window,
                                     const VkAllocationCallbacks* allocator,
                                     VkSurfaceKHR* surface,
                                     bool highDpiEnabled)
    {
    	VkResult err;
        VkIOSSurfaceCreateInfoMVK sci;
        PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;

        vkCreateIOSSurfaceMVK = (PFN_vkCreateIOSSurfaceMVK)
            vkGetInstanceProcAddr(instance, "vkCreateIOSSurfaceMVK");
        if (!vkCreateIOSSurfaceMVK)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
        sci.pView = window->ns.view;

        err = vkCreateIOSSurfaceMVK(instance, &sci, allocator, surface);

        if (err)
        {
            // do error
        }

        return err;
    }
}
