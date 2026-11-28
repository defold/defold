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
#include "engine_private.h"
#include "graphics_adapter.h"

struct EngineGraphicsTest;
static EngineGraphicsTest* g_ActiveTest = 0;

struct EngineGraphicsTest : jc_test_base_class
{
    enum AdapterIndex
    {
        METAL,
        VULKAN,
        OPENGL,
        OPENGLES,
        NULL_ADAPTER,
        ADAPTER_COUNT,
    };

    struct AdapterState
    {
        bool     m_Supported;
        bool     m_FailWindow;
        bool     m_FailContext;
        uint32_t m_ProbeCount;
    };

    static AdapterIndex GetAdapterIndex()
    {
        switch (dmGraphics::GetInstalledAdapterFamily())
        {
            case dmGraphics::ADAPTER_FAMILY_METAL:
            {
                return METAL;
            }
            case dmGraphics::ADAPTER_FAMILY_VULKAN:
            {
                return VULKAN;
            }
            case dmGraphics::ADAPTER_FAMILY_OPENGL:
            {
                return OPENGL;
            }
            case dmGraphics::ADAPTER_FAMILY_OPENGLES:
            {
                return OPENGLES;
            }
            case dmGraphics::ADAPTER_FAMILY_NULL:
            {
                return NULL_ADAPTER;
            }
            default:
            {
                break;
            }
        }

        EXPECT_TRUE(false);
        return ADAPTER_COUNT;
    }

    template <uint32_t Index>
    static bool IsSupported()
    {
        ++g_ActiveTest->m_Adapters[Index].m_ProbeCount;
        return g_ActiveTest->m_Adapters[Index].m_Supported;
    }

    static dmGraphics::HContext GetContext()
    {
        return g_ActiveTest->m_Context;
    }

    static dmGraphics::HContext NewContext(const dmGraphics::ContextParams& params)
    {
        EXPECT_TRUE(g_ActiveTest->m_WindowOpen);
        EXPECT_EQ(g_ActiveTest->m_ContextParams.m_Window, params.m_Window);
        bool vulkan = GetAdapterIndex() == VULKAN;
        EXPECT_EQ(vulkan ? 1 : 0, params.m_GraphicsApiVersionMajorHint);
        EXPECT_EQ(vulkan ? 2 : 0, params.m_GraphicsApiVersionMinorHint);
        ++g_ActiveTest->m_ContextAttempts;
        g_ActiveTest->m_LastContextParams = params;
        bool fail = g_ActiveTest->m_Adapters[GetAdapterIndex()].m_FailContext;
        g_ActiveTest->m_Context = fail ? 0 : (dmGraphics::HContext) g_ActiveTest;
        return g_ActiveTest->m_Context;
    }

    static void DeleteContext(dmGraphics::HContext context)
    {
        ASSERT_EQ(g_ActiveTest->m_Context, context);
        g_ActiveTest->m_Context = 0;
    }

    static void FinalizeAdapter()
    {
        ASSERT_EQ((dmGraphics::HContext) 0, g_ActiveTest->m_Context);
        EXPECT_FALSE(g_ActiveTest->m_WindowOpen);
        ++g_ActiveTest->m_FinalizeCount;
    }

    static dmGraphics::GraphicsAdapterFunctionTable RegisterFunctions()
    {
        dmGraphics::GraphicsAdapterFunctionTable functions = {};
        functions.m_NewContext = NewContext;
        functions.m_DeleteContext = DeleteContext;
        functions.m_Finalize = FinalizeAdapter;
        return functions;
    }

    void SetUp()
    {
        g_ActiveTest = this;
        m_Context = 0;
        m_WindowOpen = false;
        memset(m_Adapters, 0, sizeof(m_Adapters));
        m_Adapters[VULKAN].m_Supported = true;
        m_Adapters[OPENGL].m_Supported = true;
        m_Adapters[OPENGLES].m_Supported = true;
        m_WindowAttempts = 0;
        m_ContextAttempts = 0;
        m_CloseCount = 0;
        m_FinalizeCount = 0;
        memset(&m_WindowParams, 0, sizeof(m_WindowParams));
        m_WindowParams.m_GraphicsApiVersionHint = 43;
        m_WindowParams.m_OpenGLUseCoreProfileHint = false;
        m_WindowParams.m_Width = 1024;
        m_WindowParams.m_Height = 768;
        m_WindowParams.m_HighDPI = true;
        m_WindowParams.m_Samples = 4;
        m_WindowParams.m_Fullscreen = true;
        m_WindowParams.m_FocusOnShow = true;
        m_WindowParams.m_Title = "Fallback test";
        m_ContextParams = dmGraphics::ContextParams();
        m_ContextParams.m_Window = (HWindow) this;
        m_ContextParams.m_Width = 1024;
        m_ContextParams.m_Height = 768;
        m_ContextParams.m_SwapInterval = 2;
        m_ContextParams.m_VerifyGraphicsCalls = true;
        m_ContextParams.m_UseValidationLayers = true;
        m_ContextParams.m_DefaultTextureMinFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        m_ContextParams.m_DefaultTextureMagFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        m_ContextParams.m_GraphicsApiVersionMajorHint = 1;
        m_ContextParams.m_GraphicsApiVersionMinorHint = 2;

        ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    }

    void TearDown()
    {
        if (m_Context)
        {
            dmGraphics::DeleteContext(m_Context);
        }

        if (m_WindowOpen)
        {
            dmPlatform::CloseWindow(m_ContextParams.m_Window);
        }

        dmGraphics::Finalize();
        g_ActiveTest = 0;
    }

    dmGraphics::HContext Initialize(dmGraphics::AdapterFamily explicit_family = dmGraphics::ADAPTER_FAMILY_NONE)
    {
        bool automatic = explicit_family == dmGraphics::ADAPTER_FAMILY_NONE;
        dmGraphics::Result result = automatic ? dmGraphics::InstallAdapter() : dmGraphics::InstallAdapter(explicit_family);
        if (result != dmGraphics::RESULT_OK)
        {
            return 0;
        }

        return dmEngine::InitializeGraphics(m_ContextParams.m_Window, m_WindowParams, m_ContextParams, automatic);
    }

    AdapterState              m_Adapters[ADAPTER_COUNT];
    AdapterIndex              m_AttemptedAdapters[ADAPTER_COUNT];
    bool                      m_WindowOpen;
    int                       m_WindowAttempts;
    int                       m_ContextAttempts;
    int                       m_CloseCount;
    int                       m_FinalizeCount;
    dmGraphics::HContext      m_Context;
    WindowCreateParams        m_WindowParams;
    WindowCreateParams        m_LastWindowParams;
    dmGraphics::ContextParams m_ContextParams;
    dmGraphics::ContextParams m_LastContextParams;
};

// Startup uses a test-owned window shim while adapter selection uses the real graphics implementation.
namespace dmPlatform
{
    WindowResult OpenWindow(HWindow window, const WindowCreateParams& params)
    {
        EXPECT_EQ(g_ActiveTest->m_ContextParams.m_Window, window);
        EXPECT_FALSE(g_ActiveTest->m_WindowOpen);
        EngineGraphicsTest::AdapterIndex index = EngineGraphicsTest::GetAdapterIndex();
        const WindowsGraphicsApi expected_apis[] = {
            WINDOW_GRAPHICS_API_METAL, WINDOW_GRAPHICS_API_VULKAN, WINDOW_GRAPHICS_API_OPENGL,
            WINDOW_GRAPHICS_API_OPENGLES, WINDOW_GRAPHICS_API_NULL
        };
        EXPECT_EQ(expected_apis[index], params.m_GraphicsApi);
        EXPECT_LT(g_ActiveTest->m_WindowAttempts, (int) EngineGraphicsTest::ADAPTER_COUNT);
        if (g_ActiveTest->m_WindowAttempts < EngineGraphicsTest::ADAPTER_COUNT)
        {
            g_ActiveTest->m_AttemptedAdapters[g_ActiveTest->m_WindowAttempts] = index;
        }

        ++g_ActiveTest->m_WindowAttempts;
        g_ActiveTest->m_LastWindowParams = params;
        bool fail = g_ActiveTest->m_Adapters[index].m_FailWindow;
        g_ActiveTest->m_WindowOpen = !fail;
        return fail ? WINDOW_RESULT_WINDOW_OPEN_ERROR : WINDOW_RESULT_OK;
    }

    void CloseWindow(HWindow window)
    {
        EXPECT_EQ(g_ActiveTest->m_ContextParams.m_Window, window);
        ASSERT_EQ((dmGraphics::HContext) 0, g_ActiveTest->m_Context);
        ++g_ActiveTest->m_CloseCount;
        g_ActiveTest->m_WindowOpen = false;
    }

    // Shared graphics wrappers need these symbols, but startup must not call them.
    uint32_t GetWindowStateParam(HWindow, WindowState)
    {
        EXPECT_TRUE(false);
        return 0;
    }

    void SetWindowSize(HWindow, uint32_t, uint32_t)
    {
        EXPECT_TRUE(false);
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

// Config checks must find registered adapters without probing runtime support or installing one.
TEST_F(EngineGraphicsTest, RegisteredAdapterAvailability)
{
    ASSERT_TRUE(dmEngine::IsGraphicsAdapterInstalled(WINDOW_GRAPHICS_API_VULKAN));
    ASSERT_TRUE(dmEngine::IsGraphicsAdapterInstalled(WINDOW_GRAPHICS_API_OPENGL));
    ASSERT_TRUE(dmEngine::IsGraphicsAdapterInstalled(WINDOW_GRAPHICS_API_METAL));
    ASSERT_FALSE(dmEngine::IsGraphicsAdapterInstalled(WINDOW_GRAPHICS_API_WEBGPU));
    ASSERT_FALSE(dmEngine::IsGraphicsAdapterInstalled(WINDOW_GRAPHICS_API_DIRECTX));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    for (uint32_t i = 0; i < ADAPTER_COUNT; ++i)
    {
        ASSERT_EQ(0u, m_Adapters[i].m_ProbeCount);
    }
}

// A registered fallback must remain visible when another adapter is installed, even if it is unsupported.
TEST_F(EngineGraphicsTest, RegisteredAdapterIndependentOfSelection)
{
    m_Adapters[VULKAN].m_Supported = false;
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGL));
    ASSERT_TRUE(dmEngine::IsGraphicsAdapterInstalled(WINDOW_GRAPHICS_API_VULKAN));
    ASSERT_TRUE(dmEngine::IsGraphicsAdapterInstalled(WINDOW_GRAPHICS_API_OPENGL));
    ASSERT_FALSE(dmEngine::IsGraphicsAdapterInstalled(WINDOW_GRAPHICS_API_WEBGPU));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(0u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[OPENGL].m_ProbeCount);
    ASSERT_EQ(0, m_FinalizeCount);
}

// Successful initialization must not replace the adapter or reopen its window.
TEST_F(EngineGraphicsTest, VulkanSuccess)
{
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(WINDOW_GRAPHICS_API_VULKAN, m_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(1, m_LastContextParams.m_GraphicsApiVersionMajorHint);
    ASSERT_EQ(2, m_LastContextParams.m_GraphicsApiVersionMinorHint);
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(1, m_ContextAttempts);
    ASSERT_EQ(0, m_CloseCount);
    ASSERT_EQ(0, m_FinalizeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
}

// A failed native window must advance selection without attempting a context for that window.
TEST_F(EngineGraphicsTest, VulkanWindowFailure)
{
    m_Adapters[VULKAN].m_FailWindow = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(1, m_ContextAttempts);
    ASSERT_EQ(1, m_CloseCount);
    ASSERT_EQ(1, m_FinalizeCount);
}

// Retrying context creation must preserve project settings and remove hints for the previous API.
TEST_F(EngineGraphicsTest, VulkanContextFailure)
{
    m_Adapters[VULKAN].m_FailContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(WINDOW_GRAPHICS_API_OPENGL, m_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(2, m_ContextAttempts);
    ASSERT_EQ(1, m_CloseCount);
    ASSERT_EQ(1, m_FinalizeCount);
    ASSERT_EQ(43, m_LastWindowParams.m_GraphicsApiVersionHint);
    ASSERT_FALSE(m_LastWindowParams.m_OpenGLUseCoreProfileHint);
    ASSERT_EQ(1024, m_LastWindowParams.m_Width);
    ASSERT_EQ(768, m_LastWindowParams.m_Height);
    ASSERT_TRUE(m_LastWindowParams.m_HighDPI);
    ASSERT_EQ(4, m_LastWindowParams.m_Samples);
    ASSERT_TRUE(m_LastWindowParams.m_Fullscreen);
    ASSERT_STREQ("Fallback test", m_LastWindowParams.m_Title);
    ASSERT_EQ(m_ContextParams.m_Window, m_LastContextParams.m_Window);
    ASSERT_EQ(1024, m_LastContextParams.m_Width);
    ASSERT_EQ(768, m_LastContextParams.m_Height);
    ASSERT_EQ(2, m_LastContextParams.m_SwapInterval);
    ASSERT_TRUE(m_LastContextParams.m_VerifyGraphicsCalls);
    ASSERT_TRUE(m_LastContextParams.m_UseValidationLayers);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST, m_LastContextParams.m_DefaultTextureMinFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST, m_LastContextParams.m_DefaultTextureMagFilter);
    ASSERT_EQ(0, m_LastContextParams.m_GraphicsApiVersionMajorHint);
    ASSERT_EQ(0, m_LastContextParams.m_GraphicsApiVersionMinorHint);
}

// Unsupported adapters between attempts must be skipped without opening a window or being probed twice.
TEST_F(EngineGraphicsTest, SkipsUnsupportedAdapters)
{
    m_Adapters[OPENGL].m_Supported = false;
    m_Adapters[VULKAN].m_FailContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGLES, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(WINDOW_GRAPHICS_API_OPENGLES, m_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(VULKAN, m_AttemptedAdapters[0]);
    ASSERT_EQ(OPENGLES, m_AttemptedAdapters[1]);
    ASSERT_EQ(1u, m_Adapters[METAL].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[OPENGL].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[OPENGLES].m_ProbeCount);
}

// Failure of a non-Vulkan adapter must retry Vulkan with the original Vulkan version hints.
TEST_F(EngineGraphicsTest, MetalContextFailure)
{
    m_Adapters[METAL].m_Supported = true;
    m_Adapters[METAL].m_FailContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(WINDOW_GRAPHICS_API_VULKAN, m_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(1, m_LastContextParams.m_GraphicsApiVersionMajorHint);
    ASSERT_EQ(2, m_LastContextParams.m_GraphicsApiVersionMinorHint);
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(METAL, m_AttemptedAdapters[0]);
    ASSERT_EQ(VULKAN, m_AttemptedAdapters[1]);
}

// OpenGL failures must use the same retry policy even when Vulkan was unsupported at initial selection.
TEST_F(EngineGraphicsTest, OpenGLContextFailure)
{
    m_Adapters[VULKAN].m_Supported = false;
    m_Adapters[OPENGL].m_FailContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGLES, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(2, m_WindowAttempts);
    ASSERT_EQ(OPENGL, m_AttemptedAdapters[0]);
    ASSERT_EQ(OPENGLES, m_AttemptedAdapters[1]);
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_ProbeCount);
}

// Two failed windows must still allow a third supported adapter to start.
TEST_F(EngineGraphicsTest, MultipleWindowFailures)
{
    m_Adapters[VULKAN].m_FailWindow = true;
    m_Adapters[OPENGL].m_FailWindow = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGLES, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(3, m_WindowAttempts);
    ASSERT_EQ(1, m_ContextAttempts);
    ASSERT_EQ(2, m_CloseCount);
    ASSERT_EQ(2, m_FinalizeCount);
}

// Context retries must continue beyond the first fallback in registry priority order.
TEST_F(EngineGraphicsTest, MultipleContextFailures)
{
    m_Adapters[VULKAN].m_FailContext = true;
    m_Adapters[OPENGL].m_FailContext = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGLES, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(3, m_WindowAttempts);
    ASSERT_EQ(3, m_ContextAttempts);
    ASSERT_EQ(2, m_CloseCount);
    ASSERT_EQ(2, m_FinalizeCount);
    ASSERT_EQ(VULKAN, m_AttemptedAdapters[0]);
    ASSERT_EQ(OPENGL, m_AttemptedAdapters[1]);
    ASSERT_EQ(OPENGLES, m_AttemptedAdapters[2]);
}

// Exhausting all supported adapters must stop, close the last window, and never revisit an earlier failure.
TEST_F(EngineGraphicsTest, AllContextsFail)
{
    for (uint32_t i = 0; i < ADAPTER_COUNT; ++i)
    {
        m_Adapters[i].m_Supported = true;
        m_Adapters[i].m_FailContext = true;
    }

    ASSERT_EQ((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(5, m_WindowAttempts);
    ASSERT_EQ(5, m_ContextAttempts);
    ASSERT_EQ(5, m_CloseCount);
    ASSERT_EQ(4, m_FinalizeCount);
    ASSERT_FALSE(m_WindowOpen);
    for (uint32_t i = 0; i < ADAPTER_COUNT; ++i)
    {
        ASSERT_EQ(1u, m_Adapters[i].m_ProbeCount);
    }

    ASSERT_EQ(METAL, m_AttemptedAdapters[0]);
    ASSERT_EQ(VULKAN, m_AttemptedAdapters[1]);
    ASSERT_EQ(OPENGL, m_AttemptedAdapters[2]);
    ASSERT_EQ(OPENGLES, m_AttemptedAdapters[3]);
    ASSERT_EQ(NULL_ADAPTER, m_AttemptedAdapters[4]);
}

// Window failures must also terminate after every supported candidate has been tried once.
TEST_F(EngineGraphicsTest, AllWindowsFail)
{
    for (uint32_t i = 0; i < ADAPTER_COUNT; ++i)
    {
        m_Adapters[i].m_FailWindow = true;
    }

    ASSERT_EQ((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(3, m_WindowAttempts);
    ASSERT_EQ(0, m_ContextAttempts);
    ASSERT_EQ(3, m_CloseCount);
    ASSERT_EQ(2, m_FinalizeCount);
    ASSERT_FALSE(m_WindowOpen);
}

// A single supported adapter must fail cleanly if no remaining adapter can be installed.
TEST_F(EngineGraphicsTest, NoSupportedFallback)
{
    m_Adapters[OPENGL].m_Supported = false;
    m_Adapters[OPENGLES].m_Supported = false;
    m_Adapters[VULKAN].m_FailContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(1, m_CloseCount);
    ASSERT_EQ(0, m_FinalizeCount);
}

// No supported adapters must fail initial selection without attempting a window or context.
TEST_F(EngineGraphicsTest, NoSupportedAdapters)
{
    for (uint32_t i = 0; i < ADAPTER_COUNT; ++i)
    {
        m_Adapters[i].m_Supported = false;
    }

    ASSERT_EQ((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(0, m_WindowAttempts);
    ASSERT_EQ(0, m_ContextAttempts);
}

// An explicit Vulkan request must report context failure without probing or attempting another adapter.
TEST_F(EngineGraphicsTest, ExplicitAdapterDoesNotFallback)
{
    m_Adapters[VULKAN].m_FailContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(dmGraphics::ADAPTER_FAMILY_VULKAN));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(1, m_CloseCount);
    ASSERT_EQ(0, m_FinalizeCount);
    ASSERT_EQ(0u, m_Adapters[METAL].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
}

// Explicit selection must also remain strict when opening its native window fails.
TEST_F(EngineGraphicsTest, ExplicitAdapterWindowFailure)
{
    m_Adapters[VULKAN].m_FailWindow = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(dmGraphics::ADAPTER_FAMILY_VULKAN));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(0, m_ContextAttempts);
    ASSERT_EQ(1, m_CloseCount);
    ASSERT_EQ(0, m_FinalizeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
}

// Strict explicit selection applies to OpenGL as well as Vulkan.
TEST_F(EngineGraphicsTest, ExplicitOpenGLFailure)
{
    m_Adapters[OPENGL].m_FailContext = true;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(dmGraphics::ADAPTER_FAMILY_OPENGL));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1, m_WindowAttempts);
    ASSERT_EQ(1, m_CloseCount);
    ASSERT_EQ(0u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGLES].m_ProbeCount);
}

// An unsupported explicitly requested adapter must not fall back during initial installation.
TEST_F(EngineGraphicsTest, ExplicitUnsupportedAdapter)
{
    m_Adapters[VULKAN].m_Supported = false;
    ASSERT_EQ((dmGraphics::HContext) 0, Initialize(dmGraphics::ADAPTER_FAMILY_VULKAN));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(0, m_WindowAttempts);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
}

// Automatic selection must consider every registered family, including null when it is linked and supported.
TEST_F(EngineGraphicsTest, NullFallback)
{
    m_Adapters[VULKAN].m_FailContext = true;
    m_Adapters[OPENGL].m_Supported = false;
    m_Adapters[OPENGLES].m_Supported = false;
    m_Adapters[NULL_ADAPTER].m_Supported = true;
    ASSERT_NE((dmGraphics::HContext) 0, Initialize());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NULL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(WINDOW_GRAPHICS_API_NULL, m_LastWindowParams.m_GraphicsApi);
    ASSERT_EQ(2, m_WindowAttempts);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    static dmGraphics::GraphicsAdapter metal(dmGraphics::ADAPTER_FAMILY_METAL);
    static dmGraphics::GraphicsAdapter vulkan(dmGraphics::ADAPTER_FAMILY_VULKAN);
    static dmGraphics::GraphicsAdapter opengl(dmGraphics::ADAPTER_FAMILY_OPENGL);
    static dmGraphics::GraphicsAdapter opengles(dmGraphics::ADAPTER_FAMILY_OPENGLES);
    static dmGraphics::GraphicsAdapter null_adapter(dmGraphics::ADAPTER_FAMILY_NULL);
    dmGraphics::RegisterGraphicsAdapter(&metal, EngineGraphicsTest::IsSupported<EngineGraphicsTest::METAL>, EngineGraphicsTest::RegisterFunctions, EngineGraphicsTest::GetContext, 0);
    dmGraphics::RegisterGraphicsAdapter(&vulkan, EngineGraphicsTest::IsSupported<EngineGraphicsTest::VULKAN>, EngineGraphicsTest::RegisterFunctions, EngineGraphicsTest::GetContext, 1);
    dmGraphics::RegisterGraphicsAdapter(&opengl, EngineGraphicsTest::IsSupported<EngineGraphicsTest::OPENGL>, EngineGraphicsTest::RegisterFunctions, EngineGraphicsTest::GetContext, 2);
    dmGraphics::RegisterGraphicsAdapter(&opengles, EngineGraphicsTest::IsSupported<EngineGraphicsTest::OPENGLES>, EngineGraphicsTest::RegisterFunctions, EngineGraphicsTest::GetContext, 3);
    dmGraphics::RegisterGraphicsAdapter(&null_adapter, EngineGraphicsTest::IsSupported<EngineGraphicsTest::NULL_ADAPTER>, EngineGraphicsTest::RegisterFunctions, EngineGraphicsTest::GetContext, 4);
    return jc_test_run_all();
}
