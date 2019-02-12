#ifndef GRAPHICS_VULKAN_PLATFORM_H
#define GRAPHICS_VULKAN_PLATFORM_H

#if defined(__MACH__)
    #if defined(__MACH__) && !( defined(__arm__) || defined(__arm64__) )
    	#include <Carbon/Carbon.h>
    #endif

    #if defined(__OBJC__)
    	#import <Carbon/Carbon.h>
    #endif

    #if defined(__OBJC__)
    	#import <Cocoa/Cocoa.h>
    #else
    	#include <objc/objc.h>
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

    VkResult glfwCreateWindowSurface(VkInstance instance, GLFWWindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
}

#endif