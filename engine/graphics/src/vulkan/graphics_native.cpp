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

    const char** GetValidationLayers(uint16_t* num_layers, bool use_validation, bool use_randerdoc)
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
        uint32_t window_width = dmPlatform::GetWindowWidth(context->m_Window);
        uint32_t window_height = dmPlatform::GetWindowHeight(context->m_Window);

        if (window_width != context->m_WindowWidth || window_height != context->m_WindowHeight)
        {
            g_VulkanContext->m_WindowWidth  = (uint32_t) window_width;
            g_VulkanContext->m_WindowHeight = (uint32_t) window_height;

            SwapChainChanged(g_VulkanContext, &g_VulkanContext->m_WindowWidth, &g_VulkanContext->m_WindowHeight, 0, 0);
        }
    }

    bool NativeInitializeContext(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        assert(context->m_WindowSurface == VK_NULL_HANDLE);

        if (!InitializeVulkan(context))
        {
            return false;
        }

        context->m_WindowWidth         = context->m_SwapChain->m_ImageExtent.width;
        context->m_WindowHeight        = context->m_SwapChain->m_ImageExtent.height;
        context->m_CurrentRenderTarget = context->m_MainRenderTarget;

        return true;
    }

    void VulkanCloseWindow(HContext _context)
    {
        VulkanContext* context = (VulkanContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            VkDevice vk_device = context->m_LogicalDevice.m_Device;

            SynchronizeDevice(vk_device);

            VulkanDestroyResources(context);

            vkDestroySurfaceKHR(context->m_Instance, context->m_WindowSurface, 0);

            DestroyInstance(&context->m_Instance);

            if (context->m_DynamicOffsetBuffer)
            {
                free(context->m_DynamicOffsetBuffer);
            }

            DestroyTexture(vk_device, &context->m_SwapChain->m_ResolveTexture->m_Handle);
            delete context->m_SwapChain;
        }
    }

    void VulkanSetWindowSize(HContext _context, uint32_t width, uint32_t height)
    {
        VulkanContext* context = (VulkanContext*) _context;

        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            context->m_Width  = width;
            context->m_Height = height;

            dmPlatform::SetWindowSize(context->m_Window, width, height);

            context->m_WindowWidth  = dmPlatform::GetWindowWidth(context->m_Window);
            context->m_WindowHeight = dmPlatform::GetWindowHeight(context->m_Window);

            SwapChainChanged(g_VulkanContext, &context->m_WindowWidth, &context->m_WindowHeight, 0, 0);
        }
    }

    void VulkanResizeWindow(HContext _context, uint32_t width, uint32_t height)
    {
        VulkanContext* context = (VulkanContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            VulkanSetWindowSize(_context, width, height);
        }
    }
}
