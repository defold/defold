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
#include <testmain/testmain.h>
#include <platform/window.hpp>
#include "graphics.h"
#if defined(DM_TEST_WINDOW_DX12)
#include "dx12/graphics_dx12_private.h"
extern "C" void GraphicsAdapterDX12();
#else
#include "vulkan/graphics_vulkan_defines.h"
#include "vulkan/graphics_vulkan_private.h"
extern "C" void GraphicsAdapterVulkan();
#endif

using namespace dmGraphics;

struct WindowResizeTest : public jc_test_base_class
{
    HContext    m_Context;
    HWindow     m_Window;
    HJobContext m_Jobs;
    uint32_t    m_CallbackCount;
    uint32_t    m_CallbackWidth;
    uint32_t    m_CallbackHeight;

    static void OnResize(void* user_data, uint32_t width, uint32_t height)
    {
        WindowResizeTest* test = (WindowResizeTest*) user_data;
        ++test->m_CallbackCount;
        test->m_CallbackWidth = width;
        test->m_CallbackHeight = height;
    }

    void SetUp()
    {
        m_CallbackCount = 0;
        m_CallbackWidth = m_CallbackHeight = 0;
        m_Window = dmPlatform::NewWindow();
        WindowCreateParams params;
        WindowCreateParamsInitialize(&params);
        params.m_Width = 320;
        params.m_Height = 240;
        params.m_Title = "Window resize regression";
        params.m_Hidden = 1;
        params.m_FocusOnShow = 0;
        params.m_Samples = 1;
        params.m_ResizeCallback = OnResize;
        params.m_ResizeCallbackUserData = this;
#if defined(DM_TEST_WINDOW_DX12)
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_DIRECTX;
#else
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
#endif
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, params));
        JobSystemCreateParams jobs = {};
        jobs.m_ThreadCount = 1;
        m_Jobs = JobSystemCreate(&jobs);
        ContextParams context_params = {};
        context_params.m_Window = m_Window;
        context_params.m_Width = 320;
        context_params.m_Height = 240;
        context_params.m_JobContext = m_Jobs;
        context_params.m_VerifyGraphicsCalls = 1;
        m_Context = NewContext(context_params);
        ASSERT_NE((HContext)0, m_Context);
    }

    void TearDown()
    {
        CloseWindow(m_Context);
        DeleteContext(m_Context);
        JobSystemDestroy(m_Jobs);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
    }

    void CheckSize(uint32_t width, uint32_t height)
    {
        ASSERT_EQ(width, GetWindowWidth(m_Context));
        ASSERT_EQ(height, GetWindowHeight(m_Context));
        ASSERT_EQ(320u, GetWidth(m_Context));
        ASSERT_EQ(240u, GetHeight(m_Context));
#if defined(DM_TEST_WINDOW_DX12)
        DX12Context* context = (DX12Context*) m_Context;
        ASSERT_EQ(width, context->m_BackBufferWidth);
        ASSERT_EQ(height, context->m_BackBufferHeight);
#else
        VulkanContext* context = (VulkanContext*) m_Context;
        ASSERT_EQ(width, context->m_SwapChain->m_ImageExtent.width);
        ASSERT_EQ(height, context->m_SwapChain->m_ImageExtent.height);
#endif
    }

    void DrawFrame()
    {
        BeginFrame(m_Context);
        SetRenderTarget(m_Context, 0, RenderTargetBindingParams());
        Clear(m_Context, BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT, 31, 73, 149, 255, 1.0f, 0);
        Flip(m_Context);
    }
};

// Guards editor resize requests: the native window and swap chain must resize,
// while the logical game dimensions stay unchanged, including on the next frame.
TEST_F(WindowResizeTest, ResizeWindowPreservesLogicalSize)
{
    const uint32_t sizes[][2] = { { 480, 320 }, { 320, 480 }, { 320, 240 } };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(sizes); ++i)
    {
        m_CallbackCount = 0;
        ResizeWindow(m_Context, sizes[i][0], sizes[i][1]);
        ASSERT_GT(m_CallbackCount, 0u);
        ASSERT_EQ(sizes[i][0], m_CallbackWidth);
        ASSERT_EQ(sizes[i][1], m_CallbackHeight);
        CheckSize(sizes[i][0], sizes[i][1]);
        DrawFrame();
        CheckSize(sizes[i][0], sizes[i][1]);
        DrawFrame();
        CheckSize(sizes[i][0], sizes[i][1]);
    }
}

// Keeps the separate SetWindowSize contract intact when sharing resize code.
TEST_F(WindowResizeTest, SetWindowSizeUpdatesLogicalSize)
{
    SetWindowSize(m_Context, 480, 320);
    DrawFrame();
    ASSERT_EQ(480u, GetWidth(m_Context));
    ASSERT_EQ(320u, GetHeight(m_Context));
    ASSERT_EQ(480u, GetWindowWidth(m_Context));
    ASSERT_EQ(320u, GetWindowHeight(m_Context));
}

#if defined(DM_TEST_WINDOW_DX12)
// Verifies native DX12 contexts expose storage buffers and can allocate the
// 12 MiB buffer from issue #13694 through the public API.
TEST_F(WindowResizeTest, StorageBufferCapabilityAndAllocation)
{
    ASSERT_TRUE(IsContextFeatureSupported(m_Context, CONTEXT_FEATURE_STORAGE_BUFFER));

    const uint32_t size = 12 * 1024 * 1024;
    HStorageBuffer buffer = NewStorageBuffer(m_Context, size, 0, BUFFER_USAGE_DYNAMIC_DRAW);
    ASSERT_NE((HStorageBuffer)0, buffer);
    ASSERT_EQ(size, GetStorageBufferSize(m_Context, buffer));
    DeleteStorageBuffer(m_Context, buffer);
}

// Guards DX12's swap-chain recreation while commands for a frame are recorded.
TEST_F(WindowResizeTest, ResizeDuringFrame)
{
    BeginFrame(m_Context);
    SetRenderTarget(m_Context, 0, RenderTargetBindingParams());
    Clear(m_Context, BUFFER_TYPE_COLOR0_BIT, 31, 73, 149, 255, 1.0f, 0);
    ResizeWindow(m_Context, 480, 320);
    CheckSize(480, 320);
    Clear(m_Context, BUFFER_TYPE_COLOR0_BIT, 149, 73, 31, 255, 1.0f, 0);
    Flip(m_Context);
    DrawFrame();
    CheckSize(480, 320);
}
#endif

int main(int argc, char** argv)
{
    TestMainPlatformInit();
#if defined(DM_TEST_WINDOW_DX12)
    GraphicsAdapterDX12();
    AdapterFamily family = ADAPTER_FAMILY_DIRECTX;
#else
    GraphicsAdapterVulkan();
    AdapterFamily family = ADAPTER_FAMILY_VULKAN;
#endif
    if (!InstallAdapter(family) || GetInstalledAdapterFamily() != family)
    {
        return 1;
    }
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    Finalize();
    return result;
}
