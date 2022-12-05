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

#include "../graphics_vulkan_defines.h"
#include "../graphics_vulkan_private.h"

#include <string.h>

#include <graphics/glfw/glfw_native.h>
#include <vulkan/vulkan_win32.h>

namespace dmGraphics
{
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, bool enableHighDPI)
    {
        PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)
            vkGetInstanceProcAddr(vkInstance, "vkCreateWin32SurfaceKHR");
        if (!vkCreateWin32SurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkWin32SurfaceCreateInfoKHR vk_surface_create_info;
        memset(&vk_surface_create_info, 0, sizeof(vk_surface_create_info));
        vk_surface_create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        vk_surface_create_info.hinstance = GetModuleHandle(NULL);
        vk_surface_create_info.hwnd      = glfwGetWindowsHWND();

        return vkCreateWin32SurfaceKHR(vkInstance, &vk_surface_create_info, 0, vkSurfaceOut);
    }
}
