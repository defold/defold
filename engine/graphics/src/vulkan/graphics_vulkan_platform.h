#ifndef GRAPHICS_VULKAN_PLATFORM_H
#define GRAPHICS_VULKAN_PLATFORM_H

// TODO: Fix these includes
#if defined(__MACH__)
    #include <objc/objc.h>

    #if !( defined(__arm__) || defined(__arm64__) )
    	#include <Carbon/Carbon.h>
    #endif
#endif

namespace glfwWrapper
{
    #ifdef __MACH__
    struct GLFWWindow
    {
    	struct
    	{
    		id view;
	    	id layer;
	    	id object;
    	} ns;
    };
    #endif

    #ifdef __linux__
    struct GLFWWindow
    {
        void* handle;
        void* display;
    };
    #endif

    VkResult glfwCreateWindowSurface(VkInstance instance, GLFWWindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface, bool highDpiEnabled);
}

#endif