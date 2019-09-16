#ifndef DMGRAPHICS_VULKAN_DEFINES_H
#define DMGRAPHICS_VULKAN_DEFINES_H

#ifdef __MACH__
	/* note: VK_USE_PLATFORM_IOS_MVK and VK_USE_PLATFORM_MACOS_MVK are
	         apparently deprecated and VK_USE_PLATFORM_METAL_EXT should
	         be used instead. But at least on my dev machine, the ext is not
	         available.. Otherwise we'd do:

	         #define VK_USE_PLATFORM_METAL_EXT
	*/
    #if (defined(__arm__) || defined(__arm64__))
        #define VK_USE_PLATFORM_IOS_MVK
    #else
        #define VK_USE_PLATFORM_MACOS_MVK
    #endif
#elif ANDROID
	#define VK_USE_PLATFORM_ANDROID_KHR
#elif WIN32
	#define VK_USE_PLATFORM_WIN32_KHR
#elif __linux__
    #define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>

#endif /* DMGRAPHICS_VULKAN_DEFINES_H */
