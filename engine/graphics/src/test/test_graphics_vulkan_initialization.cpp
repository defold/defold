// Copyright 2020-2026 The Defold Foundation
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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <platform/window.hpp>
#include "../vulkan/graphics_vulkan_private.h"

#if defined(__MACH__)
// These macOS-only queries are never reached by the injected startup failures.
static PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2;
static PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
#endif

namespace dmGraphics
{
    bool     TestNativeInit(const ContextParams& params);
    bool     TestNativeInitializeContext(HContext context);
    void     TestLoadVulkanFunctions(VkInstance instance);
    VkResult TestCreateInstance(VkInstance* instance, uint32_t api_version,
                                const char** extensions, uint16_t extension_count, const char** layers, uint16_t layer_count,
                                const char** layer_extensions, uint16_t layer_extension_count);
}

// Exercise the real context lifetime and teardown using deterministic failures,
// without a system Vulkan loader, native window, or GPU.
#define NativeInit TestNativeInit
#define NativeInitializeContext TestNativeInitializeContext
#define LoadVulkanFunctions TestLoadVulkanFunctions
#define CreateInstance TestCreateInstance
#include "../vulkan/graphics_vulkan.cpp"
#undef NativeInit
#undef NativeInitializeContext
#undef LoadVulkanFunctions
#undef CreateInstance

static bool g_LiveHandles[64];
static uint32_t g_HandleCount;
static uint32_t g_DestroyedInstances;
static uint32_t g_DestroyedSurfaces;
static uint32_t g_DestroyedDevices;
static uint32_t g_FailureStage;

template <typename T>
static T NewHandle()
{
    ++g_HandleCount;
    assert(g_HandleCount < DM_ARRAY_SIZE(g_LiveHandles));
    g_LiveHandles[g_HandleCount] = true;
    return (T) (uintptr_t) g_HandleCount;
}

template <typename T>
static void ReleaseHandle(T handle)
{
    if (handle == VK_NULL_HANDLE)
    {
        return;
    }

    uintptr_t index = (uintptr_t) handle;
    ASSERT_LT(index, DM_ARRAY_SIZE(g_LiveHandles));
    ASSERT_TRUE(g_LiveHandles[index]);
    g_LiveHandles[index] = false;
}

template <typename T>
static void VKAPI_CALL DestroyDeviceResource(VkDevice device, T handle, const VkAllocationCallbacks*)
{
    ASSERT_TRUE(g_LiveHandles[(uintptr_t) device]);
    ReleaseHandle(handle);
}

static void VKAPI_CALL DestroyInstance(VkInstance instance, const VkAllocationCallbacks*)
{
    ++g_DestroyedInstances;
    ReleaseHandle(instance);
}

static void VKAPI_CALL DestroySurface(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks*)
{
    ASSERT_TRUE(g_LiveHandles[(uintptr_t) instance]);
    ++g_DestroyedSurfaces;
    ReleaseHandle(surface);
}

static void VKAPI_CALL DestroyDevice(VkDevice device, const VkAllocationCallbacks*)
{
    ++g_DestroyedDevices;
    ReleaseHandle(device);
}

static VkResult VKAPI_CALL DeviceWaitIdle(VkDevice device)
{
    EXPECT_TRUE(g_LiveHandles[(uintptr_t) device]);
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL EnumeratePhysicalDevices(VkInstance instance, uint32_t* count, VkPhysicalDevice*)
{
    *count = 0;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL CreateSwapchain(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR* swapchain)
{
    *swapchain = NewHandle<VkSwapchainKHR>();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL GetSwapchainImages(VkDevice, VkSwapchainKHR, uint32_t* count, VkImage* images)
{
    *count = 2;
    if (images)
    {
        // Swap-chain images are owned by Vulkan, not destroyed by the application.
        images[0] = (VkImage) (uintptr_t) 60;
        images[1] = (VkImage) (uintptr_t) 61;
    }

    return VK_SUCCESS;
}

static VkResult VKAPI_CALL CreateImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView* view)
{
    *view = NewHandle<VkImageView>();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL FailCreateSemaphore(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore*)
{
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

namespace dmPlatform
{
    const char** VulkanGetRequiredInstanceExtensions(uint32_t* count)
    {
        *count = 0;
        return 0;
    }

    void VulkanSetLoader(PFN_vkGetInstanceProcAddr)
    {
    }

}

namespace dmGraphics
{
    bool TestNativeInit(const ContextParams&)
    {
        return true;
    }

    void TestLoadVulkanFunctions(VkInstance)
    {
    }

    VkResult TestCreateInstance(VkInstance* instance, uint32_t, const char**, uint16_t,
                                const char**, uint16_t, const char**, uint16_t)
    {
        *instance = NewHandle<VkInstance>();
        return VK_SUCCESS;
    }

    VkResult CreateWindowSurface(HWindow, VkInstance, VkSurfaceKHR* surface, bool, void**)
    {
        if (g_FailureStage == 0)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        *surface = NewHandle<VkSurfaceKHR>();
        return VK_SUCCESS;
    }

    bool TestNativeInitializeContext(HContext hcontext)
    {
        if (g_FailureStage < 2)
        {
            // Real initialization fails either at surface creation or device selection.
            return InitializeVulkan(hcontext);
        }

        // Model failure after only some swap-chain and rendering resources exist.
        // Every live handle is tracked, including gaps in partially filled arrays.
        VulkanContext* context = (VulkanContext*) hcontext;
        context->m_WindowSurface = NewHandle<VkSurfaceKHR>();
        context->m_LogicalDevice.m_Device = NewHandle<VkDevice>();
        context->m_LogicalDevice.m_CommandPool = NewHandle<VkCommandPool>();
        context->m_LogicalDevice.m_QueueMutex = dmMutex::New();
        context->m_PhysicalDevice.m_QueueFamilyProperties = new VkQueueFamilyProperties[1];
        context->m_PhysicalDevice.m_DeviceExtensions = new VkExtensionProperties[1];
        context->m_VkPipelineCache = NewHandle<VkPipelineCache>();
        context->m_DynamicOffsetBuffer = (uint32_t*) malloc(sizeof(uint32_t));
        context->m_MainFrameBuffers.SetCapacity(2);
        context->m_MainFrameBuffers.SetSize(2);
        context->m_MainFrameBuffers[0] = NewHandle<VkFramebuffer>();
        context->m_MainFrameBuffers[1] = VK_NULL_HANDLE;
        context->m_MainRenderPass = NewHandle<VkRenderPass>();
        context->m_MainResourcesToDestroy[0] = new ResourcesToDestroyList;
        context->m_FrameResources[0].m_ImageAvailable = NewHandle<VkSemaphore>();
        context->m_DefaultTexture2D = new VulkanTexture;
        InitializeVulkanTexture(context->m_DefaultTexture2D);
        context->m_DefaultTexture2D->m_Handle.m_Image = NewHandle<VkImage>();
        context->m_DefaultTexture2D->m_Handle.m_ImageView = NewHandle<VkImageView>();
        context->m_DefaultTexture2D->m_DeviceBuffer.m_Allocation.m_Handle.m_Memory = NewHandle<VkDeviceMemory>();

        SwapChainCapabilities capabilities;
        VkSurfaceFormatKHR format = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        capabilities.m_SurfaceFormats.SetCapacity(1);
        capabilities.m_SurfaceFormats.Push(format);
        QueueFamily queues;
        InitializeVulkanTexture(&context->m_ResolveTexture);
        context->m_SwapChain = new SwapChain(context->m_WindowSurface, VK_SAMPLE_COUNT_1_BIT,
                                             capabilities, queues, &context->m_ResolveTexture, 0);
        if (g_FailureStage == 3)
        {
            memset(&capabilities.m_SurfaceCapabilities, 0, sizeof(capabilities.m_SurfaceCapabilities));
            capabilities.m_SurfaceCapabilities.currentExtent.width = 640;
            capabilities.m_SurfaceCapabilities.currentExtent.height = 480;
            capabilities.m_SurfaceCapabilities.minImageCount = 1;
            capabilities.m_SurfaceCapabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            // Poison unused capacity so cleanup cannot accidentally rely on zeroed allocator memory.
            context->m_SwapChain->m_ImageViews.SetCapacity(2);
            context->m_SwapChain->m_ImageViews.SetSize(2);
            context->m_SwapChain->m_RenderFinishedSemaphores.SetCapacity(2);
            context->m_SwapChain->m_RenderFinishedSemaphores.SetSize(2);
            for (uint32_t i = 0; i < 2; ++i)
            {
                context->m_SwapChain->m_ImageViews[i] = (VkImageView) (uintptr_t) 63;
                context->m_SwapChain->m_RenderFinishedSemaphores[i] = (VkSemaphore) (uintptr_t) 63;
            }

            uint32_t width = 640, height = 480;
            VkResult result = UpdateSwapChain(&context->m_PhysicalDevice, &context->m_LogicalDevice,
                                              &width, &height, true, capabilities, context->m_SwapChain);
            EXPECT_EQ(VK_ERROR_OUT_OF_DEVICE_MEMORY, result);
            return false;
        }

        context->m_SwapChain->m_SwapChain = NewHandle<VkSwapchainKHR>();
        context->m_SwapChain->m_ImageViews.SetCapacity(2);
        context->m_SwapChain->m_ImageViews.SetSize(2);
        context->m_SwapChain->m_ImageViews[0] = NewHandle<VkImageView>();
        context->m_SwapChain->m_ImageViews[1] = VK_NULL_HANDLE;
        context->m_SwapChain->m_RenderFinishedSemaphores.SetCapacity(2);
        context->m_SwapChain->m_RenderFinishedSemaphores.SetSize(2);
        context->m_SwapChain->m_RenderFinishedSemaphores[0] = NewHandle<VkSemaphore>();
        context->m_SwapChain->m_RenderFinishedSemaphores[1] = VK_NULL_HANDLE;
        context->m_ResolveTexture.m_Handle.m_Image = NewHandle<VkImage>();
        context->m_ResolveTexture.m_DeviceBuffer.m_Allocation.m_Handle.m_Memory = NewHandle<VkDeviceMemory>();
        return false;
    }

}

class VulkanInitializationTest : public jc_test_base_class
{
    public:
    void SetUp()
    {
        memset(g_LiveHandles, 0, sizeof(g_LiveHandles));
        g_HandleCount = 0;
        g_DestroyedInstances = 0;
        g_DestroyedSurfaces = 0;
        g_DestroyedDevices = 0;
        g_FailureStage = 0;
        vkDestroyInstance = DestroyInstance;
        vkDestroySurfaceKHR = DestroySurface;
        vkDestroyDevice = DestroyDevice;
        vkDeviceWaitIdle = DeviceWaitIdle;
        vkEnumeratePhysicalDevices = EnumeratePhysicalDevices;
        vkDestroyCommandPool = DestroyDeviceResource<VkCommandPool>;
        vkDestroyPipelineCache = DestroyDeviceResource<VkPipelineCache>;
        vkDestroyFramebuffer = DestroyDeviceResource<VkFramebuffer>;
        vkDestroyRenderPass = DestroyDeviceResource<VkRenderPass>;
        vkDestroySemaphore = DestroyDeviceResource<VkSemaphore>;
        vkDestroyFence = DestroyDeviceResource<VkFence>;
        vkDestroyImage = DestroyDeviceResource<VkImage>;
        vkDestroyImageView = DestroyDeviceResource<VkImageView>;
        vkDestroySwapchainKHR = DestroyDeviceResource<VkSwapchainKHR>;
        vkFreeMemory = DestroyDeviceResource<VkDeviceMemory>;
        vkCreateSwapchainKHR = CreateSwapchain;
        vkGetSwapchainImagesKHR = GetSwapchainImages;
        vkCreateImageView = CreateImageView;
        vkCreateSemaphore = FailCreateSemaphore;
        m_Params = dmGraphics::ContextParams();
        m_Params.m_Window = dmPlatform::NewWindow();
        WindowCreateParams window_params;
        WindowCreateParamsInitialize(&window_params);
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Params.m_Window, window_params));
    }

    void TearDown()
    {
        ASSERT_EQ((dmGraphics::HContext) 0, dmGraphics::VulkanGetContext());
        for (uint32_t i = 1; i <= g_HandleCount; ++i)
        {
            EXPECT_FALSE(g_LiveHandles[i]);
        }

        dmPlatform::CloseWindow(m_Params.m_Window);
        dmPlatform::DeleteWindow(m_Params.m_Window);
    }

    dmGraphics::ContextParams m_Params;
};

// A replacement swapchain can gain or lose backbuffer readback through either its usage flags or its pixel format.
TEST(VulkanSwapChainTest, BackbufferReadbackSupport)
{
    using namespace dmGraphics;
    GraphicsContext context = {};
    const VkImageUsageFlags readback_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    SetBackbufferReadbackSupport(&context, readback_usage, VK_FORMAT_B8G8R8A8_SRGB);
    ASSERT_TRUE(IsContextFeatureSupported(&context, CONTEXT_FEATURE_BACKBUFFER_READBACK));
    SetBackbufferReadbackSupport(&context, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_FORMAT_B8G8R8A8_UNORM);
    ASSERT_FALSE(IsContextFeatureSupported(&context, CONTEXT_FEATURE_BACKBUFFER_READBACK));
    SetBackbufferReadbackSupport(&context, readback_usage, VK_FORMAT_R8G8B8A8_UNORM);
    ASSERT_TRUE(IsContextFeatureSupported(&context, CONTEXT_FEATURE_BACKBUFFER_READBACK));
    SetBackbufferReadbackSupport(&context, readback_usage, VK_FORMAT_R16G16B16A16_SFLOAT);
    ASSERT_FALSE(IsContextFeatureSupported(&context, CONTEXT_FEATURE_BACKBUFFER_READBACK));
}

// A failed surface must release the instance and clear the global context for another attempt.
TEST_F(VulkanInitializationTest, SurfaceFailureCanRetry)
{
    ASSERT_EQ((dmGraphics::HContext) 0, dmGraphics::VulkanNewContext(m_Params));
    ASSERT_EQ((dmGraphics::HContext) 0, dmGraphics::VulkanNewContext(m_Params));
    ASSERT_EQ(2, g_DestroyedInstances);
    ASSERT_EQ(0, g_DestroyedSurfaces);
    ASSERT_EQ(0, g_DestroyedDevices);
}

// A surface without a usable presentation device must be destroyed before its instance.
TEST_F(VulkanInitializationTest, DeviceSelectionFailure)
{
    g_FailureStage = 1;
    ASSERT_EQ((dmGraphics::HContext) 0, dmGraphics::VulkanNewContext(m_Params));
    ASSERT_EQ(1, g_DestroyedInstances);
    ASSERT_EQ(1, g_DestroyedSurfaces);
    ASSERT_EQ(0, g_DestroyedDevices);
}

// Partial swap-chain/default-resource allocation must release every handle exactly once, including resolve memory.
TEST_F(VulkanInitializationTest, PartialRenderingResources)
{
    g_FailureStage = 2;
    ASSERT_EQ((dmGraphics::HContext) 0, dmGraphics::VulkanNewContext(m_Params));
    ASSERT_EQ(1, g_DestroyedInstances);
    ASSERT_EQ(1, g_DestroyedSurfaces);
    ASSERT_EQ(1, g_DestroyedDevices);
}

// A failure partway through swap-chain allocation must not destroy uninitialized array entries.
TEST_F(VulkanInitializationTest, SwapChainSemaphoreFailure)
{
    g_FailureStage = 3;
    ASSERT_EQ((dmGraphics::HContext) 0, dmGraphics::VulkanNewContext(m_Params));
    ASSERT_EQ(1, g_DestroyedInstances);
    ASSERT_EQ(1, g_DestroyedSurfaces);
    ASSERT_EQ(1, g_DestroyedDevices);
}

// A device whose command-pool setup failed has no queue mutex and must still be releasable.
TEST_F(VulkanInitializationTest, PartialLogicalDevice)
{
    dmGraphics::LogicalDevice device = {};
    device.m_Device = NewHandle<VkDevice>();
    device.m_CommandPool = NewHandle<VkCommandPool>();
    dmGraphics::DestroyLogicalDevice(&device);
    ASSERT_TRUE(device.m_Device == VK_NULL_HANDLE);
    ASSERT_EQ(1, g_DestroyedDevices);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
