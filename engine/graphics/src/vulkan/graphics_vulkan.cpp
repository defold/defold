#include <graphics/glfw/glfw.h>
#include <graphics/glfw/glfw_native.h>

#include <dlib/math.h>
#include <dlib/hashtable.h>
#include <dlib/array.h>
#include <dlib/profile.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <dmsdk/vectormath/cpp/vectormath_aos.h>

#include "../graphics.h"
#include "graphics_vulkan_defines.h"
#include "graphics_vulkan_private.h"

namespace dmGraphics
{
    static const char* VkResultToStr(VkResult res);
    #define CHECK_VK_ERROR(result) \
    { \
        if(g_Context->m_VerifyGraphicsCalls && result != VK_SUCCESS) { \
            dmLogError("Vulkan Error (%s:%d) %s", __FILE__, __LINE__, VkResultToStr(result)); \
            assert(0); \
        } \
    }

    // Validation layers to enable
    static const char*   g_validation_layers[]        = { "VK_LAYER_LUNARG_standard_validation" };
    static const uint8_t g_validation_layer_count     = 1;
    // Validation layer extensions
    static const char*   g_validation_layer_ext[]     = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    static const uint8_t g_validation_layer_ext_count = 1;
    // In flight frames - number of concurrent frames being processed
    static const uint8_t g_max_frames_in_flight       = 2;

    typedef dmHashTable64<Pipeline> PipelineCache;

    static struct Context
    {
        Context(const ContextParams& params, const VkInstance vk_instance)
        : m_MainRenderTarget(0)
        {
            memset(this, 0, sizeof(*this));
            m_Instance                = vk_instance;
            m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
            m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
            m_VerifyGraphicsCalls     = params.m_VerifyGraphicsCalls;
        }

        ~Context()
        {
            if (m_Instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(m_Instance, 0);
                m_Instance = VK_NULL_HANDLE;
            }
        }

        PipelineCache                m_PipelineCache;
        PipelineState                m_PipelineState;
        SwapChain*                   m_SwapChain;
        SwapChainCapabilities        m_SwapChainCapabilities;
        PhysicalDevice               m_PhysicalDevice;
        LogicalDevice                m_LogicalDevice;
        FrameResource                m_FrameResources[g_max_frames_in_flight];
        VkInstance                   m_Instance;
        VkSurfaceKHR                 m_WindowSurface;
        dmArray<DescriptorAllocator> m_DescriptorAllocatorsToDelete;
        dmArray<DeviceBuffer>        m_DeviceBuffersToDelete;
        uint32_t*                    m_DynamicOffsetBuffer;
        uint16_t                     m_DynamicOffsetBufferSize;

        // Window callbacks
        WindowResizeCallback     m_WindowResizeCallback;
        void*                    m_WindowResizeCallbackUserData;
        WindowCloseCallback      m_WindowCloseCallback;
        void*                    m_WindowCloseCallbackUserData;
        WindowFocusCallback      m_WindowFocusCallback;
        void*                    m_WindowFocusCallbackUserData;

        // Main device rendering constructs
        dmArray<VkFramebuffer>       m_MainFrameBuffers;
        dmArray<VkCommandBuffer>     m_MainCommandBuffers;
        dmArray<ScratchBuffer>       m_MainScratchBuffers;
        dmArray<DescriptorAllocator> m_MainDescriptorAllocators;
        VkRenderPass                 m_MainRenderPass;
        Texture                      m_MainTextureDepthStencil;
        RenderTarget                 m_MainRenderTarget;
        Viewport                     m_MainViewport;

        // Rendering state
        RenderTarget*            m_CurrentRenderTarget;
        DeviceBuffer*            m_CurrentVertexBuffer;
        VertexDeclaration*       m_CurrentVertexDeclaration;
        Program*                 m_CurrentProgram;

        TextureFilter            m_DefaultTextureMinFilter;
        TextureFilter            m_DefaultTextureMagFilter;
        uint32_t                 m_Width;
        uint32_t                 m_Height;
        uint32_t                 m_WindowWidth;
        uint32_t                 m_WindowHeight;
        uint32_t                 m_FrameBegun           : 1;
        uint32_t                 m_CurrentFrameInFlight : 1;
        uint32_t                 m_WindowOpened         : 1;
        uint32_t                 m_VerifyGraphicsCalls  : 1;
        uint32_t                 : 28;
    } *g_Context = 0;

    #define DM_VK_RESULT_TO_STR_CASE(x) case x: return #x
    static const char* VkResultToStr(VkResult res)
    {
        switch(res)
        {
            DM_VK_RESULT_TO_STR_CASE(VK_SUCCESS);
            DM_VK_RESULT_TO_STR_CASE(VK_NOT_READY);
            DM_VK_RESULT_TO_STR_CASE(VK_TIMEOUT);
            DM_VK_RESULT_TO_STR_CASE(VK_EVENT_SET);
            DM_VK_RESULT_TO_STR_CASE(VK_EVENT_RESET);
            DM_VK_RESULT_TO_STR_CASE(VK_INCOMPLETE);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INITIALIZATION_FAILED);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_DEVICE_LOST);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_MEMORY_MAP_FAILED);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_LAYER_NOT_PRESENT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_TOO_MANY_OBJECTS);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FRAGMENTED_POOL);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_SURFACE_LOST_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_SUBOPTIMAL_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_OUT_OF_DATE_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INVALID_SHADER_NV);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FRAGMENTATION_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_NOT_PERMITTED_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_INVALID_DEVICE_ADDRESS_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
            DM_VK_RESULT_TO_STR_CASE(VK_RESULT_MAX_ENUM);
            default: break;
        }

        return "UNKNOWN_ERROR";
    }
    #undef DM_VK_RESULT_TO_STRING_CASE

    static inline void SynchronizeDevice(VkDevice vk_device)
    {
        vkDeviceWaitIdle(vk_device);
    }

    static inline uint32_t GetNextRenderTargetId()
    {
        static uint32_t next_id = 1;

        // DM_RENDERTARGET_BACKBUFFER_ID is taken for the main framebuffer
        if (next_id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            next_id = DM_RENDERTARGET_BACKBUFFER_ID + 1;
        }

        return next_id++;
    }

    static VkResult CreateMainFrameSyncObjects(VkDevice vk_device, uint8_t numFrameResources, FrameResource* frameResourcesOut)
    {
        VkSemaphoreCreateInfo vk_create_semaphore_info;
        memset(&vk_create_semaphore_info, 0, sizeof(vk_create_semaphore_info));
        vk_create_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo vk_create_fence_info;
        memset(&vk_create_fence_info, 0, sizeof(vk_create_fence_info));
        vk_create_fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vk_create_fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for(uint8_t i=0; i < numFrameResources; i++)
        {
            if (vkCreateSemaphore(vk_device, &vk_create_semaphore_info, 0, &frameResourcesOut[i].m_ImageAvailable) != VK_SUCCESS ||
                vkCreateSemaphore(vk_device, &vk_create_semaphore_info, 0, &frameResourcesOut[i].m_RenderFinished) != VK_SUCCESS ||
                vkCreateFence(vk_device, &vk_create_fence_info, 0, &frameResourcesOut[i].m_SubmitFence) != VK_SUCCESS)
            {
                return VK_ERROR_INITIALIZATION_FAILED;
            }
        }

        return VK_SUCCESS;
    }

    static VkResult CreateMainScratchBuffers(VkPhysicalDevice vk_physical_device, VkDevice vk_device,
        uint8_t numSwapChainImages, uint32_t bufferSize, uint16_t numDescriptors,
        DescriptorAllocator* descriptorAllocatorsOut, ScratchBuffer* scratchBuffersOut)
    {
        memset(scratchBuffersOut, 0, sizeof(ScratchBuffer) * numSwapChainImages);
        memset(descriptorAllocatorsOut, 0, sizeof(DescriptorAllocator) * numSwapChainImages);
        for(uint8_t i=0; i < numSwapChainImages; i++)
        {
            VkResult res = CreateDescriptorAllocator(vk_device, numDescriptors, i, &descriptorAllocatorsOut[i]);
            if (res != VK_SUCCESS)
            {
                return res;
            }

            res = CreateScratchBuffer(vk_physical_device, vk_device, bufferSize, true, &descriptorAllocatorsOut[i], &scratchBuffersOut[i]);
            if (res != VK_SUCCESS)
            {
                return res;
            }
        }

        return VK_SUCCESS;
    }

    static void DestroyPipelineCacheCb(HContext context, const uint64_t* key, Pipeline* value)
    {
        DestroyPipeline(context->m_LogicalDevice.m_Device, value);
    }

    static bool EndRenderPass(HContext context)
    {
        assert(context->m_CurrentRenderTarget);
        if (!context->m_CurrentRenderTarget->m_IsBound)
        {
            return false;
        }

        vkCmdEndRenderPass(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex]);
        context->m_CurrentRenderTarget->m_IsBound = 0;
        return true;
    }

    static void BeginRenderPass(HContext context, RenderTarget* rt)
    {
        assert(context->m_CurrentRenderTarget);
        if (context->m_CurrentRenderTarget->m_Id == rt->m_Id &&
            context->m_CurrentRenderTarget->m_IsBound)
        {
            return;
        }

        // If we bind a render pass without explicitly unbinding
        // the current render pass, we must first unbind it.
        if (context->m_CurrentRenderTarget->m_IsBound)
        {
            EndRenderPass(context);
        }

        VkClearValue vk_clear_values[2];
        memset(vk_clear_values, 0, sizeof(vk_clear_values));

        // Clear color and depth/stencil separately
        vk_clear_values[0].color.float32[3]     = 1.0f;
        vk_clear_values[1].depthStencil.depth   = 1.0f;
        vk_clear_values[1].depthStencil.stencil = 0;

        VkRenderPassBeginInfo vk_render_pass_begin_info;
        vk_render_pass_begin_info.sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vk_render_pass_begin_info.renderPass          = rt->m_RenderPass;
        vk_render_pass_begin_info.framebuffer         = rt->m_Framebuffer;
        vk_render_pass_begin_info.pNext               = 0;
        vk_render_pass_begin_info.renderArea.offset.x = 0;
        vk_render_pass_begin_info.renderArea.offset.y = 0;
        vk_render_pass_begin_info.renderArea.extent   = rt->m_Extent;
        vk_render_pass_begin_info.clearValueCount     = 2;
        vk_render_pass_begin_info.pClearValues        = 0;

        vk_render_pass_begin_info.clearValueCount = 2;
        vk_render_pass_begin_info.pClearValues    = vk_clear_values;

        vkCmdBeginRenderPass(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex], &vk_render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

        context->m_CurrentRenderTarget = rt;
        context->m_CurrentRenderTarget->m_IsBound = 1;
    }

    static VkResult AllocateDepthStencilTexture(HContext context, uint32_t width, uint32_t height, Texture* depthStencilTextureOut)
    {
        // Depth formats are optional, so we need to query
        // what available formats we have.
        VkFormat vk_format_list_default[] = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        const VkPhysicalDevice vk_physical_device = context->m_PhysicalDevice.m_Device;
        const VkDevice vk_device                  = context->m_LogicalDevice.m_Device;
        const size_t vk_format_list_size          = sizeof(vk_format_list_default) / sizeof(vk_format_list_default[0]);

        // Check format support
        VkImageTiling vk_image_tiling = VK_IMAGE_TILING_OPTIMAL;
        VkFormat vk_depth_format      = GetSupportedTilingFormat(vk_physical_device, &vk_format_list_default[0],
            vk_format_list_size, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        if (vk_depth_format == VK_FORMAT_UNDEFINED)
        {
            vk_image_tiling = VK_IMAGE_TILING_LINEAR;
            vk_depth_format = GetSupportedTilingFormat(vk_physical_device, &vk_format_list_default[0],
                vk_format_list_size, vk_image_tiling, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }

        // The aspect flag indicates what the image should be used for,
        // it is usually color or stencil | depth.
        VkImageAspectFlags vk_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (vk_depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            vk_depth_format == VK_FORMAT_D24_UNORM_S8_UINT  ||
            vk_depth_format == VK_FORMAT_D16_UNORM_S8_UINT)
        {
            vk_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkResult res = CreateTexture2D(vk_physical_device, vk_device, width, height, 1,
            vk_depth_format, vk_image_tiling, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_aspect, depthStencilTextureOut);
        CHECK_VK_ERROR(res);

        if (res == VK_SUCCESS)
        {
            res = TransitionImageLayout(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_LogicalDevice.m_GraphicsQueue, depthStencilTextureOut->m_Image, vk_aspect,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            CHECK_VK_ERROR(res);
        }

        return res;
    }

    static VkResult CreateMainRenderTarget(HContext context)
    {
        assert(context);
        assert(context->m_SwapChain);

        // We need to create a framebuffer per swap chain image
        // so that they can be used in different states in the rendering pipeline
        context->m_MainFrameBuffers.SetCapacity(context->m_SwapChain->m_Images.Size());
        context->m_MainFrameBuffers.SetSize(context->m_SwapChain->m_Images.Size());

        SwapChain* swapChain = context->m_SwapChain;
        VkResult res;

        for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
        {
            // All swap chain images can share the same depth buffer,
            // that's why we create a single depth buffer at the start and reuse it.
            VkImageView&   vk_image_view_color     = swapChain->m_ImageViews[i];
            VkImageView&   vk_image_view_depth     = context->m_MainTextureDepthStencil.m_ImageView;
            VkImageView    vk_image_attachments[2] = { vk_image_view_color, vk_image_view_depth };

            res = CreateFramebuffer(context->m_LogicalDevice.m_Device,context->m_MainRenderPass,
                swapChain->m_ImageExtent.width, swapChain->m_ImageExtent.height,
                vk_image_attachments, 2, &context->m_MainFrameBuffers[i]);
            CHECK_VK_ERROR(res);
        }

        // Initialize the dummy rendertarget for the main framebuffer
        // The m_Framebuffer construct will be rotated sequentially
        // with the framebuffer objects created per swap chain.
        RenderTarget& rt = context->m_MainRenderTarget;
        rt.m_RenderPass  = context->m_MainRenderPass;
        rt.m_Framebuffer = context->m_MainFrameBuffers[0];
        rt.m_Extent      = swapChain->m_ImageExtent;

        return res;
    }

    static VkResult CreateMainRenderingResources(HContext context)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;

        // Create depth/stencil buffer
        memset(&context->m_MainTextureDepthStencil, 0, sizeof(context->m_MainTextureDepthStencil));
        VkResult res = AllocateDepthStencilTexture(context,
            context->m_SwapChain->m_ImageExtent.width,
            context->m_SwapChain->m_ImageExtent.height,
            &context->m_MainTextureDepthStencil);
        CHECK_VK_ERROR(res);

        // Create main render pass with two attachments
        RenderPassAttachment attachments[2];
        attachments[0].m_Format      = context->m_SwapChain->m_SurfaceFormat.format;;
        attachments[0].m_ImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[1].m_Format      = context->m_MainTextureDepthStencil.m_Format;
        attachments[1].m_ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        res = CreateRenderPass(vk_device, attachments, 1, &attachments[1], &context->m_MainRenderPass);
        CHECK_VK_ERROR(res);

        res = CreateMainRenderTarget(context);
        CHECK_VK_ERROR(res);
        context->m_CurrentRenderTarget = &context->m_MainRenderTarget;

        uint32_t num_swap_chain_images = context->m_SwapChain->m_Images.Size();
        context->m_MainCommandBuffers.SetCapacity(num_swap_chain_images);
        context->m_MainCommandBuffers.SetSize(num_swap_chain_images);

        res = CreateCommandBuffers(vk_device,
            context->m_LogicalDevice.m_CommandPool,
            context->m_MainCommandBuffers.Size(),
            context->m_MainCommandBuffers.Begin());
        CHECK_VK_ERROR(res);

        res = CreateMainFrameSyncObjects(vk_device, g_max_frames_in_flight, context->m_FrameResources);
        CHECK_VK_ERROR(res);

        // Note: These values are guessed and equals roughly 256 draw calls and 64kb
        //       of uniform memory per scratch buffer.
        const uint16_t descriptor_count_per_pool = 512;
        const uint32_t buffer_size               = 256 * descriptor_count_per_pool;

        context->m_MainScratchBuffers.SetCapacity(num_swap_chain_images);
        context->m_MainScratchBuffers.SetSize(num_swap_chain_images);
        context->m_MainDescriptorAllocators.SetCapacity(num_swap_chain_images);
        context->m_MainDescriptorAllocators.SetSize(num_swap_chain_images);

        res = CreateMainScratchBuffers(context->m_PhysicalDevice.m_Device, vk_device,
            num_swap_chain_images, buffer_size, descriptor_count_per_pool,
            context->m_MainDescriptorAllocators.Begin(), context->m_MainScratchBuffers.Begin());
        CHECK_VK_ERROR(res);

        PipelineState vk_default_pipeline;
        vk_default_pipeline.m_WriteColorMask     = DMGRAPHICS_STATE_WRITE_R | DMGRAPHICS_STATE_WRITE_G | DMGRAPHICS_STATE_WRITE_B | DMGRAPHICS_STATE_WRITE_A;
        vk_default_pipeline.m_WriteDepth         = 1;
        vk_default_pipeline.m_PrimtiveType       = PRIMITIVE_TRIANGLES;
        vk_default_pipeline.m_DepthTestEnabled   = 1;
        vk_default_pipeline.m_DepthTestFunc      = COMPARE_FUNC_LEQUAL;
        vk_default_pipeline.m_BlendEnabled       = 0;
        vk_default_pipeline.m_BlendSrcFactor     = BLEND_FACTOR_ZERO;
        vk_default_pipeline.m_BlendDstFactor     = BLEND_FACTOR_ZERO;
        vk_default_pipeline.m_StencilEnabled     = 0;
        vk_default_pipeline.m_StencilOpFail      = STENCIL_OP_KEEP;
        vk_default_pipeline.m_StencilOpDepthFail = STENCIL_OP_KEEP;
        vk_default_pipeline.m_StencilOpPass      = STENCIL_OP_KEEP;
        vk_default_pipeline.m_StencilTestFunc    = COMPARE_FUNC_ALWAYS;
        vk_default_pipeline.m_StencilWriteMask   = 0xff;
        vk_default_pipeline.m_StencilCompareMask = 0xff;
        vk_default_pipeline.m_StencilReference   = 0x0;
        vk_default_pipeline.m_CullFaceEnabled    = 0;
        vk_default_pipeline.m_CullFaceType       = FACE_TYPE_BACK;
        context->m_PipelineState = vk_default_pipeline;

        return res;
    }

    static void SwapChainChanged(HContext context, uint32_t* width, uint32_t* height)
    {
        VkDevice vk_device = context->m_LogicalDevice.m_Device;
        // Flush all current commands
        SynchronizeDevice(vk_device);

        // Update swap chain capabilities
        SwapChainCapabilities swap_chain_capabilities;
        GetSwapChainCapabilities(&context->m_PhysicalDevice, context->m_WindowSurface, swap_chain_capabilities);
        context->m_SwapChainCapabilities.Swap(swap_chain_capabilities);

        VkResult res = UpdateSwapChain(context->m_SwapChain, width, height, true, context->m_SwapChainCapabilities);
        CHECK_VK_ERROR(res);

        // Reset & create main Depth/Stencil buffer
        DestroyTexture(vk_device, &context->m_MainTextureDepthStencil);
        res = AllocateDepthStencilTexture(context,
            context->m_SwapChain->m_ImageExtent.width,
            context->m_SwapChain->m_ImageExtent.height,
            &context->m_MainTextureDepthStencil);
        CHECK_VK_ERROR(res);

        context->m_WindowWidth  = context->m_SwapChain->m_ImageExtent.width;
        context->m_WindowHeight = context->m_SwapChain->m_ImageExtent.height;

        // Reset main rendertarget (but not the render pass)
        RenderTarget* mainRenderTarget = &context->m_MainRenderTarget;
        mainRenderTarget->m_RenderPass = VK_NULL_HANDLE;
        for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
        {
            mainRenderTarget->m_Framebuffer = context->m_MainFrameBuffers[i];
            DestroyRenderTarget(&context->m_LogicalDevice, mainRenderTarget);
        }

        res = CreateMainRenderTarget(context);
        CHECK_VK_ERROR(res);

        // Flush once again to make sure all transitions are complete
        SynchronizeDevice(vk_device);
    }

    static void FlushDeviceBuffersToDelete(HContext context, uint8_t frame, bool flushAll = false)
    {
        for (uint32_t i = 0; i < context->m_DeviceBuffersToDelete.Size(); ++i)
        {
            DeviceBuffer& buffer = context->m_DeviceBuffersToDelete[i];

            if (flushAll || buffer.m_Frame == frame)
            {
                DestroyDeviceBuffer(context->m_LogicalDevice.m_Device, &buffer);
                context->m_DeviceBuffersToDelete.EraseSwap(i);
                --i;
            }
        }
    }

    static void FlushDescriptorAllocatorsToDelete(HContext context, uint8_t swap_chain_index, bool flushAll = false)
    {
        for (uint32_t i = 0; i < context->m_DescriptorAllocatorsToDelete.Size(); ++i)
        {
            DescriptorAllocator& allocator = context->m_DescriptorAllocatorsToDelete[i];

            if (flushAll || allocator.m_SwapChainIndex == swap_chain_index)
            {
                DestroyDescriptorAllocator(context->m_LogicalDevice.m_Device, &allocator);
                context->m_DescriptorAllocatorsToDelete.EraseSwap(i);
                --i;
            }
        }
    }

    static inline void SetViewportHelper(VkCommandBuffer vk_command_buffer, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        VkViewport vk_viewport;
        vk_viewport.x        = (float) x;
        vk_viewport.y        = (float) y;
        vk_viewport.width    = (float) width;
        vk_viewport.height   = (float) height;
        vk_viewport.minDepth = 0.0f;
        vk_viewport.maxDepth = 1.0f;
        vkCmdSetViewport(vk_command_buffer, 0, 1, &vk_viewport);
    }

    static bool InitializeVulkan(HContext context, const WindowParams* params)
    {
        VkResult res = CreateWindowSurface(context->m_Instance, &context->m_WindowSurface, params->m_HighDPI);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create window surface for Vulkan, reason: %s.", VkResultToStr(res));
            return false;
        }

        uint32_t device_count = GetPhysicalDeviceCount(context->m_Instance);

        if (device_count == 0)
        {
            dmLogError("Could not get any Vulkan devices.");
            return false;
        }

        PhysicalDevice* device_list     = new PhysicalDevice[device_count];
        PhysicalDevice* selected_device = NULL;
        GetPhysicalDevices(context->m_Instance, &device_list, device_count);

        const char* required_device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            // From spec:
            // "Allow negative height to be specified in the VkViewport::height field to
            // perform y-inversion of the clip-space to framebuffer-space transform.
            // This allows apps to avoid having to use gl_Position.y = -gl_Position.y
            // in shaders also targeting other APIs."
            "VK_KHR_maintenance1"
        };
        const uint8_t required_device_extension_count = sizeof(required_device_extensions)/sizeof(required_device_extensions[0]);

        QueueFamily selected_queue_family;
        SwapChainCapabilities selected_swap_chain_capabilities;
        for (uint32_t i = 0; i < device_count; ++i)
        {
            #define DESTROY_AND_CONTINUE(d) \
                DestroyPhysicalDevice(d); \
                continue;

            PhysicalDevice* device = &device_list[i];

            // Make sure we have a graphics and present queue available
            QueueFamily queue_family = GetQueueFamily(device, context->m_WindowSurface);
            if (!queue_family.IsValid())
            {
                DESTROY_AND_CONTINUE(device)
            }

            // Make sure all device extensions are supported
            bool all_extensions_found = true;
            for (int32_t ext_i = 0; ext_i < required_device_extension_count; ++ext_i)
            {
                bool found = false;
                for (uint32_t j=0; j < device->m_DeviceExtensionCount; ++j)
                {
                    if (dmStrCaseCmp(device->m_DeviceExtensions[j].extensionName, required_device_extensions[ext_i]) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                all_extensions_found &= found;
            }

            if (!all_extensions_found)
            {
                DESTROY_AND_CONTINUE(device)
            }

            // Make sure device has swap chain support
            GetSwapChainCapabilities(device, context->m_WindowSurface, selected_swap_chain_capabilities);

            if (selected_swap_chain_capabilities.m_SurfaceFormats.Size() == 0 ||
                selected_swap_chain_capabilities.m_PresentModes.Size() == 0)
            {
                DESTROY_AND_CONTINUE(device)
            }

            selected_device = device;
            selected_queue_family = queue_family;
            break;

            #undef DESTROY_AND_CONTINUE
        }

        LogicalDevice logical_device;
        uint32_t created_width  = params->m_Width;
        uint32_t created_height = params->m_Height;
        const bool want_vsync   = true;

        if (selected_device == NULL)
        {
            dmLogError("Could not select a suitable Vulkan device.");
            goto bail;
        }

        res = CreateLogicalDevice(selected_device, context->m_WindowSurface, selected_queue_family,
            required_device_extensions, required_device_extension_count,
            g_validation_layers, g_validation_layer_count, &logical_device);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a logical Vulkan device, reason: %s", VkResultToStr(res));
            goto bail;
        }

        context->m_PhysicalDevice = *selected_device;
        context->m_LogicalDevice  = logical_device;

        // Create swap chain
        context->m_SwapChainCapabilities.Swap(selected_swap_chain_capabilities);
        context->m_SwapChain = new SwapChain(&context->m_LogicalDevice, context->m_WindowSurface, context->m_SwapChainCapabilities, selected_queue_family);

        res = UpdateSwapChain(context->m_SwapChain, &created_width, &created_height, want_vsync, context->m_SwapChainCapabilities);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a swap chain for Vulkan, reason: %s", VkResultToStr(res));
            goto bail;
        }

        delete[] device_list;

        context->m_PipelineCache.SetCapacity(32,64);
        context->m_DeviceBuffersToDelete.SetCapacity(8);
        context->m_DescriptorAllocatorsToDelete.SetCapacity(context->m_SwapChain->m_ImageViews.Size());

        // Create framebuffers, default renderpass etc.
        res = CreateMainRenderingResources(context);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create main rendering resources for Vulkan, reason: %s", VkResultToStr(res));
            goto bail;
        }

        return true;
bail:
        if (context->m_SwapChain)
            delete context->m_SwapChain;
        if (device_list)
            delete[] device_list;
        return false;
    }

    HContext NewContext(const ContextParams& params)
    {
        if (g_Context == 0x0)
        {
            if (glfwInit() == 0)
            {
                dmLogError("Could not initialize glfw.");
                return 0x0;
            }

            uint16_t validation_layer_count = 0;

            const char* env_vulkan_validation = getenv("DM_VULKAN_VALIDATION");
            if (env_vulkan_validation != 0x0)
            {
                validation_layer_count = strtol(env_vulkan_validation, 0, 10) ? g_validation_layer_count : 0;
            }

            VkInstance vk_instance;
            if (CreateInstance(&vk_instance, g_validation_layers, validation_layer_count, g_validation_layer_ext, g_validation_layer_ext_count) != VK_SUCCESS)
            {
                dmLogError("Could not create Vulkan instance");
                return 0x0;
            }

            g_Context = new Context(params, vk_instance);

            return g_Context;
        }
        return 0x0;
    }

    void DeleteContext(HContext context)
    {
        if (context != 0x0)
        {
            delete context;
            g_Context = 0x0;
        }
    }

    bool Initialize()
    {
        return glfwInit() ? true : false;
    }

    void Finalize()
    {
        glfwTerminate();
    }

    static void OnWindowResize(int width, int height)
    {
        assert(g_Context);
        g_Context->m_WindowWidth  = (uint32_t)width;
        g_Context->m_WindowHeight = (uint32_t)height;

        SwapChainChanged(g_Context, &g_Context->m_WindowWidth, &g_Context->m_WindowHeight);

        if (g_Context->m_WindowResizeCallback != 0x0)
        {
            g_Context->m_WindowResizeCallback(g_Context->m_WindowResizeCallbackUserData, (uint32_t)width, (uint32_t)height);
        }
    }

    static int OnWindowClose()
    {
        assert(g_Context);
        if (g_Context->m_WindowCloseCallback != 0x0)
        {
            return g_Context->m_WindowCloseCallback(g_Context->m_WindowCloseCallbackUserData);
        }
        return 1;
    }

    static void OnWindowFocus(int focus)
    {
        assert(g_Context);
        if (g_Context->m_WindowFocusCallback != 0x0)
        {
            g_Context->m_WindowFocusCallback(g_Context->m_WindowFocusCallbackUserData, focus);
        }
    }

    uint32_t GetWindowRefreshRate(HContext context)
    {
        return 0;
    }

    WindowResult OpenWindow(HContext context, WindowParams* params)
    {
        assert(context);
        assert(context->m_WindowSurface == VK_NULL_HANDLE);

        glfwOpenWindowHint(GLFW_CLIENT_API,   GLFW_NO_API);
        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, params->m_Samples);

        int mode = params->m_Fullscreen ? GLFW_FULLSCREEN : GLFW_WINDOW;

        if (!glfwOpenWindow(params->m_Width, params->m_Height, 8, 8, 8, 8, 32, 8, mode))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        if (!InitializeVulkan(context, params))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

    #if !defined(__EMSCRIPTEN__)
        glfwSetWindowTitle(params->m_Title);
    #endif

        glfwSetWindowSizeCallback(OnWindowResize);
        glfwSetWindowCloseCallback(OnWindowClose);
        glfwSetWindowFocusCallback(OnWindowFocus);

        context->m_WindowOpened                 = 1;
        context->m_Width                        = params->m_Width;
        context->m_Height                       = params->m_Height;
        context->m_WindowWidth                  = context->m_SwapChain->m_ImageExtent.width;
        context->m_WindowHeight                 = context->m_SwapChain->m_ImageExtent.height;
        context->m_WindowResizeCallback         = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback          = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData  = params->m_CloseCallbackUserData;
        context->m_WindowFocusCallback          = params->m_FocusCallback;
        context->m_WindowFocusCallbackUserData  = params->m_FocusCallbackUserData;

        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            VkDevice vk_device = context->m_LogicalDevice.m_Device;

            SynchronizeDevice(vk_device);

            FlushDeviceBuffersToDelete(context, 0, true);
            FlushDescriptorAllocatorsToDelete(context, 0, true);

            glfwCloseWindow();

            context->m_PipelineCache.Iterate(DestroyPipelineCacheCb, context);

            DestroyTexture(vk_device, &context->m_MainTextureDepthStencil);

            vkDestroyRenderPass(vk_device, context->m_MainRenderPass, 0);

            vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_MainCommandBuffers.Size(), context->m_MainCommandBuffers.Begin());

            for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
            {
                vkDestroyFramebuffer(vk_device, context->m_MainFrameBuffers[i], 0);
            }

            for (uint8_t i=0; i < context->m_MainScratchBuffers.Size(); i++)
            {
                DestroyDeviceBuffer(vk_device, &context->m_MainScratchBuffers[i].m_DeviceBuffer);
            }

            for (uint8_t i=0; i < context->m_MainDescriptorAllocators.Size(); i++)
            {
                DestroyDescriptorAllocator(vk_device, &context->m_MainDescriptorAllocators[i]);
            }

            for (size_t i = 0; i < g_max_frames_in_flight; i++) {
                FrameResource& frame_resource = context->m_FrameResources[i];
                vkDestroySemaphore(vk_device, frame_resource.m_RenderFinished, 0);
                vkDestroySemaphore(vk_device, frame_resource.m_ImageAvailable, 0);
                vkDestroyFence(vk_device, frame_resource.m_SubmitFence, 0);
            }

            DestroySwapChain(context->m_SwapChain);
            DestroyLogicalDevice(&context->m_LogicalDevice);
            DestroyPhysicalDevice(&context->m_PhysicalDevice);

            vkDestroySurfaceKHR(context->m_Instance, context->m_WindowSurface, 0);

            DestroyInstance(&context->m_Instance);

            context->m_WindowOpened = 0;

            if (context->m_DynamicOffsetBuffer)
            {
                free(context->m_DynamicOffsetBuffer);
            }

            delete context->m_SwapChain;
        }
    }

    void IconifyWindow(HContext context)
    {}

    uint32_t GetWindowState(HContext context, WindowState state)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            return glfwGetWindowParam(state);
        }
        else
        {
            return 0;
        }
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        return 0;
    }

    uint32_t GetWidth(HContext context)
    {
        assert(context);
        return context->m_Width;
    }

    uint32_t GetHeight(HContext context)
    {
        assert(context);
        return context->m_Height;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        assert(context);
        return context->m_WindowWidth;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        assert(context);
        return context->m_WindowHeight;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            context->m_Width  = width;
            context->m_Height = height;
            glfwSetWindowSize((int)width, (int)height);
            int window_width, window_height;
            glfwGetWindowSize(&window_width, &window_height);
            context->m_WindowWidth  = window_width;
            context->m_WindowHeight = window_height;

            SwapChainChanged(context, &context->m_WindowWidth, &context->m_WindowHeight);

            // The callback is not called from glfw when the size is set manually
            if (context->m_WindowResizeCallback)
            {
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, window_width, window_height);
            }
        }
    }

    void ResizeWindow(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            glfwSetWindowSize((int)width, (int)height);
        }
    }

    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    void BeginFrame(HContext context)
    {
        FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameInFlight];

        VkDevice vk_device = context->m_LogicalDevice.m_Device;

        vkWaitForFences(vk_device, 1, &current_frame_resource.m_SubmitFence, VK_TRUE, UINT64_MAX);
        vkResetFences(vk_device, 1, &current_frame_resource.m_SubmitFence);

        if (context->m_DeviceBuffersToDelete.Size() > 0)
        {
            FlushDeviceBuffersToDelete(context, context->m_CurrentFrameInFlight);
        }

        VkResult res      = context->m_SwapChain->Advance(current_frame_resource.m_ImageAvailable);
        uint32_t frame_ix = context->m_SwapChain->m_ImageIndex;

        if (res != VK_SUCCESS)
        {
            if (res == VK_ERROR_OUT_OF_DATE_KHR)
            {
                int width, height;
                glfwGetWindowSize(&width, &height);
                context->m_WindowWidth  = width;
                context->m_WindowHeight = height;
                SwapChainChanged(context, &context->m_WindowWidth, &context->m_WindowHeight);
                res = context->m_SwapChain->Advance(current_frame_resource.m_ImageAvailable);
                assert(res == VK_SUCCESS);
            }
            else if (res == VK_SUBOPTIMAL_KHR)
            {
                // Presenting the swap chain will still work but not optimally, but we should still notify.
                dmLogOnceWarning("Vulkan swapchain is out of date, reason: VK_SUBOPTIMAL_KHR.");
            }
            else
            {
                dmLogOnceError("Vulkan swapchain is out of date, reason: %s.", VkResultToStr(res));
                return;
            }
        }

        // TODO: Test allocating a scratch buffer per frame-in-flight instead of
        //       one per swap chain image.
        if (context->m_DescriptorAllocatorsToDelete.Size() > 0)
        {
            FlushDescriptorAllocatorsToDelete(context, frame_ix);
        }

        // Reset the scratch buffer for this swapchain image, so we can reuse its descriptors
        // for the uniform resource bindings.
        ScratchBuffer* scratchBuffer = &context->m_MainScratchBuffers[frame_ix];
        ResetScratchBuffer(context->m_LogicalDevice.m_Device, scratchBuffer);

        // TODO: Investigate if we don't have to map the memory every frame
        res = scratchBuffer->MapMemory(vk_device);
        CHECK_VK_ERROR(res);

        VkCommandBufferBeginInfo vk_command_buffer_begin_info;

        vk_command_buffer_begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vk_command_buffer_begin_info.flags            = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        vk_command_buffer_begin_info.pInheritanceInfo = 0;
        vk_command_buffer_begin_info.pNext            = 0;

        vkBeginCommandBuffer(context->m_MainCommandBuffers[frame_ix], &vk_command_buffer_begin_info);
        context->m_FrameBegun                     = 1;
        context->m_MainRenderTarget.m_Framebuffer = context->m_MainFrameBuffers[frame_ix];

        BeginRenderPass(context, context->m_CurrentRenderTarget);
    }

    void Flip(HContext context)
    {
        uint32_t frame_ix = context->m_SwapChain->m_ImageIndex;
        FrameResource& current_frame_resource = context->m_FrameResources[context->m_CurrentFrameInFlight];

        if (!EndRenderPass(context))
        {
            assert(0);
            return;
        }

        context->m_MainScratchBuffers[frame_ix].UnmapMemory(context->m_LogicalDevice.m_Device);

        VkResult res = vkEndCommandBuffer(context->m_MainCommandBuffers[frame_ix]);
        CHECK_VK_ERROR(res);

        VkPipelineStageFlags vk_pipeline_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo vk_submit_info;
        vk_submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        vk_submit_info.pNext                = 0;
        vk_submit_info.waitSemaphoreCount   = 1;
        vk_submit_info.pWaitSemaphores      = &current_frame_resource.m_ImageAvailable;
        vk_submit_info.pWaitDstStageMask    = &vk_pipeline_stage_flags;
        vk_submit_info.commandBufferCount   = 1;
        vk_submit_info.pCommandBuffers      = &context->m_MainCommandBuffers[frame_ix];
        vk_submit_info.signalSemaphoreCount = 1;
        vk_submit_info.pSignalSemaphores    = &current_frame_resource.m_RenderFinished;

        res = vkQueueSubmit(context->m_LogicalDevice.m_GraphicsQueue, 1, &vk_submit_info, current_frame_resource.m_SubmitFence);
        CHECK_VK_ERROR(res);

        VkPresentInfoKHR vk_present_info;
        vk_present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        vk_present_info.pNext              = 0;
        vk_present_info.waitSemaphoreCount = 1;
        vk_present_info.pWaitSemaphores    = &current_frame_resource.m_RenderFinished;
        vk_present_info.swapchainCount     = 1;
        vk_present_info.pSwapchains        = &context->m_SwapChain->m_SwapChain;
        vk_present_info.pImageIndices      = &frame_ix;
        vk_present_info.pResults           = 0;

        res = vkQueuePresentKHR(context->m_LogicalDevice.m_PresentQueue, &vk_present_info);
        CHECK_VK_ERROR(res);

        // Advance frame index
        context->m_CurrentFrameInFlight = (context->m_CurrentFrameInFlight + 1) % g_max_frames_in_flight;
        context->m_FrameBegun           = 0;

    #if (defined(__arm__) || defined(__arm64__))
        glfwSwapBuffers();
    #endif
    }

    void SetSwapInterval(HContext context, uint32_t swap_interval)
    {}

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);
        assert(context->m_CurrentRenderTarget);
        DM_PROFILE(Graphics, "Clear");

        float r = ((float)red)/255.0f;
        float g = ((float)green)/255.0f;
        float b = ((float)blue)/255.0f;
        float a = ((float)alpha)/255.0f;

        VkClearRect vk_clear_rect;
        vk_clear_rect.rect.offset.x      = 0;
        vk_clear_rect.rect.offset.y      = 0;
        vk_clear_rect.rect.extent.width  = context->m_CurrentRenderTarget->m_Extent.width;
        vk_clear_rect.rect.extent.height = context->m_CurrentRenderTarget->m_Extent.height;
        vk_clear_rect.baseArrayLayer     = 0;
        vk_clear_rect.layerCount         = 1;

        VkClearAttachment vk_clear_attachments[2];
        memset(vk_clear_attachments, 0, sizeof(vk_clear_attachments));

        // Clear color
        vk_clear_attachments[0].aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
        vk_clear_attachments[0].clearValue.color.float32[0] = r;
        vk_clear_attachments[0].clearValue.color.float32[1] = g;
        vk_clear_attachments[0].clearValue.color.float32[2] = b;
        vk_clear_attachments[0].clearValue.color.float32[3] = a;

        // Clear depth / stencil
        vk_clear_attachments[1].aspectMask                      = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        vk_clear_attachments[1].clearValue.depthStencil.stencil = stencil;
        vk_clear_attachments[1].clearValue.depthStencil.depth   = depth;

        vkCmdClearAttachments(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
            2, vk_clear_attachments, 1, &vk_clear_rect);
    }

    static void DeviceBufferUploadHelper(HContext context, const void* data, uint32_t size, uint32_t offset, DeviceBuffer* bufferOut)
    {
        VkResult res;

        if (bufferOut->m_BufferHandle == VK_NULL_HANDLE)
        {
            res = CreateDeviceBuffer(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device,
                size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, bufferOut);
            CHECK_VK_ERROR(res);
        }

        VkCommandBuffer vk_command_buffer;
        if (context->m_FrameBegun)
        {
            vk_command_buffer = context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex];
        }
        else
        {
            CreateCommandBuffers(context->m_LogicalDevice.m_Device, context->m_LogicalDevice.m_CommandPool, 1, &vk_command_buffer);

            VkCommandBufferBeginInfo vk_command_buffer_begin_info;
            memset(&vk_command_buffer_begin_info, 0, sizeof(VkCommandBufferBeginInfo));

            vk_command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vk_command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            res = vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info);
            CHECK_VK_ERROR(res);
        }

        res = UploadToDeviceBuffer(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device,
            vk_command_buffer, size, offset, data, bufferOut);
        CHECK_VK_ERROR(res);

        if (!context->m_FrameBegun)
        {
            vkEndCommandBuffer(vk_command_buffer);
        }
    }

    static void DestroyDeviceBufferHelper(HContext context, DeviceBuffer* buffer)
    {
        assert(buffer->m_BufferHandle != VK_NULL_HANDLE);

        for (uint32_t i = 0; i < context->m_DeviceBuffersToDelete.Size(); ++i)
        {
            const DeviceBuffer buffer_to_delete = context->m_DeviceBuffersToDelete[i];
            if (buffer_to_delete.m_BufferHandle == buffer->m_BufferHandle)
            {
                assert(buffer_to_delete.m_MemoryHandle == buffer->m_MemoryHandle);
                return;
            }
        }

        if (context->m_DeviceBuffersToDelete.Full())
        {
            context->m_DeviceBuffersToDelete.OffsetCapacity(4);
        }

        DeviceBuffer buffer_copy = *buffer;
        buffer_copy.m_Frame = context->m_CurrentFrameInFlight;
        context->m_DeviceBuffersToDelete.Push(buffer_copy);

        // Since the buffer will be deleted later, we just clear the
        // buffer and memory handles here so they won't be used
        // when rendering.
        buffer->m_BufferHandle = VK_NULL_HANDLE;
        buffer->m_MemoryHandle = VK_NULL_HANDLE;
        buffer->m_MemorySize   = 0;
    }

    static Pipeline* GetOrCreatePipeline(VkDevice vk_device, const PipelineState pipelineState, PipelineCache& pipelineCache,
        Program* program, RenderTarget* rt, DeviceBuffer* vertexBuffer, HVertexDeclaration vertexDeclaration)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &program->m_Hash, sizeof(program->m_Hash));
        dmHashUpdateBuffer64(&pipeline_hash_state, &pipelineState, sizeof(pipelineState));
        dmHashUpdateBuffer64(&pipeline_hash_state, &vertexDeclaration->m_Hash, sizeof(vertexDeclaration->m_Hash));
        dmHashUpdateBuffer64(&pipeline_hash_state, &rt->m_Id, sizeof(rt->m_Id));
        uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);

        Pipeline* cached_pipeline = pipelineCache.Get(pipeline_hash);

        if (!cached_pipeline)
        {
            Pipeline new_pipeline;
            memset(&new_pipeline, 0, sizeof(new_pipeline));

            VkRect2D vk_scissor;
            memset(&vk_scissor, 0, sizeof(vk_scissor));
            vk_scissor.extent = g_Context->m_SwapChain->m_ImageExtent;

            VkResult res = CreatePipeline(vk_device, vk_scissor, pipelineState, program, vertexBuffer, vertexDeclaration, rt->m_RenderPass, &new_pipeline);
            CHECK_VK_ERROR(res);

            if (pipelineCache.Full())
            {
                pipelineCache.SetCapacity(32, pipelineCache.Capacity() + 4);
            }

            pipelineCache.Put(pipeline_hash, new_pipeline);
            cached_pipeline = pipelineCache.Get(pipeline_hash);

            // TODO: Remove this at some point!
            dmLogInfo("Created new VK Pipeline with hash %llu", (unsigned long long) pipeline_hash);
        }

        return cached_pipeline;
    }

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        assert(context);
        DeviceBuffer* buffer = new DeviceBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        if (size > 0)
        {
            DeviceBufferUploadHelper(context, data, size, 0, buffer);
        }

        return (HVertexBuffer) buffer;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        assert(buffer);
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;

        if (buffer_ptr->m_BufferHandle != VK_NULL_HANDLE)
        {
            DestroyDeviceBufferHelper(g_Context, buffer_ptr);
        }
        delete buffer_ptr;
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        assert(buffer);
        if (size == 0)
        {
            return;
        }

        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;

        if (buffer_ptr->m_BufferHandle != VK_NULL_HANDLE && size != buffer_ptr->m_MemorySize)
        {
            DestroyDeviceBufferHelper(g_Context, buffer_ptr);
        }

        DeviceBufferUploadHelper(g_Context, data, size, 0, buffer_ptr);
    }

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        assert(buffer);
        assert(size > 0);
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        assert(offset + size <= buffer_ptr->m_MemorySize);
        DeviceBufferUploadHelper(g_Context, data, size, offset, buffer_ptr);
    }

    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        assert(0 && "Not supported for Vulkan");
        return 0;
    }

    bool UnmapVertexBuffer(HVertexBuffer buffer)
    {
        assert(0 && "Not supported for Vulkan");
        return true;
    }

    uint32_t GetMaxElementsVertices(HContext context)
    {
        return context->m_PhysicalDevice.m_Properties.limits.maxDrawIndexedIndexValue;
    }

    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        assert(size > 0);
        DeviceBuffer* buffer = new DeviceBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        DeviceBufferUploadHelper(g_Context, data, size, 0, (DeviceBuffer*) buffer);
        return (HIndexBuffer) buffer;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        assert(buffer);
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        if (buffer_ptr->m_BufferHandle != VK_NULL_HANDLE)
        {
            DestroyDeviceBufferHelper(g_Context, buffer_ptr);
        }
        delete buffer_ptr;
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        assert(size > 0);
        assert(buffer);

        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;

        if (buffer_ptr->m_BufferHandle != VK_NULL_HANDLE && size != buffer_ptr->m_MemorySize)
        {
            DestroyDeviceBufferHelper(g_Context, buffer_ptr);
        }

        DeviceBufferUploadHelper(g_Context, data, size, 0, buffer_ptr);
    }

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        assert(buffer);
        DeviceBuffer* buffer_ptr = (DeviceBuffer*) buffer;
        assert(offset + size < buffer_ptr->m_MemorySize);
        DeviceBufferUploadHelper(g_Context, data, size, 0, buffer_ptr);
    }

    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        assert(0 && "Not supported for Vulkan");
        return 0;
    }

    bool UnmapIndexBuffer(HIndexBuffer buffer)
    {
        assert(0 && "Not supported for Vulkan");
        return true;
    }

    bool IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        // From VkPhysicalDeviceFeatures spec:
        //   "fullDrawIndexUint32 - If this feature is supported, maxDrawIndexedIndexValue must be 2^32-1;
        //   otherwise it must be no smaller than 2^24-1."
        return true;
    }

    static inline uint32_t GetShaderTypeSize(ShaderDesc::ShaderDataType type)
    {
        const uint8_t conversion_table[] = {
            0,  // SHADER_TYPE_UNKNOWN
            4,  // SHADER_TYPE_INT
            4,  // SHADER_TYPE_UINT
            4,  // SHADER_TYPE_FLOAT
            8,  // SHADER_TYPE_VEC2
            12, // SHADER_TYPE_VEC3
            16, // SHADER_TYPE_VEC4
            16, // SHADER_TYPE_MAT2
            36, // SHADER_TYPE_MAT3
            64, // SHADER_TYPE_MAT4
            4,  // SHADER_TYPE_SAMPLER2D
            4,  // SHADER_TYPE_SAMPLER3D
            4,  // SHADER_TYPE_SAMPLER_CUBE
        };

        return conversion_table[type];
    }

    static inline uint32_t GetGraphicsTypeSize(Type type)
    {
        if (type == TYPE_BYTE || type == TYPE_UNSIGNED_BYTE)
        {
            return 1;
        }
        else if (type == TYPE_SHORT || type == TYPE_UNSIGNED_SHORT)
        {
            return 2;
        }
        else if (type == TYPE_INT || type == TYPE_UNSIGNED_INT || type == TYPE_FLOAT)
        {
            return 4;
        }
        else if (type == TYPE_FLOAT_VEC4)
        {
            return 16;
        }
        else if (type == TYPE_FLOAT_MAT4)
        {
            return 64;
        }
        assert(0 && "Unsupported data type");
        return 0;
    }

    static inline VkFormat GetVulkanFormatFromTypeAndSize(Type type, uint16_t size)
    {
        if (type == TYPE_FLOAT)
        {
            if (size == 1)     return VK_FORMAT_R32_SFLOAT;
            else if(size == 2) return VK_FORMAT_R32G32_SFLOAT;
            else if(size == 3) return VK_FORMAT_R32G32B32_SFLOAT;
            else if(size == 4) return VK_FORMAT_R32G32B32A32_SFLOAT;
        }
        else if (type == TYPE_UNSIGNED_BYTE)
        {
            if (size == 1)     return VK_FORMAT_R8_UINT;
            else if(size == 2) return VK_FORMAT_R8G8_UINT;
            else if(size == 3) return VK_FORMAT_R8G8B8_UINT;
            else if(size == 4) return VK_FORMAT_R8G8B8A8_UINT;
        }
        else if (type == TYPE_UNSIGNED_SHORT)
        {
            if (size == 1)     return VK_FORMAT_R16_UINT;
            else if(size == 2) return VK_FORMAT_R16G16_UINT;
            else if(size == 3) return VK_FORMAT_R16G16B16_UINT;
            else if(size == 4) return VK_FORMAT_R16G16B16A16_UINT;
        }
        else if (type == TYPE_FLOAT_MAT4)
        {
            return VK_FORMAT_R32_SFLOAT;
        }
        else if (type == TYPE_FLOAT_VEC4)
        {
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        }

        assert(0 && "Unable to deduce type from dmGraphics::Type");
        return VK_FORMAT_UNDEFINED;
    }

    static VertexDeclaration* CreateAndFillVertexDeclaration(HashState64* hash, VertexElement* element, uint32_t count)
    {
        assert(element);
        assert(count <= DM_MAX_VERTEX_STREAM_COUNT);

        VertexDeclaration* vd = new VertexDeclaration();
        memset(vd, 0, sizeof(VertexDeclaration));

        vd->m_StreamCount = count;

        for (uint32_t i = 0; i < count; ++i)
        {
            VertexElement& el           = element[i];
            vd->m_Streams[i].m_NameHash = dmHashString64(el.m_Name);
            vd->m_Streams[i].m_Format   = GetVulkanFormatFromTypeAndSize(el.m_Type, el.m_Size);
            vd->m_Streams[i].m_Offset   = vd->m_Stride;
            vd->m_Streams[i].m_Location = 0;
            vd->m_Stride               += el.m_Size * GetGraphicsTypeSize(el.m_Type);

            dmHashUpdateBuffer64(hash, &el.m_Size, sizeof(el.m_Size));
            dmHashUpdateBuffer64(hash, &el.m_Type, sizeof(el.m_Type));
            dmHashUpdateBuffer64(hash, &vd->m_Streams[i].m_Format, sizeof(vd->m_Streams[i].m_Format));
        }

        return vd;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, element, count);
        dmHashUpdateBuffer64(&decl_hash_state, &vd->m_Stride, sizeof(vd->m_Stride));
        vd->m_Hash = dmHashFinal64(&decl_hash_state);
        return vd;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, element, count);
        dmHashUpdateBuffer64(&decl_hash_state, &stride, sizeof(stride));
        vd->m_Stride          = stride;
        vd->m_Hash            = dmHashFinal64(&decl_hash_state);
        return vd;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete (VertexDeclaration*) vertex_declaration;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        context->m_CurrentVertexBuffer      = (DeviceBuffer*) vertex_buffer;
        context->m_CurrentVertexDeclaration = (VertexDeclaration*) vertex_declaration;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        Program* program_ptr = (Program*) program;
        EnableVertexDeclaration(context, vertex_declaration, vertex_buffer);

        for (uint32_t i=0; i < vertex_declaration->m_StreamCount; i++)
        {
            VertexDeclaration::Stream& stream = vertex_declaration->m_Streams[i];

            stream.m_Location = 0xffff;

            ShaderModule* vertex_shader = program_ptr->m_VertexModule;

            for (uint32_t j=0; j < vertex_shader->m_AttributeCount; j++)
            {
                if (vertex_shader->m_Attributes[i].m_NameHash == stream.m_NameHash)
                {
                    stream.m_Location = vertex_shader->m_Attributes[i].m_Binding;
                    break;
                }
            }
        }
    }

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        context->m_CurrentVertexDeclaration = 0;
    }

    static void UpdateDescriptorSets(
        VkDevice        vk_device,
        VkDescriptorSet vk_descriptor_set,
        ShaderModule*   shaderModule,
        ScratchBuffer*  scratchBuffer,
        uint8_t*        uniformData,
        uint32_t*       uniformDataOffsets,
        uint32_t        dynamicAlignment,
        uint32_t*       dynamicOffsets)
    {
        for (int i = 0; i < shaderModule->m_UniformCount; ++i)
        {
            dynamicOffsets[i]                    = (uint32_t) scratchBuffer->m_MappedDataCursor;
            const uint32_t uniform_size_nonalign = GetShaderTypeSize(shaderModule->m_Uniforms[i].m_Type);
            const uint32_t uniform_size          = DM_ALIGN(uniform_size_nonalign, dynamicAlignment);

            // Copy client data to aligned host memory
            // The data_offset here is the offset into the programs uniform data,
            // i.e the source buffer.
            const uint32_t data_offset = uniformDataOffsets[i];
            memcpy(&((uint8_t*)scratchBuffer->m_MappedDataPtr)[scratchBuffer->m_MappedDataCursor], &uniformData[data_offset], uniform_size_nonalign);

            // Note in the spec about the offset being zero:
            //   "For VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC and VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC descriptor types,
            //    offset is the base offset from which the dynamic offset is applied and range is the static size
            //    used for all dynamic offsets."
            VkDescriptorBufferInfo vk_buffer_info;
            vk_buffer_info.buffer = scratchBuffer->m_DeviceBuffer.m_BufferHandle;
            vk_buffer_info.offset = 0;
            vk_buffer_info.range  = uniform_size;

            VkWriteDescriptorSet vk_write_desc_info;
            memset(&vk_write_desc_info, 0, sizeof(vk_write_desc_info));

            vk_write_desc_info.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vk_write_desc_info.dstSet          = vk_descriptor_set;
            vk_write_desc_info.dstBinding      = shaderModule->m_Uniforms[i].m_Binding;
            vk_write_desc_info.dstArrayElement = 0;
            vk_write_desc_info.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            vk_write_desc_info.descriptorCount = 1;
            vk_write_desc_info.pBufferInfo     = &vk_buffer_info;

            vkUpdateDescriptorSets(vk_device, 1, &vk_write_desc_info, 0, 0);
            scratchBuffer->m_MappedDataCursor += uniform_size;
        }
    }

    static VkResult CommitUniforms(VkCommandBuffer vk_command_buffer, VkDevice vk_device,
        Program* program_ptr, ScratchBuffer* scratchBuffer,
        uint32_t* dynamicOffsets, uint16_t dynamicOffsetCount, const uint32_t alignment)
    {
        VkDescriptorSet* vk_descriptor_set_list = 0x0;
        VkResult res = scratchBuffer->m_DescriptorAllocator->Allocate(vk_device, program_ptr->m_DescriptorSetLayout, DM_MAX_SET_COUNT, &vk_descriptor_set_list);
        if (res != VK_SUCCESS)
        {
            return res;
        }

        VkDescriptorSet vs_set = vk_descriptor_set_list[0];
        VkDescriptorSet fs_set = vk_descriptor_set_list[1];

        UpdateDescriptorSets(vk_device, vs_set, program_ptr->m_VertexModule, scratchBuffer,
            program_ptr->m_UniformData, program_ptr->m_UniformDataOffsets,
            alignment, dynamicOffsets);
        UpdateDescriptorSets(vk_device, fs_set, program_ptr->m_FragmentModule, scratchBuffer,
            program_ptr->m_UniformData, &program_ptr->m_UniformDataOffsets[program_ptr->m_VertexModule->m_UniformCount],
            alignment, &dynamicOffsets[program_ptr->m_VertexModule->m_UniformCount]);

        vkCmdBindDescriptorSets(vk_command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS, program_ptr->m_PipelineLayout,
            0, 2, vk_descriptor_set_list,
            (uint32_t)dynamicOffsetCount, dynamicOffsets);

        return VK_SUCCESS;
    }

    static VkResult ResizeDescriptorAllocator(HContext context, DescriptorAllocator* allocator, uint32_t newDescriptorCount)
    {
        if (context->m_DescriptorAllocatorsToDelete.Full())
        {
            context->m_DescriptorAllocatorsToDelete.OffsetCapacity(1);
        }

        // No need to check for duplicates here
        const DescriptorAllocator allocator_copy = *allocator;
        context->m_DescriptorAllocatorsToDelete.Push(allocator_copy);

        // Since we have handed over ownership of the allocator to the allocators-to-delete list,
        // we need to clear out the allocator state.
        allocator->m_PoolHandle     = VK_NULL_HANDLE;
        allocator->m_DescriptorSets = 0x0;

        return CreateDescriptorAllocator(context->m_LogicalDevice.m_Device, newDescriptorCount, allocator->m_SwapChainIndex, allocator);
    }

    static VkResult ResizeScratchBuffer(HContext context, uint32_t newDataSize, ScratchBuffer* scratchBuffer)
    {
        // Put old buffer on the delete queue so we don't mess the descriptors already in-use
        DestroyDeviceBufferHelper(context, &scratchBuffer->m_DeviceBuffer);
        VkResult res = CreateScratchBuffer(context->m_PhysicalDevice.m_Device, context->m_LogicalDevice.m_Device,
            newDataSize, false, scratchBuffer->m_DescriptorAllocator, scratchBuffer);

        scratchBuffer->MapMemory(context->m_LogicalDevice.m_Device);
        return res;
    }

    static void DrawSetup(HContext context, VkCommandBuffer vk_command_buffer, ScratchBuffer* scratchBuffer, DeviceBuffer* indexBuffer, Type indexBufferType)
    {
        assert(context);
        assert(context->m_CurrentVertexBuffer);

        Program* program_ptr           = context->m_CurrentProgram;
        VkDevice vk_device             = context->m_LogicalDevice.m_Device;

        bool resize_desc_allocator = (scratchBuffer->m_DescriptorAllocator->m_DescriptorIndex + DM_MAX_SET_COUNT) >
            scratchBuffer->m_DescriptorAllocator->m_DescriptorMax;
        bool resize_scratch_buffer = (program_ptr->m_VertexModule->m_UniformDataSizeAligned +
            program_ptr->m_FragmentModule->m_UniformDataSizeAligned) > scratchBuffer->m_DeviceBuffer.m_MemorySize;

        const uint8_t descriptor_increase = 32;
        if (resize_desc_allocator)
        {
            VkResult res = ResizeDescriptorAllocator(context, scratchBuffer->m_DescriptorAllocator, scratchBuffer->m_DescriptorAllocator->m_DescriptorMax + descriptor_increase);
            CHECK_VK_ERROR(res);
        }

        if (resize_scratch_buffer)
        {
            const uint32_t bytes_increase = 256 * descriptor_increase;
            ResizeScratchBuffer(context, scratchBuffer->m_DeviceBuffer.m_MemorySize + bytes_increase, scratchBuffer);
        }

        if (indexBuffer)
        {
            assert(indexBufferType == TYPE_UNSIGNED_SHORT || indexBufferType == TYPE_UNSIGNED_INT);
            VkIndexType vk_index_type = VK_INDEX_TYPE_UINT16;

            if (indexBufferType == TYPE_UNSIGNED_INT)
            {
                vk_index_type = VK_INDEX_TYPE_UINT32;
            }

            vkCmdBindIndexBuffer(vk_command_buffer, indexBuffer->m_BufferHandle, 0, vk_index_type);
        }

        const uint32_t num_uniforms = program_ptr->m_VertexModule->m_UniformCount + program_ptr->m_FragmentModule->m_UniformCount;

        if (context->m_DynamicOffsetBufferSize < num_uniforms)
        {
            if (context->m_DynamicOffsetBuffer == 0x0)
            {
                context->m_DynamicOffsetBuffer = (uint32_t*) malloc(sizeof(uint32_t) * num_uniforms);
            }
            else
            {
                context->m_DynamicOffsetBuffer = (uint32_t*) realloc(context->m_DynamicOffsetBuffer, sizeof(uint32_t) * num_uniforms);
            }

            context->m_DynamicOffsetBufferSize = num_uniforms;
        }

        uint32_t dynamic_alignment = (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment;
        VkResult res = CommitUniforms(vk_command_buffer, vk_device,
            program_ptr, scratchBuffer, context->m_DynamicOffsetBuffer, context->m_DynamicOffsetBufferSize, dynamic_alignment);
        CHECK_VK_ERROR(res);

        Pipeline* pipeline = GetOrCreatePipeline(vk_device,
            context->m_PipelineState, context->m_PipelineCache,
            program_ptr, context->m_CurrentRenderTarget,
            context->m_CurrentVertexBuffer, context->m_CurrentVertexDeclaration);
        vkCmdBindPipeline(vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

        VkBuffer vk_vertex_buffer             = context->m_CurrentVertexBuffer->m_BufferHandle;
        VkDeviceSize vk_vertex_buffer_offsets = 0;

        vkCmdBindVertexBuffers(vk_command_buffer, 0, 1, &vk_vertex_buffer, &vk_vertex_buffer_offsets);

        if (context->m_MainViewport.m_HasChanged)
        {
            Viewport& vp = context->m_MainViewport;

            // Update scissor
            VkRect2D vk_scissor;
            memset(&vk_scissor, 0, sizeof(vk_scissor));
            vk_scissor.extent = context->m_SwapChain->m_ImageExtent;
            vkCmdSetScissor(vk_command_buffer, 0, 1, &vk_scissor);

            // If we are rendering to the backbuffer, we must invert the viewport on
            // the y axis. Otherwise we just use the values as-is.
            // If we don't, all FBO rendering will be upside down.
            if (context->m_CurrentRenderTarget->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
            {
                SetViewportHelper(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
                    vp.m_X, (context->m_WindowHeight - vp.m_Y), vp.m_W, -vp.m_H);
            }
            else
            {
                SetViewportHelper(context->m_MainCommandBuffers[context->m_SwapChain->m_ImageIndex],
                    vp.m_X, vp.m_Y, vp.m_W, vp.m_H);
            }

            vp.m_HasChanged = 0;
        }
    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(context->m_FrameBegun);
        const uint8_t image_ix = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
        DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[image_ix], (DeviceBuffer*) index_buffer, type);

        // The 'first' value that comes in is intended to be a byte offset,
        // but vkCmdDrawIndexed only operates with actual offset values into the index buffer
        uint32_t index_offset = first / (type == TYPE_UNSIGNED_SHORT ? 2 : 4);
        vkCmdDrawIndexed(vk_command_buffer, count, 1, index_offset, 0, 0);
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context->m_FrameBegun);
        const uint8_t image_ix = context->m_SwapChain->m_ImageIndex;
        VkCommandBuffer vk_command_buffer = context->m_MainCommandBuffers[image_ix];
        DrawSetup(context, vk_command_buffer, &context->m_MainScratchBuffers[image_ix], 0, TYPE_BYTE);
        vkCmdDraw(vk_command_buffer, count, 1, first, 0);
    }

    static void CreateShaderResourceBindings(ShaderModule* shader, ShaderDesc::Shader* ddf, uint32_t dynamicAlignment)
    {
        if (ddf->m_Uniforms.m_Count > 0)
        {
            shader->m_Uniforms               = new ShaderResourceBinding[ddf->m_Uniforms.m_Count];
            shader->m_UniformCount           = ddf->m_Uniforms.m_Count;
            shader->m_UniformDataSizeAligned = 0;

            for (uint32_t i=0; i < ddf->m_Uniforms.m_Count; i++)
            {
                ShaderResourceBinding& res = shader->m_Uniforms[i];
                res.m_Binding              = ddf->m_Uniforms[i].m_Binding;
                res.m_Set                  = ddf->m_Uniforms[i].m_Set;
                res.m_NameHash             = ddf->m_Uniforms[i].m_Name;
                res.m_Type                 = ddf->m_Uniforms[i].m_Type;
                assert(res.m_Set <= 1);

                // Calculate aligned data size on the fly
                shader->m_UniformDataSizeAligned += DM_ALIGN(GetShaderTypeSize(res.m_Type), dynamicAlignment);
            }
        }

        if (ddf->m_Attributes.m_Count > 0)
        {
            shader->m_Attributes     = new ShaderResourceBinding[ddf->m_Attributes.m_Count];
            shader->m_AttributeCount = ddf->m_Attributes.m_Count;

            for (uint32_t i=0; i < ddf->m_Attributes.m_Count; i++)
            {
                ShaderResourceBinding& res = shader->m_Attributes[i];
                res.m_Binding              = ddf->m_Attributes[i].m_Binding;
                res.m_Set                  = ddf->m_Attributes[i].m_Set;
                res.m_NameHash             = ddf->m_Attributes[i].m_Name;
                res.m_Type                 = ddf->m_Attributes[i].m_Type;
            }
        }
    }

    HVertexProgram NewVertexProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        ShaderModule* shader = new ShaderModule;
        memset(shader, 0, sizeof(*shader));
        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf->m_Source.m_Data, ddf->m_Source.m_Count, shader);
        CHECK_VK_ERROR(res);
        CreateShaderResourceBindings(shader, ddf, (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment);
        return (HVertexProgram) shader;
    }

    HFragmentProgram NewFragmentProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        ShaderModule* shader = new ShaderModule;
        memset(shader, 0, sizeof(*shader));
        VkResult res = CreateShaderModule(context->m_LogicalDevice.m_Device, ddf->m_Source.m_Data, ddf->m_Source.m_Count, shader);
        CHECK_VK_ERROR(res);
        CreateShaderResourceBindings(shader, ddf, (uint32_t) context->m_PhysicalDevice.m_Properties.limits.minUniformBufferOffsetAlignment);
        return (HFragmentProgram) shader;
    }

    static inline uint32_t FillUniformOffsets(ShaderModule* module, uint32_t base_offset, uint32_t* offsets)
    {
        uint32_t size = base_offset;

        for (int i = 0; i < module->m_UniformCount; ++i)
        {
            ShaderResourceBinding& uniform = module->m_Uniforms[i];
            assert(uniform.m_Type         != ShaderDesc::SHADER_TYPE_UNKNOWN);
            offsets[i]                     = size;
            size                          += GetShaderTypeSize(uniform.m_Type);
        }

        return size;
    }

    static void FillDescriptorSetBindings(ShaderModule* shader, VkShaderStageFlags vk_stage_flag, VkDescriptorSetLayoutBinding* vk_bindings_out)
    {
        for(uint32_t i=0; i < shader->m_UniformCount; i++)
        {
            VkDescriptorSetLayoutBinding& vk_desc = vk_bindings_out[i];
            vk_desc.binding                       = shader->m_Uniforms[i].m_Binding;
            vk_desc.descriptorType                = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; // TODO: textures
            vk_desc.descriptorCount               = 1;
            vk_desc.stageFlags                    = vk_stage_flag;
            vk_desc.pImmutableSamplers            = 0;
        }
    }

    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        Program* program              = new Program;
        ShaderModule* vertex_module   = (ShaderModule*) vertex_program;
        ShaderModule* fragment_module = (ShaderModule*) fragment_program;

        // Set pipeline creation info
        VkPipelineShaderStageCreateInfo vk_vertex_shader_create_info;
        memset(&vk_vertex_shader_create_info, 0, sizeof(vk_vertex_shader_create_info));

        vk_vertex_shader_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vk_vertex_shader_create_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vk_vertex_shader_create_info.module = ((ShaderModule*) vertex_program)->m_Module;
        vk_vertex_shader_create_info.pName  = "main";

        VkPipelineShaderStageCreateInfo vk_fragment_shader_create_info;
        memset(&vk_fragment_shader_create_info, 0, sizeof(VkPipelineShaderStageCreateInfo));

        vk_fragment_shader_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vk_fragment_shader_create_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        vk_fragment_shader_create_info.module = ((ShaderModule*) fragment_program)->m_Module;
        vk_fragment_shader_create_info.pName  = "main";

        program->m_PipelineStageInfo[0] = vk_vertex_shader_create_info;
        program->m_PipelineStageInfo[1] = vk_fragment_shader_create_info;
        program->m_Hash                 = 0;
        program->m_UniformDataOffsets   = 0;
        program->m_UniformData          = 0;
        program->m_VertexModule         = vertex_module;
        program->m_FragmentModule       = fragment_module;

        HashState64 program_hash;
        dmHashInit64(&program_hash, false);

        if (vertex_module->m_AttributeCount > 0)
        {
            for (uint32_t i=0; i < vertex_module->m_AttributeCount; i++)
            {
                dmHashUpdateBuffer64(&program_hash, &vertex_module->m_Attributes[i].m_Binding, sizeof(vertex_module->m_Attributes[i].m_Binding));
            }
        }

        dmHashUpdateBuffer64(&program_hash, &vertex_module->m_Hash, sizeof(vertex_module->m_Hash));
        dmHashUpdateBuffer64(&program_hash, &fragment_module->m_Hash, sizeof(fragment_module->m_Hash));
        program->m_Hash = dmHashFinal64(&program_hash);

        const uint32_t num_uniforms = vertex_module->m_UniformCount + fragment_module->m_UniformCount;
        if (num_uniforms > 0)
        {
            program->m_UniformDataOffsets = new uint32_t[num_uniforms];
            uint32_t vx_last_offset   = FillUniformOffsets(vertex_module, 0, program->m_UniformDataOffsets);
            uint32_t fs_last_offset   = FillUniformOffsets(fragment_module, vx_last_offset, &program->m_UniformDataOffsets[vertex_module->m_UniformCount]);
            program->m_UniformData    = new uint8_t[vx_last_offset + fs_last_offset];
            memset(program->m_UniformData, 0, vx_last_offset + fs_last_offset);

            // Create descriptor set bindings
            VkDescriptorSetLayoutBinding* vk_descriptor_set_bindings = new VkDescriptorSetLayoutBinding[num_uniforms];
            FillDescriptorSetBindings(vertex_module, VK_SHADER_STAGE_VERTEX_BIT, vk_descriptor_set_bindings);
            FillDescriptorSetBindings(fragment_module, VK_SHADER_STAGE_FRAGMENT_BIT, &vk_descriptor_set_bindings[vertex_module->m_UniformCount]);

            VkDescriptorSetLayoutCreateInfo vk_descriptor_set_create_info[2];
            memset(&vk_descriptor_set_create_info, 0, sizeof(vk_descriptor_set_create_info));
            vk_descriptor_set_create_info[0].sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            vk_descriptor_set_create_info[0].pBindings    = vk_descriptor_set_bindings;
            vk_descriptor_set_create_info[0].bindingCount = vertex_module->m_UniformCount;

            vk_descriptor_set_create_info[1].sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            vk_descriptor_set_create_info[1].pBindings    = &vk_descriptor_set_bindings[vertex_module->m_UniformCount];
            vk_descriptor_set_create_info[1].bindingCount = fragment_module->m_UniformCount;

            vkCreateDescriptorSetLayout(context->m_LogicalDevice.m_Device, &vk_descriptor_set_create_info[0], 0, &program->m_DescriptorSetLayout[0]);
            vkCreateDescriptorSetLayout(context->m_LogicalDevice.m_Device, &vk_descriptor_set_create_info[1], 0, &program->m_DescriptorSetLayout[1]);

            VkPipelineLayoutCreateInfo vk_pipeline_layout_create_info;
            memset(&vk_pipeline_layout_create_info, 0, sizeof(vk_pipeline_layout_create_info));
            vk_pipeline_layout_create_info.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            vk_pipeline_layout_create_info.setLayoutCount = 2;
            vk_pipeline_layout_create_info.pSetLayouts    = program->m_DescriptorSetLayout;

            vkCreatePipelineLayout(context->m_LogicalDevice.m_Device, &vk_pipeline_layout_create_info, 0, &program->m_PipelineLayout);
            delete[] vk_descriptor_set_bindings;
        }

        return (HProgram) program;
    }

    void DeleteProgram(HContext context, HProgram program)
    {
        assert(program);
        Program* program_ptr = (Program*) program;
        if (program_ptr->m_UniformData)
        {
            delete[] program_ptr->m_UniformData;
            delete[] program_ptr->m_UniformDataOffsets;
        }

        if (program_ptr->m_PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(context->m_LogicalDevice.m_Device, program_ptr->m_PipelineLayout, 0);
        }

        for (int i = 0; i < 2; ++i)
        {
            if (program_ptr->m_DescriptorSetLayout[i] != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(context->m_LogicalDevice.m_Device, program_ptr->m_DescriptorSetLayout[i], 0);
            }
        }

        delete program_ptr;
    }

    bool ReloadVertexProgram(HVertexProgram prog, ShaderDesc::Shader* ddf)
    {
        return true;
    }

    bool ReloadFragmentProgram(HFragmentProgram prog, ShaderDesc::Shader* ddf)
    {
        return true;
    }

    void DeleteVertexProgram(HVertexProgram prog)
    {
        ShaderModule* shader = (ShaderModule*) prog;

        DestroyShaderModule(g_Context->m_LogicalDevice.m_Device, shader);

        if (shader->m_Attributes)
        {
            delete[] shader->m_Attributes;
        }

        if (shader->m_Uniforms)
        {
            delete[] shader->m_Uniforms;
        }

        delete shader;
    }

    void DeleteFragmentProgram(HFragmentProgram prog)
    {
        ShaderModule* shader = (ShaderModule*) prog;
        assert(shader->m_Attributes == 0);

        DestroyShaderModule(g_Context->m_LogicalDevice.m_Device, shader);

        if (shader->m_Uniforms)
        {
            delete[] shader->m_Uniforms;
        }

        delete shader;
    }

    ShaderDesc::Language GetShaderProgramLanguage(HContext context)
    {
        return ShaderDesc::LANGUAGE_SPIRV;
    }

    void EnableProgram(HContext context, HProgram program)
    {
        assert(context);
        context->m_CurrentProgram = (Program*) program;
    }

    void DisableProgram(HContext context)
    {
        assert(context);
        context->m_CurrentProgram = 0;
    }

    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        return true;
    }

    uint32_t GetUniformCount(HProgram prog)
    {
        assert(prog);
        Program* program_ptr = (Program*) prog;
        assert(program_ptr->m_VertexModule && program_ptr->m_FragmentModule);
        return program_ptr->m_VertexModule->m_UniformCount + program_ptr->m_FragmentModule->m_UniformCount;
    }

    static Type shaderDataTypeToGraphicsType(ShaderDesc::ShaderDataType shader_type)
    {
        switch(shader_type)
        {
            case ShaderDesc::SHADER_TYPE_INT:          return TYPE_INT;
            case ShaderDesc::SHADER_TYPE_UINT:         return TYPE_UNSIGNED_INT;
            case ShaderDesc::SHADER_TYPE_FLOAT:        return TYPE_FLOAT;
            case ShaderDesc::SHADER_TYPE_VEC4:         return TYPE_FLOAT_VEC4;
            case ShaderDesc::SHADER_TYPE_MAT4:         return TYPE_FLOAT_MAT4;
            case ShaderDesc::SHADER_TYPE_SAMPLER2D:    return TYPE_SAMPLER_2D;
            case ShaderDesc::SHADER_TYPE_SAMPLER_CUBE: return TYPE_SAMPLER_CUBE;
            default: break;
        }

        // Not supported
        return (Type) 0xffffffff;
    }

    uint32_t GetUniformName(HProgram prog, uint32_t index, dmhash_t* hash, Type* type)
    {
        assert(prog);
        Program* program_ptr = (Program*) prog;
        ShaderModule* module = program_ptr->m_VertexModule;

        if (index >= program_ptr->m_VertexModule->m_UniformCount)
        {
            module = program_ptr->m_FragmentModule;
            index -= program_ptr->m_VertexModule->m_UniformCount;
        }

        if (index >= module->m_UniformCount)
        {
            return 0;
        }

        ShaderResourceBinding* res = &module->m_Uniforms[index];

        *hash = res->m_NameHash;
        *type = shaderDataTypeToGraphicsType(res->m_Type);

        // Don't know the name length here
        return 1;
    }

    uint32_t GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type)
    {
        // Not supported
        return 0;
    }

    int32_t GetUniformLocation(HProgram prog, const char* name)
    {
        // Not supported
        return -1;
    }

    // In OpenGL, there is a single global resource identifier between
    // fragment and vertex uniforms for a single program. In Vulkan,
    // a uniform can be present in both shaders so we have to keep track
    // of this ourselves. Because of this we pack resource locations
    // for uniforms in a single base register with 15 bits
    // per shader location. If uniform is not found, we return -1 as usual.
    #define UNIFORM_LOCATION_MAX         0x00007FFF
    #define UNIFORM_LOCATION_BIT_COUNT   15
    #define UNIFORM_LOCATION_GET_VS(loc) (loc  & UNIFORM_LOCATION_MAX)
    #define UNIFORM_LOCATION_GET_FS(loc) ((loc & (UNIFORM_LOCATION_MAX << UNIFORM_LOCATION_BIT_COUNT)) >> UNIFORM_LOCATION_BIT_COUNT)

    static bool GetUniformIndex(ShaderResourceBinding* uniforms, uint32_t uniformCount, dmhash_t name, uint32_t* index_out)
    {
        assert(uniformCount < UNIFORM_LOCATION_MAX);
        for (uint32_t i = 0; i < uniformCount; ++i)
        {
            if (uniforms[i].m_NameHash == name)
            {
                *index_out = i;
                return true;
            }
        }

        return false;
    }

    int32_t GetUniformLocation(HProgram prog, dmhash_t name)
    {
        assert(prog);
        Program* program_ptr = (Program*) prog;
        ShaderModule* vs     = program_ptr->m_VertexModule;
        ShaderModule* fs     = program_ptr->m_FragmentModule;
        uint32_t location_vs = UNIFORM_LOCATION_MAX;
        uint32_t location_fs = UNIFORM_LOCATION_MAX;

        if (GetUniformIndex(vs->m_Uniforms, vs->m_UniformCount, name, &location_vs) ||
            GetUniformIndex(fs->m_Uniforms, fs->m_UniformCount, name, &location_fs))
        {
            return location_vs | (location_fs << UNIFORM_LOCATION_BIT_COUNT);
        }

        return -1;
    }

    void SetConstantV4(HContext context, const Vectormath::Aos::Vector4* data, int base_register)
    {
        assert(context->m_CurrentProgram);
        assert(base_register >= 0);
        Program* program_ptr = (Program*) context->m_CurrentProgram;

        uint32_t index_vs  = UNIFORM_LOCATION_GET_VS(base_register);
        uint32_t index_fs  = UNIFORM_LOCATION_GET_FS(base_register);
        assert(!(index_vs == UNIFORM_LOCATION_MAX && index_fs == UNIFORM_LOCATION_MAX));

        if (index_vs != UNIFORM_LOCATION_MAX)
        {
            assert(index_vs < program_ptr->m_VertexModule->m_UniformCount);
            uint32_t offset = program_ptr->m_UniformDataOffsets[index_vs];
            memcpy(&program_ptr->m_UniformData[offset], data, sizeof(Vectormath::Aos::Vector4));
        }

        if (index_fs != UNIFORM_LOCATION_MAX)
        {
            assert(index_fs < program_ptr->m_FragmentModule->m_UniformCount);
            // Fragment uniforms are packed behind vertex uniforms hence the extra offset here
            index_fs       += program_ptr->m_VertexModule->m_UniformCount;
            uint32_t offset = program_ptr->m_UniformDataOffsets[index_fs];
            memcpy(&program_ptr->m_UniformData[offset], data, sizeof(Vectormath::Aos::Vector4));
        }
    }

    void SetConstantM4(HContext context, const Vectormath::Aos::Vector4* data, int base_register)
    {
        Program* program_ptr = (Program*) context->m_CurrentProgram;

        uint32_t index_vs  = UNIFORM_LOCATION_GET_VS(base_register);
        uint32_t index_fs  = UNIFORM_LOCATION_GET_FS(base_register);
        assert(!(index_vs == UNIFORM_LOCATION_MAX && index_fs == UNIFORM_LOCATION_MAX));

        if (index_vs != UNIFORM_LOCATION_MAX)
        {
            assert(index_vs < program_ptr->m_VertexModule->m_UniformCount);
            uint32_t offset = program_ptr->m_UniformDataOffsets[index_vs];
            memcpy(&program_ptr->m_UniformData[offset], data, sizeof(Vectormath::Aos::Vector4) * 4);
        }

        if (index_fs != UNIFORM_LOCATION_MAX)
        {
            assert(index_fs < program_ptr->m_FragmentModule->m_UniformCount);
            // Fragment uniforms are packed behind vertex uniforms hence the extra offset here
            index_fs       += program_ptr->m_VertexModule->m_UniformCount;
            uint32_t offset = program_ptr->m_UniformDataOffsets[index_fs];
            memcpy(&program_ptr->m_UniformData[offset], data, sizeof(Vectormath::Aos::Vector4) * 4);
        }
    }

    void SetSampler(HContext context, int32_t location, int32_t unit)
    {}

    #undef UNIFORM_LOCATION_MAX
    #undef UNIFORM_LOCATION_BIT_COUNT
    #undef UNIFORM_LOCATION_GET_VS
    #undef UNIFORM_LOCATION_GET_FS

    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        // Defer the update to when we actually draw, since we *might* need to invert the viewport
        // depending on wether or not we have set a different rendertarget from when
        // this call was made.
        Viewport& viewport    = context->m_MainViewport;
        viewport.m_HasChanged = 1;
        viewport.m_X          = (uint16_t) x;
        viewport.m_Y          = (uint16_t) y;
        viewport.m_W          = (uint16_t) width;
        viewport.m_H          = (uint16_t) height;
    }

    void EnableState(HContext context, State state)
    {}

    void DisableState(HContext context, State state)
    {}

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {}

    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {}

    void SetDepthMask(HContext context, bool mask)
    {}

    void SetDepthFunc(HContext context, CompareFunc func)
    {}

    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        // While scissors are obviously supported in vulkan, we don't expose it
        // to the users via render scripts so it's a bit hard to test.
        // Leaving it unsupported for now.
        assert(0 && "Not supported");
    }

    void SetStencilMask(HContext context, uint32_t mask)
    {}

    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {}

    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {}

    void SetCullFace(HContext context, FaceType face_type)
    {}

    void SetPolygonOffset(HContext context, float factor, float units)
    {}


    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        return new RenderTarget(GetNextRenderTargetId());
    }

    void DeleteRenderTarget(HRenderTarget render_target)
    {
        delete render_target;
    }

    void SetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {}

    HTexture GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        return 0;
    }

    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {}

    void SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {}

    bool IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return true;
    }

    HTexture NewTexture(HContext context, const TextureCreationParams& params)
    {
        Texture* tex  = new Texture;
        tex->m_Type   = params.m_Type;
        tex->m_Width  = params.m_Width;
        tex->m_Height = params.m_Height;

        if (params.m_OriginalWidth == 0)
        {
            tex->m_OriginalWidth  = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
        }
        else
        {
            tex->m_OriginalWidth  = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
        }

        return (HTexture) tex;
    }

    void DeleteTexture(HTexture t)
    {
        delete t;
    }

    void SetTexture(HTexture texture, const TextureParams& params)
    {}

    void SetTextureAsync(HTexture texture, const TextureParams& paramsa)
    {}

    uint8_t* GetTextureData(HTexture texture)
    {
        return 0;
    }

    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap)
    {}

    uint32_t GetTextureResourceSize(HTexture texture)
    {
        return 0;
    }

    uint16_t GetTextureWidth(HTexture texture)
    {
        return texture->m_Width;
    }

    uint16_t GetTextureHeight(HTexture texture)
    {
        return texture->m_Height;
    }

    uint16_t GetOriginalTextureWidth(HTexture texture)
    {
        return texture->m_OriginalWidth;
    }

    uint16_t GetOriginalTextureHeight(HTexture texture)
    {
        return texture->m_OriginalHeight;
    }

    void EnableTexture(HContext context, uint32_t unit, HTexture texture)
    {}

    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {}

    uint32_t GetMaxTextureSize(HContext context)
    {
        return context->m_PhysicalDevice.m_Properties.limits.maxImageDimension2D;
    }

    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        return 0;
    }

    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {}

    void AppBootstrap(int argc, char** argv, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn)
    {
#if defined(__MACH__) && ( defined(__arm__) || defined(__arm64__) || defined(IOS_SIMULATOR) )
        glfwAppBootstrap(argc, argv, create_fn, destroy_fn, update_fn);
#endif
    }

    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }
}
