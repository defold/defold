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

#include <graphics/graphics.h>
#include <platform/window.hpp>
#include "graphics_adapter.h"

struct GraphicsAdapterTest;
static GraphicsAdapterTest* g_ActiveTest = 0;

struct GraphicsAdapterTest : jc_test_base_class
{
    enum AdapterIndex
    {
        VULKAN,
        OPENGL,
        OPENGLES,
        METAL,
        ADAPTER_COUNT,
    };

    struct AdapterState
    {
        uint32_t m_ProbeCount;
        uint32_t m_RegisterCount;
        uint32_t m_FinalizeCount;
        bool     m_Supported;
    };

    struct BufferSubDataState
    {
        uintptr_t   m_Buffer;
        uint32_t    m_CallCount;
        uint32_t    m_Offset;
        uint32_t    m_Size;
        const void* m_Data;
    };

    static void RecordBufferSubData(BufferSubDataState* state, uintptr_t buffer, uint32_t offset, uint32_t size, const void* data)
    {
        state->m_Buffer = buffer;
        ++state->m_CallCount;
        state->m_Offset = offset;
        state->m_Size = size;
        state->m_Data = data;
    }

    static void SetVertexBufferSubData(dmGraphics::HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        RecordBufferSubData(&g_ActiveTest->m_VertexSubData, buffer, offset, size, data);
    }

    static void SetIndexBufferSubData(dmGraphics::HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        RecordBufferSubData(&g_ActiveTest->m_IndexSubData, buffer, offset, size, data);
    }

    template <uint32_t Index>
    static bool IsSupported()
    {
        ++g_ActiveTest->m_Adapters[Index].m_ProbeCount;
        return g_ActiveTest->m_Adapters[Index].m_Supported;
    }

    template <uint32_t Index>
    static void FinalizeAdapter()
    {
        EXPECT_EQ((dmGraphics::HContext) 0, g_ActiveTest->m_Context);
        ++g_ActiveTest->m_Adapters[Index].m_FinalizeCount;
    }

    template <uint32_t Index>
    static uint32_t GetMaxTextureSize(dmGraphics::HContext)
    {
        return Index + 1;
    }

    static dmGraphics::HContext GetContext()
    {
        return g_ActiveTest->m_Context;
    }

    static dmGraphics::HContext NewContext(const dmGraphics::ContextParams&)
    {
        g_ActiveTest->m_Context = (dmGraphics::HContext) g_ActiveTest;
        return g_ActiveTest->m_Context;
    }

    static void DeleteContext(dmGraphics::HContext context)
    {
        ASSERT_EQ(g_ActiveTest->m_Context, context);
        g_ActiveTest->m_Context = 0;
    }

    template <uint32_t Index>
    static dmGraphics::GraphicsAdapterFunctionTable RegisterFunctions()
    {
        ++g_ActiveTest->m_Adapters[Index].m_RegisterCount;
        dmGraphics::GraphicsAdapterFunctionTable functions = {};
        functions.m_Finalize = FinalizeAdapter<Index>;
        functions.m_GetMaxTextureSize = GetMaxTextureSize<Index>;
        functions.m_NewContext = NewContext;
        functions.m_DeleteContext = DeleteContext;
        functions.m_SetVertexBufferSubData = SetVertexBufferSubData;
        functions.m_SetIndexBufferSubData = SetIndexBufferSubData;
        return functions;
    }

    void SetUp()
    {
        g_ActiveTest = this;
        m_Context = 0;
        m_VertexSubData.m_CallCount = 0;
        m_IndexSubData.m_CallCount = 0;
        for (uint32_t i = 0; i < ADAPTER_COUNT; ++i)
        {
            m_Adapters[i].m_ProbeCount = 0;
            m_Adapters[i].m_RegisterCount = 0;
            m_Adapters[i].m_FinalizeCount = 0;
            m_Adapters[i].m_Supported = true;
        }

        ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    }

    void TearDown()
    {
        if (m_Context)
        {
            dmGraphics::DeleteContext(m_Context);
        }

        dmGraphics::Finalize();
        g_ActiveTest = 0;
    }

    AdapterState         m_Adapters[ADAPTER_COUNT];
    dmGraphics::HContext m_Context;
    BufferSubDataState   m_VertexSubData;
    BufferSubDataState   m_IndexSubData;
};

// Shared graphics wrappers need these symbols, but adapter selection must not use a window backend.
namespace dmPlatform
{
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

    void SetWindowSize(HWindow, uint32_t, uint32_t)
    {
        EXPECT_TRUE(false);
    }

}

// Registration must expose priority order regardless of the order adapters are registered.
TEST_F(GraphicsAdapterTest, RegistrationOrder)
{
    ASSERT_EQ(4u, dmGraphics::GetRegisteredAdaptersCount());
    ASSERT_EQ(4u, dmGraphics::GetLinkedGraphicsAdapterCount());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetAdapterFamily(dmGraphics::GetRegisteredAdapter(0)));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_METAL, dmGraphics::GetAdapterFamily(dmGraphics::GetRegisteredAdapter(1)));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetAdapterFamily(dmGraphics::GetRegisteredAdapter(2)));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGLES, dmGraphics::GetAdapterFamily(dmGraphics::GetRegisteredAdapter(3)));
}

// A second implementation of a registered family must not replace or shadow the original adapter.
TEST_F(GraphicsAdapterTest, DuplicateFamilyRegistration)
{
    dmGraphics::HGraphicsAdapter original = dmGraphics::GetRegisteredAdapter(0);
    static dmGraphics::GraphicsAdapter duplicate(dmGraphics::ADAPTER_FAMILY_VULKAN);
    dmGraphics::RegisterGraphicsAdapter(&duplicate, IsSupported<OPENGL>, RegisterFunctions<OPENGL>, GetContext, -1);
    ASSERT_EQ(4u, dmGraphics::GetRegisteredAdaptersCount());
    ASSERT_EQ(original, dmGraphics::GetRegisteredAdapter(0));
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_VULKAN));
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_RegisterCount);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
    ASSERT_EQ(VULKAN + 1u, dmGraphics::GetMaxTextureSize(0));
}

// Registering the same object twice must preserve its callbacks and avoid a cycle in the registry.
TEST_F(GraphicsAdapterTest, RepeatedRegistration)
{
    dmGraphics::HGraphicsAdapter original = dmGraphics::GetRegisteredAdapter(0);
    dmGraphics::RegisterGraphicsAdapter(original, IsSupported<OPENGL>, RegisterFunctions<OPENGL>, GetContext, -1);
    ASSERT_EQ(original, dmGraphics::GetRegisteredAdapter(0));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_METAL, dmGraphics::GetAdapterFamily(dmGraphics::GetRegisteredAdapter(1)));
    ASSERT_EQ(4u, dmGraphics::GetRegisteredAdaptersCount());
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
    ASSERT_EQ(VULKAN + 1u, dmGraphics::GetMaxTextureSize(0));
}

// Explicit selection must skip higher-priority families without even probing them.
TEST_F(GraphicsAdapterTest, ExplicitFamily)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGLES));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGLES, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1u, m_Adapters[OPENGLES].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[OPENGLES].m_RegisterCount);
    ASSERT_EQ(0u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[METAL].m_ProbeCount);
    ASSERT_EQ(OPENGLES + 1u, dmGraphics::GetMaxTextureSize(0));
}

// An unsupported explicit family must fail even when other supported adapters are registered.
TEST_F(GraphicsAdapterTest, ExplicitUnsupported)
{
    m_Adapters[VULKAN].m_Supported = false;
    ASSERT_EQ(dmGraphics::RESULT_ADAPTER_UNSUPPORTED, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_VULKAN));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(0u, m_Adapters[VULKAN].m_RegisterCount);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGLES].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[METAL].m_ProbeCount);
}

// A family that is not linked must report unavailable without falling back.
TEST_F(GraphicsAdapterTest, ExplicitUnavailable)
{
    ASSERT_EQ(dmGraphics::RESULT_ADAPTER_UNAVAILABLE, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_DIRECTX));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(0u, m_Adapters[VULKAN].m_ProbeCount);
}

// NONE is not an explicit family; automatic installation requires the no-argument overload.
TEST_F(GraphicsAdapterTest, ExplicitNone)
{
    ASSERT_EQ(dmGraphics::RESULT_ADAPTER_UNAVAILABLE, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
}

// Automatic selection installs the highest-priority supported adapter and stops probing.
TEST_F(GraphicsAdapterTest, AutomaticPriority)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_RegisterCount);
    ASSERT_EQ(0u, m_Adapters[OPENGL].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGLES].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[METAL].m_ProbeCount);
}

// Missing runtime support must advance selection to the next supported adapter.
TEST_F(GraphicsAdapterTest, AutomaticSkipsUnsupported)
{
    m_Adapters[VULKAN].m_Supported = false;
    m_Adapters[METAL].m_Supported = false;
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[METAL].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[OPENGL].m_ProbeCount);
    ASSERT_EQ(0u, m_Adapters[OPENGLES].m_ProbeCount);
}

// Equal priorities preserve the existing preference for the most recently registered adapter.
TEST_F(GraphicsAdapterTest, AutomaticEqualPriority)
{
    m_Adapters[VULKAN].m_Supported = false;
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_METAL, dmGraphics::GetInstalledAdapterFamily());
}

// Registered but unsupported adapters must be distinguished from unavailable families.
TEST_F(GraphicsAdapterTest, AutomaticUnsupported)
{
    for (uint32_t i = 0; i < ADAPTER_COUNT; ++i)
    {
        m_Adapters[i].m_Supported = false;
    }

    ASSERT_EQ(dmGraphics::RESULT_ADAPTER_UNSUPPORTED, dmGraphics::InstallAdapter());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    for (uint32_t i = 0; i < ADAPTER_COUNT; ++i)
    {
        ASSERT_EQ(1u, m_Adapters[i].m_ProbeCount);
        ASSERT_EQ(0u, m_Adapters[i].m_RegisterCount);
    }
}

// Reinstalling the same adapter must preserve its live context and function table.
TEST_F(GraphicsAdapterTest, RepeatedInstallation)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    dmGraphics::HContext context = dmGraphics::NewContext(dmGraphics::ContextParams());
    ASSERT_NE((dmGraphics::HContext) 0, context);
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_VULKAN));
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    ASSERT_EQ(context, dmGraphics::GetInstalledContext());
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_ProbeCount);
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_RegisterCount);
    ASSERT_EQ(0u, m_Adapters[VULKAN].m_FinalizeCount);
}

// Explicit replacement must install the requested family instead of returning success for the old one.
TEST_F(GraphicsAdapterTest, ReplaceInstalledAdapter)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGL));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_OPENGL, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_FinalizeCount);
    ASSERT_EQ(1u, m_Adapters[OPENGL].m_RegisterCount);
    ASSERT_EQ(OPENGL + 1u, dmGraphics::GetMaxTextureSize(0));
}

// Failed explicit replacement must report failure and preserve the installed adapter.
TEST_F(GraphicsAdapterTest, FailedReplacement)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    m_Adapters[OPENGL].m_Supported = false;
    ASSERT_EQ(dmGraphics::RESULT_ADAPTER_UNSUPPORTED, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGL));
    ASSERT_EQ(dmGraphics::RESULT_ADAPTER_UNAVAILABLE, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_DIRECTX));
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(0u, m_Adapters[VULKAN].m_FinalizeCount);
    ASSERT_EQ(VULKAN + 1u, dmGraphics::GetMaxTextureSize(0));
}

// Automatic installation must evaluate priority even when a lower-priority adapter is installed.
TEST_F(GraphicsAdapterTest, AutomaticAfterExplicit)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGLES));
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_VULKAN, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1u, m_Adapters[OPENGLES].m_FinalizeCount);
}

// Finalization must clear installation state so a later startup installs a fresh function table.
TEST_F(GraphicsAdapterTest, FinalizeAndReinstall)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    dmGraphics::Finalize();
    dmGraphics::Finalize();
    ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_NONE, dmGraphics::GetInstalledAdapterFamily());
    ASSERT_EQ(1u, m_Adapters[VULKAN].m_FinalizeCount);
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    ASSERT_EQ(2u, m_Adapters[VULKAN].m_RegisterCount);
}

// Empty vertex updates must skip the backend to prevent assertions; nonempty updates retain their arguments.
TEST_F(GraphicsAdapterTest, VertexBufferSubDataSkipsEmptyUpdates)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    const uint32_t data[] = { 1, 2, 3, 4 };
    dmGraphics::HVertexBuffer buffer = (dmGraphics::HVertexBuffer) data;

    dmGraphics::SetVertexBufferSubData(buffer, 0, 0, data);
    dmGraphics::SetVertexBufferSubData(buffer, sizeof(data), 0, 0);
    ASSERT_EQ(0u, m_VertexSubData.m_CallCount);

    dmGraphics::SetVertexBufferSubData(buffer, sizeof(data[0]), 2 * sizeof(data[0]), data + 1);
    ASSERT_EQ(1u, m_VertexSubData.m_CallCount);
    ASSERT_EQ(buffer, m_VertexSubData.m_Buffer);
    ASSERT_EQ(sizeof(data[0]), m_VertexSubData.m_Offset);
    ASSERT_EQ(2 * sizeof(data[0]), m_VertexSubData.m_Size);
    ASSERT_EQ((const void*) (data + 1), m_VertexSubData.m_Data);
    ASSERT_EQ(0u, m_IndexSubData.m_CallCount);
}

// Empty index updates must skip the backend to prevent assertions; nonempty updates retain their arguments.
TEST_F(GraphicsAdapterTest, IndexBufferSubDataSkipsEmptyUpdates)
{
    ASSERT_EQ(dmGraphics::RESULT_OK, dmGraphics::InstallAdapter());
    const uint32_t data[] = { 1, 2, 3, 4 };
    dmGraphics::HIndexBuffer buffer = (dmGraphics::HIndexBuffer) data;

    dmGraphics::SetIndexBufferSubData(buffer, 0, 0, data);
    dmGraphics::SetIndexBufferSubData(buffer, sizeof(data), 0, 0);
    ASSERT_EQ(0u, m_IndexSubData.m_CallCount);

    dmGraphics::SetIndexBufferSubData(buffer, sizeof(data[0]), 2 * sizeof(data[0]), data + 1);
    ASSERT_EQ(1u, m_IndexSubData.m_CallCount);
    ASSERT_EQ(buffer, m_IndexSubData.m_Buffer);
    ASSERT_EQ(sizeof(data[0]), m_IndexSubData.m_Offset);
    ASSERT_EQ(2 * sizeof(data[0]), m_IndexSubData.m_Size);
    ASSERT_EQ((const void*) (data + 1), m_IndexSubData.m_Data);
    ASSERT_EQ(0u, m_VertexSubData.m_CallCount);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    static dmGraphics::GraphicsAdapter opengles(dmGraphics::ADAPTER_FAMILY_OPENGLES);
    static dmGraphics::GraphicsAdapter opengl(dmGraphics::ADAPTER_FAMILY_OPENGL);
    static dmGraphics::GraphicsAdapter vulkan(dmGraphics::ADAPTER_FAMILY_VULKAN);
    static dmGraphics::GraphicsAdapter metal(dmGraphics::ADAPTER_FAMILY_METAL);
    dmGraphics::RegisterGraphicsAdapter(&opengles, GraphicsAdapterTest::IsSupported<GraphicsAdapterTest::OPENGLES>, GraphicsAdapterTest::RegisterFunctions<GraphicsAdapterTest::OPENGLES>, GraphicsAdapterTest::GetContext, 2);
    dmGraphics::RegisterGraphicsAdapter(&opengl, GraphicsAdapterTest::IsSupported<GraphicsAdapterTest::OPENGL>, GraphicsAdapterTest::RegisterFunctions<GraphicsAdapterTest::OPENGL>, GraphicsAdapterTest::GetContext, 1);
    dmGraphics::RegisterGraphicsAdapter(&vulkan, GraphicsAdapterTest::IsSupported<GraphicsAdapterTest::VULKAN>, GraphicsAdapterTest::RegisterFunctions<GraphicsAdapterTest::VULKAN>, GraphicsAdapterTest::GetContext, 0);
    dmGraphics::RegisterGraphicsAdapter(&metal, GraphicsAdapterTest::IsSupported<GraphicsAdapterTest::METAL>, GraphicsAdapterTest::RegisterFunctions<GraphicsAdapterTest::METAL>, GraphicsAdapterTest::GetContext, 1);
    return jc_test_run_all();
}
