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

#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <graphics/graphics.h>
#include <platform/window.hpp>
#include "graphics_adapter.h"

namespace dmPlatform
{
    WindowResult TestOpenWindow(HWindow window, const WindowCreateParams& params);
    void         TestCloseWindow(HWindow window);
}

// Replace only native window operations; adapter selection and function-table
// replacement use the real graphics implementation with registered test adapters.
#define OpenWindow TestOpenWindow
#define CloseWindow TestCloseWindow
#include "../engine_graphics.cpp"
#undef OpenWindow
#undef CloseWindow

static bool g_OpenGLSupported;
static bool g_OpenGLESSupported;
static bool g_FailVulkanWindow;
static bool g_FailVulkanContext;
static bool g_FailFallbackWindow;
static bool g_FailFallbackContext;
static bool g_WindowOpen;
static int g_WindowAttempts;
static int g_ContextAttempts;
static int g_CloseCount;
static int g_FinalizeCount;
static dmGraphics::HContext g_Context;
static WindowCreateParams g_LastWindowParams;
static dmGraphics::ContextParams g_LastContextParams;

namespace dmPlatform
{
    WindowResult TestOpenWindow(HWindow window, const WindowCreateParams& params)
    {
        EXPECT_FALSE(g_WindowOpen);
        ++g_WindowAttempts;
        g_LastWindowParams = params;
        bool fail = params.m_GraphicsApi == WINDOW_GRAPHICS_API_VULKAN ? g_FailVulkanWindow : g_FailFallbackWindow;
        g_WindowOpen = !fail;
        return fail ? WINDOW_RESULT_WINDOW_OPEN_ERROR : WINDOW_RESULT_OK;
    }

    void TestCloseWindow(HWindow window)
    {
        ASSERT_EQ((dmGraphics::HContext) 0, g_Context);
        ++g_CloseCount;
        g_WindowOpen = false;
    }

}

static bool VulkanSupported()
{
    return true;
}

static bool OpenGLSupported()
{
    return g_OpenGLSupported;
}

static bool OpenGLESSupported()
{
    return g_OpenGLESSupported;
}

static dmGraphics::HContext GetContext()
{
    return g_Context;
}

static dmGraphics::HContext NewContext(const dmGraphics::ContextParams& params)
{
    EXPECT_TRUE(g_WindowOpen);
    ++g_ContextAttempts;
    g_LastContextParams = params;
    bool fail = dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_VULKAN ? g_FailVulkanContext : g_FailFallbackContext;
    g_Context = fail ? 0 : (dmGraphics::HContext) &g_Context;
    return g_Context;
}

static void Finalize()
{
    ASSERT_EQ((dmGraphics::HContext) 0, g_Context);
    ++g_FinalizeCount;
}

static dmGraphics::GraphicsAdapterFunctionTable RegisterFunctions()
{
    dmGraphics::GraphicsAdapterFunctionTable functions = {};
    functions.m_NewContext = NewContext;
    functions.m_Finalize = Finalize;
    return functions;
}

class EngineGraphicsTest : public jc_test_base_class
{
    public:
    void SetUp()
    {
        g_Context = 0;
        g_WindowOpen = false;
        g_OpenGLSupported = true;
        g_OpenGLESSupported = true;
        static bool registered = false;
        if (!registered)
        {
            static dmGraphics::GraphicsAdapter vulkan(dmGraphics::ADAPTER_FAMILY_VULKAN);
            static dmGraphics::GraphicsAdapter opengl(dmGraphics::ADAPTER_FAMILY_OPENGL);
            static dmGraphics::GraphicsAdapter opengles(dmGraphics::ADAPTER_FAMILY_OPENGLES);
            dmGraphics::RegisterGraphicsAdapter(&vulkan, VulkanSupported, RegisterFunctions, GetContext, 0);
            dmGraphics::RegisterGraphicsAdapter(&opengl, OpenGLSupported, RegisterFunctions, GetContext, 1);
            dmGraphics::RegisterGraphicsAdapter(&opengles, OpenGLESSupported, RegisterFunctions, GetContext, 2);
            ASSERT_TRUE(dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE));
            registered = true;
        }
        else if (dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_VULKAN)
        {
            ASSERT_TRUE(dmGraphics::InstallAdapterFallback(dmGraphics::ADAPTER_FAMILY_VULKAN));
        }

        g_FailVulkanWindow = false;
        g_FailVulkanContext = false;
        g_FailFallbackWindow = false;
        g_FailFallbackContext = false;
        g_WindowAttempts = 0;
        g_ContextAttempts = 0;
        g_CloseCount = 0;
        g_FinalizeCount = 0;
        WindowCreateParamsInitialize(&m_WindowParams);
        m_WindowParams.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        m_WindowParams.m_GraphicsApiVersionHint = 43;
        m_WindowParams.m_OpenGLUseCoreProfileHint = false;
        m_WindowParams.m_Width = 1024;
        m_WindowParams.m_Height = 768;
        m_WindowParams.m_HighDPI = true;
        m_WindowParams.m_Title = "Fallback test";
        m_ContextParams = dmGraphics::ContextParams();
        m_ContextParams.m_Window = (HWindow) this;
        m_ContextParams.m_Width = 1024;
        m_ContextParams.m_Height = 768;
        m_ContextParams.m_SwapInterval = 2;
        m_ContextParams.m_GraphicsApiVersionMajorHint = 1;
        m_ContextParams.m_GraphicsApiVersionMinorHint = 2;
    }

    dmGraphics::HContext Initialize(bool allow_fallback = true)
    {
        return dmEngine::InitializeGraphics(m_ContextParams.m_Window, m_WindowParams, m_ContextParams, allow_fallback);
    }

    WindowCreateParams        m_WindowParams;
    dmGraphics::ContextParams m_ContextParams;
};

// Successful Vulkan initialization must not replace the adapter or reopen its window.
TEST_F(EngineGraphicsTest, VulkanSuccess)
{
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, g_WindowAttempts);
    ASSERT_EQ(1, g_ContextAttempts);
    ASSERT_EQ(0, g_CloseCount);
    ASSERT_EQ(0, g_FinalizeCount);
}

// A failed native Vulkan window must retry OpenGL without attempting a Vulkan context.
TEST_F(EngineGraphicsTest, VulkanWindowFailure)
{
    g_FailVulkanWindow = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(2, g_WindowAttempts);
    ASSERT_EQ(1, g_ContextAttempts);
    ASSERT_EQ(1, g_CloseCount);
    ASSERT_EQ(1, g_FinalizeCount);
}

// A positive support probe followed by context failure must recreate the window and preserve project settings.
TEST_F(EngineGraphicsTest, VulkanContextFailure)
{
    g_FailVulkanContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(2, g_WindowAttempts);
    ASSERT_EQ(2, g_ContextAttempts);
    ASSERT_EQ(1, g_CloseCount);
    ASSERT_EQ(1, g_FinalizeCount);
    ASSERT_EQ(WINDOW_GRAPHICS_API_OPENGL, g_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(43, g_LastWindowParams.m_GraphicsApiVersionHint);
    ASSERT_FALSE(g_LastWindowParams.m_OpenGLUseCoreProfileHint);
    ASSERT_EQ(1024, g_LastWindowParams.m_Width);
    ASSERT_EQ(768, g_LastWindowParams.m_Height);
    ASSERT_TRUE(g_LastWindowParams.m_HighDPI);
    ASSERT_STREQ("Fallback test", g_LastWindowParams.m_Title);
    ASSERT_EQ(m_ContextParams.m_Window, g_LastContextParams.m_Window);
    ASSERT_EQ(1024, g_LastContextParams.m_Width);
    ASSERT_EQ(768, g_LastContextParams.m_Height);
    ASSERT_EQ(2, g_LastContextParams.m_SwapInterval);
    ASSERT_EQ(0, g_LastContextParams.m_GraphicsApiVersionMajorHint);
    ASSERT_EQ(0, g_LastContextParams.m_GraphicsApiVersionMinorHint);
}

// ARM Linux builds can supply OpenGL ES instead of desktop OpenGL as the fallback.
TEST_F(EngineGraphicsTest, OpenGLESFallback)
{
    g_OpenGLSupported = false;
    g_FailVulkanContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGLES, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(WINDOW_GRAPHICS_API_OPENGLES, g_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(1, g_FinalizeCount);
}

// Builds without a compatible GL adapter must fail without reselecting Vulkan or installing null graphics.
TEST_F(EngineGraphicsTest, NoSupportedFallback)
{
    g_OpenGLSupported = false;
    g_OpenGLESSupported = false;
    g_FailVulkanContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, g_WindowAttempts);
    ASSERT_EQ(0, g_FinalizeCount);
    ASSERT_FALSE(dmGraphics::InstallAdapterFallback(dmGraphics::ADAPTER_FAMILY_NULL));
}

// An explicit Vulkan request must report its initialization failure without silently changing adapters.
TEST_F(EngineGraphicsTest, ExplicitAdapterDoesNotFallback)
{
    g_FailVulkanContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(false));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, g_WindowAttempts);
    ASSERT_EQ(0, g_CloseCount);
    ASSERT_EQ(0, g_FinalizeCount);
}

// Non-Vulkan startup failures must not trigger the Vulkan-specific fallback policy.
TEST_F(EngineGraphicsTest, OpenGLFailureDoesNotFallback)
{
    ASSERT_TRUE(dmGraphics::InstallAdapterFallback(dmGraphics::ADAPTER_FAMILY_OPENGL));
    m_WindowParams.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
    g_FailFallbackContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(1, g_WindowAttempts);
    ASSERT_EQ(0, g_CloseCount);
}

// Failed fallback window creation must terminate the retry and skip context creation.
TEST_F(EngineGraphicsTest, FallbackWindowFailure)
{
    g_FailVulkanContext = true;
    g_FailFallbackWindow = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(2, g_WindowAttempts);
    ASSERT_EQ(1, g_ContextAttempts);
    ASSERT_EQ(1, g_FinalizeCount);
}

// Failed OpenGL context creation must terminate after one retry, without looping through adapters.
TEST_F(EngineGraphicsTest, FallbackContextFailure)
{
    g_FailVulkanContext = true;
    g_FailFallbackContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(2, g_WindowAttempts);
    ASSERT_EQ(2, g_ContextAttempts);
    ASSERT_EQ(1, g_FinalizeCount);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
