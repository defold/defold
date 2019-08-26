#ifndef __GRAPHICS_DEVICE_VULKAN__
#define __GRAPHICS_DEVICE_VULKAN__

#include <vulkan/vulkan.h>

namespace dmGraphics
{
    struct Texture
    {
        uint32_t dummy;
    };

    struct VertexDeclaration
    {
        uint32_t dummy;
    };

    struct RenderTarget
    {
        uint32_t dummy;
    };

    struct PhysicalDevice
    {
        VkQueueFamilyProperties*         m_QueueFamilyProperties;
        VkExtensionProperties*           m_DeviceExtensions;
        VkPhysicalDevice                 m_Device;
        VkPhysicalDeviceProperties       m_Properties;
        VkPhysicalDeviceFeatures         m_Features;
        VkPhysicalDeviceMemoryProperties m_MemoryProperties;
        uint16_t                         m_QueueFamilyCount;
        uint16_t                         m_DeviceExtensionCount;
    };

    struct QueueFamily
    {
        QueueFamily()
        : m_GraphicsQueueIx(0xffff)
        , m_PresentQueueIx(0xffff)
        {}

        uint16_t m_GraphicsQueueIx;
        uint16_t m_PresentQueueIx;

        bool IsValid() { return m_GraphicsQueueIx != 0xffff && m_PresentQueueIx != 0xffff; }
    };

    struct Context
    {
        VkInstance     m_Instance;
        VkSurfaceKHR   m_WindowSurface;
        PhysicalDevice m_PhysicalDevice;
        uint32_t       m_WindowOpened : 1;
        uint32_t       : 31;
    };

    // Implemented in graphics_vulkan_context.cpp
    VkResult VKCreateInstance(VkInstance* vkInstanceOut, bool enableValidation);

    // Implemented in graphics_vulkan_device.cpp
    uint32_t    VKGetPhysicalDeviceCount(VkInstance vkInstance);
    VkResult    VKGetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize);
    void        VKResetPhysicalDevice(PhysicalDevice* device);
    bool        VKIsPhysicalDeviceSuitable(PhysicalDevice* device, VkSurfaceKHR surface);
    QueueFamily VKGetQueueFamily(PhysicalDevice* device, VkSurfaceKHR surface);

    // Implemented per supported platform
    VkResult VKCreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, bool enableHighDPI);
}
#endif // __GRAPHICS_DEVICE_VULKAN__

