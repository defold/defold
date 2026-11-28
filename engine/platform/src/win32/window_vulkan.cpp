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

#include "window_win32.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "../platform_window_vulkan.h"

namespace dmPlatform
{
    const char** VulkanGetRequiredInstanceExtensions(uint32_t* count)
    {
        static const char* extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
        *count = sizeof(extensions) / sizeof(extensions[0]);
        return extensions;
    }

    VkResult VulkanCreateWindowSurface(VkInstance instance, HWindow window, VkSurfaceKHR* surface)
    {
        if (!surface)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        *surface = VK_NULL_HANDLE;
        if (!instance || !window || !window->m_Window)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        PFN_vkCreateWin32SurfaceKHR create = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
        if (!create)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkWin32SurfaceCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        info.hwnd = window->m_Window;
        info.hinstance = (HINSTANCE)GetWindowLongPtrW(info.hwnd, GWLP_HINSTANCE);
        return create(instance, &info, 0, surface);
    }

}
