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
#include "../engine_graphics.h"

struct EngineGraphicsTest;
// Adapter callbacks without arguments use the active fixture; window callbacks use HWindow.
static EngineGraphicsTest* g_ActiveTest = 0;

struct EngineGraphicsTest : jc_test_base_class
{
    static bool VulkanSupported()
    {
        // Always pass the support probe so Vulkan is selected first. Tests inject
        // window/context creation failures to exercise the initialization fallback.
        return true;
    }

    static bool OpenGLSupported()
    {
        return g_ActiveTest->m_OpenGLSupported;
    }

    static bool OpenGLESSupported()
    {
        return g_ActiveTest->m_OpenGLESSupported;
    }

    static dmGraphics::HContext GetContext()
    {
        return g_ActiveTest->m_Context;
    }

    static dmGraphics::HContext NewContext(const dmGraphics::ContextParams& params)
    {
        EngineGraphicsTest* test = (EngineGraphicsTest*) params.m_Window;
        EXPECT_TRUE(test->m_WindowOpen);
        ++test->m_ContextAttempts;

        bool fail = test->m_FailFallbackContext;
        if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_VULKAN)
        {
             fail = test->m_FailVulkanContext;
        }

        test->m_LastContextParams = params;
        test->m_Context           = fail ? 0 : (dmGraphics::HContext) &test->m_Context;
        return test->m_Context;
    }

    static void Finalize()
    {
        ASSERT_EQ((dmGraphics::HContext) 0, g_ActiveTest->m_Context);
        ++g_ActiveTest->m_FinalizeCount;
    }

    static dmGraphics::GraphicsAdapterFunctionTable RegisterFunctions()
    {
        dmGraphics::GraphicsAdapterFunctionTable functions = {};
        functions.m_NewContext = NewContext;
        functions.m_Finalize = Finalize;
        return functions;
    }

    void SetUp()
    {
        g_ActiveTest = this;
        m_Context = 0;
        m_WindowOpen = false;
        m_OpenGLSupported = true;
        m_OpenGLESSupported = true;
        m_FailVulkanWindow = false;
        m_FailVulkanContext = false;
        m_FailFallbackWindow = false;
        m_FailFallbackContext = false;
        m_WindowAttempts = 0;
        m_ContextAttempts = 0;
        m_CloseCount = 0;
        m_FinalizeCount = 0;
        m_LastWindowParams = WindowCreateParams();
        m_LastContextParams = dmGraphics::ContextParams();

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

        // Restoring the initial adapter is setup, not part of the test's finalization count.
        m_FinalizeCount = 0;
        m_WindowParams = WindowCreateParams();
        m_WindowParams.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        m_WindowParams.m_GraphicsApiVersionHint = 43;
        m_WindowParams.m_OpenGLUseCoreProfileHint = false;
        m_WindowParams.m_Width = 1024;
        m_WindowParams.m_Height = 768;
        m_WindowParams.m_Samples = 1;
        m_WindowParams.m_HighDPI = true;
        m_WindowParams.m_FocusOnShow = true;
        m_WindowParams.m_Title = "Fallback test";
        m_ContextParams = dmGraphics::ContextParams();
        m_ContextParams.m_Window = (HWindow) this;
        m_ContextParams.m_Width = 1024;
        m_ContextParams.m_Height = 768;
        m_ContextParams.m_SwapInterval = 2;
        m_ContextParams.m_GraphicsApiVersionMajorHint = 1;
        m_ContextParams.m_GraphicsApiVersionMinorHint = 2;
    }

    void TearDown()
    {
        g_ActiveTest = 0;
    }

    dmGraphics::HContext Initialize(bool allow_fallback)
    {
        return dmEngine::InitializeGraphics(m_ContextParams.m_Window, m_WindowParams, m_ContextParams, allow_fallback);
    }

    WindowCreateParams        m_LastWindowParams;
    WindowCreateParams        m_WindowParams;
    dmGraphics::ContextParams m_LastContextParams;
    dmGraphics::ContextParams m_ContextParams;
    dmGraphics::HContext      m_Context;
    int                       m_WindowAttempts;
    int                       m_ContextAttempts;
    int                       m_CloseCount;
    int                       m_FinalizeCount;
    bool                      m_OpenGLSupported;
    bool                      m_OpenGLESSupported;
    bool                      m_FailVulkanWindow;
    bool                      m_FailVulkanContext;
    bool                      m_FailFallbackWindow;
    bool                      m_FailFallbackContext;
    bool                      m_WindowOpen;
};

// This executable supplies its own platform implementation and links no window backend.
namespace dmPlatform
{
    WindowResult OpenWindow(HWindow window, const WindowCreateParams& params)
    {
        EngineGraphicsTest* test = (EngineGraphicsTest*) window;
        EXPECT_FALSE(test->m_WindowOpen);
        ++test->m_WindowAttempts;
        test->m_LastWindowParams = params;

        bool fail = test->m_FailFallbackWindow;
        if (params.m_GraphicsApi == WINDOW_GRAPHICS_API_VULKAN)
        {
            fail = test->m_FailVulkanWindow;
        }
        test->m_WindowOpen = !fail;
        return fail ? WINDOW_RESULT_WINDOW_OPEN_ERROR : WINDOW_RESULT_OK;
    }

    void CloseWindow(HWindow window)
    {
        EngineGraphicsTest* test = (EngineGraphicsTest*) window;
        ASSERT_EQ((dmGraphics::HContext) 0, test->m_Context);
        ++test->m_CloseCount;
        test->m_WindowOpen = false;
    }

    // Shared graphics wrappers reference these functions, but startup must not call them.
    uint32_t GetWindowStateParam(HWindow, WindowState)
    {
        EXPECT_TRUE(false);
        return 0;
    }

    uint32_t GetWindowWidth(HWindow)
    {
        EXPECT_TRUE(false);
        return 0;
    }

    uint32_t GetWindowHeight(HWindow)
    {
        EXPECT_TRUE(false);
        return 0;
    }

    float GetDisplayScaleFactor(HWindow)
    {
        EXPECT_TRUE(false);
        return 0.0f;
    }

    void IconifyWindow(HWindow)
    {
        EXPECT_TRUE(false);
    }

    void SetSwapInterval(HWindow, uint32_t)
    {
        EXPECT_TRUE(false);
    }
}

// Successful Vulkan initialization must not replace the adapter or reopen its window.
TEST_F(EngineGraphicsTest, VulkanSuccess)
{
    ASSERT_NE((dmGraphics::HContext) 0, Initialize(true));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(1, m_ContextAttempts);
    ASSERT_EQ(0, m_CloseCount);
    ASSERT_EQ(0, m_FinalizeCount);
}

// A failed native Vulkan window must retry OpenGL without attempting a Vulkan context.
TEST_F(EngineGraphicsTest, VulkanWindowFailure)
{
    m_FailVulkanWindow = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize(true));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(1, m_ContextAttempts);
    ASSERT_EQ(1, m_CloseCount);
    ASSERT_EQ(1, m_FinalizeCount);
}

// A positive support probe followed by context failure must recreate the window and preserve project settings.
TEST_F(EngineGraphicsTest, VulkanContextFailure)
{
    m_FailVulkanContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize(true));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(2, m_ContextAttempts);
    ASSERT_EQ(1, m_CloseCount);
    ASSERT_EQ(1, m_FinalizeCount);
    ASSERT_EQ(WINDOW_GRAPHICS_API_OPENGL, m_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(43, m_LastWindowParams.m_GraphicsApiVersionHint);
    ASSERT_FALSE(m_LastWindowParams.m_OpenGLUseCoreProfileHint);
    ASSERT_EQ(1024, m_LastWindowParams.m_Width);
    ASSERT_EQ(768, m_LastWindowParams.m_Height);
    ASSERT_TRUE(m_LastWindowParams.m_HighDPI);
    ASSERT_STREQ("Fallback test", m_LastWindowParams.m_Title);
    ASSERT_EQ(m_ContextParams.m_Window, m_LastContextParams.m_Window);
    ASSERT_EQ(1024, m_LastContextParams.m_Width);
    ASSERT_EQ(768, m_LastContextParams.m_Height);
    ASSERT_EQ(2, m_LastContextParams.m_SwapInterval);
    ASSERT_EQ(0, m_LastContextParams.m_GraphicsApiVersionMajorHint);
    ASSERT_EQ(0, m_LastContextParams.m_GraphicsApiVersionMinorHint);
}

// ARM Linux builds can supply OpenGL ES instead of desktop OpenGL as the fallback.
TEST_F(EngineGraphicsTest, OpenGLESFallback)
{
    m_OpenGLSupported = false;
    m_FailVulkanContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize(true));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGLES, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(WINDOW_GRAPHICS_API_OPENGLES, m_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(1, m_FinalizeCount);
}

// Builds without a compatible GL adapter must fail without reselecting Vulkan or installing null graphics.
TEST_F(EngineGraphicsTest, NoSupportedFallback)
{
    m_OpenGLSupported = false;
    m_OpenGLESSupported = false;
    m_FailVulkanContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(true));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(0, m_FinalizeCount);
    ASSERT_FALSE(dmGraphics::InstallAdapterFallback(dmGraphics::ADAPTER_FAMILY_NULL));
}

// An explicit Vulkan request must report its initialization failure without silently changing adapters.
TEST_F(EngineGraphicsTest, ExplicitAdapterDoesNotFallback)
{
    m_FailVulkanContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(false));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(0, m_CloseCount);
    ASSERT_EQ(0, m_FinalizeCount);
}

// Non-Vulkan startup failures must not trigger the Vulkan-specific fallback policy.
TEST_F(EngineGraphicsTest, OpenGLFailureDoesNotFallback)
{
    ASSERT_TRUE(dmGraphics::InstallAdapterFallback(dmGraphics::ADAPTER_FAMILY_OPENGL));
    m_WindowParams.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
    m_FailFallbackContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(true));
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(0, m_CloseCount);
}

// Failed fallback window creation must terminate the retry and skip context creation.
TEST_F(EngineGraphicsTest, FallbackWindowFailure)
{
    m_FailVulkanContext = true;
    m_FailFallbackWindow = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(true));
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(1, m_ContextAttempts);
    ASSERT_EQ(1, m_FinalizeCount);
}

// Failed OpenGL context creation must terminate after one retry, without looping through adapters.
TEST_F(EngineGraphicsTest, FallbackContextFailure)
{
    m_FailVulkanContext = true;
    m_FailFallbackContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(true));
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(2, m_ContextAttempts);
    ASSERT_EQ(1, m_FinalizeCount);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
