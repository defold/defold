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

#include <string.h>
#include <dlib/math.h>
#include <dlib/array.h>
#include <dlib/log.h>

#include "graphics_vulkan_defines.h"
#include "graphics_vulkan_private.h"

#include <stdio.h>

namespace dmGraphics
{
    static VkDebugUtilsMessengerEXT g_vk_debug_callback_handle = 0x0;

    static void GetExtensions(dmArray<VkExtensionProperties>& extensions)
    {
        uint32_t count = 0;
        VkResult result = vkEnumerateInstanceExtensionProperties(0, &count, 0);
        if (result != VK_SUCCESS) {
            dmLogInfo("vkEnumerateInstanceExtensionProperties (%d) failed: %d", __LINE__, result);
            return;
        }

        extensions.SetCapacity(count);
        extensions.SetSize(count);

        // Get the extensions
        result = vkEnumerateInstanceExtensionProperties(0, &count, extensions.Begin());
        if (result != VK_SUCCESS) {
            dmLogInfo("vkEnumerateInstanceExtensionProperties (%d) failed: %d", __LINE__, result);
            return;
        }
    }

    static bool IsSupported(const dmArray<VkExtensionProperties>& extensions, const char* name)
    {
        for (uint32_t i = 0; i < extensions.Size(); ++i)
        {
            const VkExtensionProperties& extension = extensions[i];
            if (strcmp(extension.extensionName, name) == 0)
                return true;
        }
        return false;
    }

    static bool AddIfSupported(const dmArray<VkExtensionProperties>& supported_extensions, const char* extension, dmArray<const char*>& extensions)
    {
        if (!IsSupported(supported_extensions, extension))
        {
            dmLogDebug("%s was not supported", extension);
            return false;
        }

        if (extensions.Full())
            extensions.OffsetCapacity(4);
        extensions.Push(extension);
        return true;
    }


    // This functions is invoked by the vulkan layer whenever
    // it has something to say, which can be info, warnings, errors and such.
    // Only used when validation layers are enabled.
    static VKAPI_ATTR VkBool32 VKAPI_CALL g_vk_debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        // Filter specific messages
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
        {
            // UNASSIGNED-CoreValidation-DrawState-ClearCmdBeforeDraw
            //   * We cannot force using render.clear in a specific way, so we will get
            //     spammed by the validation layers unless we filter this.
            static const char* ClearCmdBeforeDrawIdName = "UNASSIGNED-CoreValidation-DrawState-ClearCmdBeforeDraw";
            if (callbackData->pMessageIdName && strcmp(callbackData->pMessageIdName, ClearCmdBeforeDrawIdName) == 0)
            {
                return VK_FALSE;
            }
        }

        dmLogInfo("Validation Layer: %s", callbackData->pMessage);
        return VK_FALSE;
    }

    static bool ValidateRequiredExtensions(const dmArray<VkExtensionProperties>& extensions, const dmArray<const char*>& required_extensions)
    {
        bool result = true;
        for (uint32_t i = 0; i < required_extensions.Size(); ++i)
        {
            if (!IsSupported(extensions, required_extensions[i]))
            {
                dmLogError("Vulkan instance extension '%s' is not supported", required_extensions[i]);
                result = false;
            }
        }
        return result;
    }

    static bool GetValidationSupport(const char** validationLayers, const uint16_t validationLayersCount)
    {
        if (validationLayersCount == 0)
        {
            return false;
        }

        uint32_t vk_layer_count                = 0;
        VkLayerProperties* vk_layer_properties = 0;

        vkEnumerateInstanceLayerProperties(&vk_layer_count, 0);

        vk_layer_properties = new VkLayerProperties[vk_layer_count];

        vkEnumerateInstanceLayerProperties(&vk_layer_count, vk_layer_properties);

        bool all_layers_found = true;

        for(uint16_t ext=0; ext < validationLayersCount; ext++)
        {
            bool layer_found = false;
            for(uint32_t layer_index=0; layer_index < vk_layer_count; ++layer_index)
            {
                if (strcmp(vk_layer_properties[layer_index].layerName, validationLayers[ext]) == 0)
                {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found)
            {
                dmLogError("Vulkan validation layer '%s' is not supported", validationLayers[ext]);
                all_layers_found = false;
            }
        }

         delete[] vk_layer_properties;


    #ifdef __MACH__
        if (!all_layers_found)
        {
            dmLogError("Vulkan validation layers have been requested, but none was found. OSX requires that the engine has been built with the '--with-vulkan-validation' build switch.");
        }
    #endif

         return all_layers_found;
    }

    void DestroyInstance(VkInstance* vkInstance)
    {
        assert(vkInstance);

        if (g_vk_debug_callback_handle != 0x0)
        {
            PFN_vkDestroyDebugUtilsMessengerEXT func_ptr =
                (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(*vkInstance, "vkDestroyDebugUtilsMessengerEXT");

            if (func_ptr != NULL)
            {
                func_ptr(*vkInstance, g_vk_debug_callback_handle, 0);
            }
        }
        g_vk_debug_callback_handle = 0;

        vkDestroyInstance(*vkInstance, 0);
        *vkInstance = VK_NULL_HANDLE;
    }

    VkResult CreateInstance(VkInstance* vkInstanceOut,
                            const char** extensionNames, uint16_t extensionNameCount,
                            const char** validationLayers, uint16_t validationLayerCount,
                            const char** validationLayerExtensions, uint16_t validationLayerExtensionCount)
    {
        VkApplicationInfo    vk_application_info     = {};
        VkInstanceCreateInfo vk_instance_create_info = {};
        dmArray<const char*> vk_required_extensions;

        vk_application_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        vk_application_info.pApplicationName   = "Defold";
        vk_application_info.pEngineName        = "Defold Engine";
        vk_application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        vk_application_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        vk_application_info.apiVersion         = VK_API_VERSION_1_0;

        vk_required_extensions.SetCapacity(extensionNameCount + validationLayerExtensionCount);

        for (uint16_t i = 0; i < extensionNameCount; ++i)
        {
            vk_required_extensions.Push(extensionNames[i]);
        }

        int32_t enabled_layer_count = 0;
        dmArray<VkExtensionProperties> extensions;
        GetExtensions(extensions);

        AddIfSupported(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, vk_required_extensions);
        AddIfSupported(extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, vk_required_extensions); // VK_EXT_fragment_shader_interlock

    #ifdef __MACH__
        static const char* VK_EXT_metal_objects_str = "VK_EXT_metal_objects";
        AddIfSupported(extensions, VK_EXT_metal_objects_str, vk_required_extensions);
    #else
        static const char* VK_KHR_portability_enumeration_str = "VK_KHR_portability_enumeration";
        AddIfSupported(extensions, VK_KHR_portability_enumeration_str, vk_required_extensions);
    #endif

        if (validationLayerCount > 0)
        {
            if (GetValidationSupport(validationLayers, validationLayerCount))
            {
                enabled_layer_count = validationLayerCount;

                for (uint16_t i=0; i < validationLayerExtensionCount; ++i)
                {
                    AddIfSupported(extensions, validationLayerExtensions[i], vk_required_extensions);
                }
            }
        }

        if (!ValidateRequiredExtensions(extensions, vk_required_extensions))
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        vk_instance_create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vk_instance_create_info.pApplicationInfo        = &vk_application_info;
        vk_instance_create_info.pNext                   = NULL;
        vk_instance_create_info.enabledExtensionCount   = (uint32_t) vk_required_extensions.Size();
        vk_instance_create_info.ppEnabledExtensionNames = vk_required_extensions.Begin();
        vk_instance_create_info.enabledLayerCount       = enabled_layer_count;
        vk_instance_create_info.ppEnabledLayerNames     = validationLayers;
        vk_instance_create_info.flags                   = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        VkResult res = vkCreateInstance(&vk_instance_create_info, 0, vkInstanceOut);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        if (enabled_layer_count > 0)
        {
            assert(g_vk_debug_callback_handle == 0);
            VkDebugUtilsMessengerCreateInfoEXT vk_debug_messenger_create_info = {};

            vk_debug_messenger_create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            vk_debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

            vk_debug_messenger_create_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

            vk_debug_messenger_create_info.pfnUserCallback = g_vk_debug_callback;

            PFN_vkCreateDebugUtilsMessengerEXT func_ptr =
                (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(*vkInstanceOut, "vkCreateDebugUtilsMessengerEXT");

            if (func_ptr)
            {
                if(func_ptr(*vkInstanceOut, &vk_debug_messenger_create_info, 0, &g_vk_debug_callback_handle) != VK_SUCCESS)
                {
                    dmLogError("Vulkan validation requested but could not create validation layer callback, \
                        call to vkCreateDebugUtilsMessengerEXT failed");
                }
            }
            else
            {
                dmLogError("Vulkan validation requested but could not create validation layer callback, \
                    could not find 'vkCreateDebugUtilsMessengerEXT'");
            }
        }

        return VK_SUCCESS;
    }
}
