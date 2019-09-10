#ifndef __GRAPHICS_DEVICE_VULKAN__
#define __GRAPHICS_DEVICE_VULKAN__

#include <dlib/array.h>
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

    struct LogicalDevice
    {
        VkDevice m_Device;
        VkQueue  m_GraphicsQueue;
        VkQueue  m_PresentQueue;
    };

    struct SwapChainCapabilities
    {
        dmArray<VkSurfaceFormatKHR> m_SurfaceFormats;
        dmArray<VkPresentModeKHR>   m_PresentModes;
        VkSurfaceCapabilitiesKHR    m_SurfaceCapabilities;

        void Swap(SwapChainCapabilities& rhs)
        {
            VkSurfaceCapabilitiesKHR tmp = rhs.m_SurfaceCapabilities;
            rhs.m_SurfaceCapabilities    = m_SurfaceCapabilities;
            m_SurfaceCapabilities        = tmp;

            m_PresentModes.Swap(rhs.m_PresentModes);
            m_SurfaceFormats.Swap(rhs.m_SurfaceFormats);
        }
    };

    struct SwapChain
    {
        dmArray<VkImage>     m_Images;
        const LogicalDevice* m_LogicalDevice;
        const VkSurfaceKHR   m_Surface;
        const QueueFamily    m_QueueFamily;
        VkSurfaceFormatKHR   m_SurfaceFormat;
        VkSwapchainKHR       m_SwapChain;
        VkExtent2D           m_ImageExtent;

        SwapChain(const LogicalDevice* logicalDevice, const VkSurfaceKHR surface,
            const SwapChainCapabilities& capabilities, const QueueFamily queueFamily);
    };

    struct Context
    {
        SwapChain*            m_SwapChain;
        SwapChainCapabilities m_SwapChainCapabilities;
        VkInstance            m_Instance;
        VkSurfaceKHR          m_WindowSurface;
        PhysicalDevice        m_PhysicalDevice;
        LogicalDevice         m_LogicalDevice;
        uint32_t              m_WindowOpened : 1;
        uint32_t              : 31;
    };

    // Implemented in graphics_vulkan_context.cpp
    VkResult CreateInstance(VkInstance* vkInstanceOut, const char** validationLayers, const uint8_t validationLayerCount);
    void     DestroyInstance(VkInstance* vkInstance);

    // Implemented in graphics_vulkan_device.cpp
    uint32_t    GetPhysicalDeviceCount(VkInstance vkInstance);
    void        GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, const uint32_t deviceListSize);
    void        ResetPhysicalDevice(PhysicalDevice* device);
    QueueFamily GetQueueFamily(PhysicalDevice* device, const VkSurfaceKHR surface);
    VkResult    CreateLogicalDevice(PhysicalDevice* device, const VkSurfaceKHR surface, const QueueFamily queueFamily,
        const char** deviceExtensions, const uint8_t deviceExtensionCount,
        const char** validationLayers, const uint8_t validationLayerCount,
        LogicalDevice* logicalDeviceOut);

    // Implemented in graphics_vulkan_swap_chain.cpp
    //   wantedWidth and wantedHeight might be written to, we might not get the
    //   dimensions we wanted from Vulkan.
    VkResult UpdateSwapChain(SwapChain* swapChain, uint32_t* wantedWidth, uint32_t* wantedHeight,
        const bool wantVSync, SwapChainCapabilities& capabilities);
    void     ResetSwapChain(SwapChain* swapChain);
    void     GetSwapChainCapabilities(PhysicalDevice* device, const VkSurfaceKHR surface, SwapChainCapabilities& capabilities);

    // Implemented per supported platform
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI);
}
#endif // __GRAPHICS_DEVICE_VULKAN__
