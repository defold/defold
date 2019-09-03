#include <string.h>

#include <dlib/array.h>
#include <dlib/log.h>

#include "graphics_vulkan_defines.h"
#include "graphics_vulkan.h"

namespace dmGraphics
{
    static VkDebugUtilsMessengerEXT g_vk_debug_callback_handle = NULL;

    // This functions is invoked by the vulkan layer whenever
    // it has something to say, which can be info, warnings, errors and such.
    // Only used when validation layers are enabled.
    static VKAPI_ATTR VkBool32 VKAPI_CALL g_vk_debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        dmLogInfo("Validation Layer: %s", pCallbackData->pMessage);
        return VK_FALSE;
    }

    static bool ValidateRequiredExtensions(const char** extensionNames, const uint8_t extensionCount)
    {
        uint32_t vk_extension_count              = 0;
        VkExtensionProperties* vk_extension_list = 0;

        if (vkEnumerateInstanceExtensionProperties(NULL, &vk_extension_count, NULL) != VK_SUCCESS)
        {
            return false;
        }

        vk_extension_list = new VkExtensionProperties[vk_extension_count];

        if (vkEnumerateInstanceExtensionProperties(NULL, &vk_extension_count, vk_extension_list) != VK_SUCCESS)
        {
            delete[] vk_extension_list;
            return false;
        }

        uint16_t extensions_found = 0;

        for (uint32_t req_ext = 0; req_ext < extensionCount; req_ext++)
        {
            uint16_t extensions_found_save = extensions_found;
            const char* req_ext_name = extensionNames[req_ext];
            for (uint32_t vk_ext = 0; vk_ext < vk_extension_count; vk_ext++)
            {
                const char* vk_ext_name = vk_extension_list[vk_ext].extensionName;
                if (strcmp(req_ext_name, vk_ext_name) == 0)
                {
                    extensions_found++;
                    break;
                }
            }

            if (extensions_found_save == extensions_found)
            {
                dmLogError("Vulkan instance extension '%s' is not supported.", req_ext_name);
            }
        }

        delete[] vk_extension_list;

        return extensions_found == extensionCount;
    }

    static bool GetValidationSupport(const char** validationLayers, const uint8_t validationLayersCount)
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

        for(uint8_t ext=0; ext < validationLayersCount && validationLayers[ext] != NULL; ext++)
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

         return all_layers_found;
    }

    void DestroyInstance(VkInstance* vkInstance)
    {
        assert(vkInstance);

        if (g_vk_debug_callback_handle != NULL)
        {
            PFN_vkDestroyDebugUtilsMessengerEXT func_ptr =
                (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(*vkInstance, "vkDestroyDebugUtilsMessengerEXT");

            if (func_ptr != NULL)
            {
                func_ptr(*vkInstance, g_vk_debug_callback_handle, 0);
            }
        }

        vkDestroyInstance(*vkInstance, 0);
        *vkInstance = VK_NULL_HANDLE;
    }

    VkResult CreateInstance(VkInstance* vkInstanceOut, const char** validationLayers, const uint8_t validationLayerCount)
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

        vk_required_extensions.SetCapacity(3);
        vk_required_extensions.Push(VK_KHR_SURFACE_EXTENSION_NAME);

    #if defined(VK_USE_PLATFORM_WIN32_KHR)
        vk_required_extensions.Push(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    #elif defined(VK_USE_PLATFORM_ANDROID_KHR)
        vk_required_extensions.Push(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
    #elif defined(VK_USE_PLATFORM_XCB_KHR)
        vk_required_extensions.Push(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
    #elif defined(VK_USE_PLATFORM_MACOS_MVK)
        vk_required_extensions.Push(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
    #elif defined(VK_USE_PLATFORM_IOS_MVK)
        vk_required_extensions.Push(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
    #elif defined(VK_USE_PLATFORM_METAL_EXT)
        vk_required_extensions.Push(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    #endif

        int enabled_layer_count = 0;

        if (validationLayerCount > 0)
        {
            if (GetValidationSupport(validationLayers, validationLayerCount))
            {
                for (; enabled_layer_count < validationLayerCount; ++enabled_layer_count)
                {
                    vk_required_extensions.Push(validationLayers[enabled_layer_count]);
                }
            }
        }

        if (!ValidateRequiredExtensions(vk_required_extensions.Begin(), vk_required_extensions.Size()))
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
