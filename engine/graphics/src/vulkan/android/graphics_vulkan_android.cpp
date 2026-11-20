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

#include <dlfcn.h>
#include <android_native_app_glue.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dmsdk/dlib/android.h>
#include <platform/platform_window_android.h>
#include <vkquality/vkquality.h>

#include "../graphics_vulkan_defines.h"
#include "../graphics_vulkan_private.h"
#include "graphics_vulkan_android.h"

namespace dmGraphics
{
    static const char* const VKQUALITY_LIBRARY_NAME = "libvkquality.so";
    static const char* const VKQUALITY_DATA_FILE = "vkqualitydata.vkq";
    static const uint32_t VKQUALITY_RECOMMENDATION_RETRY_COUNT = 10;
    static const uint32_t VKQUALITY_RECOMMENDATION_RETRY_INTERVAL_US = 25 * 1000;

    typedef vkQualityInitResult (*VkQualityInitializeFlagsInfoFn)(JNIEnv*, AAssetManager*, const char*, const char*, const vkqGraphicsAPIInfo*, int32_t);
    typedef vkQualityRecommendation (*VkQualityGetRecommendationFn)();
    typedef void (*VkQualityDestroyFn)(JNIEnv*);

    struct VkQualityLibrary
    {
        void*                           m_Handle;
        VkQualityInitializeFlagsInfoFn m_InitializeFlagsInfo;
        VkQualityGetRecommendationFn   m_GetRecommendation;
        VkQualityDestroyFn             m_Destroy;
    };

    static bool LoadVkQualityLibrary(VkQualityLibrary* library)
    {
        memset(library, 0, sizeof(*library));
        library->m_Handle = dlopen(VKQUALITY_LIBRARY_NAME, RTLD_NOW | RTLD_LOCAL);
        if (!library->m_Handle)
            return false;

        library->m_InitializeFlagsInfo = (VkQualityInitializeFlagsInfoFn) dlsym(library->m_Handle, "vkQuality_initializeFlagsInfo");
        library->m_GetRecommendation = (VkQualityGetRecommendationFn) dlsym(library->m_Handle, "vkQuality_getRecommendation");
        library->m_Destroy = (VkQualityDestroyFn) dlsym(library->m_Handle, "vkQuality_destroy");
        if (!library->m_InitializeFlagsInfo || !library->m_GetRecommendation || !library->m_Destroy)
        {
            dlclose(library->m_Handle);
            memset(library, 0, sizeof(*library));
            return false;
        }
        return true;
    }

    bool AndroidVulkanIsRecommended(const VkPhysicalDeviceProperties* physical_device_properties)
    {
        if (!physical_device_properties)
        {
            dmLogWarning("VkQuality device information unavailable, allowing Vulkan.");
            return true;
        }

        VkQualityLibrary library;
        if (!LoadVkQualityLibrary(&library))
        {
            dmLogWarning("VkQuality native library unavailable, allowing Vulkan.");
            return true;
        }

        bool recommended = true;
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        android_app* app = dmAndroid::GetAndroidApp();
        if (!env || !app || !app->activity)
        {
            dmLogWarning("VkQuality Android runtime information unavailable, allowing Vulkan.");
            dlclose(library.m_Handle);
            return true;
        }

        vkqGraphicsAPIInfo api_info = {};
        api_info.vk_physical_device_properties = (void*) physical_device_properties;
        vkQualityInitResult init_result = library.m_InitializeFlagsInfo(
            env,
            app->activity->assetManager,
            app->activity->internalDataPath,
            VKQUALITY_DATA_FILE,
            &api_info,
            kInitFlagSkipFingerprintRecommendationCheck);

        if (init_result == kSuccess)
        {
            vkQualityRecommendation recommendation = library.m_GetRecommendation();
            for (uint32_t i = 0; recommendation == kRecommendationNotReady && i < VKQUALITY_RECOMMENDATION_RETRY_COUNT; ++i)
            {
                dmTime::Sleep(VKQUALITY_RECOMMENDATION_RETRY_INTERVAL_US);
                recommendation = library.m_GetRecommendation();
            }

            if (recommendation >= kRecommendationVulkanBecauseDeviceMatch &&
                recommendation <= kRecommendationVulkanBecauseFutureAndroid)
            {
                dmLogInfo("VkQuality recommends Vulkan (%d).", recommendation);
            }
            else if (recommendation >= kRecommendationGLESBecauseOldDevice &&
                     recommendation <= kRecommendationGLESBecausePredictionMatch)
            {
                dmLogInfo("VkQuality recommends OpenGL ES (%d), disabling Vulkan adapter.", recommendation);
                recommended = false;
            }
            else if (recommendation == kRecommendationNotReady)
            {
                dmLogWarning("VkQuality recommendation was not ready after waiting, disabling Vulkan adapter.");
                recommended = false;
            }
            else
            {
                dmLogWarning("VkQuality returned unknown recommendation (%d), allowing Vulkan.", recommendation);
            }
        }
        else
        {
            dmLogWarning("VkQuality initialization failed (%d), allowing Vulkan.", init_result);
        }

        library.m_Destroy(env);
        dlclose(library.m_Handle);
        return recommended;
    }

    void DestroyAndroidWindowSurface(VkInstance instance, VkSurfaceKHR surface)
    {
        // Releasing the last native-window reference can destroy a BLASTBufferQueue
        // callback on Android 13. Its JNI cleanup assumes the thread is attached
        // to the JVM, just like the OpenGL crash workaround for issue #6956.
        // Keep this guard around every Android Vulkan surface destruction (#13270).
        dmAndroid::ThreadAttacher thread;
        vkDestroySurfaceKHR(instance, surface, 0);
    }

    VkResult CreateWindowSurface(HWindow window, VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI, void** nativeWindowOut)
    {
        PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)
            vkGetInstanceProcAddr(vkInstance, "vkCreateAndroidSurfaceKHR");

        if (!vkCreateAndroidSurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        *vkSurfaceOut = VK_NULL_HANDLE;
        if (nativeWindowOut)
            *nativeWindowOut = 0;

        // Android may destroy and replace the native window while the app is
        // paused or resumed. WaitForAndroidWindow() acquires the current
        // native window or returns null when the activity is shutting down.
        //
        // The lifecycle may still change while the Vulkan driver is creating
        // the surface. The loop lets us revalidate the window afterwards and,
        // if it became stale, discard that surface and retry with the window
        // supplied by the next lifecycle state.
        while (true)
        {
            ANativeWindow* native_window = dmPlatform::WaitForAndroidWindow();
            if (!native_window)
                return VK_ERROR_SURFACE_LOST_KHR;

            VkAndroidSurfaceCreateInfoKHR vk_surface_create_info = {};
            vk_surface_create_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            vk_surface_create_info.window = native_window;

            VkResult result = vkCreateAndroidSurfaceKHR(vkInstance, &vk_surface_create_info, 0, vkSurfaceOut);
            bool is_current = dmPlatform::IsAndroidWindowCurrent(native_window);

            // A successfully created Vulkan surface retains the native window.
            // Keep its pointer only as an identity token so later frames can
            // detect when Android has replaced the window.
            if (is_current && result == VK_SUCCESS && nativeWindowOut)
                *nativeWindowOut = native_window;

            dmPlatform::ReleaseAndroidWindow(native_window);

            if (is_current)
                return result;

            // The lifecycle changed while Vulkan was creating the surface.
            // Discard it and retry with the next current native window.
            if (result == VK_SUCCESS)
            {
                DestroyAndroidWindowSurface(vkInstance, *vkSurfaceOut);
            }
            *vkSurfaceOut = VK_NULL_HANDLE;
        }
    }

    void SyncAndroidVulkanWindowSize(VulkanContext* context)
    {
        uint32_t width  = context->m_SwapChain->m_ImageExtent.width;
        uint32_t height = context->m_SwapChain->m_ImageExtent.height;

        context->m_WindowWidth          = width;
        context->m_WindowHeight         = height;

        if (dmPlatform::GetWindowWidth(context->m_BaseContext.m_Window) != width ||
            dmPlatform::GetWindowHeight(context->m_BaseContext.m_Window) != height)
        {
            dmPlatform::SetWindowSize(context->m_BaseContext.m_Window, width, height);
        }

        context->m_AndroidVulkanWindowWidth  = width;
        context->m_AndroidVulkanWindowHeight = height;

    }

    VkResult RecreateAndroidWindowSurface(void* ctx)
    {
        VulkanContext* context = (VulkanContext*) ctx;

        if (context->m_WindowSurface != VK_NULL_HANDLE)
        {
            DestroyAndroidWindowSurface(context->m_Instance, context->m_WindowSurface);
            context->m_WindowSurface = VK_NULL_HANDLE;
        }

        VkResult res = CreateWindowSurface(context->m_BaseContext.m_Window, context->m_Instance, &context->m_WindowSurface, dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_HIGH_DPI), &context->m_AndroidVulkanWindow);
        if (res == VK_SUCCESS)
        {
            context->m_SwapChain->m_Surface = context->m_WindowSurface;
        }
        return res;
    }

    void AndroidVulkanBeginFrame(VulkanContext* context)
    {
        dmPlatform::AndroidBeginFrame(context->m_BaseContext.m_Window);
    }

    bool AndroidVulkanHandleWindowSurfaceChange(VulkanContext* context, uint32_t window_width, uint32_t window_height)
    {
        ANativeWindow* native_window = dmPlatform::AcquireAndroidWindow();
        if (!native_window)
            return false;

        ANativeWindow* context_native_window = (ANativeWindow*) context->m_AndroidVulkanWindow;
        bool window_changed = native_window != context_native_window;
        uint32_t target_window_width = window_width;
        uint32_t target_window_height = window_height;

        int native_width = ANativeWindow_getWidth(native_window);
        int native_height = ANativeWindow_getHeight(native_window);
        dmPlatform::ReleaseAndroidWindow(native_window);
        if (native_width > 0 && native_height > 0)
        {
            target_window_width = (uint32_t) native_width;
            target_window_height = (uint32_t) native_height;
        }

        if (window_changed ||
            target_window_width != context->m_AndroidVulkanWindowWidth ||
            target_window_height != context->m_AndroidVulkanWindowHeight)
        {
            if (window_width != target_window_width || window_height != target_window_height)
            {
                dmPlatform::SetWindowSize(context->m_BaseContext.m_Window, target_window_width, target_window_height);
            }

            context->m_WindowWidth  = target_window_width;
            context->m_WindowHeight = target_window_height;
            SwapChainChanged(context,
                &context->m_WindowWidth,
                &context->m_WindowHeight,
                window_changed ? RecreateAndroidWindowSurface : 0,
                context);
            SyncAndroidVulkanWindowSize(context);
            return true;
        }

        return false;
    }

    void AndroidVulkanInitializeContext(VulkanContext* context)
    {
        SyncAndroidVulkanWindowSize(context);
    }

}
