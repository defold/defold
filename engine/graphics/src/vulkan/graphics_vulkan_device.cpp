#include "../graphics.h"
#include "graphics_vulkan.h"

namespace dmGraphics
{
    uint32_t VKGetPhysicalDeviceCount(VkInstance vkInstance)
    {
        uint32_t vk_device_count = 0;
        vkEnumeratePhysicalDevices(vkInstance, &vk_device_count, 0);
        return vk_device_count;
    }

    VkResult VKGetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize)
    {
        if (deviceListSize == 0)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        PhysicalDevice* device_list = *deviceListOut;
        uint32_t vk_device_count = deviceListSize;
        VkPhysicalDevice* vk_device_list = new VkPhysicalDevice[vk_device_count];
        vkEnumeratePhysicalDevices(vkInstance, &vk_device_count, vk_device_list);
        assert(vk_device_count == deviceListSize);

        const uint8_t vk_max_extension_count = 128;
        VkExtensionProperties vk_device_extensions[vk_max_extension_count];

        for (uint32_t i=0; i < vk_device_count; ++i)
        {
            VkPhysicalDevice vk_device = vk_device_list[i];
            uint32_t vk_device_extension_count, vk_queue_family_count;

            vkGetPhysicalDeviceProperties(vk_device, &device_list[i].m_Properties);
            vkGetPhysicalDeviceFeatures(vk_device, &device_list[i].m_Features);
            vkGetPhysicalDeviceMemoryProperties(vk_device, &device_list[i].m_MemoryProperties);

            vkGetPhysicalDeviceQueueFamilyProperties(vk_device, &vk_queue_family_count, 0);
            device_list[i].m_QueueFamilyProperties = new VkQueueFamilyProperties[vk_queue_family_count];
            vkGetPhysicalDeviceQueueFamilyProperties(vk_device, &vk_queue_family_count, device_list[i].m_QueueFamilyProperties);

            vkEnumerateDeviceExtensionProperties(vk_device, 0, &vk_device_extension_count, 0);

            // Note: If this is triggered, we need a higher limit for the max extension count
            //       or simply allocate the exact memory dynamically to hold the extensions data..
            assert(vk_device_extension_count < vk_max_extension_count);

            if (vkEnumerateDeviceExtensionProperties(vk_device, 0, &vk_device_extension_count, vk_device_extensions) == VK_SUCCESS)
            {
                device_list[i].m_DeviceExtensions = new VkExtensionProperties[vk_device_extension_count];

                for (uint8_t j = 0; j < vk_device_extension_count; ++j)
                {
                    device_list[i].m_DeviceExtensions[j] = vk_device_extensions[j];
                }
            }

            device_list[i].m_Device               = vk_device;
            device_list[i].m_QueueFamilyCount     = (uint16_t) vk_queue_family_count;
            device_list[i].m_DeviceExtensionCount = (uint16_t) vk_device_extension_count;
        }

        delete[] vk_device_list;

        return VK_SUCCESS;
    }

    void VKResetPhysicalDevice(PhysicalDevice* device)
    {
        assert(device);

        if (device->m_QueueFamilyProperties)
        {
            delete[] device->m_QueueFamilyProperties;
        }

        if (device->m_DeviceExtensions)
        {
            delete[] device->m_DeviceExtensions;
        }

        memset((void*)device, 0, sizeof(*device));
    }

    // All GPU operations are pushed to various queues. The physical device can have multiple
    // queues with different properties supported, so we need to find a combination of queues
    // that will work for our needs. Note that the present queue might not be the same queue as the
    // graphics queue. The graphics queue support is needed to do graphics operations at all.
    QueueFamily VKGetQueueFamily(PhysicalDevice* device, VkSurfaceKHR surface)
    {
        assert(device);

        #define QUEUE_FAMILY_INVALID 0xffff

        QueueFamily qf;

        if (device->m_QueueFamilyCount == 0)
        {
            return qf;
        }

        VkBool32* vk_present_queues = new VkBool32[device->m_QueueFamilyCount];

        // Try to find a queue that has both graphics and present capabilities,
        // and if we can't find one that has both, we take the first of each queue
        // that we can find.
        for (uint32_t i = 0; i < device->m_QueueFamilyCount; ++i)
        {
            QueueFamily candidate;
            vkGetPhysicalDeviceSurfaceSupportKHR(device->m_Device, i, surface, vk_present_queues+i);
            VkQueueFamilyProperties vk_properties = device->m_QueueFamilyProperties[i];

            if (vk_properties.queueCount > 0 && vk_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                candidate.m_GraphicsQueueIx = i;

                if (qf.m_GraphicsQueueIx == QUEUE_FAMILY_INVALID)
                {
                    qf.m_GraphicsQueueIx = i;
                }
            }

            if (vk_properties.queueCount > 0 && vk_present_queues[i])
            {
                candidate.m_PresentQueueIx = i;

                if (qf.m_PresentQueueIx == QUEUE_FAMILY_INVALID)
                {
                    qf.m_PresentQueueIx = i;
                }
            }

            if (candidate.IsValid() && candidate.m_GraphicsQueueIx == candidate.m_PresentQueueIx)
            {
                qf = candidate;
                break;
            }
        }

        delete[] vk_present_queues;
        #undef QUEUE_FAMILY_INVALID

        return qf;
    }

    void VKGetSwapChainCapabilities(PhysicalDevice* device, VkSurfaceKHR surface, SwapChainCapabilities& capabilities)
    {
        assert(device);

        VkPhysicalDevice vk_device = device->m_Device;
        uint32_t format_count        = 0;
        uint32_t present_modes_count = 0;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_device, surface, &capabilities.m_SurfaceCapabilities);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk_device, surface, &format_count, 0);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk_device, surface, &present_modes_count, 0);

        if (format_count > 0)
        {
            capabilities.m_SurfaceFormats.SetCapacity(format_count);
            capabilities.m_SurfaceFormats.SetSize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(vk_device, surface, &format_count, capabilities.m_SurfaceFormats.Begin());
        }

        if (present_modes_count > 0)
        {
            capabilities.m_PresentModes.SetCapacity(present_modes_count);
            capabilities.m_PresentModes.SetSize(present_modes_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(vk_device, surface, &present_modes_count, capabilities.m_PresentModes.Begin());
        }
    }
}
