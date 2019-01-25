#ifndef GRAPHICS_VULKAN_PLATFORM_H
#define GRAPHICS_VULKAN_PLATFORM_H

#ifndef __MACH__
	#error "MacOS only supported platform for now.."
#endif

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

#include <vulkan/vulkan.h>

namespace glfwWrapper
{
    struct GLWFWindow
    {
    	struct
    	{
    		id view;
	    	id layer;
	    	id object;
    	} ns;
    };

    VkResult glfwCreateWindowSurface(VkInstance instance, GLWFWindow* window, const VkAllocationCallbacks* allocator, VkSurfaceKHR* surface);
}

#endif