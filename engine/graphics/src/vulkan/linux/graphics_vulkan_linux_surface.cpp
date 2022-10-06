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

#include <string.h>

#include <graphics/glfw/glfw_native.h>

#include <dlib/math.h>
#include <dlib/array.h>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>

#include "../graphics_vulkan_defines.h"
#include "../graphics_vulkan_private.h"

namespace dmGraphics
{
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        xcb_connection_t* connection = XGetXCBConnection(glfwGetX11Display());
        if (!connection)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)
            vkGetInstanceProcAddr(vkInstance, "vkCreateXcbSurfaceKHR");
        if (!vkCreateXcbSurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkXcbSurfaceCreateInfoKHR vk_surface_create_info;
        memset(&vk_surface_create_info, 0, sizeof(vk_surface_create_info));
        vk_surface_create_info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        vk_surface_create_info.connection = connection;
        vk_surface_create_info.window     = glfwGetX11Window();

        return vkCreateXcbSurfaceKHR(vkInstance, &vk_surface_create_info, 0, vkSurfaceOut);
    }
}
