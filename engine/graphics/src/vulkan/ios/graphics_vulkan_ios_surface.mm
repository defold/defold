#import <Foundation/Foundation.h>

#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_ios.h>

#include <graphics/glfw/glfw_native.h>

namespace dmGraphics
{
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        VkResult err;
        VkIOSSurfaceCreateInfoMVK sci;
        PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;

        vkCreateIOSSurfaceMVK = (PFN_vkCreateIOSSurfaceMVK)
            vkGetInstanceProcAddr(vkInstance, "vkCreateIOSSurfaceMVK");

        if (!vkCreateIOSSurfaceMVK)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
        sci.pView = glfwGetiOSUIView();

        return vkCreateIOSSurfaceMVK(vkInstance, &sci, 0, vkSurfaceOut);
    }
}
