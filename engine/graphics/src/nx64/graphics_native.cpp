#include <stdio.h>

#include <dlib/log.h>

#ifndef VK_USE_PLATFORM_VI_NN
#define VK_USE_PLATFORM_VI_NN
#endif

#include <dlib/sys.h>
#include "../graphics.h"
#include "../vulkan/graphics_vulkan_defines.h"
#include "../vulkan/graphics_vulkan_private.h"

#include <nn/nn_Assert.h>
#include <nn/nn_Log.h>

#include <nn/nn_Result.h>
#include <nn/fs.h>
#include <nn/os.h>
#include <nn/init.h>
#include <nn/vi.h>
#include <nn/hid.h>

#include <nv/nv_MemoryManagement.h>

#include <nn/mem.h>

namespace
{
    void* Allocate(size_t size, size_t alignment, void*)
    {
        return aligned_alloc(alignment, nn::util::align_up(size, alignment));
    }
    void Free(void* addr, void*)
    {
        free(addr);
    }
    void* Reallocate(void* addr, size_t newSize, void*)
    {
        return realloc(addr, newSize);
    }

}; // end anonymous namespace

const size_t        g_GraphicsSystemMemorySize = 32 * 1024 * 1024;

nn::vi::Display*    g_pDisplay = 0;
nn::vi::Layer*      g_pLayer = 0;
int                 g_NativeInitialized = 0;

namespace dmGraphics
{
    bool NativeInit()
    {
        if (!g_NativeInitialized)
        {
            nv::SetGraphicsAllocator(Allocate, Free, Reallocate, NULL);
            nv::SetGraphicsDevtoolsAllocator(Allocate, Free, Reallocate, NULL);
            nv::InitializeGraphics(malloc(g_GraphicsSystemMemorySize), g_GraphicsSystemMemorySize);
            nn::vi::Initialize();

            nn::Result result = nn::vi::OpenDefaultDisplay(&g_pDisplay);
            NN_ASSERT(result.IsSuccess());
            NN_UNUSED(result);

            result = nn::vi::CreateLayer(&g_pLayer, g_pDisplay);
            NN_ASSERT(result.IsSuccess());

            g_NativeInitialized = 1;
        }
        return true;
    }

    void NativeExit()
    {
        nn::vi::DestroyLayer(g_pLayer);
        nn::vi::CloseDisplay(g_pDisplay);
        nn::vi::Finalize();
    }

    static const char*   g_extension_names[]       = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_NN_VI_SURFACE_EXTENSION_NAME,
    };
    // Validation layers to enable
    static const char* g_validation_layers[3];
    static const char* DM_VULKAN_LAYER_SWAPCHAIN    = "VK_LAYER_NN_vi_swapchain";
    static const char* DM_VULKAN_LAYER_VALIDATION   = "VK_LAYER_LUNARG_standard_validation";
    static const char* DM_VULKAN_LAYER_RENDERDOC    = "VK_LAYER_RENDERDOC_Capture";

    // Validation layer extensions
    static const char*   g_validation_layer_ext[]     = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

    const char** GetExtensionNames(uint16_t* num_extensions)
    {
        *num_extensions = sizeof(g_extension_names) / sizeof(g_extension_names[0]);
        return g_extension_names;
    }

    const char** GetValidationLayers(uint16_t* num_layers)
    {
        uint16_t count = 0;
        g_validation_layers[count++] = DM_VULKAN_LAYER_SWAPCHAIN;

        if (dmSys::GetEnv("DM_VULKAN_VALIDATION"))
        {
            g_validation_layers[count++] = DM_VULKAN_LAYER_VALIDATION;
        }
        if (dmSys::GetEnv("DM_VULKAN_RENDERDOC"))
        {
            g_validation_layers[count++] = DM_VULKAN_LAYER_RENDERDOC;
        }

        *num_layers = count;
        return g_validation_layers;
    }

    const char** GetValidationLayersExt(uint16_t* num_layers)
    {
        *num_layers = sizeof(g_validation_layer_ext) / sizeof(g_validation_layer_ext[0]);
        return g_validation_layer_ext;
    }

    // Called from InitializeVulkan
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        VkViSurfaceCreateInfoNN createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN;
        createInfo.pNext = NULL;
        createInfo.flags = 0;

        nn::vi::NativeWindowHandle nativeWindow;
        nn::vi::GetNativeWindow(&nativeWindow, g_pLayer);
        createInfo.window = nativeWindow;

        VkResult result = vkCreateViSurfaceNN(vkInstance, &createInfo, NULL, vkSurfaceOut);
        NN_ASSERT(result == VK_SUCCESS);

        return result;
    }

    WindowResult OpenWindow(HContext context, WindowParams* params)
    {
        assert(context);
        assert(params);

        if (context->m_WindowOpened)
            return WINDOW_RESULT_ALREADY_OPENED;

        if (!InitializeVulkan(context, params))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        context->m_WindowOpened                  = 1;
        context->m_Width                         = params->m_Width;
        context->m_Height                        = params->m_Height;
        context->m_WindowWidth                   = context->m_SwapChain->m_ImageExtent.width;
        context->m_WindowHeight                  = context->m_SwapChain->m_ImageExtent.height;
        context->m_WindowResizeCallback          = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData  = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback           = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData   = params->m_CloseCallbackUserData;
        context->m_WindowFocusCallback           = params->m_FocusCallback;
        context->m_WindowFocusCallbackUserData   = params->m_FocusCallbackUserData;
        context->m_WindowIconifyCallback         = params->m_IconifyCallback;
        context->m_WindowIconifyCallbackUserData = params->m_IconifyCallbackUserData;

        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HContext context)
    {
        if (context->m_WindowOpened)
        {
            VkDevice vk_device = context->m_LogicalDevice.m_Device;

            vkDeviceWaitIdle(vk_device);

            context->m_PipelineCache.Iterate(DestroyPipelineCacheCb, context);

            DestroyDeviceBuffer(vk_device, &context->m_MainTextureDepthStencil.m_DeviceBuffer.m_Handle);
            DestroyTexture(vk_device, &context->m_MainTextureDepthStencil.m_Handle);

            vkDestroyRenderPass(vk_device, context->m_MainRenderPass, 0);

            vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_MainCommandBuffers.Size(), context->m_MainCommandBuffers.Begin());
            vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &context->m_MainCommandBufferUploadHelper);

            for (uint8_t i=0; i < context->m_MainFrameBuffers.Size(); i++)
            {
                vkDestroyFramebuffer(vk_device, context->m_MainFrameBuffers[i], 0);
            }

            for (uint8_t i=0; i < context->m_TextureSamplers.Size(); i++)
            {
                DestroyTextureSampler(vk_device, &context->m_TextureSamplers[i]);
            }

            for (uint8_t i=0; i < context->m_MainScratchBuffers.Size(); i++)
            {
                DestroyDeviceBuffer(vk_device, &context->m_MainScratchBuffers[i].m_DeviceBuffer.m_Handle);
            }

            for (uint8_t i=0; i < context->m_MainDescriptorAllocators.Size(); i++)
            {
                DestroyDescriptorAllocator(vk_device, &context->m_MainDescriptorAllocators[i].m_Handle);
            }

            for (uint8_t i=0; i < context->m_MainCommandBuffers.Size(); i++)
            {
                FlushResourcesToDestroy(vk_device, context->m_MainResourcesToDestroy[i]);
            }

            for (size_t i = 0; i < g_max_frames_in_flight; i++) {
                FrameResource& frame_resource = context->m_FrameResources[i];
                vkDestroySemaphore(vk_device, frame_resource.m_RenderFinished, 0);
                vkDestroySemaphore(vk_device, frame_resource.m_ImageAvailable, 0);
                vkDestroyFence(vk_device, frame_resource.m_SubmitFence, 0);
            }

            DestroySwapChain(vk_device, context->m_SwapChain);
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
    {
        assert(context);
        if (context->m_WindowOpened)
        {
        }
    }


    uint32_t GetWindowState(HContext context, WindowState state)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            switch(state) {
            case WINDOW_STATE_OPENED:    return 1;
            case WINDOW_STATE_ICONIFIED: return 0; // not supported
            default:
                {
                    printf("UNKNOWN WINDOW STATE: %d\n", state);
                    return 1;
                }
            }
        }
        return 0;
    }

    uint32_t GetWindowRefreshRate(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
            return 60;//glfwGetWindowRefreshRate();
        else
            return 0;
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        return 0;
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

    void GetNativeWindowSize(uint32_t* width, uint32_t* height)
    {
        *width = 1280;
        *height = 720;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            context->m_Width = width;
            context->m_Height = height;
            context->m_WindowWidth = width;
            context->m_WindowHeight = height;
            if (context->m_WindowResizeCallback)
            {
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, width, height);
            }
        }
    }

    void ResizeWindow(HContext context, uint32_t width, uint32_t height)
    {
    }

    void SwapBuffers()
    {
    }

    void SetSwapInterval(HContext context, uint32_t swap_interval)
    {
    }

    typedef void* id;
    typedef void* EGLContext;
    typedef void* EGLSurface;
    typedef void* JavaVM;
    typedef void* jobject;
    typedef void* android_app;
    typedef void* HWND;
    typedef void* HGLRC;
    typedef void* Window;
    typedef void* GLXContext;
    typedef void* Display;

    id GetNativeiOSUIWindow()               { return 0; }
    id GetNativeiOSUIView()                 { return 0; }
    id GetNativeiOSEAGLContext()            { return 0; }
    id GetNativeOSXNSWindow()               { return 0; }
    id GetNativeOSXNSView()                 { return 0; }
    id GetNativeOSXNSOpenGLContext()        { return 0; }
    HWND GetNativeWindowsHWND()             { return 0; }
    HGLRC GetNativeWindowsHGLRC()           { return 0; }
    EGLContext GetNativeAndroidEGLContext() { return 0; }
    EGLSurface GetNativeAndroidEGLSurface() { return 0; }
    JavaVM* GetNativeAndroidJavaVM()        { return 0; }
    jobject GetNativeAndroidActivity()      { return 0; }
    android_app* GetNativeAndroidApp()      { return 0; }
    Window GetNativeX11Window()             { return 0; }
    GLXContext GetNativeX11GLXContext()     { return 0; }
}
