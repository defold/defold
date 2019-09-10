#include "../graphics.h"
#include "graphics_vulkan.h"

#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <graphics/glfw/glfw.h>
#include <graphics/glfw/glfw_native.h>

#include <dmsdk/vectormath/cpp/vectormath_aos.h>

namespace dmGraphics
{
    static const char* g_validation_layers[]      = { "VK_LAYER_LUNARG_standard_validation" };
    static const uint8_t g_validation_layer_count = 1;

    static Context* g_Context = 0;

    static const char* VkResultToStr(VkResult res)
    {
        switch(res)
        {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_EVENT_SET: return "VK_EVENT_SET";
            case VK_EVENT_RESET: return "VK_EVENT_RESET";
            case VK_INCOMPLETE: return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
            case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
            case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
            case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
            case VK_ERROR_FRAGMENTATION_EXT: return "VK_ERROR_FRAGMENTATION_EXT";
            case VK_ERROR_NOT_PERMITTED_EXT: return "VK_ERROR_NOT_PERMITTED_EXT";
            case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT: return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
            case VK_RESULT_MAX_ENUM: return "VK_RESULT_MAX_ENUM";
            default: break;
        }

        return "UNKNOWN_ERROR";
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

            bool enable_validation = false;

            const char* env_vulkan_validation = getenv("DM_VULKAN_VALIDATION");
            if (env_vulkan_validation != 0x0)
            {
                enable_validation = strtol(env_vulkan_validation, 0, 10);
            }

            VkInstance vk_instance;
            if (CreateInstance(&vk_instance, g_validation_layers, enable_validation) != VK_SUCCESS)
            {
                dmLogError("Could not create Vulkan instance");
                return 0x0;
            }

            g_Context = new Context();
            g_Context->m_Instance = vk_instance;

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
        return glfwInit();
    }

    void Finalize()
    {
        glfwTerminate();
    }

    uint32_t GetWindowRefreshRate(HContext context)
    {
        return 0;
    }

    WindowResult OpenWindow(HContext context, WindowParams *params)
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

        VkResult res = CreateWindowSurface(context->m_Instance, &context->m_WindowSurface, params->m_HighDPI);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create window surface for Vulkan, reason: %s.", VkResultToStr(res));
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        uint32_t device_count = GetPhysicalDeviceCount(context->m_Instance);

        if (device_count == 0)
        {
            dmLogError("Could not get any Vulkan devices.");
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        PhysicalDevice* device_list     = new PhysicalDevice[device_count];
        PhysicalDevice* selected_device = NULL;
        GetPhysicalDevices(context->m_Instance, &device_list, device_count);

        const uint8_t required_device_extension_count = 2;
        const char* required_device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            // From spec:
            // "Allow negative height to be specified in the VkViewport::height field to
            // perform y-inversion of the clip-space to framebuffer-space transform.
            // This allows apps to avoid having to use gl_Position.y = -gl_Position.y
            // in shaders also targeting other APIs."
            "VK_KHR_maintenance1"
        };

        QueueFamily selected_queue_family;
        SwapChainCapabilities selected_swap_chain_capabilities;
        for (uint32_t i = 0; i < device_count; ++i)
        {
            #define RESET_AND_CONTINUE(d) \
                ResetPhysicalDevice(d); \
                continue;

            PhysicalDevice* device = &device_list[i];

            if (selected_device)
            {
                RESET_AND_CONTINUE(device)
            }

            // Make sure we have a graphics and present queue available
            QueueFamily queue_family = GetQueueFamily(device, context->m_WindowSurface);
            if (!queue_family.IsValid())
            {
                RESET_AND_CONTINUE(device)
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
                RESET_AND_CONTINUE(device)
            }

            // Make sure device has swap chain support
            GetSwapChainCapabilities(device, context->m_WindowSurface, selected_swap_chain_capabilities);

            if (selected_swap_chain_capabilities.m_SurfaceFormats.Size() == 0 ||
                selected_swap_chain_capabilities.m_PresentModes.Size() == 0)
            {
                RESET_AND_CONTINUE(device)
            }

            selected_device = device;
            selected_queue_family = queue_family;

            #undef RESET_AND_CONTINUE
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

        context->m_WindowOpened   = 1;
        context->m_PhysicalDevice = *selected_device;
        context->m_LogicalDevice  = logical_device;

        // Create swap chain
        context->m_SwapChainCapabilities.Swap(selected_swap_chain_capabilities);
        context->m_SwapChain    = new SwapChain(&context->m_LogicalDevice, context->m_WindowSurface, context->m_SwapChainCapabilities, selected_queue_family);

        res = UpdateSwapChain(context->m_SwapChain, &created_width, &created_height, want_vsync, context->m_SwapChainCapabilities);
        if (res != VK_SUCCESS)
        {
            dmLogError("Could not create a swap chain for Vulkan, reason: %s", VkResultToStr(res));
            goto bail;
        }

        delete[] device_list;

        return WINDOW_RESULT_OK;
bail:
        if (context->m_SwapChain)
            delete context->m_SwapChain;
        if (device_list)
            delete[] device_list;

        return WINDOW_RESULT_WINDOW_OPEN_ERROR;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            glfwCloseWindow();

            ResetSwapChain(context->m_SwapChain);

            ResetPhysicalDevice(&context->m_PhysicalDevice);

            vkDestroyDevice(context->m_LogicalDevice.m_Device, 0);

            vkDestroySurfaceKHR(context->m_Instance, context->m_WindowSurface, 0);

            DestroyInstance(&context->m_Instance);

            context->m_WindowOpened = 0;

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
        return 0;
    }

    uint32_t GetHeight(HContext context)
    {
        return 0;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        return 0;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        return 0;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {}

    void ResizeWindow(HContext context, uint32_t width, uint32_t height)
    {}

    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {}

    void Flip(HContext context)
    {}

    void SetSwapInterval(HContext context, uint32_t swap_interval)
    {}

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {}

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return (HVertexBuffer) new uint32_t;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        delete (uint32_t*) buffer;
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {}

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {}

    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        return 0;
    }

    bool UnmapVertexBuffer(HVertexBuffer buffer)
    {
        return true;
    }

    uint32_t GetMaxElementsVertices(HContext context)
    {
        return 65536;
    }

    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return (HIndexBuffer) new uint32_t;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        delete (uint32_t*) buffer;
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {}

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {}

    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        return 0;
    }

    bool UnmapIndexBuffer(HIndexBuffer buffer)
    {
        return true;
    }

    bool IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        return true;
    }

    uint32_t GetMaxElementsIndices(HContext context)
    {
        return 65536;
    }


    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        return new VertexDeclaration;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        return new VertexDeclaration;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {}

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {}

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {}

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {}


    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {}

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {}

    HVertexProgram NewVertexProgram(HContext context, const void* program, uint32_t program_size)
    {
        return (HVertexProgram) new uint32_t;
    }

    HFragmentProgram NewFragmentProgram(HContext context, const void* program, uint32_t program_size)
    {
        return (HFragmentProgram) new uint32_t;
    }

    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        return (HProgram) new uint32_t;
    }

    void DeleteProgram(HContext context, HProgram program)
    {
        delete (uint32_t*) program;
    }

    bool ReloadVertexProgram(HVertexProgram prog, const void* program, uint32_t program_size)
    {
        return true;
    }

    bool ReloadFragmentProgram(HFragmentProgram prog, const void* program, uint32_t program_size)
    {
        return true;
    }

    void DeleteVertexProgram(HVertexProgram prog)
    {
        delete (uint32_t*) prog;
    }

    void DeleteFragmentProgram(HFragmentProgram prog)
    {
        delete (uint32_t*) prog;
    }

    ShaderDesc::Language GetShaderProgramLanguage(HContext context)
    {
        return ShaderDesc::LANGUAGE_SPIRV;
    }


    void EnableProgram(HContext context, HProgram program)
    {}

    void DisableProgram(HContext context)
    {}

    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        return true;
    }


    uint32_t GetUniformCount(HProgram prog)
    {
        return 0;
    }

    void GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type)
    {}

    int32_t GetUniformLocation(HProgram prog, const char* name)
    {
        return 0;
    }

    void SetConstantV4(HContext context, const Vectormath::Aos::Vector4* data, int base_register)
    {}

    void SetConstantM4(HContext context, const Vectormath::Aos::Vector4* data, int base_register)
    {}

    void SetSampler(HContext context, int32_t location, int32_t unit)
    {}


    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {}

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
    {}

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
        return new RenderTarget;
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
        return new Texture;
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
        return 0;
    }

    uint16_t GetTextureHeight(HTexture texture)
    {
        return 0;
    }

    uint16_t GetOriginalTextureWidth(HTexture texture)
    {
        return 0;
    }

    uint16_t GetOriginalTextureHeight(HTexture texture)
    {
        return 0;
    }

    void EnableTexture(HContext context, uint32_t unit, HTexture texture)
    {}

    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {}

    uint32_t GetMaxTextureSize(HContext context)
    {
        return 0;
    }

    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        return 0;
    }

    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {}

    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }
}
