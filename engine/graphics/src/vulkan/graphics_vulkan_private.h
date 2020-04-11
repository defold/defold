#ifndef __GRAPHICS_DEVICE_VULKAN__
#define __GRAPHICS_DEVICE_VULKAN__

namespace dmGraphics
{
    const static uint8_t DM_MAX_SET_COUNT              = 2;
    const static uint8_t DM_MAX_VERTEX_STREAM_COUNT    = 8;
    const static uint8_t DM_MAX_TEXTURE_UNITS          = 32;
    const static uint8_t DM_RENDERTARGET_BACKBUFFER_ID = 0;

    enum VulkanResourceType
    {
        RESOURCE_TYPE_DEVICE_BUFFER        = 0,
        RESOURCE_TYPE_TEXTURE              = 1,
        RESOURCE_TYPE_DESCRIPTOR_ALLOCATOR = 2,
    };

    struct DeviceBuffer
    {
        DeviceBuffer(const VkBufferUsageFlags usage)
        : m_MappedDataPtr(0)
        , m_Usage(usage)
        , m_MemorySize(0)
        , m_Destroyed(0)
        {
            memset(&m_Handle, 0, sizeof(m_Handle));
        }

        struct VulkanHandle
        {
            VkBuffer       m_Buffer;
            VkDeviceMemory m_Memory;
        };

        void*              m_MappedDataPtr;
        VulkanHandle       m_Handle;
        VkBufferUsageFlags m_Usage;
        uint32_t           m_MemorySize : 31;
        uint32_t           m_Destroyed : 1;

        VkResult MapMemory(VkDevice vk_device, uint32_t offset = 0, uint32_t size = 0);
        void     UnmapMemory(VkDevice vk_device);

        const VulkanResourceType GetType();
    };

    struct Texture
    {
    	Texture();

        struct VulkanHandle
        {
            VkImage     m_Image;
            VkImageView m_ImageView;
        };

        VulkanHandle   m_Handle;
        TextureType    m_Type;
        TextureFormat  m_GraphicsFormat;
        VkFormat       m_Format;
        DeviceBuffer   m_DeviceBuffer;
        uint16_t       m_Width;
        uint16_t       m_Height;
        uint16_t       m_OriginalWidth;
        uint16_t       m_OriginalHeight;
        uint16_t       m_MipMapCount         : 5;
        uint16_t       m_TextureSamplerIndex : 10;
        uint32_t       m_Destroyed           : 1;

        const VulkanResourceType GetType();
    };

    struct TextureSampler
    {
        VkSampler     m_Sampler;
        TextureFilter m_MinFilter;
        TextureFilter m_MagFilter;
        TextureWrap   m_AddressModeU;
        TextureWrap   m_AddressModeV;
        uint8_t       m_MaxLod;
    };

    struct DescriptorAllocator
    {
        DescriptorAllocator()
        : m_DescriptorMax(0)
        , m_DescriptorIndex(0)
        , m_Destroyed(0)
        {
            memset(&m_Handle, 0, sizeof(m_Handle));
        }

        struct VulkanHandle
        {
            VkDescriptorPool m_DescriptorPool;
            VkDescriptorSet* m_DescriptorSets;
        };

        VulkanHandle     m_Handle;
        // 16 bits supports max 65536 draw calls
        uint32_t         m_DescriptorMax   : 15;
        uint32_t         m_DescriptorIndex : 15;
        uint32_t         m_Destroyed       : 1;
        uint32_t                           : 1; // unused

        VkResult Allocate(VkDevice vk_device, VkDescriptorSetLayout* vk_descriptor_set_layout, uint8_t setCount, VkDescriptorSet** vk_descriptor_set_out);
        void     Release(VkDevice vk_device);
        const    VulkanResourceType GetType();
    };

    struct ResourceToDestroy
    {
        union
        {
            DeviceBuffer::VulkanHandle        m_DeviceBuffer;
            Texture::VulkanHandle             m_Texture;
            DescriptorAllocator::VulkanHandle m_DescriptorAllocator;
        };
        VulkanResourceType m_ResourceType;
    };

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

    struct ScratchBuffer
    {
        ScratchBuffer()
        : m_DeviceBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        , m_MappedDataCursor(0)
        {}

        DescriptorAllocator* m_DescriptorAllocator;
        DeviceBuffer         m_DeviceBuffer;
        uint32_t             m_MappedDataCursor;
    };

    struct RenderTarget
    {
    	RenderTarget(const uint32_t rtId);
        Texture*       m_TextureColor;
        Texture*       m_TextureDepthStencil;
        TextureParams  m_BufferTextureParams[MAX_BUFFER_TYPE_COUNT];
        VkRenderPass   m_RenderPass;
        VkFramebuffer  m_Framebuffer;
        VkExtent2D     m_Extent;
        const uint16_t m_Id;
        uint8_t        m_IsBound : 1;
        uint8_t                  : 7; // unused
    };

    struct Viewport
    {
        uint16_t m_X;
        uint16_t m_Y;
        uint16_t m_W;
        uint16_t m_H;
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
            uint64_t m_WriteColorMask           : 4;
            uint64_t m_WriteDepth               : 1;
            uint64_t m_PrimtiveType             : 3;
            // Depth Test
            uint64_t m_DepthTestEnabled         : 1;
            uint64_t m_DepthTestFunc            : 3;
            // Stencil Test
            uint64_t m_StencilEnabled           : 1;
            uint64_t m_StencilOpFail            : 3;
            uint64_t m_StencilOpPass            : 3;
            uint64_t m_StencilOpDepthFail       : 3;
            uint64_t m_StencilTestFunc          : 3;
            uint64_t m_StencilWriteMask         : 8;
            uint64_t m_StencilCompareMask       : 8;
            uint64_t m_StencilReference         : 8;
            // Blending
            uint64_t m_BlendEnabled             : 1;
            uint64_t m_BlendSrcFactor           : 4;
            uint64_t m_BlendDstFactor           : 4;
            // Culling
            uint64_t m_CullFaceEnabled          : 1;
            uint64_t m_CullFaceType             : 2;
            // Polygon offset
            uint64_t m_PolygonOffsetFillEnabled : 1;
            uint64_t                            : 2; // Unused
        };

        uint64_t m_State;
    };

    typedef VkPipeline Pipeline;

    struct ShaderResourceBinding
    {
        char*                      m_Name;
        uint64_t                   m_NameHash;
        ShaderDesc::ShaderDataType m_Type;
        uint16_t                   m_Set;
        uint16_t                   m_Binding;
        union
        {
            uint16_t               m_UniformDataIndex;
            uint16_t               m_TextureUnit;
        };
    };

    struct ShaderModule
    {
        uint64_t               m_Hash;
        VkShaderModule         m_Module;
        ShaderResourceBinding* m_Uniforms;
        ShaderResourceBinding* m_Attributes;
        uint32_t               m_UniformDataSizeAligned;
        uint16_t               m_UniformCount;
        uint16_t               m_AttributeCount;
        uint16_t               m_UniformBufferCount;
    };

    struct Program
    {
        enum ModuleType
        {
            MODULE_TYPE_VERTEX   = 0,
            MODULE_TYPE_FRAGMENT = 1,
            MODULE_TYPE_COUNT    = 2
        };

        uint64_t                        m_Hash;
        uint32_t*                       m_UniformDataOffsets;
        uint8_t*                        m_UniformData;
        ShaderModule*                   m_VertexModule;
        ShaderModule*                   m_FragmentModule;
        VkDescriptorSetLayout           m_DescriptorSetLayout[MODULE_TYPE_COUNT];
        VkPipelineShaderStageCreateInfo m_PipelineStageInfo[MODULE_TYPE_COUNT];
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
        dmArray<VkImage>      m_Images;
        dmArray<VkImageView>  m_ImageViews;
        Texture               m_ResolveTexture;
        const VkSurfaceKHR    m_Surface;
        const QueueFamily     m_QueueFamily;
        VkSurfaceFormatKHR    m_SurfaceFormat;
        VkSwapchainKHR        m_SwapChain;
        VkExtent2D            m_ImageExtent;
        VkSampleCountFlagBits m_SampleCountFlag;
        uint8_t               m_ImageIndex;

        SwapChain(const VkSurfaceKHR surface, VkSampleCountFlagBits vk_sample_flag,
            const SwapChainCapabilities& capabilities, const QueueFamily queueFamily);
        VkResult Advance(VkDevice vk_device, VkSemaphore);
        bool     HasMultiSampling();
    };

    // Implemented in graphics_vulkan_context.cpp
    VkResult CreateInstance(VkInstance* vkInstanceOut,
        // Validation Layer Names, i.e "VK_LAYER_LUNARG_standard_validation"
        const char** validationLayers, uint16_t validationLayerCount,
        // Req. Validation Layer Extensions, i.e "VK_EXT_DEBUG_UTILS_EXTENSION_NAME"
        const char** validationLayerExtensions, uint16_t validationLayerExtensionCount);
    void     DestroyInstance(VkInstance* vkInstance);

    // Implemented in graphics_vulkan_device.cpp
    // Create functions
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
    VkResult CreateDescriptorAllocator(VkDevice vk_device, uint32_t descriptor_count, DescriptorAllocator* descriptorAllocator);
    VkResult CreateScratchBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        uint32_t bufferSize, bool clearData, DescriptorAllocator* descriptorAllocator, ScratchBuffer* scratchBufferOut);
    VkResult CreateTexture2D(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        uint32_t imageWidth, uint32_t imageHeight, uint16_t imageMips,
        VkSampleCountFlagBits vk_sample_count, VkFormat vk_format, VkImageTiling vk_tiling,
        VkImageUsageFlags vk_usage, VkMemoryPropertyFlags vk_memory_flags,
        VkImageAspectFlags vk_aspect, VkImageLayout vk_initial_layout, Texture* textureOut);
    VkResult CreateTextureSampler(VkDevice vk_device,
        VkFilter vk_min_filter, VkFilter vk_mag_filter, VkSamplerMipmapMode vk_mipmap_mode,
        VkSamplerAddressMode vk_wrap_u, VkSamplerAddressMode vk_wrap_v,
        float minLod, float maxLod, VkSampler* vk_sampler_out);
    VkResult CreateRenderPass(VkDevice vk_device, VkSampleCountFlagBits vk_sample_flags,
        RenderPassAttachment* colorAttachments, uint8_t numColorAttachments,
        RenderPassAttachment* depthStencilAttachment,
        RenderPassAttachment* resolveAttachment, VkRenderPass* renderPassOut);
    VkResult CreateDeviceBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        VkDeviceSize vk_size, VkMemoryPropertyFlags vk_memory_flags, DeviceBuffer* bufferOut);
    VkResult CreateShaderModule(VkDevice vk_device,
        const void* source, uint32_t sourceSize, ShaderModule* shaderModuleOut);
    VkResult CreatePipeline(VkDevice vk_device, VkRect2D vk_scissor, VkSampleCountFlagBits vk_sample_count,
        const PipelineState pipelineState, Program* program, DeviceBuffer* vertexBuffer,
        HVertexDeclaration vertexDeclaration, const VkRenderPass vk_render_pass, Pipeline* pipelineOut);
    // Reset functions
    void           ResetScratchBuffer(VkDevice vk_device, ScratchBuffer* scratchBuffer);
    // Destroy funcions
    void           DestroyPhysicalDevice(PhysicalDevice* device);
    void           DestroyLogicalDevice(LogicalDevice* device);
    void           DestroyRenderTarget(LogicalDevice* logicalDevice, RenderTarget* renderTarget);
    void           DestroyTexture(VkDevice vk_device, Texture::VulkanHandle* handle);
    void           DestroyDeviceBuffer(VkDevice vk_device, DeviceBuffer::VulkanHandle* handle);
    void           DestroyShaderModule(VkDevice vk_device, ShaderModule* shaderModule);
    void           DestroyPipeline(VkDevice vk_device, Pipeline* pipeline);
    void           DestroyDescriptorAllocator(VkDevice vk_device, DescriptorAllocator::VulkanHandle* handle);
    void           DestroyScratchBuffer(VkDevice vk_device, ScratchBuffer* scratchBuffer);
    void           DestroyTextureSampler(VkDevice vk_device, TextureSampler* sampler);
    // Get functions
    uint32_t       GetPhysicalDeviceCount(VkInstance vkInstance);
    void           GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize);
    bool           GetMemoryTypeIndex(VkPhysicalDevice vk_physical_device, uint32_t typeFilter, VkMemoryPropertyFlags vk_property_flags, uint32_t* memoryIndexOut);
    QueueFamily    GetQueueFamily(PhysicalDevice* device, const VkSurfaceKHR surface);
    const VkFormat GetSupportedTilingFormat(VkPhysicalDevice vk_physical_device, const VkFormat* vk_format_candidates,
        uint32_t vk_num_format_candidates, VkImageTiling vk_tiling_type, VkFormatFeatureFlags vk_format_flags);
    VkSampleCountFlagBits GetClosestSampleCountFlag(PhysicalDevice* physicalDevice, uint32_t bufferFlagBits, uint8_t sampleCount);
    // Misc functions
    VkResult TransitionImageLayout(VkDevice vk_device, VkCommandPool vk_command_pool, VkQueue vk_graphics_queue, VkImage vk_image,
        VkImageAspectFlags vk_image_aspect, VkImageLayout vk_from_layout, VkImageLayout vk_to_layout,
        uint32_t baseMipLevel = 0, uint32_t levelCount = 1);
    VkResult WriteToDeviceBuffer(VkDevice vk_device, VkDeviceSize size, VkDeviceSize offset, const void* data, DeviceBuffer* buffer);

    // Implemented in graphics_vulkan_swap_chain.cpp
    //   wantedWidth and wantedHeight might be written to, we might not get the
    //   dimensions we wanted from Vulkan.
    VkResult UpdateSwapChain(PhysicalDevice* physicalDevice, LogicalDevice* logicalDevice,
        uint32_t* wantedWidth, uint32_t* wantedHeight, const bool wantVSync,
        SwapChainCapabilities& capabilities, SwapChain* swapChain);
    void     DestroySwapChain(VkDevice vk_device, SwapChain* swapChain);
    void     GetSwapChainCapabilities(VkPhysicalDevice vk_device, const VkSurfaceKHR surface, SwapChainCapabilities& capabilities);

    // Implemented per supported platform
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI);
    bool     LoadVulkanLibrary();
    void     LoadVulkanFunctions(VkInstance vk_instance);
}
#endif // __GRAPHICS_DEVICE_VULKAN__
