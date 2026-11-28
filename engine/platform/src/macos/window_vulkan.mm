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

#include "window_macos.h"
#import <QuartzCore/CAMetalLayer.h>
#define VK_USE_PLATFORM_METAL_EXT
#define VK_USE_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include "../platform_window_vulkan.h"

namespace dmPlatform
{
    const char** VulkanGetRequiredInstanceExtensions(uint32_t* count)
    {
        static const char* metal[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_EXTENSION_NAME };
        static const char* macos[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_MVK_MACOS_SURFACE_EXTENSION_NAME };
        *count = 0;
        dmArray<VkExtensionProperties> properties;
        uint32_t size = 0;
        VkResult result;
        do
        {
            if (vkEnumerateInstanceExtensionProperties(0, &size, 0) != VK_SUCCESS || !size)
            {
                return 0;
            }

            properties.SetCapacity(size);
            properties.SetSize(size);
            result = vkEnumerateInstanceExtensionProperties(0, &size, properties.Begin());
        } while (result == VK_INCOMPLETE);

        if (result != VK_SUCCESS)
        {
            return 0;
        }

        bool has_surface = false, has_metal = false, has_macos = false;
        for (uint32_t i = 0; i < size; ++i)
        {
            const char* name = properties[i].extensionName;
            has_surface |= !strcmp(name, VK_KHR_SURFACE_EXTENSION_NAME);
            has_metal |= !strcmp(name, VK_EXT_METAL_SURFACE_EXTENSION_NAME);
            has_macos |= !strcmp(name, VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
        }

        if (!has_surface || (!has_metal && !has_macos))
        {
            return 0;
        }

        *count = 2;
        return has_metal ? metal : macos;
    }

    VkResult VulkanCreateWindowSurface(VkInstance instance, HWindow window, VkSurfaceKHR* surface)
    {
        PFN_vkCreateMetalSurfaceEXT create_metal = (PFN_vkCreateMetalSurfaceEXT)vkGetInstanceProcAddr(instance, "vkCreateMetalSurfaceEXT");
        NSView* view = window->m_View;
        CAMetalLayer* layer = [CAMetalLayer layer];
        layer.contentsScale = window->m_Scale;
        layer.drawableSize = window->m_DrawableSize;
        view.layer = layer;
        view.wantsLayer = YES;
        if (create_metal)
        {
            VkMetalSurfaceCreateInfoEXT info = {};
            info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
            info.pLayer = layer;
            return create_metal(instance, &info, 0, surface);
        }

        PFN_vkCreateMacOSSurfaceMVK create_macos = (PFN_vkCreateMacOSSurfaceMVK)vkGetInstanceProcAddr(instance, "vkCreateMacOSSurfaceMVK");
        if (!create_macos)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkMacOSSurfaceCreateInfoMVK info = {};
        info.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        info.pView = view;
        return create_macos(instance, &info, 0, surface);
    }

}
