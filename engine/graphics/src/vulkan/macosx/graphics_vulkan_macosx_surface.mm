
#include "../graphics_vulkan.h"

#include <graphics/glfw/glfw_native.h>
#include <vulkan/vulkan_macos.h>
#include <objc/objc.h>

 #if !(defined(__arm__) || defined(__arm64__))
    #include <Carbon/Carbon.h>
#endif

namespace dmGraphics
{
    // Source: GLFW3
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        VkMacOSSurfaceCreateInfoMVK sci;
        PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;

         vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)
            vkGetInstanceProcAddr(vkInstance, "vkCreateMacOSSurfaceMVK");
        if (!vkCreateMacOSSurfaceMVK)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        id window_layer  = glfwGetOSXCALayer();
        id window_view   = glfwGetOSXNSView();
        id window_object = glfwGetOSXNSWindow();

        if (enableHighDPI)
        {
            [window_layer setContentsScale:[window_object backingScaleFactor]];
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        sci.pView = window_view;

        return vkCreateMacOSSurfaceMVK(vkInstance, &sci, 0, vkSurfaceOut);
    }
}
