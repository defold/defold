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

#include <dlib/math.h>
#include <dlib/array.h>

#include <graphics/glfw/glfw_native.h>
#include <objc/objc.h>

 #if !(defined(__arm__) || defined(__arm64__))
    #include <Carbon/Carbon.h>
#endif

#include "../graphics_vulkan_defines.h"
#include "../graphics_vulkan_private.h"

namespace dmGraphics
{
    // Source: GLFW3
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        VkMacOSSurfaceCreateInfoMVK sci;
        PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;

         vkCreateMacOSSurfaceMVK = (PFN_vkCreateMacOSSurfaceMVK)
            vkGetInstanceProcAddr(vkInstance, "vkCreateMacOSSurfaceMVK");
        if (!vkCreateMacOSSurfaceMVK)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        id window_layer  = glfwGetOSXCALayer();
        id window_view   = glfwGetOSXNSView();
        id window_object = glfwGetOSXNSWindow();

        if (enableHighDPI)
        {
            [window_layer setContentsScale:[window_object backingScaleFactor]];
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        sci.pView = window_view;

        return vkCreateMacOSSurfaceMVK(vkInstance, &sci, 0, vkSurfaceOut);
    }
}
