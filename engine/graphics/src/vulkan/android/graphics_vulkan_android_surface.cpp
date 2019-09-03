
#include "../graphics_vulkan.h"

#include <graphics/glfw/glfw_native.h>
#include <vulkan/vulkan_android.h>

#include <android_native_app_glue.h>

extern struct android_app* __attribute__((weak)) g_AndroidApp;

namespace dmGraphics
{
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)
            vkGetInstanceProcAddr(vkInstance, "vkCreateAndroidSurfaceKHR");

        if (!vkCreateAndroidSurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkAndroidSurfaceCreateInfoKHR vk_surface_create_info = {};
        vk_surface_create_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        vk_surface_create_info.window = g_AndroidApp->window;

        return vkCreateAndroidSurfaceKHR(vkInstance, &vk_surface_create_info, 0, vkSurfaceOut);
    }
}
