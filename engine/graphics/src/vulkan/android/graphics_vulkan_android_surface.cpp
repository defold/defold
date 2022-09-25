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
#include <android_native_app_glue.h>

#include "../graphics_vulkan_defines.h"
#include "../graphics_vulkan_private.h"

extern struct android_app* __attribute__((weak)) g_AndroidApp;

namespace dmGraphics
{
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)
            vkGetInstanceProcAddr(vkInstance, "vkCreateAndroidSurfaceKHR");

        if (!vkCreateAndroidSurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkAndroidSurfaceCreateInfoKHR vk_surface_create_info = {};
        vk_surface_create_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        vk_surface_create_info.window = g_AndroidApp->window;

        return vkCreateAndroidSurfaceKHR(vkInstance, &vk_surface_create_info, 0, vkSurfaceOut);
    }
}
