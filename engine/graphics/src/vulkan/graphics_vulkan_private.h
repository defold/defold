// Copyright 2020-2025 The Defold Foundation
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
#include <dlib/opaque_handle_container.h>

#include "../graphics_private.h"

#include <dmsdk/dlib/atomic.h>
#include <dmsdk/graphics/graphics_vulkan.h>

namespace dmGraphics
{
    struct ResourceToDestroy;

    typedef VkPipeline                 Pipeline;
    typedef dmHashTable64<Pipeline>    PipelineCache;
    typedef dmArray<ResourceToDestroy> ResourcesToDestroyList;

    const static uint8_t DM_MAX_FRAMES_IN_FLIGHT = 2; // In flight frames - number of concurrent frames being processed
    const static uint8_t MAX_FENCE_RESOURCES_TO_DESTROY_PER_ENTRY = 2; // Increase if necessary (or make fully dynamic)

    enum VulkanResourceType
    {
        RESOURCE_TYPE_DEVICE_BUFFER  = 0,
        RESOURCE_TYPE_TEXTURE        = 1,
        RESOURCE_TYPE_PROGRAM        = 2,
        RESOURCE_TYPE_RENDER_TARGET  = 3,
        RESOURCE_TYPE_COMMAND_BUFFER = 4,
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
            uint8_t        m_LastUsedFrame;
        };

        void*              m_MappedDataPtr;
        VulkanHandle       m_Handle;
        VkBufferUsageFlags m_Usage;
        uint32_t           m_MemorySize : 31;
        uint32_t           m_Destroyed  : 1;

        VkResult MapMemory(VkDevice vk_device, uint32_t offset = 0, uint32_t size = 0);
        void     UnmapMemory(VkDevice vk_device);

        const VulkanResourceType GetType();
    };

    struct VulkanTexture
    {
        struct VulkanHandle
        {
            VkImage     m_Image;
            VkImageView m_ImageView;
            uint8_t     m_LastUsedFrame;
        };

        VulkanHandle      m_Handle;
        TextureType       m_Type;
        TextureFormat     m_GraphicsFormat;
        VkFormat          m_Format;
        VkImageLayout     m_ImageLayout[16];
        VkImageUsageFlags m_UsageFlags;
        DeviceBuffer      m_DeviceBuffer;
        int32_atomic_t    m_DataState; // data state per mip-map (mipX = bitX). 0=ok, 1=pending
        HOpaqueHandle     m_PendingUpload;
        uint16_t          m_Width;
        uint16_t          m_Height;
        uint16_t          m_Depth;
        uint16_t          m_OriginalWidth;
        uint16_t          m_OriginalHeight;
        uint16_t          m_OriginalDepth;
        uint16_t          m_MipMapCount         : 5;
        uint16_t          m_TextureSamplerIndex : 10;
        uint32_t          m_Destroyed           : 1;
        uint32_t          m_UsageHintFlags      : 8;
        uint8_t           m_LayerCount;
        uint8_t           m_PageCount; // page count of texture array

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

    struct DescriptorPool
    {
        VkDescriptorPool m_DescriptorPool;
        uint32_t         m_DescriptorCount;
    };

    struct DescriptorAllocator
    {
        dmArray<DescriptorPool> m_DescriptorPools;
        VkDescriptorSet*        m_DescriptorSets;
        uint32_t                m_DescriptorSetIndex;
        uint32_t                m_DescriptorSetMax;
        uint32_t                m_DescriptorPoolIndex;
        uint32_t                m_DescriptorsPerPool;

        VkResult Allocate(VkDevice vk_device, VkDescriptorSetLayout* vk_descriptor_set_layout, uint8_t setCount, uint32_t descriptor_count, VkDescriptorSet** vk_descriptor_set_out);
        void     Reset(VkDevice vk_device);
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

    struct SubPass
    {
        dmArray<uint8_t> m_ColorAttachments;
        dmArray<uint8_t> m_InputAttachments;
        bool             m_DepthStencilAttachment;
    };

    struct RenderTarget
    {
    	RenderTarget(const uint32_t rtId);

        struct VulkanHandle
        {
            VkRenderPass  m_RenderPass;
            VkFramebuffer m_Framebuffer;
            uint8_t       m_LastUsedFrame;
        };

        VulkanHandle   m_Handle;

        AttachmentOp   m_ColorBufferLoadOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        AttachmentOp   m_ColorBufferStoreOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        float          m_ColorAttachmentClearValue[MAX_BUFFER_COLOR_ATTACHMENTS][4];

        BufferType     m_ColorAttachmentBufferTypes[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams  m_ColorTextureParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams  m_DepthStencilTextureParams;
        SubPass*       m_SubPasses;
        HTexture       m_TextureColor[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture       m_TextureDepthStencil;

        VkExtent2D     m_Extent;
        VkRect2D       m_Scissor;

        const uint16_t m_Id;
        uint32_t       m_Destroyed            : 1;
        uint32_t       m_IsBound              : 1;
        uint32_t       m_ColorAttachmentCount : 7;
        uint32_t       m_SubPassCount         : 8;
        uint32_t       m_SubPassIndex         : 8;

        const VulkanResourceType GetType();
    };

    struct StorageBufferBinding
    {
        HStorageBuffer m_Buffer;
        uint32_t       m_BufferOffset;
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
        VkFormat            m_Format;
        VkImageLayout       m_ImageLayout;
        VkImageLayout       m_ImageLayoutInitial;
        VkAttachmentLoadOp  m_LoadOp;
        VkAttachmentStoreOp m_StoreOp;
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
        VkPhysicalDeviceFeatures2        m_Features2;
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
        VkCommandPool m_CommandPoolWorker;
    };

    struct ShaderModule
    {
        uint64_t                        m_Hash;
        VkShaderModule                  m_Module;
        VkPipelineShaderStageCreateInfo m_PipelineStageInfo;
    };

    struct VulkanProgram
    {
        VulkanProgram()
        {
            memset(this, 0, sizeof(*this));
        }

        struct VulkanHandle
        {
            VkDescriptorSetLayout m_DescriptorSetLayouts[MAX_SET_COUNT];
            VkPipelineLayout      m_PipelineLayout;
            uint8_t               m_DescriptorSetLayoutsCount;
            uint8_t               m_LastUsedFrame;
        };

        Program        m_BaseProgram;

        uint64_t       m_Hash;
        uint8_t*       m_UniformData;
        VulkanHandle   m_Handle;

        ShaderModule*  m_VertexModule;
        ShaderModule*  m_FragmentModule;
        ShaderModule*  m_ComputeModule;

        uint32_t       m_UniformDataSizeAligned;
        uint16_t       m_UniformBufferCount;
        uint16_t       m_StorageBufferCount;
        uint16_t       m_TextureSamplerCount;
        uint16_t       m_TotalResourcesCount;
        uint8_t        m_Destroyed : 1;

        const VulkanResourceType GetType();
    };

    struct VulkanCommandBuffer
    {
        struct VulkanHandle
        {
            VkCommandBuffer m_CommandBuffer;
            VkCommandPool   m_CommandPool;
        };

        VulkanHandle m_Handle;
    };

    struct ResourceToDestroy
    {
        union
        {
            DeviceBuffer::VulkanHandle        m_DeviceBuffer;
            VulkanTexture::VulkanHandle       m_Texture;
            VulkanProgram::VulkanHandle       m_Program;
            RenderTarget::VulkanHandle        m_RenderTarget;
            VulkanCommandBuffer::VulkanHandle m_CommandBuffer;
        };
        VulkanResourceType m_ResourceType;
    };

    struct FenceResourcesToDestroy
    {
        VkFence           m_Fence;
        ResourceToDestroy m_Resources[MAX_FENCE_RESOURCES_TO_DESTROY_PER_ENTRY];
        uint8_t           m_ResourcesCount;
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
        SwapChain(const VkSurfaceKHR     surface,
            VkSampleCountFlagBits        vk_sample_flag,
            const SwapChainCapabilities& capabilities,
            const QueueFamily            queueFamily,
            VulkanTexture*               resolveTexture);

        dmArray<VkImage>      m_Images;
        dmArray<VkImageView>  m_ImageViews;
        VulkanTexture*        m_ResolveTexture;
        const VkSurfaceKHR    m_Surface;
        const QueueFamily     m_QueueFamily;
        VkSurfaceFormatKHR    m_SurfaceFormat;
        VkSwapchainKHR        m_SwapChain;
        VkExtent2D            m_ImageExtent;
        VkSampleCountFlagBits m_SampleCountFlag;
        uint8_t               m_ImageIndex;

        VkResult Advance(VkDevice vk_device, VkSemaphore);
        bool     HasMultiSampling();

        VkImage     Image()     { return m_Images[m_ImageIndex]; }
        VkImageView ImageView() { return m_ImageViews[m_ImageIndex]; }
    };

    struct VulkanContext
    {
        VulkanContext(const ContextParams& params, const VkInstance vk_instance);

        dmPlatform::HWindow                m_Window;
        dmPlatform::WindowResizeCallback   m_WindowResizeCallback;
        HTexture                           m_TextureUnits[DM_MAX_TEXTURE_UNITS];
        dmOpaqueHandleContainer<uintptr_t> m_AssetHandleContainer;
        PipelineCache                      m_PipelineCache;
        PipelineState                      m_PipelineState;
        SwapChain*                         m_SwapChain;
        SwapChainCapabilities              m_SwapChainCapabilities;
        PhysicalDevice                     m_PhysicalDevice;
        LogicalDevice                      m_LogicalDevice;
        FrameResource                      m_FrameResources[DM_MAX_FRAMES_IN_FLIGHT];
        VkInstance                         m_Instance;
        VkSurfaceKHR                       m_WindowSurface;
        dmArray<TextureSampler>            m_TextureSamplers;
        uint32_t*                          m_DynamicOffsetBuffer;
        uint16_t                           m_DynamicOffsetBufferSize;

        VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT m_FragmentShaderInterlockFeatures;

        // Async process resources
        dmJobThread::HContext              m_JobThread;
        SetTextureAsyncState               m_SetTextureAsyncState;
        dmMutex::HMutex                    m_AssetHandleContainerMutex;

        // Per-fence resources
        dmOpaqueHandleContainer<FenceResourcesToDestroy> m_FenceResourcesToDestroy;

        // Main device rendering constructs
        dmArray<VkFramebuffer>          m_MainFrameBuffers; // One per swap chain image
        VkCommandBuffer                 m_MainCommandBuffers[DM_MAX_FRAMES_IN_FLIGHT];
        VkCommandBuffer                 m_MainCommandBufferUploadHelper;
        ResourcesToDestroyList*         m_MainResourcesToDestroy[DM_MAX_FRAMES_IN_FLIGHT];
        ScratchBuffer                   m_MainScratchBuffers[DM_MAX_FRAMES_IN_FLIGHT];
        DescriptorAllocator             m_MainDescriptorAllocators[DM_MAX_FRAMES_IN_FLIGHT];
        VkRenderPass                    m_MainRenderPass;
        VulkanTexture                   m_MainTextureDepthStencil;
        HRenderTarget                   m_MainRenderTarget;
        Viewport                        m_MainViewport;
        VertexDeclaration               m_MainVertexDeclaration[MAX_VERTEX_BUFFERS];

        // Rendering state
        HRenderTarget                   m_CurrentRenderTarget;
        DeviceBuffer*                   m_CurrentVertexBuffer[MAX_VERTEX_BUFFERS];
        VertexDeclaration*              m_CurrentVertexDeclaration[MAX_VERTEX_BUFFERS];
        uint32_t                        m_CurrentVertexBufferOffset[MAX_VERTEX_BUFFERS];
        StorageBufferBinding            m_CurrentStorageBuffers[MAX_STORAGE_BUFFERS];
        VulkanProgram*                  m_CurrentProgram;
        Pipeline*                       m_CurrentPipeline;
        HTexture                        m_CurrentSwapchainTexture;

        // Misc state
        TextureFilter                   m_DefaultTextureMinFilter;
        TextureFilter                   m_DefaultTextureMagFilter;
        VulkanTexture*                  m_DefaultTexture2D;
        VulkanTexture*                  m_DefaultTexture2DArray;
        VulkanTexture*                  m_DefaultTextureCubeMap;
        VulkanTexture*                  m_DefaultTexture2D32UI;
        VulkanTexture*                  m_DefaultStorageImage2D;
        VulkanTexture                   m_ResolveTexture;
        uint64_t                        m_TextureFormatSupport;
        int32_atomic_t                  m_DeleteContextRequested;

        uint32_t                        m_Width;
        uint32_t                        m_Height;
        uint32_t                        m_WindowWidth;
        uint32_t                        m_WindowHeight;
        uint32_t                        m_SwapInterval;
        uint32_t                        m_FrameBegun           : 1;
        uint32_t                        m_CurrentFrameInFlight : 1;
        uint32_t                        m_NumFramesInFlight    : 2;
        uint32_t                        m_VerifyGraphicsCalls  : 1;
        uint32_t                        m_ViewportChanged      : 1;
        uint32_t                        m_CullFaceChanged      : 1;
        uint32_t                        m_UseValidationLayers  : 1;
        uint32_t                        m_RenderDocSupport     : 1;
        uint32_t                        m_ASTCSupport          : 1;
        // See OpenGL backend: separate flag for ASTC array textures
        uint32_t                        m_ASTCArrayTextureSupport : 1;
        uint32_t                        m_AsyncProcessingSupport : 1;
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
    VkResult CreateMainFrameBuffers(VulkanContext* context);
    VkResult DestroyMainFrameBuffers(VulkanContext* context);
    void     SwapChainChanged(VulkanContext* context, uint32_t* width, uint32_t* height, VkResult (*cb)(void* ctx), void* cb_ctx);

    // Implemented in graphics_vulkan_device.cpp
    // Create functions
    VkResult CreateFramebuffer(VkDevice vk_device, VkRenderPass vk_render_pass, uint32_t width, uint32_t height, VkImageView* vk_attachments, uint8_t attachmentCount, VkFramebuffer* vk_framebuffer_out);
    VkResult CreateCommandBuffers(VkDevice vk_device, VkCommandPool vk_command_pool, uint32_t numBuffersToCreate, VkCommandBuffer* vk_command_buffers_out);
    VkResult CreateDescriptorPool(VkDevice vk_device, uint16_t maxDescriptors, VkDescriptorPool* vk_descriptor_pool_out);
    VkResult CreateLogicalDevice(PhysicalDevice* device, const VkSurfaceKHR surface, const QueueFamily queueFamily, const char** deviceExtensions, const uint8_t deviceExtensionCount, const char** validationLayers, const uint8_t validationLayerCount, void* pNext, LogicalDevice* logicalDeviceOut);
    VkResult CreateDescriptorAllocator(VkDevice vk_device, uint32_t descriptor_count, DescriptorAllocator* descriptorAllocator);
    VkResult CreateScratchBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device, uint32_t bufferSize, bool clearData, DescriptorAllocator* descriptorAllocator, ScratchBuffer* scratchBufferOut);
    VkResult CreateTexture(VkPhysicalDevice vk_physical_device, VkDevice vk_device, uint32_t imageWidth, uint32_t imageHeight, uint32_t imageDepth, uint32_t imageLayers, uint16_t imageMips, VkSampleCountFlagBits vk_sample_count, VkFormat vk_format, VkImageTiling vk_tiling, VkImageUsageFlags vk_usage, VkMemoryPropertyFlags vk_memory_flags, VkImageAspectFlags vk_aspect, VulkanTexture* textureOut);
    VkResult CreateTextureSampler(VkDevice vk_device, VkFilter vk_min_filter, VkFilter vk_mag_filter, VkSamplerMipmapMode vk_mipmap_mode, VkSamplerAddressMode vk_wrap_u, VkSamplerAddressMode vk_wrap_v, float minLod, float maxLod, float max_anisotropy, VkSampler* vk_sampler_out);
    VkResult CreateRenderPass(VkDevice vk_device, VkSampleCountFlagBits vk_sample_flags, RenderPassAttachment* colorAttachments, uint8_t numColorAttachments, RenderPassAttachment* depthStencilAttachment, RenderPassAttachment* resolveAttachment, VkRenderPass* renderPassOut);
    VkResult CreateDeviceBuffer(VkPhysicalDevice vk_physical_device, VkDevice vk_device, VkDeviceSize vk_size, VkMemoryPropertyFlags vk_memory_flags, DeviceBuffer* bufferOut);
    VkResult CreateShaderModule(VkDevice vk_device, const void* source, uint32_t sourceSize, VkShaderStageFlagBits stage_flag, ShaderModule* shaderModuleOut);
    VkResult CreateGraphicsPipeline(VkDevice vk_device, VkRect2D vk_scissor, VkSampleCountFlagBits vk_sample_count, const PipelineState pipelineState, VulkanProgram* program, VertexDeclaration** vertexDeclarations, uint32_t vertexDeclarationCount, RenderTarget* render_target, Pipeline* pipelineOut);
    VkResult CreateComputePipeline(VkDevice vk_device, VulkanProgram* program, Pipeline* pipelineOut);

    // Destroy functions
    void DestroyDeviceBuffer(VkDevice vk_device, DeviceBuffer::VulkanHandle* handle);
    void DestroyDescriptorAllocator(VkDevice vk_device, DescriptorAllocator* allocator);
    void DestroyFrameBuffer(VkDevice vk_device, VkFramebuffer vk_framebuffer);
    void DestroyLogicalDevice(LogicalDevice* device);
    void DestroyPhysicalDevice(PhysicalDevice* device);
    void DestroyPipeline(VkDevice vk_device, Pipeline* pipeline);
    void DestroyProgram(VkDevice vk_device, VulkanProgram::VulkanHandle* handle);
    void DestroyRenderPass(VkDevice vk_device, VkRenderPass render_pass);
    void DestroyScratchBuffer(VkDevice vk_device, ScratchBuffer* scratchBuffer);
    void DestroyShaderModule(VkDevice vk_device, ShaderModule* shaderModule);
    void DestroyTextureSampler(VkDevice vk_device, TextureSampler* sampler);
    void DestroyTexture(VkDevice vk_device, VulkanTexture::VulkanHandle* handle);
    void DestroyRenderTarget(VkDevice vk_device, RenderTarget::VulkanHandle* handle);

    // Get functions
    uint32_t              GetPhysicalDeviceCount(VkInstance vkInstance);
    void                  GetPhysicalDevices(VkInstance vkInstance, PhysicalDevice** deviceListOut, uint32_t deviceListSize, void* pNextFeature);
    bool                  GetMemoryTypeIndex(VkPhysicalDevice vk_physical_device, uint32_t typeFilter, VkMemoryPropertyFlags vk_property_flags, uint32_t* memoryIndexOut);
    QueueFamily           GetQueueFamily(PhysicalDevice* device, const VkSurfaceKHR surface);
    const VkFormat        GetSupportedTilingFormat(VkPhysicalDevice vk_physical_device, const VkFormat* vk_format_candidates, uint32_t vk_num_format_candidates, VkImageTiling vk_tiling_type, VkFormatFeatureFlags vk_format_flags);
    void                  GetFormatProperties(VkPhysicalDevice vk_physical_device, VkFormat vk_format, VkFormatProperties* properties);
    VkSampleCountFlagBits GetClosestSampleCountFlag(PhysicalDevice* physicalDevice, uint32_t bufferFlagBits, uint8_t sampleCount);

    // Misc functions
    void            TransitionImageLayoutWithCmdBuffer(VkCommandBuffer vk_command_buffer, VulkanTexture* texture, VkImageAspectFlags vk_image_aspect, VkImageLayout vk_to_layout, uint32_t base_mip_level, uint32_t layer_count);
    VkResult        TransitionImageLayout(VkDevice vk_device, VkCommandPool vk_command_pool, VkQueue vk_graphics_queue, VulkanTexture* texture, VkImageAspectFlags vk_image_aspect, VkImageLayout vk_to_layout, uint32_t baseMipLevel = 0, uint32_t layer_count = 1);
    VkResult        WriteToDeviceBuffer(VkDevice vk_device, VkDeviceSize size, VkDeviceSize offset, const void* data, DeviceBuffer* buffer);
    void            DestroyPipelineCacheCb(VulkanContext* context, const uint64_t* key, Pipeline* value);
    void            FlushResourcesToDestroy(VulkanContext* context, ResourcesToDestroyList* resource_list);
    void            ResetScratchBuffer(VkDevice vk_device, ScratchBuffer* scratchBuffer);
    VkCommandBuffer BeginSingleTimeCommands(VkDevice device, VkCommandPool cmd_pool);
    VkResult        SubmitCommandBuffer(VkDevice vk_device, VkQueue queue, VkCommandBuffer cmd, VkFence* fence_out);

    // Implemented in graphics_vulkan_swap_chain.cpp
    //   wantedWidth and wantedHeight might be written to, we might not get the
    //   dimensions we wanted from Vulkan.
    VkResult UpdateSwapChain(PhysicalDevice* physicalDevice, LogicalDevice* logicalDevice, uint32_t* wantedWidth, uint32_t* wantedHeight, bool wantVSync, SwapChainCapabilities& capabilities, SwapChain* swapChain);
    void     DestroySwapChain(VkDevice vk_device, SwapChain* swapChain);
    void     GetSwapChainCapabilities(VkPhysicalDevice vk_device, const VkSurfaceKHR surface, SwapChainCapabilities& capabilities);

    bool InitializeVulkan(HContext context);
    void InitializeVulkanTexture(VulkanTexture* t);

    void OnWindowResize(int width, int height);
    int  OnWindowClose();
    void OnWindowFocus(int focus);

    static inline void SynchronizeDevice(VkDevice vk_device)
    {
        vkDeviceWaitIdle(vk_device);
    }

    // Implemented per supported platform
    const char** GetExtensionNames(uint16_t* num_extensions);
    const char** GetValidationLayers(uint16_t* num_layers, bool use_validation, bool use_renderdoc);
    const char** GetValidationLayersExt(uint16_t* num_layers);

    VkResult     CreateWindowSurface(dmPlatform::HWindow window, VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI);

    bool         LoadVulkanLibrary();
    void         LoadVulkanFunctions(VkInstance vk_instance);

    bool         NativeInit(const struct ContextParams& params);
    void         NativeExit();
    void         NativeBeginFrame(HContext context);
    bool         NativeInitializeContext(HContext context);

    void         VulkanCloseWindow(HContext context);
    void         VulkanDestroyResources(HContext context);
    float        VulkanGetDisplayScaleFactor(HContext context);
    void         VulkanSetWindowSize(HContext context, uint32_t width, uint32_t height);
    void         VulkanResizeWindow(HContext context, uint32_t width, uint32_t height);
    void         VulkanSetWindowSize(HContext context, uint32_t width, uint32_t height);
    void         VulkanGetNativeWindowSize(HContext context, uint32_t* width, uint32_t* height);
}
#endif // __GRAPHICS_DEVICE_VULKAN__
