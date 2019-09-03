
#include "../graphics_vulkan.h"

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <string.h>

#include <graphics/glfw/glfw_native.h>
#include <vulkan/vulkan_xcb.h>

namespace dmGraphics
{
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        xcb_connection_t* connection = XGetXCBConnection(glfwGetX11Display());
        if (!connection)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)
            vkGetInstanceProcAddr(vkInstance, "vkCreateXcbSurfaceKHR");
        if (!vkCreateXcbSurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkXcbSurfaceCreateInfoKHR vk_surface_create_info;
        memset(&vk_surface_create_info, 0, sizeof(vk_surface_create_info));
        vk_surface_create_info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        vk_surface_create_info.connection = connection;
        vk_surface_create_info.window     = glfwGetX11Window();

        return vkCreateXcbSurfaceKHR(vkInstance, &vk_surface_create_info, 0, vkSurfaceOut);
    }
}
