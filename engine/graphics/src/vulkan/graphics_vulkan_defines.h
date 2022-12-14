// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
	#define VK_NO_PROTOTYPES
#elif _MSC_VER
	#define VK_USE_PLATFORM_WIN32_KHR
#elif __linux__
    #define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>

#ifdef ANDROID
	#include "graphics_vulkan_dynamic.h"
#endif

#endif /* DMGRAPHICS_VULKAN_DEFINES_H */
