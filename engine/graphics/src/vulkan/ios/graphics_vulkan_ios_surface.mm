// Copyright 2020-2026 The Defold Foundation
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

#import <Foundation/Foundation.h>

#include <string.h>
#include <dlib/math.h>
#include <dlib/array.h>

#include  <glfw/glfw_native.h>

#include "../graphics_vulkan_defines.h"
#include "../../graphics.h"

namespace dmGraphics
{
    VkResult CreateWindowSurface(dmPlatform::HWindow window, VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        VkIOSSurfaceCreateInfoMVK sci;
        PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;

        vkCreateIOSSurfaceMVK = (PFN_vkCreateIOSSurfaceMVK)
            vkGetInstanceProcAddr(vkInstance, "vkCreateIOSSurfaceMVK");

        if (!vkCreateIOSSurfaceMVK)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        memset(&sci, 0, sizeof(sci));
        sci.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
        sci.pView = glfwGetiOSUIView();

        return vkCreateIOSSurfaceMVK(vkInstance, &sci, 0, vkSurfaceOut);
    }
}
