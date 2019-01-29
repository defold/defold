#include <string.h>
#include <assert.h>
#include <vectormath/cpp/vectormath_aos.h>

// TODO: hack!
#define VK_USE_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <graphics/glfw/glfw_native.h>

#include "../graphics.h"
#include "../graphics_native.h"
#include "graphics_vulkan.h"
#include "../null/glsl_uniform_parser.h"

#include "graphics_vulkan_platform.h"

using namespace Vectormath::Aos;

uint64_t g_DrawCount = 0;
uint64_t g_Flipped = 0;

// Used only for tests
bool g_ForceFragmentReloadFail = false;
bool g_ForceVertexReloadFail = false;

namespace dmGraphics
{
    namespace Vulkan
    {
        // This is not an enum. Queue families are index values
        #define QUEUE_FAMILY_INVALID -1
        #define VK_ZERO_MEMORY(ptr,size) memset((void*)ptr,0,size)

        const int g_enable_validation_layers = 1;
        const char* g_validation_layers[]    = {
        #ifdef __MACH__
            "MoltenVK",
        #else
            "VK_LAYER_LUNARG_standard_validation",
        #endif
            NULL
        };

        const char* g_device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            NULL
        };

        struct Context
        {
            VkInstance                     m_Instance;
            VkPhysicalDevice               m_PhysicalDevice;
            VkDevice                       m_LogicalDevice;
            VkQueue                        m_PresentQueue;
            VkQueue                        m_GraphicsQueue;
            VkSwapchainKHR                 m_SwapChain;
            VkFormat                       m_SwapChainImageFormat;
            VkExtent2D                     m_SwapChainImageExtent;
            VkRenderPass                   m_DefaultRenderPass;

            VkSurfaceKHR                   m_Surface;
            VkApplicationInfo              m_ApplicationInfo;
            VkDebugUtilsMessengerEXT       m_DebugCallback;

            dmArray<VkImage>               m_SwapChainImages;
            dmArray<VkImageView>           m_SwapChainImageViews;
            dmArray<VkExtensionProperties> m_Extensions;
        } g_vk_context;

        struct QueueFamily
        {
            int32_t m_GraphicsFamily;
            int32_t m_PresentFamily;
        };

        struct SwapChainSupport
        {
            VkSurfaceCapabilitiesKHR    m_SurfaceCapabilities;
            dmArray<VkSurfaceFormatKHR> m_Formats;
            dmArray<VkPresentModeKHR>   m_PresentModes;
        };

        // This functions is invoked by the vulkan layer whenever
        // it has something to say, which can be info, warnings, errors and such.
        // Only used in debug.
        static VKAPI_ATTR VkBool32 VKAPI_CALL g_vk_debug_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT             messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData)
        {
            dmLogInfo("Validation Layer: %s", pCallbackData->pMessage);

            return VK_FALSE;
        }

        static inline uint32_t CountStringArray(const char** arr)
        {
            int str_c = -1;
            while(arr[++str_c]);
            return str_c;
        }

        static inline bool IsInDeviceExtensionList(const char* ext_to_find)
        {
            int g_ext_c = -1;
            while(g_device_extensions[++g_ext_c])
            {
                if (dmStrCaseCmp(ext_to_find, g_device_extensions[g_ext_c]) == 0)
                {
                    return true;
                }
            }

            return false;
        }

        void SetInstanceExtensionList()
        {
            uint32_t extension_count             = 0;
            VkExtensionProperties* extension_ptr = 0;

            vkEnumerateInstanceExtensionProperties(0, &extension_count, 0);

            g_vk_context.m_Extensions.SetCapacity(extension_count);
            g_vk_context.m_Extensions.SetSize(extension_count);

            extension_ptr = g_vk_context.m_Extensions.Begin();

            vkEnumerateInstanceExtensionProperties(0, &extension_count, extension_ptr);

            for(unsigned int i=0; i < extension_count; i++ )
            {
                dmLogInfo("Extension Enabled: %s", g_vk_context.m_Extensions[i].extensionName);
            }
        }

        bool GetValidationSupport()
        {
            uint32_t layer_count          = 0;
            VkLayerProperties* layer_list = 0;

            vkEnumerateInstanceLayerProperties(&layer_count, 0);

            layer_list = new VkLayerProperties[layer_count];

            vkEnumerateInstanceLayerProperties(&layer_count, layer_list);

            bool all_layers_found = true;

            for(uint32_t ext=0;;ext++)
            {
                bool layer_found = false;

                if (g_validation_layers[ext] == NULL)
                {
                    break;
                }

                for(uint32_t layer_index=0; layer_index < layer_count; ++layer_index)
                {
                    if (strcmp(layer_list[layer_index].layerName, g_validation_layers[ext]) == 0)
                    {
                        layer_found = true;
                        break;
                    }
                }

                if (!layer_found)
                {
                    dmLogError("Validation Layer '%s' is not supported", g_validation_layers[ext]);
                    all_layers_found = false;
                }
            }

            delete[] layer_list;

            return all_layers_found;
        }

        bool GetRequiredInstanceExtensions(dmArray<const char*>& extensionListOut)
        {
            uint32_t extension_count                 = 0;
            VkExtensionProperties* vk_extension_list = 0;

            if (vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL) != VK_SUCCESS)
            {
                return false;
            }

            vk_extension_list = new VkExtensionProperties[extension_count];

            extensionListOut.SetCapacity(extension_count);

            for (uint32_t i = 0; i < extension_count; i++)
            {
                if (strcmp(VK_KHR_SURFACE_EXTENSION_NAME, vk_extension_list[i].extensionName) == 0)
                {
                    extensionListOut.Push(VK_KHR_SURFACE_EXTENSION_NAME);
                }

        // need similar ifdefs for other platforms here..
        #if defined(__MACH__)
                if (strcmp(VK_MVK_MACOS_SURFACE_EXTENSION_NAME, vk_extension_list[i].extensionName) == 0)
                {
                    extensionListOut.Push(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
                }
        #endif

                if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, vk_extension_list[i].extensionName) == 0)
                {
                    if (g_enable_validation_layers) {
                        extensionListOut.Push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                    }
                }
            }

            delete[] vk_extension_list;

            return true;
        }

        // All GPU operations are pushed to various queues. The physical device can have multiple
        // queues with different properties supported, so we need to find a combination of queues
        // that will work for our needs. Note that the present queue might not be the same queue as the
        // graphics queue. The graphics queue support is needed to do graphics operations at all.
        QueueFamily GetQueueFamily(VkPhysicalDevice device)
        {
            QueueFamily vk_family_selected = { QUEUE_FAMILY_INVALID, QUEUE_FAMILY_INVALID };
            uint32_t queue_family_count    = 0;

            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, 0);

            if (queue_family_count == 0)
            {
                return vk_family_selected;
            }

            VkQueueFamilyProperties* vk_family_list = new VkQueueFamilyProperties[queue_family_count];
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, vk_family_list);

            for (uint32_t i=0; i < queue_family_count; i++)
            {
                VkQueueFamilyProperties candidate = vk_family_list[i];

                if (candidate.queueCount > 0 && candidate.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    vk_family_selected.m_GraphicsFamily = i;
                }

                uint32_t present_support = 0;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, g_vk_context.m_Surface, &present_support);

                if (candidate.queueCount && present_support)
                {
                    vk_family_selected.m_PresentFamily = i;
                }

                if (vk_family_selected.m_GraphicsFamily >= 0 && vk_family_selected.m_PresentFamily >= 0)
                {
                    break;
                }
            }

            delete[] vk_family_list;

            return vk_family_selected;
        }

        bool IsExtensionsSupported(VkPhysicalDevice device)
        {
            uint32_t ext_available_count = 0;
            int32_t  ext_to_enable_count = 0;

            while(g_device_extensions[++ext_to_enable_count]);

            vkEnumerateDeviceExtensionProperties(device, 0, &ext_available_count, 0);

            VkExtensionProperties* ext_properties = new VkExtensionProperties[ext_available_count];

            vkEnumerateDeviceExtensionProperties(device, 0, &ext_available_count, ext_properties);

            for (uint32_t i=0; i < ext_available_count; i++)
            {
                VkExtensionProperties ext_property = ext_properties[i];

                if (IsInDeviceExtensionList(ext_property.extensionName))
                {
                    ext_to_enable_count--;
                }
            }

            delete[] ext_properties;

            return ext_to_enable_count == 0;
        }

        bool SetLogicalDevice()
        {
            QueueFamily vk_queue_family = GetQueueFamily(g_vk_context.m_PhysicalDevice);

            // NOTE: Different queues can have different priority from [0..1], but
            //       we only have a single queue right now so set to 1.0f
            float queue_priority    = 1.0f;
            int queue_family_set[2] = { QUEUE_FAMILY_INVALID, QUEUE_FAMILY_INVALID };
            int queue_family_c      = 0;

            VkDeviceQueueCreateInfo queue_create_info_list[2];
            VK_ZERO_MEMORY(queue_create_info_list, sizeof(queue_create_info_list));

            if (vk_queue_family.m_PresentFamily != vk_queue_family.m_GraphicsFamily)
            {
                queue_family_set[0] = vk_queue_family.m_PresentFamily;
                queue_family_set[1] = vk_queue_family.m_GraphicsFamily;
            }
            else
            {
                queue_family_set[0] = vk_queue_family.m_PresentFamily;
            }

            while(queue_family_set[queue_family_c] != QUEUE_FAMILY_INVALID)
            {
                int queue_family_index                         = queue_family_set[queue_family_c];
                VkDeviceQueueCreateInfo& vk_queue_create_info = queue_create_info_list[queue_family_c];

                vk_queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                vk_queue_create_info.queueFamilyIndex = queue_family_index;
                vk_queue_create_info.queueCount       = 1;
                vk_queue_create_info.pQueuePriorities = &queue_priority;

                queue_family_c++;
            }

            if (!queue_family_c)
            {
                return false;
            }

            VkDeviceCreateInfo vk_device_create_info;
            VK_ZERO_MEMORY(&vk_device_create_info,sizeof(vk_device_create_info));

            vk_device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            vk_device_create_info.pQueueCreateInfos       = queue_create_info_list;
            vk_device_create_info.queueCreateInfoCount    = queue_family_c;
            vk_device_create_info.pEnabledFeatures        = 0;
            vk_device_create_info.enabledExtensionCount   = CountStringArray(g_device_extensions);
            vk_device_create_info.ppEnabledExtensionNames = g_device_extensions;

            if (g_enable_validation_layers)
            {
                vk_device_create_info.enabledLayerCount   = CountStringArray(g_validation_layers);
                vk_device_create_info.ppEnabledLayerNames = g_validation_layers;
            }
            else
            {
                vk_device_create_info.enabledLayerCount = 0;
            }

            if (vkCreateDevice(g_vk_context.m_PhysicalDevice, &vk_device_create_info, 0, &g_vk_context.m_LogicalDevice) != VK_SUCCESS)
            {
                return false;
            }

            vkGetDeviceQueue(g_vk_context.m_LogicalDevice, vk_queue_family.m_GraphicsFamily, 0, &g_vk_context.m_GraphicsQueue);
            vkGetDeviceQueue(g_vk_context.m_LogicalDevice, vk_queue_family.m_PresentFamily, 0, &g_vk_context.m_PresentQueue);

            return true;
        }

        // Extract all swap chain formats and present modes. We use these tables to select the most suitable modes later.
        void GetSwapChainSupport(VkPhysicalDevice device, SwapChainSupport& swapChain)
        {
            uint32_t format_count        = 0;
            uint32_t present_modes_count = 0;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, g_vk_context.m_Surface, &swapChain.m_SurfaceCapabilities);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_vk_context.m_Surface, &format_count, 0);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_vk_context.m_Surface, &present_modes_count, 0);

            if (format_count > 0)
            {
                swapChain.m_Formats.SetCapacity(format_count);
                swapChain.m_Formats.SetSize(format_count);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_vk_context.m_Surface, &format_count, swapChain.m_Formats.Begin());
            }

            if (present_modes_count > 0)
            {
                swapChain.m_PresentModes.SetCapacity(present_modes_count);
                swapChain.m_PresentModes.SetSize(present_modes_count);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_vk_context.m_Surface, &present_modes_count, swapChain.m_PresentModes.Begin());
            }
        }

        bool IsDeviceCompatible(VkPhysicalDevice device)
        {
            QueueFamily vk_queue_family  = GetQueueFamily(device);
            bool is_extensions_supported = IsExtensionsSupported(device);
            bool is_swap_chain_supported = false;

            if (is_extensions_supported)
            {
                SwapChainSupport swap_chain_support;
                GetSwapChainSupport(device, swap_chain_support);

                is_swap_chain_supported  = swap_chain_support.m_Formats.Size() > 0;
                is_swap_chain_supported &= swap_chain_support.m_PresentModes.Size() > 0;
            }

            return vk_queue_family.m_GraphicsFamily != QUEUE_FAMILY_INVALID &&
                   vk_queue_family.m_PresentFamily  != QUEUE_FAMILY_INVALID &&
                   is_extensions_supported && is_swap_chain_supported;
        }

        bool SetPhysicalDevice()
        {
            VkPhysicalDevice vk_selected_device = VK_NULL_HANDLE;
            uint32_t device_count               = 0;

            // Get number of present devices
            vkEnumeratePhysicalDevices(g_vk_context.m_Instance, &device_count, 0);

            if (device_count == 0)
            {
                return false;
            }

            VkPhysicalDevice* vk_device_list = new VkPhysicalDevice[device_count];

            vkEnumeratePhysicalDevices(g_vk_context.m_Instance, &device_count, vk_device_list);

            // Iterate list of possible physical devices and pick the first that is suitable
            // There can be many GPUs present, but we can only support one.
            for (uint32_t i=0; i < device_count; i++)
            {
                VkPhysicalDevice vk_candidate = vk_device_list[i];

                if (IsDeviceCompatible(vk_candidate))
                {
                    vk_selected_device = vk_candidate;
                    break;
                }
            }

            delete[] vk_device_list;

            if (vk_selected_device == VK_NULL_HANDLE)
            {
                return false;
            }

            g_vk_context.m_PhysicalDevice = vk_selected_device;

            return true;
        }

        VkSurfaceFormatKHR GetSuitableSwapChainFormat(dmArray<VkSurfaceFormatKHR>& availableFormats)
        {
            if (availableFormats.Size() > 0)
            {
                VkSurfaceFormatKHR format_head = availableFormats[0];

                VkSurfaceFormatKHR format_wanted;
                format_wanted.format     = VK_FORMAT_B8G8R8A8_UNORM;
                format_wanted.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

                // The window surface has no preferred format so we can return whatever we want here.
                if (format_head.format == VK_FORMAT_UNDEFINED)
                {
                    return format_wanted;
                }

                // Try to find a suitable format candidate that matches our
                // suggested format. If not, just return the first.
                // NOTE: We could get the "closest" format here aswell,
                //       but not sure how to rank formats really.
                for (uint32_t i=0; i < availableFormats.Size(); i++)
                {
                    VkSurfaceFormatKHR format_candidate = availableFormats[i];

                    if (format_candidate.format     == format_wanted.format &&
                        format_candidate.colorSpace == format_wanted.colorSpace)
                    {
                        return format_candidate;
                    }
                }

                return format_head;
            }
            else
            {
                VkSurfaceFormatKHR format_null;
                VK_ZERO_MEMORY(&format_null, sizeof(format_null));
                return format_null;
            }
        }

        // Present modes: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPresentModeKHR.html
        VkPresentModeKHR GetSuitablePresentMode(dmArray<VkPresentModeKHR>& presentModes)
        {
            VkPresentModeKHR mode_most_suitable = VK_PRESENT_MODE_FIFO_KHR;

            for (uint32_t i=0; i < presentModes.Size(); i++)
            {
                VkPresentModeKHR mode_candidate = presentModes[i];

                if (mode_candidate == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    return mode_candidate;
                }
                else if (mode_candidate == VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    mode_most_suitable = mode_candidate;
                }
            }

            return mode_most_suitable;
        }

        VkExtent2D GetSwapChainExtent(VkSurfaceCapabilitiesKHR capabilities, uint32_t wantedWidth, uint32_t wantedHeight)
        {
            VkExtent2D extent_current = capabilities.currentExtent;

            if (extent_current.width == 0xFFFFFFFF || extent_current.width == 0xFFFFFFFF)
            {
                VkExtent2D extent_wanted;
                // Clamp swap buffer extent to our wanted width / height.
                // TODO: Figure out how will this change when running in fullscreen mode?
                extent_wanted.width  = dmMath::Clamp<uint32_t>(wantedWidth, 0, extent_current.width);
                extent_wanted.height = dmMath::Clamp<uint32_t>(wantedHeight, 0, extent_current.height);

                return extent_wanted;
            }
            else
            {
                return extent_current;
            }
        }

        bool CreateSwapChain(uint32_t wantedWidth, uint32_t wantedHeight)
        {
            SwapChainSupport swap_chain_support;
            GetSwapChainSupport(g_vk_context.m_PhysicalDevice, swap_chain_support);

            VkSurfaceFormatKHR swap_chain_format       = GetSuitableSwapChainFormat(swap_chain_support.m_Formats);
            VkPresentModeKHR   swap_chain_present_mode = GetSuitablePresentMode(swap_chain_support.m_PresentModes);
            VkExtent2D         swap_chain_extent       = GetSwapChainExtent(swap_chain_support.m_SurfaceCapabilities, wantedWidth, wantedHeight);

            // From the docs:
            // ==============
            // minImageCount: the minimum number of images the specified device supports for a swapchain created for the surface, and will be at least one.
            // maxImageCount: the maximum number of images the specified device supports for a swapchain created for the surface,
            //  and will be either 0, or greater than or equal to minImageCount. A value of 0 means that there is no limit on the number of images,
            //  though there may be limits related to the total amount of memory used by presentable images.

            // The +1 here is to add a little bit of headroom, from vulkan-tutorial.com:
            // "we may sometimes have to wait on the driver to complete internal operations
            // before we can acquire another image to render to"
            uint32_t swap_chain_image_count = swap_chain_support.m_SurfaceCapabilities.minImageCount + 1;

            if (swap_chain_support.m_SurfaceCapabilities.maxImageCount > 0)
            {
                swap_chain_image_count = dmMath::Clamp(swap_chain_image_count,
                    swap_chain_support.m_SurfaceCapabilities.minImageCount,
                    swap_chain_support.m_SurfaceCapabilities.maxImageCount);
            }

            VkSwapchainCreateInfoKHR swap_chain_create_info;
            VK_ZERO_MEMORY(&swap_chain_create_info,sizeof(swap_chain_create_info));

            swap_chain_create_info.sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swap_chain_create_info.surface = g_vk_context.m_Surface;

            swap_chain_create_info.minImageCount    = swap_chain_image_count;
            swap_chain_create_info.imageFormat      = swap_chain_format.format;
            swap_chain_create_info.imageColorSpace  = swap_chain_format.colorSpace;
            swap_chain_create_info.imageExtent      = swap_chain_extent;

            // imageArrayLayers: the number of views in a multiview/stereo surface.
            // For non-stereoscopic-3D applications, this value is 1
            swap_chain_create_info.imageArrayLayers = 1;
            swap_chain_create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            QueueFamily queue_family = GetQueueFamily(g_vk_context.m_PhysicalDevice);

            // Move queue indices over to uint32_t array
            uint32_t queue_family_indices[2] = {queue_family.m_GraphicsFamily, queue_family.m_PresentFamily};

            // If we have different queues for the different types, we just stick with
            // concurrent mode. An option here would be to do ownership transfers:
            // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#synchronization-queue-transfers
            if (queue_family.m_GraphicsFamily != queue_family.m_PresentFamily)
            {
                swap_chain_create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
                swap_chain_create_info.queueFamilyIndexCount = 2;
                swap_chain_create_info.pQueueFamilyIndices   = queue_family_indices;
            }
            else
            {
                // This mode is best for performance and can be used with no penality
                // if we are using the same queue for everything.
                swap_chain_create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
                swap_chain_create_info.queueFamilyIndexCount = 0;
                swap_chain_create_info.pQueueFamilyIndices   = 0;
            }

            // The preTransform field can be used to rotate the swap chain when presenting
            swap_chain_create_info.preTransform   = swap_chain_support.m_SurfaceCapabilities.currentTransform;
            swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            swap_chain_create_info.presentMode    = swap_chain_present_mode;
            swap_chain_create_info.clipped        = VK_TRUE;
            swap_chain_create_info.oldSwapchain   = VK_NULL_HANDLE;

            if (vkCreateSwapchainKHR(g_vk_context.m_LogicalDevice,
                &swap_chain_create_info, 0, &g_vk_context.m_SwapChain) != VK_SUCCESS)
            {
                return false;
            }

            vkGetSwapchainImagesKHR(g_vk_context.m_LogicalDevice, g_vk_context.m_SwapChain, &swap_chain_image_count, 0);

            g_vk_context.m_SwapChainImages.SetCapacity(swap_chain_image_count);
            g_vk_context.m_SwapChainImages.SetSize(swap_chain_image_count);

            vkGetSwapchainImagesKHR(g_vk_context.m_LogicalDevice, g_vk_context.m_SwapChain, &swap_chain_image_count, g_vk_context.m_SwapChainImages.Begin());

            g_vk_context.m_SwapChainImageFormat = swap_chain_format.format;
            g_vk_context.m_SwapChainImageExtent = swap_chain_extent;

            return true;
        }

        bool CreateSwapChainImageViews()
        {
            uint32_t num_views = g_vk_context.m_SwapChainImages.Size();

            g_vk_context.m_SwapChainImageViews.SetCapacity(num_views);
            g_vk_context.m_SwapChainImageViews.SetSize(num_views);

            for (uint32_t i=0; i < num_views; i++)
            {
                VkImageViewCreateInfo create_info_image_view;
                VK_ZERO_MEMORY(&create_info_image_view, sizeof(create_info_image_view));

                create_info_image_view.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                create_info_image_view.image        = g_vk_context.m_SwapChainImages[i];
                create_info_image_view.viewType     = VK_IMAGE_VIEW_TYPE_2D;
                create_info_image_view.format       = g_vk_context.m_SwapChainImageFormat;
                create_info_image_view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info_image_view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info_image_view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info_image_view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

                create_info_image_view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                create_info_image_view.subresourceRange.baseMipLevel   = 0;
                create_info_image_view.subresourceRange.levelCount     = 1;
                create_info_image_view.subresourceRange.baseArrayLayer = 0;
                create_info_image_view.subresourceRange.layerCount     = 1;

                if (vkCreateImageView(g_vk_context.m_LogicalDevice,
                    &create_info_image_view, 0, &g_vk_context.m_SwapChainImageViews[i]) != VK_SUCCESS)
                {
                    g_vk_context.m_SwapChainImageViews.SetCapacity(0);
                    g_vk_context.m_SwapChainImageViews.SetSize(0);
                    return false;
                }
            }

            return true;
        }

        VkFormat GetSupportedFormat(VkFormat* formatCandidates, uint32_t numFormatCandidates,
            VkImageTiling tilingType, VkFormatFeatureFlags formatFlags)
        {
            #define HAS_FLAG(v,flag) ((v & flag) == flag)

            for (uint32_t i=0; i < numFormatCandidates; i++)
            {
                VkFormatProperties format_properties;
                VkFormat formatCandidate = formatCandidates[i];

                vkGetPhysicalDeviceFormatProperties(g_vk_context.m_PhysicalDevice, formatCandidate, &format_properties);

                if ((tilingType == VK_IMAGE_TILING_LINEAR && HAS_FLAG(format_properties.linearTilingFeatures, formatFlags)) ||
                    (tilingType == VK_IMAGE_TILING_OPTIMAL && HAS_FLAG(format_properties.optimalTilingFeatures, formatFlags)))
                {
                    return formatCandidate;
                }
            }

            #undef HAS_FLAG

            return VK_FORMAT_UNDEFINED;
        }

        bool CreateDefaultRenderPass()
        {
            // Depth formats are optional, so we need to query
            // what available formats we have.
            VkFormat format_depth_list[] = {
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM
            };

            VkFormat format_color = g_vk_context.m_SwapChainImageFormat;
            VkFormat format_depth = GetSupportedFormat(format_depth_list,
                sizeof(format_depth_list) / sizeof(VkFormat), VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

            VkAttachmentDescription attachments[2];
            VK_ZERO_MEMORY(attachments, sizeof(attachments));

            // TODO: Refactor this into helper functions!

            // Color attachment
            // NOTE: For multisampling, we must pass a different
            //       enum into the samples property.
            attachments[0].format         = format_color;
            attachments[0].samples        = VK_SAMPLE_COUNT_1_BIT;
            attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            // Depth attachment
            attachments[1].format         = format_depth;
            attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
            attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentReference attachment_references[2];
            VK_ZERO_MEMORY(attachment_references, sizeof(attachment_references));

            attachment_references[0].attachment = 0;
            attachment_references[0].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment_references[1].attachment = 1;
            attachment_references[1].layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkSubpassDescription sub_pass_description;
            VK_ZERO_MEMORY(&sub_pass_description, sizeof(sub_pass_description));

            sub_pass_description.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            sub_pass_description.colorAttachmentCount    = 1;
            sub_pass_description.pColorAttachments       = &attachment_references[0];
            sub_pass_description.pDepthStencilAttachment = &attachment_references[1];
            sub_pass_description.inputAttachmentCount    = 0;
            sub_pass_description.pInputAttachments       = 0;
            sub_pass_description.preserveAttachmentCount = 0;
            sub_pass_description.pPreserveAttachments    = 0;
            sub_pass_description.pResolveAttachments     = 0;

            // Subpass sub_pass_dependencies for layout transitions
            VkSubpassDependency sub_pass_dependencies[2];
            VK_ZERO_MEMORY(sub_pass_dependencies, sizeof(sub_pass_dependencies));

            sub_pass_dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
            sub_pass_dependencies[0].dstSubpass      = 0;
            sub_pass_dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            sub_pass_dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            sub_pass_dependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
            sub_pass_dependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sub_pass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            sub_pass_dependencies[1].srcSubpass      = 0;
            sub_pass_dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
            sub_pass_dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            sub_pass_dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            sub_pass_dependencies[1].srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sub_pass_dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
            sub_pass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            VkRenderPassCreateInfo render_pass_create_info;
            VK_ZERO_MEMORY(&render_pass_create_info, sizeof(render_pass_create_info));

            render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            render_pass_create_info.attachmentCount = sizeof(attachments) / sizeof(VkAttachmentDescription);
            render_pass_create_info.pAttachments    = attachments;
            render_pass_create_info.subpassCount    = 1;
            render_pass_create_info.pSubpasses      = &sub_pass_description;
            render_pass_create_info.dependencyCount = sizeof(sub_pass_dependencies) / sizeof(VkSubpassDependency);
            render_pass_create_info.pDependencies   = sub_pass_dependencies;

            if(vkCreateRenderPass(g_vk_context.m_LogicalDevice, &render_pass_create_info, 0, &g_vk_context.m_DefaultRenderPass) != VK_SUCCESS)
            {
                return false;
            }

            return true;
        }

        bool Initialize()
        {
            g_vk_context.m_ApplicationInfo;
            VK_ZERO_MEMORY(&g_vk_context.m_ApplicationInfo, sizeof(g_vk_context.m_ApplicationInfo));

            g_vk_context.m_ApplicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            g_vk_context.m_ApplicationInfo.pApplicationName   = "Defold";
            g_vk_context.m_ApplicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            g_vk_context.m_ApplicationInfo.pEngineName        = "Defold Engine";
            g_vk_context.m_ApplicationInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
            g_vk_context.m_ApplicationInfo.apiVersion         = VK_API_VERSION_1_0;

            VkInstanceCreateInfo instance_create_info;
            VK_ZERO_MEMORY(&instance_create_info, sizeof(instance_create_info));

            instance_create_info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_create_info.pApplicationInfo = &g_vk_context.m_ApplicationInfo;

            // Populate the extension list with available API extensions (and print them)
            SetInstanceExtensionList();

            if (g_enable_validation_layers && GetValidationSupport())
            {
                dmLogInfo("Required validation layers are supported");

                uint32_t enabled_layer_count = 0;

                while(g_validation_layers[enabled_layer_count])
                {
                    enabled_layer_count++;
                }

                instance_create_info.enabledLayerCount   = enabled_layer_count;
                instance_create_info.ppEnabledLayerNames = g_validation_layers;
            }

            dmArray<const char*> required_extensions;
            GetRequiredInstanceExtensions(required_extensions);

            instance_create_info.enabledExtensionCount   = required_extensions.Size();
            instance_create_info.ppEnabledExtensionNames = required_extensions.Begin();

            if (vkCreateInstance(&instance_create_info, 0, &g_vk_context.m_Instance) == VK_SUCCESS)
            {
                dmLogInfo("Successfully created Vulkan instance!");
            }
            else
            {
                // TODO: We should do cleanup here and return
                dmLogError("Failed to create Vulkan instance");
            }

            if (g_enable_validation_layers)
            {
                VkDebugUtilsMessengerCreateInfoEXT debug_callback_create_info;
                VK_ZERO_MEMORY(&debug_callback_create_info, sizeof(debug_callback_create_info));

                debug_callback_create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debug_callback_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

                debug_callback_create_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

                debug_callback_create_info.pfnUserCallback = g_vk_debug_callback;

                PFN_vkCreateDebugUtilsMessengerEXT func_ptr =
                    (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(g_vk_context.m_Instance, "vkCreateDebugUtilsMessengerEXT");

                if (func_ptr)
                {
                    func_ptr(g_vk_context.m_Instance, &debug_callback_create_info, 0, &g_vk_context.m_DebugCallback);
                }
                else
                {
                    dmLogError("Unable to create validation layer callback");
                }
            }

            return true;
        }

        bool OpenWindow(WindowParams* params)
        {
            if (!glfwOpenWindow(params->m_Width, params->m_Height, 8, 8, 8, 8, 32, 8, GLFW_WINDOW))
            {
                return false;
            }

            glfwWrapper::GLWFWindow wnd;

            wnd.ns.view   = glfwGetOSXNSView();
            wnd.ns.object = glfwGetOSXNSWindow();

            if (glfwWrapper::glfwCreateWindowSurface(g_vk_context.m_Instance, &wnd, 0, &g_vk_context.m_Surface) != VK_SUCCESS)
            {
                dmLogError("Unable to create window");
                return false;
            }

            if (!SetPhysicalDevice())
            {
                dmLogError("Unable to set a physical device");
                return false;
            }

            if (!SetLogicalDevice())
            {
                dmLogError("Unable to set a logical device");
                return false;
            }

            if (!CreateSwapChain(params->m_Width, params->m_Height))
            {
                dmLogError("Unable to create swap chain");
                return false;
            }

            if (!CreateSwapChainImageViews())
            {
                dmLogError("Unable to create swap chain image views");
                return false;
            }

            if (!CreateDefaultRenderPass())
            {
                dmLogError("Unable to create default renderpass");
            }

            return true;
        }
    }

    uint16_t TYPE_SIZE[] =
    {
        sizeof(char), // TYPE_BYTE
        sizeof(unsigned char), // TYPE_UNSIGNED_BYTE
        sizeof(short), // TYPE_SHORT
        sizeof(unsigned short), // TYPE_UNSIGNED_SHORT
        sizeof(int), // TYPE_INT
        sizeof(unsigned int), // TYPE_UNSIGNED_INT
        sizeof(float) // TYPE_FLOAT
    };

    bool g_ContextCreated = false;

    bool Initialize()
    {
        return Vulkan::Initialize();
    }

    void Finalize()
    {
        // nop
    }

    Context::Context(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
    }

    HContext NewContext(const ContextParams& params)
    {
        if (!g_ContextCreated)
        {
            if (glfwInit() == GL_FALSE)
            {
                dmLogError("Could not initialize glfw.");
                return 0x0;
            }

            g_ContextCreated = true;
            return new Context(params);
        }
        else
        {
            return 0x0;
        }
    }

    void DeleteContext(HContext context)
    {
        assert(context);
        if (g_ContextCreated)
        {
            delete context;
            g_ContextCreated = false;
        }
    }

    WindowResult OpenWindow(HContext context, WindowParams* params)
    {
        assert(context);
        assert(params);
        if (context->m_WindowOpened)
            return WINDOW_RESULT_ALREADY_OPENED;

        if (!Vulkan::OpenWindow(params))
        {
            dmLogError("Unable to open Vulkan window");
        }

        int width, height;
        glfwGetWindowSize(&width, &height);
        context->m_WindowWidth = (uint32_t)width;
        context->m_WindowHeight = (uint32_t)height;
        context->m_Dpi = 0;

        context->m_WindowResizeCallback = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData = params->m_CloseCallbackUserData;
        context->m_Width = params->m_Width;
        context->m_Height = params->m_Height;
        context->m_WindowWidth = params->m_Width;
        context->m_WindowHeight = params->m_Height;
        context->m_Dpi = 0;
        context->m_WindowOpened = 1;
        uint32_t buffer_size = 4 * context->m_WindowWidth * context->m_WindowHeight;
        context->m_MainFrameBuffer.m_ColorBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_DepthBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_StencilBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_ColorBufferSize = buffer_size;
        context->m_MainFrameBuffer.m_DepthBufferSize = buffer_size;
        context->m_MainFrameBuffer.m_StencilBufferSize = buffer_size;
        context->m_CurrentFrameBuffer = &context->m_MainFrameBuffer;
        context->m_Program = 0x0;

        if (params->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: vulkan");
        }

        return WINDOW_RESULT_OK;
    }

    uint32_t GetWindowRefreshRate(HContext context)
    {
        assert(context);
        return 0;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer;
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_WindowOpened = 0;
            context->m_Width = 0;
            context->m_Height = 0;
            context->m_WindowWidth = 0;
            context->m_WindowHeight = 0;
        }
    }

    void IconifyWindow(HContext context)
    {
        assert(context);
    }

    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }

    uint32_t GetWindowState(HContext context, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
                return context->m_WindowOpened;
            default:
                return 0;
        }
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        assert(context);
        return context->m_Dpi;
    }

    uint32_t GetWidth(HContext context)
    {
        return context->m_Width;
    }

    uint32_t GetHeight(HContext context)
    {
        return context->m_Height;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        return context->m_WindowWidth;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        return context->m_WindowHeight;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer;
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_Width = width;
            context->m_Height = height;
            context->m_WindowWidth = width;
            context->m_WindowHeight = height;
            uint32_t buffer_size = 4 * width * height;
            main.m_ColorBuffer = new char[buffer_size];
            main.m_ColorBufferSize = buffer_size;
            main.m_DepthBuffer = new char[buffer_size];
            main.m_DepthBufferSize = buffer_size;
            main.m_StencilBuffer = new char[buffer_size];
            main.m_StencilBufferSize = buffer_size;
            if (context->m_WindowResizeCallback)
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, width, height);
        }
    }

    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);
        if (flags & dmGraphics::BUFFER_TYPE_COLOR_BIT)
        {
            uint32_t colour = (red << 24) | (green << 16) | (blue << 8) | alpha;
            uint32_t* buffer = (uint32_t*)context->m_CurrentFrameBuffer->m_ColorBuffer;
            uint32_t count = context->m_CurrentFrameBuffer->m_ColorBufferSize / sizeof(uint32_t);
            for (uint32_t i = 0; i < count; ++i)
                buffer[i] = colour;
        }
        if (flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT)
        {
            float* buffer = (float*)context->m_CurrentFrameBuffer->m_DepthBuffer;
            uint32_t count = context->m_CurrentFrameBuffer->m_DepthBufferSize / sizeof(uint32_t);
            for (uint32_t i = 0; i < count; ++i)
                buffer[i] = depth;
        }
        if (flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT)
        {
            uint32_t* buffer = (uint32_t*)context->m_CurrentFrameBuffer->m_StencilBuffer;
            uint32_t count = context->m_CurrentFrameBuffer->m_StencilBufferSize / sizeof(uint32_t);
            for (uint32_t i = 0; i < count; ++i)
                buffer[i] = stencil;
        }
    }

    void Flip(HContext context)
    {
        glfwSwapBuffers();
        /*
        // Mimick glfw
        if (context->m_RequestWindowClose)
        {
            if (context->m_WindowCloseCallback != 0x0 && context->m_WindowCloseCallback(context->m_WindowCloseCallbackUserData))
            {
                CloseWindow(context);
            }
        }
        g_Flipped = 1;
        */
    }

    void SetSwapInterval(HContext /*context*/, uint32_t /*swap_interval*/)
    {
        // NOP
    }

    #define NATIVE_HANDLE_IMPL(return_type, func_name) return_type GetNative##func_name() { return NULL; }

    NATIVE_HANDLE_IMPL(id, iOSUIWindow);
    NATIVE_HANDLE_IMPL(id, iOSUIView);
    NATIVE_HANDLE_IMPL(id, iOSEAGLContext);
    NATIVE_HANDLE_IMPL(id, OSXNSWindow);
    NATIVE_HANDLE_IMPL(id, OSXNSView);
    NATIVE_HANDLE_IMPL(id, OSXNSOpenGLContext);
    NATIVE_HANDLE_IMPL(HWND, WindowsHWND);
    NATIVE_HANDLE_IMPL(HGLRC, WindowsHGLRC);
    NATIVE_HANDLE_IMPL(EGLContext, AndroidEGLContext);
    NATIVE_HANDLE_IMPL(EGLSurface, AndroidEGLSurface);
    NATIVE_HANDLE_IMPL(JavaVM*, AndroidJavaVM);
    NATIVE_HANDLE_IMPL(jobject, AndroidActivity);
    NATIVE_HANDLE_IMPL(android_app*, AndroidApp);
    NATIVE_HANDLE_IMPL(Window, X11Window);
    NATIVE_HANDLE_IMPL(GLXContext, X11GLXContext);

    #undef NATIVE_HANDLE_IMPL

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VertexBuffer* vb = new VertexBuffer();
        vb->m_Buffer = new char[size];
        vb->m_Copy = 0x0;
        vb->m_Size = size;
        if (size > 0 && data != 0x0)
            memcpy(vb->m_Buffer, data, size);
        return (uintptr_t)vb;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        assert(vb->m_Copy == 0x0);
        delete [] vb->m_Buffer;
        delete vb;
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        assert(vb->m_Copy == 0x0);
        delete [] vb->m_Buffer;
        vb->m_Buffer = new char[size];
        vb->m_Size = size;
        if (data != 0x0)
            memcpy(vb->m_Buffer, data, size);
    }

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        if (offset + size <= vb->m_Size && data != 0x0)
            memcpy(&(vb->m_Buffer)[offset], data, size);
    }

    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        vb->m_Copy = new char[vb->m_Size];
        memcpy(vb->m_Copy, vb->m_Buffer, vb->m_Size);
        return vb->m_Copy;
    }

    bool UnmapVertexBuffer(HVertexBuffer buffer)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        memcpy(vb->m_Buffer, vb->m_Copy, vb->m_Size);
        delete [] vb->m_Copy;
        vb->m_Copy = 0x0;
        return true;
    }

    uint32_t GetMaxElementsVertices(HContext context)
    {
        return 65536;
    }

    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        IndexBuffer* ib = new IndexBuffer();
        ib->m_Buffer = new char[size];
        ib->m_Copy = 0x0;
        ib->m_Size = size;
        memcpy(ib->m_Buffer, data, size);
        return (uintptr_t)ib;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        assert(ib->m_Copy == 0x0);
        delete [] ib->m_Buffer;
        delete ib;
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        assert(ib->m_Copy == 0x0);
        delete [] ib->m_Buffer;
        ib->m_Buffer = new char[size];
        ib->m_Size = size;
        if (data != 0x0)
            memcpy(ib->m_Buffer, data, size);
    }

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        if (offset + size <= ib->m_Size && data != 0x0)
            memcpy(&(ib->m_Buffer)[offset], data, size);
    }

    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        ib->m_Copy = new char[ib->m_Size];
        memcpy(ib->m_Copy, ib->m_Buffer, ib->m_Size);
        return ib->m_Copy;
    }

    bool UnmapIndexBuffer(HIndexBuffer buffer)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        memcpy(ib->m_Buffer, ib->m_Copy, ib->m_Size);
        delete [] ib->m_Copy;
        ib->m_Copy = 0x0;
        return true;
    }

    uint32_t GetMaxElementsIndices(HContext context)
    {
        return 65536;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        return NewVertexDeclaration(context, element, count);
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        VertexDeclaration* vd = new VertexDeclaration();
        memset(vd, 0, sizeof(*vd));
        vd->m_Count = count;
        for (uint32_t i = 0; i < count; ++i)
        {
            assert(vd->m_Elements[element[i].m_Stream].m_Size == 0);
            vd->m_Elements[element[i].m_Stream] = element[i];
        }
        return vd;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete vertex_declaration;
    }

    static void EnableVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
        VertexStream& s = context->m_VertexStreams[stream];
        assert(s.m_Source == 0x0);
        assert(s.m_Buffer == 0x0);
        s.m_Source = vertex_buffer;
        s.m_Size = size * TYPE_SIZE[type - dmGraphics::TYPE_BYTE];
        s.m_Stride = stride;
    }

    static void DisableVertexStream(HContext context, uint16_t stream)
    {
        assert(context);
        VertexStream& s = context->m_VertexStreams[stream];
        s.m_Size = 0;
        if (s.m_Buffer != 0x0)
        {
            delete [] (char*)s.m_Buffer;
            s.m_Buffer = 0x0;
        }
        s.m_Source = 0x0;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        assert(context);
        assert(vertex_declaration);
        assert(vertex_buffer);
        VertexBuffer* vb = (VertexBuffer*)vertex_buffer;
        uint16_t stride = 0;
        for (uint32_t i = 0; i < vertex_declaration->m_Count; ++i)
            stride += vertex_declaration->m_Elements[i].m_Size * TYPE_SIZE[vertex_declaration->m_Elements[i].m_Type - dmGraphics::TYPE_BYTE];
        uint32_t offset = 0;
        for (uint16_t i = 0; i < vertex_declaration->m_Count; ++i)
        {
            VertexElement& ve = vertex_declaration->m_Elements[i];
            if (ve.m_Size > 0)
            {
                EnableVertexStream(context, i, ve.m_Size, ve.m_Type, stride, &vb->m_Buffer[offset]);
                offset += ve.m_Size * TYPE_SIZE[ve.m_Type - dmGraphics::TYPE_BYTE];
            }
        }
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        EnableVertexDeclaration(context, vertex_declaration, vertex_buffer);
    }

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        assert(context);
        assert(vertex_declaration);
        for (uint32_t i = 0; i < vertex_declaration->m_Count; ++i)
            if (vertex_declaration->m_Elements[i].m_Size > 0)
                DisableVertexStream(context, i);
    }

    static uint32_t GetIndex(Type type, HIndexBuffer ib, uint32_t index)
    {
        const void* index_buffer = ((IndexBuffer*) ib)->m_Buffer;
        uint32_t result = ~0;
        switch (type)
        {
            case dmGraphics::TYPE_BYTE:
                result = ((char*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_UNSIGNED_BYTE:
                result = ((unsigned char*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_SHORT:
                result = ((short*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_UNSIGNED_SHORT:
                result = ((unsigned short*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_INT:
                result = ((int*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_UNSIGNED_INT:
                result = ((unsigned int*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_FLOAT:
                result = ((float*)index_buffer)[index];
                break;
            default:
                assert(0);
                break;
        }
        return result;
    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(context);
        assert(index_buffer);
        for (uint32_t i = 0; i < MAX_VERTEX_STREAM_COUNT; ++i)
        {
            VertexStream& vs = context->m_VertexStreams[i];
            if (vs.m_Size > 0)
            {
                vs.m_Buffer = new char[vs.m_Size * count];
            }
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t index = GetIndex(type, index_buffer, i + first);
            for (uint32_t j = 0; j < MAX_VERTEX_STREAM_COUNT; ++j)
            {
                VertexStream& vs = context->m_VertexStreams[j];
                if (vs.m_Size > 0)
                    memcpy(&((char*)vs.m_Buffer)[i * vs.m_Size], &((char*)vs.m_Source)[index * vs.m_Stride], vs.m_Size);
            }
        }

        if (g_Flipped)
        {
            g_Flipped = 0;
            g_DrawCount = 0;
        }
        g_DrawCount++;
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);

        if (g_Flipped)
        {
            g_Flipped = 0;
            g_DrawCount = 0;
        }
        g_DrawCount++;
    }

    uint64_t GetDrawCount()
    {
        return g_DrawCount;
    }

    struct VertexProgram
    {
        char* m_Data;
    };

    struct FragmentProgram
    {
        char* m_Data;
    };

    static void NullUniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uintptr_t userdata);

    struct Uniform
    {
        Uniform() : m_Name(0) {};
        char* m_Name;
        uint32_t m_Index;
        Type m_Type;
    };

    struct Program
    {
        Program(VertexProgram* vp, FragmentProgram* fp)
        {
            m_Uniforms.SetCapacity(16);
            m_VP = vp;
            m_FP = fp;
            if (m_VP != 0x0)
                GLSLUniformParse(m_VP->m_Data, NullUniformCallback, (uintptr_t)this);
            if (m_FP != 0x0)
                GLSLUniformParse(m_FP->m_Data, NullUniformCallback, (uintptr_t)this);
        }

        ~Program()
        {
            for(uint32_t i = 0; i < m_Uniforms.Size(); ++i)
                delete[] m_Uniforms[i].m_Name;
        }

        VertexProgram* m_VP;
        FragmentProgram* m_FP;
        dmArray<Uniform> m_Uniforms;
    };

    static void NullUniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uintptr_t userdata)
    {
        Program* program = (Program*) userdata;
        if(program->m_Uniforms.Full())
            program->m_Uniforms.OffsetCapacity(16);
        Uniform uniform;
        name_length++;
        uniform.m_Name = new char[name_length];
        dmStrlCpy(uniform.m_Name, name, name_length);
        uniform.m_Index = program->m_Uniforms.Size();
        uniform.m_Type = type;
        program->m_Uniforms.Push(uniform);
    }

    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        VertexProgram* vertex = 0x0;
        FragmentProgram* fragment = 0x0;
        if (vertex_program != INVALID_VERTEX_PROGRAM_HANDLE)
            vertex = (VertexProgram*) vertex_program;
        if (fragment_program != INVALID_FRAGMENT_PROGRAM_HANDLE)
            fragment = (FragmentProgram*) fragment_program;
        return (HProgram) new Program(vertex, fragment);
    }

    void DeleteProgram(HContext context, HProgram program)
    {
        delete (Program*) program;
    }

    HVertexProgram NewVertexProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);
        VertexProgram* p = new VertexProgram();
        p->m_Data = new char[program_size+1];
        memcpy(p->m_Data, program, program_size);
        p->m_Data[program_size] = '\0';
        return (uintptr_t)p;
    }

    HFragmentProgram NewFragmentProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);
        FragmentProgram* p = new FragmentProgram();
        p->m_Data = new char[program_size+1];
        memcpy(p->m_Data, program, program_size);
        p->m_Data[program_size] = '\0';
        return (uintptr_t)p;
    }

    bool ReloadVertexProgram(HVertexProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[program_size];
        memcpy((char*)p->m_Data, program, program_size);
        return !g_ForceVertexReloadFail;
    }

    bool ReloadFragmentProgram(HFragmentProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[program_size];
        memcpy((char*)p->m_Data, program, program_size);
        return !g_ForceFragmentReloadFail;
    }

    void DeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    void DeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    void EnableProgram(HContext context, HProgram program)
    {
        assert(context);
        context->m_Program = (void*)program;
    }

    void DisableProgram(HContext context)
    {
        assert(context);
        context->m_Program = 0x0;
    }

    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        (void) context;
        (void) program;

        return true;
    }

    uint32_t GetUniformCount(HProgram prog)
    {
        return ((Program*)prog)->m_Uniforms.Size();
    }

    void GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type)
    {
        Program* program = (Program*)prog;
        assert(index < program->m_Uniforms.Size());
        Uniform& uniform = program->m_Uniforms[index];
        *buffer = '\0';
        dmStrlCat(buffer, uniform.m_Name, buffer_size);
        *type = uniform.m_Type;
    }

    int32_t GetUniformLocation(HProgram prog, const char* name)
    {
        Program* program = (Program*)prog;
        uint32_t count = program->m_Uniforms.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Uniform& uniform = program->m_Uniforms[i];
            if (dmStrCaseCmp(uniform.m_Name, name)==0)
            {
                return (int32_t)uniform.m_Index;
            }
        }
        return -1;
    }

    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
    }

    const Vector4& GetConstantV4Ptr(HContext context, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        return context->m_ProgramRegisters[base_register];
    }

    void SetConstantV4(HContext context, const Vector4* data, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4));
    }

    void SetConstantM4(HContext context, const Vector4* data, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4) * 4);
    }

    void SetSampler(HContext context, int32_t location, int32_t unit)
    {
    }

    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        RenderTarget* rt = new RenderTarget();
        memset(rt, 0, sizeof(RenderTarget));

        void** buffers[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBuffer, &rt->m_FrameBuffer.m_DepthBuffer, &rt->m_FrameBuffer.m_StencilBuffer};
        uint32_t* buffer_sizes[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBufferSize, &rt->m_FrameBuffer.m_DepthBufferSize, &rt->m_FrameBuffer.m_StencilBufferSize};
        BufferType buffer_types[MAX_BUFFER_TYPE_COUNT] = {BUFFER_TYPE_COLOR_BIT, BUFFER_TYPE_DEPTH_BIT, BUFFER_TYPE_STENCIL_BIT};
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            assert(GetBufferTypeIndex(buffer_types[i]) == i);
            if (buffer_type_flags & buffer_types[i])
            {
                uint32_t buffer_size = sizeof(uint32_t) * params[i].m_Width * params[i].m_Height;
                *(buffer_sizes[i]) = buffer_size;
                rt->m_BufferTextureParams[i] = params[i];
                rt->m_BufferTextureParams[i].m_Data = 0x0;
                rt->m_BufferTextureParams[i].m_DataSize = 0;

                if(i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR_BIT))
                {
                    rt->m_BufferTextureParams[i].m_DataSize = buffer_size;
                    rt->m_ColorBufferTexture = NewTexture(context, creation_params[i]);
                    SetTexture(rt->m_ColorBufferTexture, rt->m_BufferTextureParams[i]);
                    *(buffers[i]) = rt->m_ColorBufferTexture->m_Data;
                } else {
                    *(buffers[i]) = new char[buffer_size];
                }
            }
        }

        return rt;
    }

    void DeleteRenderTarget(HRenderTarget rt)
    {
        if (rt->m_ColorBufferTexture)
            DeleteTexture(rt->m_ColorBufferTexture);
        delete [] (char*)rt->m_FrameBuffer.m_DepthBuffer;
        delete [] (char*)rt->m_FrameBuffer.m_StencilBuffer;
        delete rt;
    }

    void SetRenderTarget(HContext context, HRenderTarget rendertarget, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        assert(context);
        context->m_CurrentFrameBuffer = &rendertarget->m_FrameBuffer;
    }

    HTexture GetRenderTargetTexture(HRenderTarget rendertarget, BufferType buffer_type)
    {
        if(buffer_type != BUFFER_TYPE_COLOR_BIT)
            return 0;
        return rendertarget->m_ColorBufferTexture;
    }

    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        assert(render_target);
        uint32_t i = GetBufferTypeIndex(buffer_type);
        assert(i < MAX_BUFFER_TYPE_COUNT);
        width = render_target->m_BufferTextureParams[i].m_Width;
        height = render_target->m_BufferTextureParams[i].m_Height;
    }

    void SetRenderTargetSize(HRenderTarget rt, uint32_t width, uint32_t height)
    {
        uint32_t buffer_size = sizeof(uint32_t) * width * height;

        void** buffers[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBuffer, &rt->m_FrameBuffer.m_DepthBuffer, &rt->m_FrameBuffer.m_StencilBuffer};
        uint32_t* buffer_sizes[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBufferSize, &rt->m_FrameBuffer.m_DepthBufferSize, &rt->m_FrameBuffer.m_StencilBufferSize};
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            if (buffers[i])
            {
                *(buffer_sizes[i]) = buffer_size;
                rt->m_BufferTextureParams[i].m_Width = width;
                rt->m_BufferTextureParams[i].m_Height = height;
                if(i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR_BIT))
                {
                    rt->m_BufferTextureParams[i].m_DataSize = buffer_size;
                    SetTexture(rt->m_ColorBufferTexture, rt->m_BufferTextureParams[i]);
                    *(buffers[i]) = rt->m_ColorBufferTexture->m_Data;
                } else {
                    delete [] (char*)*(buffers[i]);
                    *(buffers[i]) = new char[buffer_size];
                }
            }
        }
    }

    bool IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (context->m_TextureFormatSupport & (1 << format)) != 0;
    }

    uint32_t GetMaxTextureSize(HContext context)
    {
        return 1024;
    }

    HTexture NewTexture(HContext context, const TextureCreationParams& params)
    {
        Texture* tex = new Texture();

        tex->m_Width = params.m_Width;
        tex->m_Height = params.m_Height;
        tex->m_MipMapCount = 0;
        tex->m_Data = 0;

        if (params.m_OriginalWidth == 0) {
            tex->m_OriginalWidth = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
        } else {
            tex->m_OriginalWidth = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
        }

        return tex;
    }

    void DeleteTexture(HTexture t)
    {
        assert(t);
        if (t->m_Data != 0x0)
            delete [] (char*)t->m_Data;
        delete t;
    }

    HandleResult GetTextureHandle(HTexture texture, void** out_handle)
    {
        *out_handle = 0x0;

        if (!texture) {
            return HANDLE_RESULT_ERROR;
        }

        *out_handle = texture->m_Data;

        return HANDLE_RESULT_OK;
    }

    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap)
    {
        assert(texture);
    }

    void SetTexture(HTexture texture, const TextureParams& params)
    {
        assert(texture);
        assert(!params.m_SubUpdate || params.m_SubUpdate && (params.m_X + params.m_Width <= texture->m_Width));
        assert(!params.m_SubUpdate || params.m_SubUpdate && (params.m_Y + params.m_Height <= texture->m_Height));

        if (texture->m_Data != 0x0)
            delete [] (char*)texture->m_Data;
        texture->m_Format = params.m_Format;
        // Allocate even for 0x0 size so that the rendertarget dummies will work.
        texture->m_Data = new char[params.m_DataSize];
        if (params.m_Data != 0x0)
            memcpy(texture->m_Data, params.m_Data, params.m_DataSize);
        texture->m_MipMapCount = dmMath::Max(texture->m_MipMapCount, (uint16_t)(params.m_MipMap+1));
    }

    uint8_t* GetTextureData(HTexture texture) {
        return 0x0;
    }

    uint32_t GetTextureFormatBPP(TextureFormat format)
    {
        static TextureFormatToBPP g_TextureFormatToBPP;
        assert(format < TEXTURE_FORMAT_COUNT);
        return g_TextureFormatToBPP.m_FormatToBPP[format];
    }

    uint32_t GetTextureResourceSize(HTexture texture)
    {
        uint32_t size_total = 0;
        uint32_t size = (texture->m_Width * texture->m_Height * GetTextureFormatBPP(texture->m_Format)) >> 3;
        for(uint32_t i = 0; i < texture->m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        return size_total + sizeof(Texture);
    }

    uint16_t GetTextureWidth(HTexture texture)
    {
        return texture->m_Width;
    }

    uint16_t GetTextureHeight(HTexture texture)
    {
        return texture->m_Height;
    }

    uint16_t GetOriginalTextureWidth(HTexture texture)
    {
        return texture->m_OriginalWidth;
    }

    uint16_t GetOriginalTextureHeight(HTexture texture)
    {
        return texture->m_OriginalHeight;
    }

    void EnableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        assert(texture);
        assert(texture->m_Data);
        context->m_Textures[unit] = texture;
    }

    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        context->m_Textures[unit] = 0;
    }

    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
        uint32_t w = dmGraphics::GetWidth(context);
        uint32_t h = dmGraphics::GetHeight(context);
        assert (buffer_size >= w * h * 4);
        memset(buffer, 0, w * h * 4);
    }

    void EnableState(HContext context, State state)
    {
        assert(context);
    }

    void DisableState(HContext context, State state)
    {
        assert(context);
    }

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
    }

    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        context->m_RedMask = red;
        context->m_GreenMask = green;
        context->m_BlueMask = blue;
        context->m_AlphaMask = alpha;
    }

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);
        context->m_DepthMask = mask;
    }

    void SetDepthFunc(HContext context, CompareFunc func)
    {
        assert(context);
        context->m_DepthFunc = func;
    }

    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
        context->m_ScissorRect[0] = (int32_t) x;
        context->m_ScissorRect[1] = (int32_t) y;
        context->m_ScissorRect[2] = (int32_t) x+width;
        context->m_ScissorRect[3] = (int32_t) y+height;
    }

    void SetStencilMask(HContext context, uint32_t mask)
    {
        assert(context);
        context->m_StencilMask = mask;
    }

    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        context->m_StencilFunc = func;
        context->m_StencilFuncRef = ref;
        context->m_StencilFuncMask = mask;
    }

    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(context);
        context->m_StencilOpSFail = sfail;
        context->m_StencilOpDPFail = dpfail;
        context->m_StencilOpDPPass = dppass;
    }

    void SetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
    }

    void SetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
    }

    bool AcquireSharedContext()
    {
        return false;
    }

    void UnacquireContext()
    {

    }

    void SetTextureAsync(HTexture texture, const TextureParams& params)
    {
        SetTexture(texture, params);
    }

    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        return TEXTURE_STATUS_OK;
    }

    void SetForceFragmentReloadFail(bool should_fail)
    {
        g_ForceFragmentReloadFail = should_fail;
    }

    void SetForceVertexReloadFail(bool should_fail)
    {
        g_ForceVertexReloadFail = should_fail;
    }

}

