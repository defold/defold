
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_macos.h>
#include "graphics_vulkan_platform.h"

namespace glfwWrapper
{
    // Taken from GLFW3 source
    VkResult glfwCreateWindowSurface(VkInstance instance,
                                     GLFWWindow* window,
                                     const VkAllocationCallbacks* allocator,
                                     VkSurfaceKHR* surface,
                                     bool highDpiEnabled)
    {
    #if MAC_OS_X_VERSION_MAX_ALLOWED >= 101100
        VkResult err;
        VkMacOSSurfaceCreateInfoMVK sci;
        PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;

        vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)
            vkGetInstanceProcAddr(instance, "vkCreateMacOSSurfaceMVK");
        if (!vkCreateMacOSSurfaceMVK)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        // HACK: Dynamically load Core Animation to avoid adding an extra
        //       dependency for the majority who don't use MoltenVK
        NSBundle* bundle = [NSBundle bundleWithPath:@"/System/Library/Frameworks/QuartzCore.framework"];
        if (!bundle)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        // NOTE: Create the layer here as makeBackingLayer should not return nil
        window->ns.layer = [[bundle classNamed:@"CAMetalLayer"] layer];
        if (!window->ns.layer)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        [window->ns.view setLayer: window->ns.layer];

        if (highDpiEnabled)
        {
            [window->ns.layer setContentsScale:[window->ns.object backingScaleFactor]];
        }

        [window->ns.view setWantsLayer:YES];

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        sci.pView = window->ns.view;

        err = vkCreateMacOSSurfaceMVK(instance, &sci, allocator, surface);

        if (err)
        {
            // do error
        }

        return err;
    #else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    #endif
    }
}