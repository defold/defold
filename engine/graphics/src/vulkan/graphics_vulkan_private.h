#ifndef __GRAPHICS_DEVICE_VULKAN__
#define __GRAPHICS_DEVICE_VULKAN__

namespace dmGraphics
{
    struct DeviceMemory
    {
        VkDeviceMemory m_Memory;
        size_t         m_MemorySize;
    };

    struct Texture
    {
        VkImage      m_Image;
        VkImageView  m_ImageView;
        VkFormat     m_Format;
        DeviceMemory m_DeviceMemory;
        TextureType  m_Type;
        uint16_t     m_Width;
        uint16_t     m_Height;
        uint16_t     m_OriginalWidth;
        uint16_t     m_OriginalHeight;
    };

    struct VertexDeclaration
    {
        uint32_t dummy;
    };

    struct RenderTarget
    {
        RenderTarget(const uint32_t rtId)
        : m_RenderPass(VK_NULL_HANDLE)
        , m_Framebuffer(VK_NULL_HANDLE)
        , m_Id(rtId)
        , m_IsBound(0)
        {}

        VkRenderPass   m_RenderPass;
        VkFramebuffer  m_Framebuffer;
        VkExtent2D     m_Extent;
        const uint32_t m_Id;
        uint8_t        m_IsBound : 1;
        uint8_t        : 7;
    };

    struct FrameResource
    {
        VkSemaphore m_ImageAvailable;
        VkSemaphore m_RenderFinished;
        VkFence     m_SubmitFence;
    };

    struct RenderPassAttachment
    {
        VkFormat      m_Format;
        VkImageLayout m_ImageLayout;
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
        VkDevice      m_Device;
        VkQueue       m_GraphicsQueue;
        VkQueue       m_PresentQueue;
        VkCommandPool m_CommandPool;
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
        dmArray<VkImageView> m_ImageViews;
        const LogicalDevice* m_LogicalDevice;
        const VkSurfaceKHR   m_Surface;
        const QueueFamily    m_QueueFamily;
        VkSurfaceFormatKHR   m_SurfaceFormat;
        VkSwapchainKHR       m_SwapChain;
        VkExtent2D           m_ImageExtent;
        uint8_t              m_ImageIndex;

        SwapChain(const LogicalDevice* logicalDevice, const VkSurfaceKHR surface,
            const SwapChainCapabilities& capabilities, const QueueFamily queueFamily);
        VkResult Advance(VkSemaphore);
    };

    // Implemented in graphics_vulkan_context.cpp
    VkResult CreateInstance(VkInstance* vkInstanceOut,
        // Validation Layer Names, i.e "VK_LAYER_LUNARG_standard_validation"
        const char** validationLayers, uint16_t validationLayerCount,
        // Req. Validation Layer Extensions, i.e "VK_EXT_DEBUG_UTILS_EXTENSION_NAME"
        const char** validationLayerExtensions, uint16_t validationLayerExtensionCount);
    void     DestroyInstance(VkInstance* vkInstance);

    // Implemented in graphics_vulkan_device.cpp
    VkResult CreateFramebuffer(VkDevice vk_device, VkRenderPass vk_render_pass,
        uint32_t width, uint32_t height,
        VkImageView* vk_attachments, uint8_t attachmentCount, // Color & depth/stencil attachments
        VkFramebuffer* vk_framebuffer_out);
    VkResult CreateCommandBuffers(VkDevice vk_device, VkCommandPool vk_command_pool,
        uint32_t numBuffersToCreate, VkCommandBuffer* vk_command_buffers_out);
    VkResult CreateLogicalDevice(PhysicalDevice* device, const VkSurfaceKHR surface, const QueueFamily queueFamily,
        const char** deviceExtensions, const uint8_t deviceExtensionCount,
        const char** validationLayers, const uint8_t validationLayerCount,
        LogicalDevice* logicalDeviceOut);
    VkResult CreateTexture2D(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        uint32_t imageWidth, uint32_t imageHeight, uint16_t imageMips,
        VkFormat vk_format, VkImageTiling vk_tiling, VkImageUsageFlags vk_usage,
        VkMemoryPropertyFlags vk_memory_flags, VkImageAspectFlags vk_aspect, Texture* textureOut);
    VkResult CreateRenderPass(VkDevice vk_device,
        RenderPassAttachment* colorAttachments, uint8_t numColorAttachments,
        RenderPassAttachment* depthStencilAttachment, VkRenderPass* renderPassOut);
    void           ResetPhysicalDevice(PhysicalDevice* device);
    void           ResetLogicalDevice(LogicalDevice* device);
    void           ResetRenderTarget(LogicalDevice* logicalDevice, RenderTarget* renderTarget);
    void           ResetTexture(VkDevice vk_device, Texture* texture);
    uint32_t       GetPhysicalDeviceCount(VkInstance vkInstance);
    void           GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize);
    bool           GetMemoryTypeIndex(VkPhysicalDevice vk_physical_device, uint32_t typeFilter, VkMemoryPropertyFlags vk_property_flags, uint32_t* memoryIndexOut);
    QueueFamily    GetQueueFamily(PhysicalDevice* device, const VkSurfaceKHR surface);
    const VkFormat GetSupportedTilingFormat(VkPhysicalDevice vk_physical_device, VkFormat* vk_format_candidates,
        uint32_t vk_num_format_candidates, VkImageTiling vk_tiling_type, VkFormatFeatureFlags vk_format_flags);
    VkResult TransitionImageLayout(VkDevice vk_device, VkCommandPool vk_command_pool, VkQueue vk_graphics_queue, VkImage vk_image,
        VkImageAspectFlags vk_image_aspect, VkImageLayout vk_from_layout, VkImageLayout vk_to_layout);

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
