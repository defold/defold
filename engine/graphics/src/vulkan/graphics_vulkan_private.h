// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef __GRAPHICS_DEVICE_VULKAN__
#define __GRAPHICS_DEVICE_VULKAN__

#include <stdint.h>
#include <dlib/hashtable.h>

#include "../graphics_private.h"

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
        RESOURCE_TYPE_PROGRAM              = 3,
    };

    struct DeviceBuffer
    {
        DeviceBuffer(){}
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
        float         m_MaxAnisotropy;
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
        // 15 bits supports max 32768 draw calls
        uint32_t         m_DescriptorMax   : 15;
        uint32_t         m_DescriptorIndex : 15;
        uint32_t         m_Destroyed       : 1;
        uint32_t                           : 1; // unused

        VkResult Allocate(VkDevice vk_device, VkDescriptorSetLayout* vk_descriptor_set_layout, uint8_t setCount, VkDescriptorSet** vk_descriptor_set_out);
        void     Release(VkDevice vk_device);
        const    VulkanResourceType GetType();
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
        Texture*       m_TextureColor[MAX_BUFFER_COLOR_ATTACHMENTS];
        Texture*       m_TextureDepthStencil;
        BufferType     m_ColorAttachmentBufferTypes[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams  m_BufferTextureParams[MAX_BUFFER_TYPE_COUNT];
        VkRenderPass   m_RenderPass;
        VkFramebuffer  m_Framebuffer;
        VkExtent2D     m_Extent;
        const uint16_t m_Id;
        uint8_t        m_IsBound              : 1;
        uint8_t        m_ColorAttachmentCount : 2;
        uint8_t                               : 5; // unused
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

    typedef VkPipeline Pipeline;

    struct ShaderResourceBinding
    {
        char*                      m_Name;
        uint64_t                   m_NameHash;
        ShaderDesc::ShaderDataType m_Type;
        uint16_t                   m_ElementCount;
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
        Program();

        enum ModuleType
        {
            MODULE_TYPE_VERTEX   = 0,
            MODULE_TYPE_FRAGMENT = 1,
            MODULE_TYPE_COUNT    = 2
        };

        struct VulkanHandle
        {
            VkDescriptorSetLayout m_DescriptorSetLayout[MODULE_TYPE_COUNT];
            VkPipelineLayout      m_PipelineLayout;
        };

        uint64_t                        m_Hash;
        uint32_t*                       m_UniformDataOffsets;
        uint8_t*                        m_UniformData;
        VulkanHandle                    m_Handle;
        ShaderModule*                   m_VertexModule;
        ShaderModule*                   m_FragmentModule;
        VkPipelineShaderStageCreateInfo m_PipelineStageInfo[MODULE_TYPE_COUNT];
        uint8_t                         m_Destroyed : 1;

        const VulkanResourceType GetType();
    };

    struct ResourceToDestroy
    {
        union
        {
            DeviceBuffer::VulkanHandle        m_DeviceBuffer;
            Texture::VulkanHandle             m_Texture;
            DescriptorAllocator::VulkanHandle m_DescriptorAllocator;
            Program::VulkanHandle             m_Program;
        };
        VulkanResourceType m_ResourceType;
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
        Texture*              m_ResolveTexture;
        const VkSurfaceKHR    m_Surface;
        const QueueFamily     m_QueueFamily;
        VkSurfaceFormatKHR    m_SurfaceFormat;
        VkSwapchainKHR        m_SwapChain;
        VkExtent2D            m_ImageExtent;
        VkSampleCountFlagBits m_SampleCountFlag;
        uint8_t               m_ImageIndex;

        SwapChain(const VkSurfaceKHR surface, VkSampleCountFlagBits vk_sample_flag,
            const SwapChainCapabilities& capabilities, const QueueFamily queueFamily,
            Texture* resolveTexture);
        VkResult Advance(VkDevice vk_device, VkSemaphore);
        bool     HasMultiSampling();
    };

    typedef dmHashTable64<Pipeline>    PipelineCache;
    typedef dmArray<ResourceToDestroy> ResourcesToDestroyList;

    // In flight frames - number of concurrent frames being processed
    static const uint8_t g_max_frames_in_flight       = 2;

    struct Context
    {
        Context(const ContextParams& params, const VkInstance vk_instance);

        Texture*                        m_TextureUnits[DM_MAX_TEXTURE_UNITS];
        PipelineCache                   m_PipelineCache;
        PipelineState                   m_PipelineState;
        SwapChain*                      m_SwapChain;
        SwapChainCapabilities           m_SwapChainCapabilities;
        PhysicalDevice                  m_PhysicalDevice;
        LogicalDevice                   m_LogicalDevice;
        FrameResource                   m_FrameResources[g_max_frames_in_flight];
        VkInstance                      m_Instance;
        VkSurfaceKHR                    m_WindowSurface;
        dmArray<TextureSampler>         m_TextureSamplers;
        uint32_t*                       m_DynamicOffsetBuffer;
        uint16_t                        m_DynamicOffsetBufferSize;
        // Window callbacks
        WindowResizeCallback            m_WindowResizeCallback;
        void*                           m_WindowResizeCallbackUserData;
        WindowCloseCallback             m_WindowCloseCallback;
        void*                           m_WindowCloseCallbackUserData;
        WindowFocusCallback             m_WindowFocusCallback;
        void*                           m_WindowFocusCallbackUserData;
        WindowIconifyCallback           m_WindowIconifyCallback;
        void*                           m_WindowIconifyCallbackUserData;
        // Main device rendering constructs
        dmArray<VkFramebuffer>          m_MainFrameBuffers;
        dmArray<VkCommandBuffer>        m_MainCommandBuffers;
        VkCommandBuffer                 m_MainCommandBufferUploadHelper;
        ResourcesToDestroyList*         m_MainResourcesToDestroy[3];
        dmArray<ScratchBuffer>          m_MainScratchBuffers;
        dmArray<DescriptorAllocator>    m_MainDescriptorAllocators;
        VkRenderPass                    m_MainRenderPass;
        Texture                         m_MainTextureDepthStencil;
        RenderTarget                    m_MainRenderTarget;
        Viewport                        m_MainViewport;
        // Rendering state
        RenderTarget*                   m_CurrentRenderTarget;
        DeviceBuffer*                   m_CurrentVertexBuffer;
        VertexDeclaration*              m_CurrentVertexDeclaration;
        Program*                        m_CurrentProgram;
        // Misc state
        TextureFilter                   m_DefaultTextureMinFilter;
        TextureFilter                   m_DefaultTextureMagFilter;
        Texture*                        m_DefaultTexture;
        uint64_t                        m_TextureFormatSupport;
        uint32_t                        m_Width;
        uint32_t                        m_Height;
        uint32_t                        m_WindowWidth;
        uint32_t                        m_WindowHeight;
        uint32_t                        m_FrameBegun           : 1;
        uint32_t                        m_CurrentFrameInFlight : 1;
        uint32_t                        m_WindowOpened         : 1;
        uint32_t                        m_VerifyGraphicsCalls  : 1;
        uint32_t                        m_ViewportChanged      : 1;
        uint32_t                        m_CullFaceChanged      : 1;
        uint32_t                        m_UseValidationLayers  : 1;
        uint32_t                        m_RenderDocSupport     : 1;
        uint32_t                                               : 24;
    };

    // Implemented in graphics_vulkan_context.cpp
    VkResult CreateInstance(VkInstance* vkInstanceOut,
        // Extension names, e.g. "VK_KHR_SURFACE_EXTENSION_NAME"
        const char** extensionNames, uint16_t extensionNameCount,
        // Validation Layer Names, i.e "VK_LAYER_LUNARG_standard_validation"
        const char** validationLayers, uint16_t validationLayerCount,
        // Req. Validation Layer Extensions, i.e "VK_EXT_DEBUG_UTILS_EXTENSION_NAME"
        const char** validationLayerExtensions, uint16_t validationLayerExtensionCount);
    void     DestroyInstance(VkInstance* vkInstance);

    // Implemented in graphics_vulkan.cpp
    VkResult CreateMainFrameBuffers(HContext context);
    VkResult DestroyMainFrameBuffers(HContext context);
    void SwapChainChanged(HContext context, uint32_t* width, uint32_t* height, VkResult (*cb)(void* ctx), void* cb_ctx);

    // Implemented in graphics_vulkan_device.cpp
    // Create functions
    VkResult CreateFramebuffer(VkDevice vk_device, VkRenderPass vk_render_pass,
        uint32_t width, uint32_t height,
        VkImageView* vk_attachments, uint8_t attachmentCount, // Color & depth/stencil attachments
        VkFramebuffer* vk_framebuffer_out);
    VkResult DestroyFrameBuffer(VkDevice vk_device, VkFramebuffer vk_framebuffer);
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
        float minLod, float maxLod, float max_anisotropy, VkSampler* vk_sampler_out);
    VkResult CreateRenderPass(VkDevice vk_device, VkSampleCountFlagBits vk_sample_flags,
        RenderPassAttachment* colorAttachments, uint8_t numColorAttachments,
        RenderPassAttachment* depthStencilAttachment,
        RenderPassAttachment* resolveAttachment, VkRenderPass* renderPassOut);
    void DestroyRenderPass(VkDevice vk_device, VkRenderPass render_pass);
    VkResult CreateDeviceBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        VkDeviceSize vk_size, VkMemoryPropertyFlags vk_memory_flags, DeviceBuffer* bufferOut);
    VkResult CreateShaderModule(VkDevice vk_device,
        const void* source, uint32_t sourceSize, ShaderModule* shaderModuleOut);
    VkResult CreatePipeline(VkDevice vk_device, VkRect2D vk_scissor, VkSampleCountFlagBits vk_sample_count,
        const PipelineState pipelineState, Program* program, DeviceBuffer* vertexBuffer,
        HVertexDeclaration vertexDeclaration, RenderTarget* render_target, Pipeline* pipelineOut);
    // Reset functions
    void           ResetScratchBuffer(VkDevice vk_device, ScratchBuffer* scratchBuffer);
    // Destroy funcions
    void           DestroyPhysicalDevice(PhysicalDevice* device);
    void           DestroyLogicalDevice(LogicalDevice* device);
    void           DestroyTexture(VkDevice vk_device, Texture::VulkanHandle* handle);
    void           DestroyDeviceBuffer(VkDevice vk_device, DeviceBuffer::VulkanHandle* handle);
    void           DestroyShaderModule(VkDevice vk_device, ShaderModule* shaderModule);
    void           DestroyPipeline(VkDevice vk_device, Pipeline* pipeline);
    void           DestroyDescriptorAllocator(VkDevice vk_device, DescriptorAllocator::VulkanHandle* handle);
    void           DestroyScratchBuffer(VkDevice vk_device, ScratchBuffer* scratchBuffer);
    void           DestroyTextureSampler(VkDevice vk_device, TextureSampler* sampler);
    void           DestroyProgram(VkDevice vk_device, Program::VulkanHandle* handle);
    // Get functions
    uint32_t       GetPhysicalDeviceCount(VkInstance vkInstance);
    void           GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize);
    bool           GetMemoryTypeIndex(VkPhysicalDevice vk_physical_device, uint32_t typeFilter, VkMemoryPropertyFlags vk_property_flags, uint32_t* memoryIndexOut);
    QueueFamily    GetQueueFamily(PhysicalDevice* device, const VkSurfaceKHR surface);
    const VkFormat GetSupportedTilingFormat(VkPhysicalDevice vk_physical_device, const VkFormat* vk_format_candidates,
        uint32_t vk_num_format_candidates, VkImageTiling vk_tiling_type, VkFormatFeatureFlags vk_format_flags);
    void           GetFormatProperties(VkPhysicalDevice vk_physical_device, VkFormat vk_format, VkFormatProperties* properties);
    VkSampleCountFlagBits GetClosestSampleCountFlag(PhysicalDevice* physicalDevice, uint32_t bufferFlagBits, uint8_t sampleCount);
    // Misc functions
    VkResult TransitionImageLayout(VkDevice vk_device, VkCommandPool vk_command_pool, VkQueue vk_graphics_queue, VkImage vk_image,
        VkImageAspectFlags vk_image_aspect, VkImageLayout vk_from_layout, VkImageLayout vk_to_layout,
        uint32_t baseMipLevel = 0, uint32_t layer_count = 1);
    VkResult WriteToDeviceBuffer(VkDevice vk_device, VkDeviceSize size, VkDeviceSize offset, const void* data, DeviceBuffer* buffer);

    void DestroyPipelineCacheCb(HContext context, const uint64_t* key, Pipeline* value);
    void FlushResourcesToDestroy(VkDevice vk_device, ResourcesToDestroyList* resource_list);

    // Implemented in graphics_vulkan_swap_chain.cpp
    //   wantedWidth and wantedHeight might be written to, we might not get the
    //   dimensions we wanted from Vulkan.
    VkResult UpdateSwapChain(PhysicalDevice* physicalDevice, LogicalDevice* logicalDevice,
        uint32_t* wantedWidth, uint32_t* wantedHeight, bool wantVSync,
        SwapChainCapabilities& capabilities, SwapChain* swapChain);
    void     DestroySwapChain(VkDevice vk_device, SwapChain* swapChain);
    void     GetSwapChainCapabilities(VkPhysicalDevice vk_device, const VkSurfaceKHR surface, SwapChainCapabilities& capabilities);

    // called from OpenWindow
    bool InitializeVulkan(HContext context, const WindowParams* params);
    void InitializeVulkanTexture(Texture* t);

    void OnWindowResize(int width, int height);
    int OnWindowClose();
    void OnWindowFocus(int focus);

    inline void SynchronizeDevice(VkDevice vk_device)
    {
        vkDeviceWaitIdle(vk_device);
    }

    // Implemented per supported platform

    const char** GetExtensionNames(uint16_t* num_extensions);
    const char** GetValidationLayers(uint16_t* num_layers, bool use_validation, bool use_renderdoc);
    const char** GetValidationLayersExt(uint16_t* num_layers);

    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI);

    bool     LoadVulkanLibrary();
    void     LoadVulkanFunctions(VkInstance vk_instance);


    void NativeSwapBuffers(HContext context);
    bool NativeInit(const struct ContextParams& params);
    void NativeExit();

    void NativeBeginFrame(HContext context);

    WindowResult VulkanOpenWindow(HContext context, WindowParams* params);
    void VulkanCloseWindow(HContext context);
    uint32_t VulkanGetDisplayDpi(HContext context);
    uint32_t VulkanGetWidth(HContext context);
    uint32_t VulkanGetHeight(HContext context);
    uint32_t VulkanGetWindowWidth(HContext context);
    uint32_t VulkanGetWindowHeight(HContext context);
    float VulkanGetDisplayScaleFactor(HContext context);
    uint32_t VulkanGetWindowRefreshRate(HContext context);
    void VulkanSetWindowSize(HContext context, uint32_t width, uint32_t height);
    void VulkanResizeWindow(HContext context, uint32_t width, uint32_t height);
    void VulkanSetWindowSize(HContext context, uint32_t width, uint32_t height);
    void VulkanGetNativeWindowSize(uint32_t* width, uint32_t* height);
    void VulkanIconifyWindow(HContext context);
    uint32_t VulkanGetWindowState(HContext context, WindowState state);
}
#endif // __GRAPHICS_DEVICE_VULKAN__
