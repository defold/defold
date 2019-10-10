#ifndef __GRAPHICS_DEVICE_VULKAN__
#define __GRAPHICS_DEVICE_VULKAN__

namespace dmGraphics
{
    struct DeviceMemory
    {
        DeviceMemory()
        : m_Memory(VK_NULL_HANDLE)
        , m_MemorySize(0)
        {}

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

    const static uint8_t DM_RENDERTARGET_BACKBUFFER_ID = 0;
    const static uint8_t DM_MAX_VERTEX_STREAM_COUNT    = 8;

    struct VertexDeclaration
    {
        struct Stream
        {
            uint64_t m_NameHash;
            uint16_t m_Location;
            uint16_t m_Offset;
            VkFormat m_Format;

            // TODO: Not sure how to deal with normalizing
            //       a vertex stream in VK.
            // bool        m_Normalize;
        };

        uint64_t    m_Hash;
        Stream      m_Streams[DM_MAX_VERTEX_STREAM_COUNT];
        uint16_t    m_StreamCount;
        uint16_t    m_Stride;
    };

    struct GeometryBuffer
    {
        GeometryBuffer(const VkBufferUsageFlags usage)
        : m_Buffer(VK_NULL_HANDLE)
        , m_Usage(usage)
        , m_Frame(0)
        {}

        DeviceMemory       m_DeviceMemory;
        VkBuffer           m_Buffer;
        VkBufferUsageFlags m_Usage;
        uint8_t            m_Frame : 1;
        uint8_t                    : 7; // unused
    };

    struct ScratchBuffer
    {
        ScratchBuffer()
        : m_Buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        {}

        GeometryBuffer   m_Buffer;
        VkDescriptorSet* m_DescriptorSets;
        void*            m_Data;
        uint32_t         m_DataCursor;
        uint16_t         m_DescriptorSetsCount;
        uint16_t         m_DescriptorIndex;
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
        uint8_t                  : 7; // unused
    };

    struct Viewport
    {
        uint16_t m_X;
        uint16_t m_Y;
        uint16_t m_W;
        uint16_t m_H;
        uint8_t  m_HasChanged : 1;
        uint8_t               : 7; // unused
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

    union PipelineState
    {
        struct
        {
            uint64_t m_WriteColorMask     : 4;
            uint64_t m_WriteDepth         : 1;
            uint64_t m_PrimtiveType       : 3;
            // Depth Test
            uint64_t m_DepthTestEnabled   : 1;
            uint64_t m_DepthTestFunc      : 3;
            // Stencil Test
            uint64_t m_StencilEnabled     : 1;
            uint64_t m_StencilOpFail      : 3;
            uint64_t m_StencilOpPass      : 3;
            uint64_t m_StencilOpDepthFail : 3;
            uint64_t m_StencilTestFunc    : 3;
            uint64_t m_StencilWriteMask   : 8;
            uint64_t m_StencilCompareMask : 8;
            uint64_t m_StencilReference   : 8;
            // Blending
            uint64_t m_BlendEnabled       : 1;
            uint64_t m_BlendSrcFactor     : 4;
            uint64_t m_BlendDstFactor     : 4;
            // Culling
            uint64_t m_CullFaceEnabled    : 1;
            uint64_t m_CullFaceType       : 2;
            uint64_t                      : 3; // Unused
        };

        uint64_t m_State;
    };

    typedef VkPipeline Pipeline;

    struct ShaderResourceBinding
    {
        uint64_t                   m_NameHash;
        ShaderDesc::ShaderDataType m_Type;
        uint16_t                   m_Set;
        uint16_t                   m_Binding;
    };

    struct ShaderModule
    {
        uint64_t               m_Hash;
        VkShaderModule         m_Module;
        ShaderResourceBinding* m_Uniforms;
        ShaderResourceBinding* m_Attributes;
        uint32_t               m_UniformDataSize;
        uint16_t               m_UniformCount;
        uint16_t               m_AttributeCount;
    };

    struct Program
    {
        uint64_t                        m_Hash;
        uint32_t*                       m_UniformOffsets;
        uint8_t*                        m_UniformData;
        ShaderModule*                   m_VertexModule;
        ShaderModule*                   m_FragmentModule;
        VkDescriptorSetLayout           m_DescriptorSetLayout[2];
        VkPipelineShaderStageCreateInfo m_PipelineStageInfo[2];
        VkPipelineLayout                m_PipelineLayout;
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
    VkResult CreateDescriptorPool(VkDevice vk_device, VkDescriptorPoolSize* vk_pool_sizes, uint8_t numPoolSizes,
        uint16_t maxDescriptors, VkDescriptorPool* vk_descriptor_pool_out);
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
    VkResult CreateGeometryBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        VkDeviceSize vk_size, VkMemoryPropertyFlags vk_memory_flags, GeometryBuffer* bufferOut);
    VkResult CreateShaderModule(VkDevice vk_device,
        const void* source, size_t sourceSize, ShaderModule* shaderModuleOut);
    VkResult CreatePipeline(VkDevice vk_device, VkRect2D vk_scissor,
        const PipelineState pipelineState, Program* program, GeometryBuffer* vertexBuffer,
        HVertexDeclaration vertexDeclaration, const VkRenderPass vk_render_pass, Pipeline* pipelineOut);
    void           ResetPhysicalDevice(PhysicalDevice* device);
    void           ResetLogicalDevice(LogicalDevice* device);
    void           ResetRenderTarget(LogicalDevice* logicalDevice, RenderTarget* renderTarget);
    void           ResetTexture(VkDevice vk_device, Texture* texture);
    void           ResetGeometryBuffer(VkDevice vk_device, GeometryBuffer* buffer);
    void           ResetShaderModule(VkDevice vk_device, ShaderModule* shaderModule);
    void           ResetPipeline(VkDevice vk_device, Pipeline* pipeline);
    uint32_t       GetPhysicalDeviceCount(VkInstance vkInstance);
    void           GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize);
    bool           GetMemoryTypeIndex(VkPhysicalDevice vk_physical_device, uint32_t typeFilter, VkMemoryPropertyFlags vk_property_flags, uint32_t* memoryIndexOut);
    QueueFamily    GetQueueFamily(PhysicalDevice* device, const VkSurfaceKHR surface);
    const VkFormat GetSupportedTilingFormat(VkPhysicalDevice vk_physical_device, VkFormat* vk_format_candidates,
        uint32_t vk_num_format_candidates, VkImageTiling vk_tiling_type, VkFormatFeatureFlags vk_format_flags);
    VkResult TransitionImageLayout(VkDevice vk_device, VkCommandPool vk_command_pool, VkQueue vk_graphics_queue, VkImage vk_image,
        VkImageAspectFlags vk_image_aspect, VkImageLayout vk_from_layout, VkImageLayout vk_to_layout);
    VkResult UploadToGeometryBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device, VkCommandBuffer vk_command_buffer,
        VkDeviceSize size, VkDeviceSize offset, const void* data, GeometryBuffer* buffer);

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
