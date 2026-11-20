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

#include "window_linux.h"
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>
#include <X11/Xlib-xcb.h>
#include <dlib/array.h>
#include <string.h>
#include "../platform_window_vulkan.h"

namespace dmPlatform
{
    static PFN_vkGetInstanceProcAddr g_GetInstanceProcAddr;

    void VulkanSetLoader(PFN_vkGetInstanceProcAddr get_instance_proc_addr)
    {
        g_GetInstanceProcAddr = get_instance_proc_addr;
    }

    static bool HasExtension(const dmArray<VkExtensionProperties>& props, const char* name)
    {
        for (uint32_t i = 0; i < props.Size(); ++i)
        {
            if (!strcmp(props[i].extensionName, name))
            {
                return true;
            }
        }

        return false;
    }

    static const char* SurfaceExtension(HWindow w)
    {
        PFN_vkEnumerateInstanceExtensionProperties enumerate = (PFN_vkEnumerateInstanceExtensionProperties)g_GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
        if (!enumerate)
        {
            return 0;
        }

        dmArray<VkExtensionProperties> props;
        uint32_t count = 0;
        VkResult result;
        do
        {
            if (enumerate(0, &count, 0) != VK_SUCCESS || !count)
            {
                return 0;
            }

            props.EnsureSize(count);
            result = enumerate(0, &count, props.Begin());
        } while (result == VK_INCOMPLETE);

        if (result != VK_SUCCESS)
        {
            return 0;
        }

        props.SetSize(count);
        if (!HasExtension(props, VK_KHR_SURFACE_EXTENSION_NAME))
        {
            return 0;
        }

        if (w->m_WlDisplay)
        {
            return HasExtension(props, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME) ? VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME : 0;
        }

        if (HasExtension(props, VK_KHR_XCB_SURFACE_EXTENSION_NAME) && XGetXCBConnection(w->m_Display))
        {
            return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
        }

        return HasExtension(props, VK_KHR_XLIB_SURFACE_EXTENSION_NAME) ? VK_KHR_XLIB_SURFACE_EXTENSION_NAME : 0;
    }

    const char** VulkanGetRequiredInstanceExtensions(uint32_t* count)
    {
        static const char* xcb[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME };
        static const char* xlib[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME };
        static const char* wayland[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME };
        if (!g_GetInstanceProcAddr)
        {
            *count = 0;
            return 0;
        }

        HWindow w = LinuxActiveWindow();
        bool probe = !w;
        if (probe)
        {
            w = NewWindow();
        }

        const char* extension = w ? SurfaceExtension(w) : 0;
        if (probe)
        {
            DeleteWindow(w);
        }

        *count = extension ? 2 : 0;
        if (!extension)
        {
            return 0;
        }

        if (!strcmp(extension, wayland[1]))
        {
            return wayland;
        }

        return !strcmp(extension, xcb[1]) ? xcb : xlib;
    }

    VkResult VulkanCreateWindowSurface(VkInstance instance, HWindow w, VkSurfaceKHR* surface)
    {
        if (!surface)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        *surface = VK_NULL_HANDLE;
        if (!instance || !w || !w->m_Open || !g_GetInstanceProcAddr)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (w->m_WlDisplay)
        {
            PFN_vkCreateWaylandSurfaceKHR create = (PFN_vkCreateWaylandSurfaceKHR)g_GetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
            if (!create)
            {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }

            VkWaylandSurfaceCreateInfoKHR info = {};
            info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
            info.display = w->m_WlDisplay;
            info.surface = w->m_Surface;
            return create(instance, &info, 0, surface);
        }

        PFN_vkCreateXcbSurfaceKHR xcb = (PFN_vkCreateXcbSurfaceKHR)g_GetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR");
        if (xcb)
        {
            VkXcbSurfaceCreateInfoKHR info = {};
            info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            info.connection = XGetXCBConnection(w->m_Display);
            info.window = w->m_Xid;
            if (info.connection)
            {
                return xcb(instance, &info, 0, surface);
            }
        }

        PFN_vkCreateXlibSurfaceKHR xlib = (PFN_vkCreateXlibSurfaceKHR)g_GetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR");
        if (!xlib)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        VkXlibSurfaceCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        info.dpy = w->m_Display;
        info.window = w->m_Xid;
        return xlib(instance, &info, 0, surface);
    }

}
