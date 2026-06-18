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
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dmsdk/dlib/android.h>
#include <platform/platform_window_android.h>

#include "../graphics_vulkan_defines.h"
#include "../graphics_vulkan_private.h"
#include "graphics_vulkan_android.h"

// Loader functions
PFN_vkCreateInstance vkCreateInstance;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;

// Device functions
PFN_vkCreateDevice vkCreateDevice;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
PFN_vkCreateShaderModule vkCreateShaderModule;
PFN_vkCreateBuffer vkCreateBuffer;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
PFN_vkMapMemory vkMapMemory;
PFN_vkUnmapMemory vkUnmapMemory;
PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
PFN_vkBindBufferMemory vkBindBufferMemory;
PFN_vkDestroyBuffer vkDestroyBuffer;
PFN_vkAllocateMemory vkAllocateMemory;
PFN_vkBindImageMemory vkBindImageMemory;
PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
PFN_vkCmdCopyImage vkCmdCopyImage;
PFN_vkCmdBlitImage vkCmdBlitImage;
PFN_vkCmdClearAttachments vkCmdClearAttachments;
PFN_vkCmdClearColorImage vkCmdClearColorImage;
PFN_vkCreateSampler vkCreateSampler;
PFN_vkDestroySampler vkDestroySampler;
PFN_vkDestroyImage vkDestroyImage;
PFN_vkFreeMemory vkFreeMemory;
PFN_vkCreateRenderPass vkCreateRenderPass;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
PFN_vkCmdNextSubpass vkCmdNextSubpass;
PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
PFN_vkCreateImage vkCreateImage;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
PFN_vkCreateImageView vkCreateImageView;
PFN_vkDestroyImageView vkDestroyImageView;
PFN_vkCreateSemaphore vkCreateSemaphore;
PFN_vkDestroySemaphore vkDestroySemaphore;
PFN_vkCreateFence vkCreateFence;
PFN_vkDestroyFence vkDestroyFence;
PFN_vkWaitForFences vkWaitForFences;
PFN_vkResetFences vkResetFences;
PFN_vkCreateCommandPool vkCreateCommandPool;
PFN_vkDestroyCommandPool vkDestroyCommandPool;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
PFN_vkEndCommandBuffer vkEndCommandBuffer;
PFN_vkGetDeviceQueue vkGetDeviceQueue;
PFN_vkQueueSubmit vkQueueSubmit;
PFN_vkQueueWaitIdle vkQueueWaitIdle;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
PFN_vkCreateFramebuffer vkCreateFramebuffer;
PFN_vkCreatePipelineCache vkCreatePipelineCache;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
PFN_vkCreateComputePipelines vkCreateComputePipelines;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
PFN_vkCmdBindPipeline vkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
PFN_vkCmdSetViewport vkCmdSetViewport;
PFN_vkCmdSetScissor vkCmdSetScissor;
PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
PFN_vkCmdPushConstants vkCmdPushConstants;
PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
PFN_vkCmdDraw vkCmdDraw;
PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
PFN_vkCmdDispatch vkCmdDispatch;
PFN_vkDestroyPipeline vkDestroyPipeline;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
PFN_vkDestroyDevice vkDestroyDevice;
PFN_vkDestroyInstance vkDestroyInstance;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
PFN_vkDestroyRenderPass vkDestroyRenderPass;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
PFN_vkDestroyShaderModule vkDestroyShaderModule;
PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
PFN_vkCreateQueryPool vkCreateQueryPool;
PFN_vkDestroyQueryPool vkDestroyQueryPool;
PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
PFN_vkCmdBeginQuery vkCmdBeginQuery;
PFN_vkCmdEndQuery vkCmdEndQuery;
PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkQueuePresentKHR vkQueuePresentKHR;
PFN_vkResetCommandBuffer vkResetCommandBuffer;
PFN_vkResetDescriptorPool vkResetDescriptorPool;
PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
PFN_vkGetFenceStatus vkGetFenceStatus;

namespace dmGraphics
{
    void* g_lib_vulkan = 0;

    enum VkQualityLogLevel
    {
        VKQUALITY_LOG_INFO    = 1,
        VKQUALITY_LOG_WARNING = 2
    };

    static void AndroidSetVkQualityJavaExceptionError(JNIEnv* env, const char* message, char* error_buffer, uint32_t error_buffer_size)
    {
        if (!error_buffer || error_buffer_size == 0)
        {
            return;
        }

        if (!env->ExceptionCheck())
        {
            dmSnPrintf(error_buffer, error_buffer_size, "%s", message);
            return;
        }

        jthrowable exception = env->ExceptionOccurred();
        env->ExceptionClear();

        jclass throwable_class = env->FindClass("java/lang/Throwable");
        jmethodID to_string_method = throwable_class ? env->GetMethodID(throwable_class, "toString", "()Ljava/lang/String;") : 0;
        jstring exception_string = (jstring) (to_string_method && exception ? env->CallObjectMethod(exception, to_string_method) : 0);

        if (exception_string && !env->ExceptionCheck())
        {
            const char* exception_utf = env->GetStringUTFChars(exception_string, 0);
            dmSnPrintf(error_buffer, error_buffer_size, "%s: %s", message, exception_utf ? exception_utf : "<exception string unavailable>");
            if (exception_utf)
            {
                env->ReleaseStringUTFChars(exception_string, exception_utf);
            }
        }
        else
        {
            env->ExceptionClear();
            dmSnPrintf(error_buffer, error_buffer_size, "%s: <exception details unavailable>", message);
        }

        if (exception_string)
        {
            env->DeleteLocalRef(exception_string);
        }
        if (throwable_class)
        {
            env->DeleteLocalRef(throwable_class);
        }
        if (exception)
        {
            env->DeleteLocalRef(exception);
        }
    }

    static bool AndroidLoadDefoldVkQualityClass(JNIEnv* env, jclass* activity_class, char* error_buffer, uint32_t error_buffer_size)
    {
        *activity_class = dmAndroid::LoadClass(env, "com.dynamo.android.DefoldVkQuality");
        if (env->ExceptionCheck() || !*activity_class)
        {
            AndroidSetVkQualityJavaExceptionError(env, "Could not load com.dynamo.android.DefoldVkQuality", error_buffer, error_buffer_size);
            return false;
        }

        return true;
    }

    static bool AndroidCallDefoldVkQualityStaticBoolean(const char* method_name, bool* value, char* error_buffer, uint32_t error_buffer_size)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "JNI environment unavailable while calling DefoldVkQuality.%s", method_name);
            return false;
        }

        jclass activity_class = 0;
        if (!AndroidLoadDefoldVkQualityClass(env, &activity_class, error_buffer, error_buffer_size))
        {
            return false;
        }

        jmethodID method_id = env->GetStaticMethodID(activity_class, method_name, "()Z");
        if (env->ExceptionCheck() || !method_id)
        {
            char message[128];
            dmSnPrintf(message, sizeof(message), "Could not find DefoldVkQuality.%s()Z", method_name);
            AndroidSetVkQualityJavaExceptionError(env, message, error_buffer, error_buffer_size);
            env->DeleteLocalRef(activity_class);
            return false;
        }

        jboolean result = env->CallStaticBooleanMethod(activity_class, method_id);
        if (env->ExceptionCheck())
        {
            char message[128];
            dmSnPrintf(message, sizeof(message), "Exception while calling DefoldVkQuality.%s()Z", method_name);
            AndroidSetVkQualityJavaExceptionError(env, message, error_buffer, error_buffer_size);
            env->DeleteLocalRef(activity_class);
            return false;
        }

        env->DeleteLocalRef(activity_class);
        *value = result == JNI_TRUE;
        return true;
    }

    static bool AndroidCallDefoldVkQualityStaticInt(const char* method_name, int32_t* value, char* error_buffer, uint32_t error_buffer_size)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "JNI environment unavailable while calling DefoldVkQuality.%s", method_name);
            return false;
        }

        jclass activity_class = 0;
        if (!AndroidLoadDefoldVkQualityClass(env, &activity_class, error_buffer, error_buffer_size))
        {
            return false;
        }

        jmethodID method_id = env->GetStaticMethodID(activity_class, method_name, "()I");
        if (env->ExceptionCheck() || !method_id)
        {
            char message[128];
            dmSnPrintf(message, sizeof(message), "Could not find DefoldVkQuality.%s()I", method_name);
            AndroidSetVkQualityJavaExceptionError(env, message, error_buffer, error_buffer_size);
            env->DeleteLocalRef(activity_class);
            return false;
        }

        jint result = env->CallStaticIntMethod(activity_class, method_id);
        if (env->ExceptionCheck())
        {
            char message[128];
            dmSnPrintf(message, sizeof(message), "Exception while calling DefoldVkQuality.%s()I", method_name);
            AndroidSetVkQualityJavaExceptionError(env, message, error_buffer, error_buffer_size);
            env->DeleteLocalRef(activity_class);
            return false;
        }

        env->DeleteLocalRef(activity_class);
        *value = (int32_t) result;
        return true;
    }

    static bool AndroidCallDefoldVkQualityStaticString(const char* method_name, char* value_buffer, uint32_t value_buffer_size, char* error_buffer, uint32_t error_buffer_size)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "JNI environment unavailable while calling DefoldVkQuality.%s", method_name);
            return false;
        }

        jclass activity_class = 0;
        if (!AndroidLoadDefoldVkQualityClass(env, &activity_class, error_buffer, error_buffer_size))
        {
            return false;
        }

        jmethodID method_id = env->GetStaticMethodID(activity_class, method_name, "()Ljava/lang/String;");
        if (env->ExceptionCheck() || !method_id)
        {
            char message[128];
            dmSnPrintf(message, sizeof(message), "Could not find DefoldVkQuality.%s()Ljava/lang/String;", method_name);
            AndroidSetVkQualityJavaExceptionError(env, message, error_buffer, error_buffer_size);
            env->DeleteLocalRef(activity_class);
            return false;
        }

        jstring result = (jstring) env->CallStaticObjectMethod(activity_class, method_id);
        if (env->ExceptionCheck())
        {
            char message[128];
            dmSnPrintf(message, sizeof(message), "Exception while calling DefoldVkQuality.%s()Ljava/lang/String;", method_name);
            AndroidSetVkQualityJavaExceptionError(env, message, error_buffer, error_buffer_size);
            env->DeleteLocalRef(activity_class);
            return false;
        }

        if (result)
        {
            const char* result_utf = env->GetStringUTFChars(result, 0);
            dmSnPrintf(value_buffer, value_buffer_size, "%s", result_utf ? result_utf : "");
            if (result_utf)
            {
                env->ReleaseStringUTFChars(result, result_utf);
            }
            env->DeleteLocalRef(result);
        }
        else if (value_buffer && value_buffer_size > 0)
        {
            value_buffer[0] = 0;
        }

        env->DeleteLocalRef(activity_class);
        return true;
    }

    bool AndroidVulkanIsRecommended()
    {
        bool is_recommended = true;
        char error_buffer[256];
        if (!AndroidCallDefoldVkQualityStaticBoolean("isVulkanRecommended", &is_recommended, error_buffer, sizeof(error_buffer)))
        {
            // MVP1 policy: missing or unavailable VkQuality must not disable Vulkan.
            // Warn and keep Defold's existing Vulkan support probe as the source of truth.
            dmLogWarning("VkQuality result unavailable (%s), allowing Vulkan support probe.", error_buffer);
            return true;
        }

        int32_t log_level = VKQUALITY_LOG_WARNING;
        if (!AndroidCallDefoldVkQualityStaticInt("getLogLevel", &log_level, error_buffer, sizeof(error_buffer)))
        {
            dmLogWarning("VkQuality log level unavailable (%s).", error_buffer);
        }

        char message_buffer[256];
        if (AndroidCallDefoldVkQualityStaticString("getRecommendationMessage", message_buffer, sizeof(message_buffer), error_buffer, sizeof(error_buffer)) &&
            message_buffer[0] != 0)
        {
            if (log_level == VKQUALITY_LOG_WARNING)
            {
                dmLogWarning("%s", message_buffer);
            }
            else
            {
                dmLogInfo("%s", message_buffer);
            }
        }
        else
        {
            dmLogWarning("VkQuality recommendation message unavailable (%s).", error_buffer);
        }

        return is_recommended;
    }

    VkResult CreateWindowSurface(HWindow window, VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)
            vkGetInstanceProcAddr(vkInstance, "vkCreateAndroidSurfaceKHR");

        if (!vkCreateAndroidSurfaceKHR)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        struct android_app* app = dmAndroid::GetAndroidApp();
        assert(app);

        VkAndroidSurfaceCreateInfoKHR vk_surface_create_info = {};
        vk_surface_create_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        vk_surface_create_info.window = app->window;

        return vkCreateAndroidSurfaceKHR(vkInstance, &vk_surface_create_info, 0, vkSurfaceOut);
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
            vkDestroySurfaceKHR(context->m_Instance, context->m_WindowSurface, 0);
            context->m_WindowSurface = VK_NULL_HANDLE;
        }

        VkResult res = CreateWindowSurface(context->m_BaseContext.m_Window, context->m_Instance, &context->m_WindowSurface, dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_HIGH_DPI));
        if (res == VK_SUCCESS)
        {
            android_app* app = dmAndroid::GetAndroidApp();
            context->m_AndroidVulkanWindow = (void*) (app ? app->window : 0);
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
        android_app* app = dmAndroid::GetAndroidApp();
        ANativeWindow* native_window = app ? app->window : 0;
        ANativeWindow* context_native_window = (ANativeWindow*) context->m_AndroidVulkanWindow;
        uint32_t target_window_width = window_width;
        uint32_t target_window_height = window_height;

        if (native_window)
        {
            int native_width = ANativeWindow_getWidth(native_window);
            int native_height = ANativeWindow_getHeight(native_window);
            if (native_width > 0 && native_height > 0)
            {
                target_window_width = (uint32_t) native_width;
                target_window_height = (uint32_t) native_height;
            }
        }

        if (native_window && (native_window != context_native_window ||
            target_window_width != context->m_AndroidVulkanWindowWidth ||
            target_window_height != context->m_AndroidVulkanWindowHeight))
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
                native_window != context_native_window ? RecreateAndroidWindowSurface : 0,
                context);
            SyncAndroidVulkanWindowSize(context);
            return true;
        }

        return false;
    }

    void AndroidVulkanInitializeContext(VulkanContext* context)
    {
        android_app* app = dmAndroid::GetAndroidApp();
        context->m_AndroidVulkanWindow = (void*) (app ? app->window : 0);
        SyncAndroidVulkanWindowSize(context);
    }

    bool LoadVulkanLibrary()
    {
        if (g_lib_vulkan)
        {
            return true;
        }

        g_lib_vulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);

        if (!g_lib_vulkan)
        {
            return false;
        }

        // Load base function pointers
        vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties) dlsym(g_lib_vulkan, "vkEnumerateInstanceExtensionProperties");
        vkEnumerateInstanceLayerProperties     = (PFN_vkEnumerateInstanceLayerProperties) dlsym(g_lib_vulkan, "vkEnumerateInstanceLayerProperties");
        vkCreateInstance                       = (PFN_vkCreateInstance) dlsym(g_lib_vulkan, "vkCreateInstance");
        vkGetInstanceProcAddr                  = (PFN_vkGetInstanceProcAddr) dlsym(g_lib_vulkan, "vkGetInstanceProcAddr");
        vkGetDeviceProcAddr                    = (PFN_vkGetDeviceProcAddr) dlsym(g_lib_vulkan, "vkGetDeviceProcAddr");

        return true;
    }

    void LoadVulkanFunctions(VkInstance vk_instance)
    {
        vkCreateDevice = (PFN_vkCreateDevice) vkGetInstanceProcAddr(vk_instance, "vkCreateDevice");
        vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices) vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices");
        vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties");
        vkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties) vkGetInstanceProcAddr(vk_instance, "vkEnumerateDeviceExtensionProperties");
        vkEnumerateDeviceLayerProperties = (PFN_vkEnumerateDeviceLayerProperties) vkGetInstanceProcAddr(vk_instance, "vkEnumerateDeviceLayerProperties");
        vkGetPhysicalDeviceFormatProperties = (PFN_vkGetPhysicalDeviceFormatProperties) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFormatProperties");
        vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures");
        vkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures2");
        if (!vkGetPhysicalDeviceFeatures2)
        {
            vkGetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures2KHR");
        }
        vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceQueueFamilyProperties");
        vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceMemoryProperties");
        vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier) vkGetInstanceProcAddr(vk_instance, "vkCmdPipelineBarrier");
        vkCreateShaderModule = (PFN_vkCreateShaderModule) vkGetInstanceProcAddr(vk_instance, "vkCreateShaderModule");
        vkCreateBuffer = (PFN_vkCreateBuffer) vkGetInstanceProcAddr(vk_instance, "vkCreateBuffer");
        vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements) vkGetInstanceProcAddr(vk_instance, "vkGetBufferMemoryRequirements");
        vkMapMemory = (PFN_vkMapMemory) vkGetInstanceProcAddr(vk_instance, "vkMapMemory");
        vkUnmapMemory = (PFN_vkUnmapMemory) vkGetInstanceProcAddr(vk_instance, "vkUnmapMemory");
        vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges) vkGetInstanceProcAddr(vk_instance, "vkFlushMappedMemoryRanges");
        vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges) vkGetInstanceProcAddr(vk_instance, "vkInvalidateMappedMemoryRanges");
        vkBindBufferMemory = (PFN_vkBindBufferMemory) vkGetInstanceProcAddr(vk_instance, "vkBindBufferMemory");
        vkDestroyBuffer = (PFN_vkDestroyBuffer) vkGetInstanceProcAddr(vk_instance, "vkDestroyBuffer");
        vkAllocateMemory = (PFN_vkAllocateMemory) vkGetInstanceProcAddr(vk_instance, "vkAllocateMemory");
        vkBindImageMemory = (PFN_vkBindImageMemory) vkGetInstanceProcAddr(vk_instance, "vkBindImageMemory");
        vkGetImageSubresourceLayout = (PFN_vkGetImageSubresourceLayout) vkGetInstanceProcAddr(vk_instance, "vkGetImageSubresourceLayout");
        vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyBuffer");
        vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyBufferToImage");
        vkCmdCopyImage = (PFN_vkCmdCopyImage) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyImage");
        vkCmdBlitImage = (PFN_vkCmdBlitImage) vkGetInstanceProcAddr(vk_instance, "vkCmdBlitImage");
        vkCmdClearAttachments = (PFN_vkCmdClearAttachments) vkGetInstanceProcAddr(vk_instance, "vkCmdClearAttachments");
        vkCmdClearColorImage = (PFN_vkCmdClearColorImage) vkGetInstanceProcAddr(vk_instance, "vkCmdClearColorImage");
        vkCreateSampler = (PFN_vkCreateSampler) vkGetInstanceProcAddr(vk_instance, "vkCreateSampler");
        vkDestroySampler = (PFN_vkDestroySampler) vkGetInstanceProcAddr(vk_instance, "vkDestroySampler");
        vkDestroyImage = (PFN_vkDestroyImage) vkGetInstanceProcAddr(vk_instance, "vkDestroyImage");
        vkFreeMemory = (PFN_vkFreeMemory) vkGetInstanceProcAddr(vk_instance, "vkFreeMemory");
        vkCreateRenderPass = (PFN_vkCreateRenderPass) vkGetInstanceProcAddr(vk_instance, "vkCreateRenderPass");
        vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass) vkGetInstanceProcAddr(vk_instance, "vkCmdBeginRenderPass");
        vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass) vkGetInstanceProcAddr(vk_instance, "vkCmdEndRenderPass");
        vkCmdNextSubpass = (PFN_vkCmdNextSubpass) vkGetInstanceProcAddr(vk_instance, "vkCmdNextSubpass");
        vkCmdExecuteCommands = (PFN_vkCmdExecuteCommands) vkGetInstanceProcAddr(vk_instance, "vkCmdExecuteCommands");
        vkCreateImage = (PFN_vkCreateImage) vkGetInstanceProcAddr(vk_instance, "vkCreateImage");
        vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements) vkGetInstanceProcAddr(vk_instance, "vkGetImageMemoryRequirements");
        vkCreateImageView = (PFN_vkCreateImageView) vkGetInstanceProcAddr(vk_instance, "vkCreateImageView");
        vkDestroyImageView = (PFN_vkDestroyImageView) vkGetInstanceProcAddr(vk_instance, "vkDestroyImageView");
        vkCreateSemaphore = (PFN_vkCreateSemaphore) vkGetInstanceProcAddr(vk_instance, "vkCreateSemaphore");
        vkDestroySemaphore = (PFN_vkDestroySemaphore) vkGetInstanceProcAddr(vk_instance, "vkDestroySemaphore");
        vkCreateFence = (PFN_vkCreateFence) vkGetInstanceProcAddr(vk_instance, "vkCreateFence");
        vkDestroyFence = (PFN_vkDestroyFence) vkGetInstanceProcAddr(vk_instance, "vkDestroyFence");
        vkWaitForFences = (PFN_vkWaitForFences) vkGetInstanceProcAddr(vk_instance, "vkWaitForFences");
        vkResetFences = (PFN_vkResetFences) vkGetInstanceProcAddr(vk_instance, "vkResetFences");
        vkCreateCommandPool = (PFN_vkCreateCommandPool) vkGetInstanceProcAddr(vk_instance, "vkCreateCommandPool");
        vkDestroyCommandPool = (PFN_vkDestroyCommandPool) vkGetInstanceProcAddr(vk_instance, "vkDestroyCommandPool");
        vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers) vkGetInstanceProcAddr(vk_instance, "vkAllocateCommandBuffers");
        vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer) vkGetInstanceProcAddr(vk_instance, "vkBeginCommandBuffer");
        vkEndCommandBuffer = (PFN_vkEndCommandBuffer) vkGetInstanceProcAddr(vk_instance, "vkEndCommandBuffer");
        vkGetDeviceQueue = (PFN_vkGetDeviceQueue) vkGetInstanceProcAddr(vk_instance, "vkGetDeviceQueue");
        vkQueueSubmit = (PFN_vkQueueSubmit) vkGetInstanceProcAddr(vk_instance, "vkQueueSubmit");
        vkQueueWaitIdle = (PFN_vkQueueWaitIdle) vkGetInstanceProcAddr(vk_instance, "vkQueueWaitIdle");
        vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle) vkGetInstanceProcAddr(vk_instance, "vkDeviceWaitIdle");
        vkCreateFramebuffer = (PFN_vkCreateFramebuffer) vkGetInstanceProcAddr(vk_instance, "vkCreateFramebuffer");
        vkCreatePipelineCache = (PFN_vkCreatePipelineCache) vkGetInstanceProcAddr(vk_instance, "vkCreatePipelineCache");
        vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout) vkGetInstanceProcAddr(vk_instance, "vkCreatePipelineLayout");
        vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines) vkGetInstanceProcAddr(vk_instance, "vkCreateGraphicsPipelines");
        vkCreateComputePipelines = (PFN_vkCreateComputePipelines) vkGetInstanceProcAddr(vk_instance, "vkCreateComputePipelines");
        vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool) vkGetInstanceProcAddr(vk_instance, "vkCreateDescriptorPool");
        vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout) vkGetInstanceProcAddr(vk_instance, "vkCreateDescriptorSetLayout");
        vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets) vkGetInstanceProcAddr(vk_instance, "vkAllocateDescriptorSets");
        vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets) vkGetInstanceProcAddr(vk_instance, "vkFreeDescriptorSets");
        vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets) vkGetInstanceProcAddr(vk_instance, "vkUpdateDescriptorSets");
        vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets) vkGetInstanceProcAddr(vk_instance, "vkCmdBindDescriptorSets");
        vkCmdBindPipeline = (PFN_vkCmdBindPipeline) vkGetInstanceProcAddr(vk_instance, "vkCmdBindPipeline");
        vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers) vkGetInstanceProcAddr(vk_instance, "vkCmdBindVertexBuffers");
        vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer) vkGetInstanceProcAddr(vk_instance, "vkCmdBindIndexBuffer");
        vkCmdSetViewport = (PFN_vkCmdSetViewport) vkGetInstanceProcAddr(vk_instance, "vkCmdSetViewport");
        vkCmdSetScissor = (PFN_vkCmdSetScissor) vkGetInstanceProcAddr(vk_instance, "vkCmdSetScissor");
        vkCmdSetLineWidth = (PFN_vkCmdSetLineWidth) vkGetInstanceProcAddr(vk_instance, "vkCmdSetLineWidth");
        vkCmdSetDepthBias = (PFN_vkCmdSetDepthBias) vkGetInstanceProcAddr(vk_instance, "vkCmdSetDepthBias");
        vkCmdPushConstants = (PFN_vkCmdPushConstants) vkGetInstanceProcAddr(vk_instance, "vkCmdPushConstants");
        vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed) vkGetInstanceProcAddr(vk_instance, "vkCmdDrawIndexed");
        vkCmdDraw = (PFN_vkCmdDraw) vkGetInstanceProcAddr(vk_instance, "vkCmdDraw");
        vkCmdDrawIndexedIndirect = (PFN_vkCmdDrawIndexedIndirect) vkGetInstanceProcAddr(vk_instance, "vkCmdDrawIndexedIndirect");
        vkCmdDrawIndirect = (PFN_vkCmdDrawIndirect) vkGetInstanceProcAddr(vk_instance, "vkCmdDrawIndirect");
        vkCmdDispatch = (PFN_vkCmdDispatch) vkGetInstanceProcAddr(vk_instance, "vkCmdDispatch");
        vkDestroyPipeline = (PFN_vkDestroyPipeline) vkGetInstanceProcAddr(vk_instance, "vkDestroyPipeline");
        vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout) vkGetInstanceProcAddr(vk_instance, "vkDestroyPipelineLayout");
        vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout) vkGetInstanceProcAddr(vk_instance, "vkDestroyDescriptorSetLayout");
        vkDestroyDevice = (PFN_vkDestroyDevice) vkGetInstanceProcAddr(vk_instance, "vkDestroyDevice");
        vkDestroyInstance = (PFN_vkDestroyInstance) vkGetInstanceProcAddr(vk_instance, "vkDestroyInstance");
        vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool) vkGetInstanceProcAddr(vk_instance, "vkDestroyDescriptorPool");
        vkFreeCommandBuffers = (PFN_vkFreeCommandBuffers) vkGetInstanceProcAddr(vk_instance, "vkFreeCommandBuffers");
        vkDestroyRenderPass = (PFN_vkDestroyRenderPass) vkGetInstanceProcAddr(vk_instance, "vkDestroyRenderPass");
        vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer) vkGetInstanceProcAddr(vk_instance, "vkDestroyFramebuffer");
        vkDestroyShaderModule = (PFN_vkDestroyShaderModule) vkGetInstanceProcAddr(vk_instance, "vkDestroyShaderModule");
        vkDestroyPipelineCache = (PFN_vkDestroyPipelineCache) vkGetInstanceProcAddr(vk_instance, "vkDestroyPipelineCache");
        vkCreateQueryPool = (PFN_vkCreateQueryPool) vkGetInstanceProcAddr(vk_instance, "vkCreateQueryPool");
        vkDestroyQueryPool = (PFN_vkDestroyQueryPool) vkGetInstanceProcAddr(vk_instance, "vkDestroyQueryPool");
        vkGetQueryPoolResults = (PFN_vkGetQueryPoolResults) vkGetInstanceProcAddr(vk_instance, "vkGetQueryPoolResults");
        vkCmdBeginQuery = (PFN_vkCmdBeginQuery) vkGetInstanceProcAddr(vk_instance, "vkCmdBeginQuery");
        vkCmdEndQuery = (PFN_vkCmdEndQuery) vkGetInstanceProcAddr(vk_instance, "vkCmdEndQuery");
        vkCmdResetQueryPool = (PFN_vkCmdResetQueryPool) vkGetInstanceProcAddr(vk_instance, "vkCmdResetQueryPool");
        vkCmdCopyQueryPoolResults = (PFN_vkCmdCopyQueryPoolResults) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyQueryPoolResults");
        vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR) vkGetInstanceProcAddr(vk_instance, "vkCreateAndroidSurfaceKHR");
        vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR) vkGetInstanceProcAddr(vk_instance, "vkDestroySurfaceKHR");
        vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
        vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR) vkGetInstanceProcAddr(vk_instance, "vkDestroySwapchainKHR");
        vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR) vkGetInstanceProcAddr(vk_instance, "vkAcquireNextImageKHR");
        vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR) vkGetInstanceProcAddr(vk_instance, "vkCreateSwapchainKHR");
        vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR) vkGetInstanceProcAddr(vk_instance, "vkGetSwapchainImagesKHR");
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR) vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
        vkQueuePresentKHR = (PFN_vkQueuePresentKHR) vkGetInstanceProcAddr(vk_instance, "vkQueuePresentKHR");
        vkResetCommandBuffer = (PFN_vkResetCommandBuffer) vkGetInstanceProcAddr(vk_instance, "vkResetCommandBuffer");
        vkResetDescriptorPool = (PFN_vkResetDescriptorPool) vkGetInstanceProcAddr(vk_instance, "vkResetDescriptorPool");
        vkCmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer) vkGetInstanceProcAddr(vk_instance, "vkCmdCopyImageToBuffer");
        vkGetFenceStatus = (PFN_vkGetFenceStatus) vkGetInstanceProcAddr(vk_instance, "vkGetFenceStatus");
    }
}
