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

#include <dlib/log.h>

#include "../vulkan/graphics_vulkan_defines.h"
#include "../vulkan/graphics_vulkan_private.h"

#include <platform/platform_window_vulkan.h>

#if ANDROID
#include <android_native_app_glue.h>
#include <dmsdk/dlib/android.h>
#include <platform/platform_window_android.h>
#endif

namespace dmGraphics
{
    static const char*   g_extension_names[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    #if defined(VK_USE_PLATFORM_WIN32_KHR)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    #elif defined(VK_USE_PLATFORM_ANDROID_KHR)
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
    #elif defined(VK_USE_PLATFORM_XCB_KHR)
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
    #elif defined(VK_USE_PLATFORM_IOS_MVK)
        VK_MVK_IOS_SURFACE_EXTENSION_NAME,
    #endif
    };

    static const char* DM_VULKAN_LAYER_VALIDATION   = "VK_LAYER_KHRONOS_validation";
    static const char* g_validation_layers[1];
    static const char* g_validation_layer_ext[]     = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

    extern VulkanContext* g_VulkanContext;

#if ANDROID
    static ANativeWindow* g_AndroidVulkanWindow = 0;
    static uint32_t       g_AndroidVulkanWindowWidth = 0;
    static uint32_t       g_AndroidVulkanWindowHeight = 0;

    static void SyncAndroidVulkanWindowSize(VulkanContext* context)
    {
        uint32_t width  = context->m_SwapChain->m_ImageExtent.width;
        uint32_t height = context->m_SwapChain->m_ImageExtent.height;

        context->m_WindowWidth          = width;
        context->m_WindowHeight         = height;
        context->m_BaseContext.m_Width  = width;
        context->m_BaseContext.m_Height = height;

        if (dmPlatform::GetWindowWidth(context->m_BaseContext.m_Window) != width ||
            dmPlatform::GetWindowHeight(context->m_BaseContext.m_Window) != height)
        {
            dmPlatform::SetWindowSize(context->m_BaseContext.m_Window, width, height);
        }

        g_AndroidVulkanWindowWidth  = width;
        g_AndroidVulkanWindowHeight = height;
    }

    static VkResult RecreateAndroidWindowSurface(void* ctx)
    {
        VulkanContext* context = (VulkanContext*) ctx;

        if (context->m_WindowSurface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(context->m_Instance, context->m_WindowSurface, 0);
            context->m_WindowSurface = VK_NULL_HANDLE;
        }

        VkResult res = CreateWindowSurface(context->m_BaseContext.m_Window, context->m_Instance, &context->m_WindowSurface, dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_HIGH_DPI));
        if (res == VK_SUCCESS)
        {
            android_app* app = dmAndroid::GetAndroidApp();
            g_AndroidVulkanWindow = app ? app->window : 0;
            context->m_SwapChain->m_Surface = context->m_WindowSurface;
        }
        return res;
    }
#endif

    const char** GetExtensionNames(uint16_t* num_extensions)
    {
        uint32_t required_platform_extensions_count = 0;
        const char** required_platform_extensions   = dmPlatform::VulkanGetRequiredInstanceExtensions(&required_platform_extensions_count);

        if (required_platform_extensions_count > 0)
        {
            *num_extensions = required_platform_extensions_count;
            return required_platform_extensions;
        }

        *num_extensions = DM_ARRAY_SIZE(g_extension_names);
        return g_extension_names;
    }

    const char** GetValidationLayers(uint16_t* num_layers, bool use_validation)
    {
        uint16_t count = 0;
        if (use_validation)
        {
            g_validation_layers[count++] = DM_VULKAN_LAYER_VALIDATION;
        }
        *num_layers = count;
        return g_validation_layers;
    }

    const char** GetValidationLayersExt(uint16_t* num_layers)
    {
        *num_layers = sizeof(g_validation_layer_ext) / sizeof(g_validation_layer_ext[0]);
        return g_validation_layer_ext;
    }

    bool NativeInit(const ContextParams& params)
    {
#if ANDROID
        if (!LoadVulkanLibrary())
        {
            dmLogError("Could not load Vulkan functions.");
            return false;
        }
#endif
        return true;
    }

    void NativeExit()
    {
    }

    void NativeBeginFrame(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
#if ANDROID
        dmPlatform::AndroidBeginFrame(context->m_BaseContext.m_Window);
#endif
        uint32_t window_width = dmPlatform::GetWindowWidth(context->m_BaseContext.m_Window);
        uint32_t window_height = dmPlatform::GetWindowHeight(context->m_BaseContext.m_Window);

#if ANDROID
        android_app* app = dmAndroid::GetAndroidApp();
        ANativeWindow* native_window = app ? app->window : 0;
        if (native_window && (native_window != g_AndroidVulkanWindow ||
            window_width != g_AndroidVulkanWindowWidth ||
            window_height != g_AndroidVulkanWindowHeight))
        {
            context->m_WindowWidth  = window_width;
            context->m_WindowHeight = window_height;
            SwapChainChanged(context,
                &context->m_WindowWidth,
                &context->m_WindowHeight,
                native_window != g_AndroidVulkanWindow ? RecreateAndroidWindowSurface : 0,
                context);
            SyncAndroidVulkanWindowSize(context);
            return;
        }
#endif

        if (window_width != context->m_WindowWidth || window_height != context->m_WindowHeight)
        {
            context->m_WindowWidth  = (uint32_t) window_width;
            context->m_WindowHeight = (uint32_t) window_height;

            SwapChainChanged(context, &context->m_WindowWidth, &context->m_WindowHeight, 0, 0);
#if ANDROID
            SyncAndroidVulkanWindowSize(context);
#endif
        }
    }

    bool NativeInitializeContext(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_WindowSurface == VK_NULL_HANDLE);

        if (!InitializeVulkan(_context))
        {
            return false;
        }

        context->m_WindowWidth         = context->m_SwapChain->m_ImageExtent.width;
        context->m_WindowHeight        = context->m_SwapChain->m_ImageExtent.height;
        context->m_CurrentRenderTarget = context->m_MainRenderTarget;

#if ANDROID
        android_app* app = dmAndroid::GetAndroidApp();
        g_AndroidVulkanWindow = app ? app->window : 0;
        SyncAndroidVulkanWindowSize(context);
#endif

        return true;
    }

    void VulkanCloseWindow(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_OPENED))
        {
            VkDevice vk_device = context->m_LogicalDevice.m_Device;

            SynchronizeDevice(vk_device);

            VulkanDestroyResources(_context);

            vkDestroySurfaceKHR(context->m_Instance, context->m_WindowSurface, 0);

            DestroyInstance(&context->m_Instance);

            if (context->m_DynamicOffsetBuffer)
            {
                free(context->m_DynamicOffsetBuffer);
            }

            delete context->m_SwapChain;
        }
    }

    void VulkanSetWindowSize(HContext _context, uint32_t width, uint32_t height)
    {
        VulkanContext* context = (VulkanContext*) _context;

        if (dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_OPENED))
        {
            context->m_BaseContext.m_Width  = width;
            context->m_BaseContext.m_Height = height;

            dmPlatform::SetWindowSize(context->m_BaseContext.m_Window, width, height);

            context->m_WindowWidth  = dmPlatform::GetWindowWidth(context->m_BaseContext.m_Window);
            context->m_WindowHeight = dmPlatform::GetWindowHeight(context->m_BaseContext.m_Window);

            SwapChainChanged(context, &context->m_WindowWidth, &context->m_WindowHeight, 0, 0);
#if ANDROID
            SyncAndroidVulkanWindowSize(context);
#endif
        }
    }

    void VulkanResizeWindow(HContext _context, uint32_t width, uint32_t height)
    {
        VulkanContext* context = (VulkanContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_OPENED))
        {
            VulkanSetWindowSize(_context, width, height);
        }
    }
}
