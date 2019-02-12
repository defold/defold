
#include <vulkan/vulkan.h>
#include "graphics_vulkan_platform.h"

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <string.h>

namespace glfwWrapper
{
    typedef VkFlags VkXlibSurfaceCreateFlagsKHR;
    typedef VkFlags VkXcbSurfaceCreateFlagsKHR;
    typedef XID xcb_window_t;
    typedef XID xcb_visualid_t;
    typedef struct xcb_connection_t xcb_connection_t;

    typedef struct VkXlibSurfaceCreateInfoKHR
    {
        VkStructureType             sType;
        const void*                 pNext;
        VkXlibSurfaceCreateFlagsKHR flags;
        Display*                    dpy;
        Window                      window;
    } VkXlibSurfaceCreateInfoKHR;

    typedef struct VkXcbSurfaceCreateInfoKHR
    {
        VkStructureType             sType;
        const void*                 pNext;
        VkXcbSurfaceCreateFlagsKHR  flags;
        xcb_connection_t*           connection;
        xcb_window_t                window;
    } VkXcbSurfaceCreateInfoKHR;

    typedef VkResult (*PFN_vkCreateXlibSurfaceKHR)(VkInstance,const VkXlibSurfaceCreateInfoKHR*,const VkAllocationCallbacks*,VkSurfaceKHR*);
    typedef VkBool32 (*PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR)(VkPhysicalDevice,uint32_t,Display*,VisualID);
    typedef VkResult (*PFN_vkCreateXcbSurfaceKHR)(VkInstance,const VkXcbSurfaceCreateInfoKHR*,const VkAllocationCallbacks*,VkSurfaceKHR*);
    typedef VkBool32 (*PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR)(VkPhysicalDevice,uint32_t,xcb_connection_t*,xcb_visualid_t);

    /*
    struct {
        int        available;
        void*      handle;
        char*      extensions[2];
#if !defined(_GLFW_VULKAN_STATIC)
        PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
        PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
#endif
        int        KHR_surface;
        int        KHR_xlib_surface;
        int        KHR_xcb_surface;
    } g_vk_library;
    */


    // Taken from GLFW3 source
    VkResult glfwCreateWindowSurface(VkInstance instance,
        GLFWWindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface)
    {
        Window* handle = (Window*) window->handle;
        Display* display = (Display*) window->display;

        //if (_glfw.vk.KHR_xcb_surface && _glfw.x11.x11xcb.handle)
        
        // yeah uh... sure
        if(true)
        {
            VkResult err;
            VkXcbSurfaceCreateInfoKHR sci;
            PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;

            xcb_connection_t* connection = XGetXCBConnection(display);
            if (!connection)
            {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }

            vkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)
                vkGetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR");
            if (!vkCreateXcbSurfaceKHR)
            {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }

            memset(&sci, 0, sizeof(sci));
            sci.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            sci.connection = connection;
            sci.window = *handle;

            return vkCreateXcbSurfaceKHR(instance, &sci, allocator, surface);
        }
        else
        {
            VkResult err;
            VkXlibSurfaceCreateInfoKHR sci;
            PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;

            vkCreateXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)
                vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
            if (!vkCreateXlibSurfaceKHR)
            {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }

            memset(&sci, 0, sizeof(sci));
            sci.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
            sci.dpy = display;
            sci.window = *handle;

            return vkCreateXlibSurfaceKHR(instance, &sci, allocator, surface);
        }
    }
}