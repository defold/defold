#include "../graphics.h"
#include "graphics_vulkan.h"

namespace dmGraphics
{
    VkResult VKGetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** devicesOut, uint32_t* deviceCountOut)
    {
        // Get number of present devices
        uint32_t vk_device_count = 0;
        vkEnumeratePhysicalDevices(vkInstance, &vk_device_count, 0);

        if (vk_device_count == 0)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        PhysicalDevice* devices_out = new PhysicalDevice[vk_device_count];
        VkPhysicalDevice* vk_device_list = new VkPhysicalDevice[vk_device_count];
        vkEnumeratePhysicalDevices(vkInstance, &vk_device_count, vk_device_list);

        const uint8_t vk_max_extension_count = 128;
        VkExtensionProperties vk_device_extensions[vk_max_extension_count];

        for (uint32_t i=0; i < vk_device_count; ++i)
        {
            VkPhysicalDevice vk_device = vk_device_list[i];
            uint32_t vk_device_extension_count, vk_queue_family_count;

            vkGetPhysicalDeviceProperties(vk_device, &devices_out[i].m_Properties);
            vkGetPhysicalDeviceFeatures(vk_device, &devices_out[i].m_Features);
            vkGetPhysicalDeviceMemoryProperties(vk_device, &devices_out[i].m_MemoryProperties);

            vkGetPhysicalDeviceQueueFamilyProperties(vk_device, &vk_queue_family_count, 0);
            devices_out[i].m_QueueFamilyProperties = new VkQueueFamilyProperties[vk_queue_family_count];
            vkGetPhysicalDeviceQueueFamilyProperties(vk_device, &vk_queue_family_count, devices_out[i].m_QueueFamilyProperties);

            vkEnumerateDeviceExtensionProperties(vk_device, 0, &vk_device_extension_count, 0);
            assert(vk_device_extension_count < vk_max_extension_count);

            if (vkEnumerateDeviceExtensionProperties(vk_device, 0, &vk_device_extension_count, vk_device_extensions) == VK_SUCCESS)
            {
                devices_out[i].m_DeviceExtensions = new VkExtensionProperties[vk_device_extension_count];

                for (uint8_t j = 0; j < vk_device_extension_count; ++j)
                {
                    devices_out[i].m_DeviceExtensions[j] = vk_device_extensions[j];
                }
            }

            devices_out[i].m_Device               = vk_device;
            devices_out[i].m_QueueFamilyCount     = (uint16_t) vk_queue_family_count;
            devices_out[i].m_DeviceExtensionCount = (uint16_t) vk_device_extension_count;
        }

        delete[] vk_device_list;

        *devicesOut = devices_out;
        *deviceCountOut = vk_device_count;

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

        QueueFamily qf = { QUEUE_FAMILY_INVALID, QUEUE_FAMILY_INVALID };

        if (device->m_QueueFamilyCount == 0)
        {
            return qf;
        }

        uint32_t* vk_present_queues = new uint32_t[device->m_QueueFamilyCount];

        for (uint32_t i = 0; i < device->m_QueueFamilyCount; ++i)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(device->m_Device, i, surface, vk_present_queues+i);

            VkQueueFamilyProperties vk_properties = device->m_QueueFamilyProperties[i];

            /*
            QueueFamily candidate = qf;

            if (vk_properties.queueCount > 0 && vk_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                candidate.m_GraphicsQueueIx = i;
            }

            if (vk_properties.queueCount > 0 && vk_present_queues[i])
            {
                candidate.m_PresentQueueIx = i;
            }

            if (candidate.IsValid() && candidate.m_GraphicsQueueIx == candidate.m_PresentQueueIx)
            {
                qf = candidate;
                break;
            }
            */
        }

        /*
        if (!qf.IsValid())
        {
            for (uint32_t i = 0; i < device->m_QueueFamilyCount; ++i)
            {

            }
        }
        */

        delete[] vk_present_queues;

        #undef QUEUE_FAMILY_INVALID
    }

    bool VKIsPhysicalDeviceSuitable(PhysicalDevice* device, VkSurfaceKHR surface)
    {
        assert(device);

        QueueFamily queue_family = VKGetQueueFamily(device, surface);

        if (device->m_QueueFamilyCount == 0)
        {
            return false;
        }

        return true;
    }
}
