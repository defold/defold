#include "graphics_vulkan.h"

namespace dmGraphics
{
    uint32_t GetPhysicalDeviceCount(VkInstance vkInstance)
    {
        uint32_t vk_device_count = 0;
        vkEnumeratePhysicalDevices(vkInstance, &vk_device_count, 0);
        return vk_device_count;
    }

    void GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, const uint32_t deviceListSize)
    {
        assert(deviceListOut);
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
    }

    void ResetPhysicalDevice(PhysicalDevice* device)
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

    #define QUEUE_FAMILY_INVALID 0xffff

    // All GPU operations are pushed to various queues. The physical device can have multiple
    // queues with different properties supported, so we need to find a combination of queues
    // that will work for our needs. Note that the present queue might not be the same queue as the
    // graphics queue. The graphics queue support is needed to do graphics operations at all.
    QueueFamily GetQueueFamily(PhysicalDevice* device, const VkSurfaceKHR surface)
    {
        assert(device);

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
        return qf;
    }

    VkResult CreateLogicalDevice(PhysicalDevice* device, const VkSurfaceKHR surface, const QueueFamily queueFamily,
        const char** deviceExtensions, const uint8_t deviceExtensionCount,
        const char** validationLayers, const uint8_t validationLayerCount,
        LogicalDevice* logicalDeviceOut)
    {
        assert(device);

        // NOTE: Different queues can have different priority from [0..1], but
        //       we only have a single queue right now so set to 1.0f
        float queue_priority        = 1.0f;
        int32_t queue_family_set[2] = { queueFamily.m_PresentQueueIx, QUEUE_FAMILY_INVALID };
        int32_t queue_family_c      = 0;

        VkDeviceQueueCreateInfo vk_device_queue_create_info[2];
        memset(vk_device_queue_create_info, 0, sizeof(vk_device_queue_create_info));

        if (queueFamily.m_PresentQueueIx != queueFamily.m_GraphicsQueueIx)
        {
            queue_family_set[1] = queueFamily.m_GraphicsQueueIx;
        }

        while(queue_family_set[queue_family_c] != QUEUE_FAMILY_INVALID)
        {
            int queue_family_index = queue_family_set[queue_family_c];
            VkDeviceQueueCreateInfo& vk_queue_create_info = vk_device_queue_create_info[queue_family_c];

            vk_queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            vk_queue_create_info.queueFamilyIndex = queue_family_index;
            vk_queue_create_info.queueCount       = 1;
            vk_queue_create_info.pQueuePriorities = &queue_priority;

            queue_family_c++;
        }

        VkDeviceCreateInfo vk_device_create_info;
        memset(&vk_device_create_info, 0, sizeof(vk_device_create_info));

        vk_device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        vk_device_create_info.pQueueCreateInfos       = vk_device_queue_create_info;
        vk_device_create_info.queueCreateInfoCount    = queue_family_c;
        vk_device_create_info.pEnabledFeatures        = 0;
        vk_device_create_info.enabledExtensionCount   = deviceExtensionCount;
        vk_device_create_info.ppEnabledExtensionNames = deviceExtensions;
        vk_device_create_info.enabledLayerCount       = validationLayerCount;
        vk_device_create_info.ppEnabledLayerNames     = validationLayers;

        VkResult res = vkCreateDevice(device->m_Device, &vk_device_create_info, 0, &logicalDeviceOut->m_Device);

        if (res == VK_SUCCESS)
        {
            vkGetDeviceQueue(logicalDeviceOut->m_Device, queueFamily.m_GraphicsQueueIx, 0, &logicalDeviceOut->m_GraphicsQueue);
            vkGetDeviceQueue(logicalDeviceOut->m_Device, queueFamily.m_PresentQueueIx, 0, &logicalDeviceOut->m_PresentQueue);
        }

        return res;
    }

    #undef QUEUE_FAMILY_INVALID
}
