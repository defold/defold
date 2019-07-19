#include <string.h>

#include <dlib/array.h>
#include <dlib/log.h>

#include "graphics_vulkan_defines.h"
#include "graphics_vulkan.h"

namespace dmGraphics
{
    static bool ValidateRequiredExtensions(const char** extensionNames, uint8_t extensionCount)
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
            return false;
        }

        uint8_t extensions_found = 0;

        for (uint32_t req_ext = 0; req_ext < extensionCount; req_ext++)
        {
            uint8_t extensions_found_save = extensions_found;
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
                dmLogError("Required extension '%s' is not supported.", req_ext_name);
            }
        }

        return extensions_found == extensionCount;
    }

    static bool GetValidationSupport(const char** validationLayers, uint8_t validationLayersCount)
    {
        uint32_t vk_layer_count                = 0;
        VkLayerProperties* vk_layer_properties = 0;

        vkEnumerateInstanceLayerProperties(&vk_layer_count, 0);

        vk_layer_properties = new VkLayerProperties[vk_layer_count];

        vkEnumerateInstanceLayerProperties(&vk_layer_count, vk_layer_properties);

        bool all_layers_found = true;

        for(uint8_t ext=0; ext < validationLayersCount; ext++)
        {
            bool layer_found = false;

            if (validationLayers[ext] == NULL)
            {
                break;
            }

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
                dmLogError("Validation Layer '%s' is not supported", validationLayers[ext]);
                all_layers_found = false;
            }
        }

         delete[] vk_layer_properties;

         return all_layers_found;
    }

    bool CreateVkInstance(bool enableValidation)
    {
        VkInstance           vk_instance;
        VkApplicationInfo    vk_application_info     = {};
        VkInstanceCreateInfo vk_instance_create_info = {};
        dmArray<const char*> vk_required_extensions;

        vk_application_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        vk_application_info.pApplicationName   = "Defold";
        vk_application_info.pEngineName        = "Defold Engine";
        vk_application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        vk_application_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        vk_application_info.apiVersion         = VK_API_VERSION_1_0;

        vk_required_extensions.SetCapacity(2);

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
        const char* enabled_layer_names = 0;

        if (enableValidation)
        {
            enabled_layer_names = "VK_LAYER_LUNARG_standard_validation";

            if (GetValidationSupport(&enabled_layer_names, 1))
            {
                enabled_layer_count = 1;
                vk_required_extensions.Push(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            }
        }

        ValidateRequiredExtensions(vk_required_extensions.Begin(), vk_required_extensions.Size());

        vk_instance_create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vk_instance_create_info.pApplicationInfo        = &vk_application_info;
        vk_instance_create_info.pNext                   = NULL;
        vk_instance_create_info.enabledExtensionCount   = (uint32_t) vk_required_extensions.Size();
        vk_instance_create_info.ppEnabledExtensionNames = vk_required_extensions.Begin();
        vk_instance_create_info.enabledLayerCount       = enabled_layer_count;
        vk_instance_create_info.ppEnabledLayerNames     = &enabled_layer_names;

        VkResult res = vkCreateInstance(&vk_instance_create_info, 0, &vk_instance);

        if (res != VK_SUCCESS)
        {
            return false;
        }

        return true;
    }
}
