
#include "../graphics_vulkan.h"

#include <string.h>

#include <graphics/glfw/glfw_native.h>
#include <vulkan/vulkan_win32.h>

namespace dmGraphics
{
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, bool enableHighDPI)
    {
        PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)
            vkGetInstanceProcAddr(vkInstance, "vkCreateWin32SurfaceKHR");
        if (!vkCreateWin32SurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkWin32SurfaceCreateInfoKHR vk_surface_create_info;
        memset(&vk_surface_create_info, 0, sizeof(vk_surface_create_info));
        vk_surface_create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        vk_surface_create_info.hinstance = GetModuleHandle(NULL);
        vk_surface_create_info.hwnd      = glfwGetWindowsHWND();

        return vkCreateWin32SurfaceKHR(vkInstance, &vk_surface_create_info, 0, vkSurfaceOut);
    }
}
