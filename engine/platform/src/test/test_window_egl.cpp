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

#include <jc_test/jc_test.h>
#include "../window_egl.cpp"

struct TestConfig
{
    EGLint m_Samples, m_SurfaceTypes, m_DepthBits, m_StencilBits;
};

struct TestDisplay
{
    TestConfig m_Configs[3];
    EGLint     m_Count;
};

static int EGL_CALL GetConfigAttrib(EGLDisplay, EGLConfig config, EGLint attribute, EGLint* value)
{
    const TestConfig* format = (const TestConfig*)config;
    const EGLint attributes[][2] = {
        { EGL_SAMPLES, format->m_Samples }, { EGL_SURFACE_TYPE, format->m_SurfaceTypes },
        { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT }, { EGL_DEPTH_SIZE, format->m_DepthBits }, { EGL_STENCIL_SIZE, format->m_StencilBits },
        { EGL_RED_SIZE, 8 }, { EGL_GREEN_SIZE, 8 }, { EGL_BLUE_SIZE, 8 }, { EGL_ALPHA_SIZE, 8 }
    };
    for (size_t i = 0; i < sizeof(attributes) / sizeof(attributes[0]); ++i)
    {
        if (attributes[i][0] == attribute)
        {
            *value = attributes[i][1];
            return 1;
        }
    }

    return 0;
}

static int EGL_CALL ChooseConfigs(EGLDisplay handle, const EGLint* attributes, EGLConfig* configs, EGLint capacity, EGLint* count)
{
    TestDisplay* display = (TestDisplay*)handle;
    *count = 0;
    for (int i = 0; i < display->m_Count; ++i)
    {
        TestConfig* format = &display->m_Configs[i];
        const EGLint* attr = attributes;
        for (; *attr != EGL_NONE; attr += 2)
        {
            EGLint value;
            if (!GetConfigAttrib(handle, format, attr[0], &value))
            {
                return 0;
            }

            const bool mask = attr[0] == EGL_SURFACE_TYPE || attr[0] == EGL_RENDERABLE_TYPE;
            if (mask ? (value & attr[1]) != attr[1] : value < attr[1])
            {
                break;
            }
        }

        if (*attr == EGL_NONE)
        {
            if (configs && *count < capacity)
            {
                configs[*count] = format;
            }

            ++*count;
        }
    }

    return 1;
}

// A window-only format can be the sole or closest match. Pbuffer support breaks ties between equivalent window formats.
TEST(WindowEGL, OptionalPbuffer)
{
    TestDisplay display = { { { 4, EGL_WINDOW_BIT, 24, 8 },
        { 0, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, 24, 8 },
        { 4, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, 24, 8 } }, 0 };
    g_EGL.m_ChooseConfig = ChooseConfigs;
    g_EGL.m_GetConfigAttrib = GetConfigAttrib;
    g_Display = &display;
    WindowCreateParams params = {};
    params.m_Samples = 4;
    for (display.m_Count = 1; display.m_Count <= 3; ++display.m_Count)
    {
        uint32_t samples = 0;
        bool pbuffer = false;
        EGLConfig selected = dmPlatform::ChooseWindowConfig(params, &samples, &pbuffer);
        const bool shared_format = display.m_Count == 3;
        EGLConfig expected = &display.m_Configs[shared_format ? 2 : 0];
        EXPECT_EQ(expected, selected);
        EXPECT_EQ(4u, samples);
        EXPECT_EQ(shared_format, pbuffer);
    }

    g_Display = 0;
    g_EGL = {};
}

// A missing stencil buffer can cost MSAA. A shallower depth buffer with stencil can preserve it.
TEST(WindowEGL, FramebufferPrecisionFallback)
{
    TestDisplay display = { { { 4, EGL_WINDOW_BIT | EGL_PBUFFER_BIT, 16, 0 }, { 0, EGL_WINDOW_BIT, 24, 8 } }, 1 };
    g_EGL.m_ChooseConfig = ChooseConfigs;
    g_EGL.m_GetConfigAttrib = GetConfigAttrib;
    g_Display = &display;

    WindowCreateParams params = {};
    params.m_Samples = 4;
    uint32_t samples = 0;
    bool pbuffer = false;
    EXPECT_EQ((EGLConfig) & display.m_Configs[0], dmPlatform::ChooseWindowConfig(params, &samples, &pbuffer));
    EXPECT_EQ(4u, samples);
    EXPECT_TRUE(pbuffer);

    display.m_Count = 2;
    EXPECT_EQ((EGLConfig) & display.m_Configs[1], dmPlatform::ChooseWindowConfig(params, &samples, &pbuffer));
    EXPECT_EQ(1u, samples);
    EXPECT_FALSE(pbuffer);

    display.m_Configs[0].m_StencilBits = 8;
    EXPECT_EQ((EGLConfig) & display.m_Configs[0], dmPlatform::ChooseWindowConfig(params, &samples, &pbuffer));
    EXPECT_EQ(4u, samples);
    EXPECT_TRUE(pbuffer);

    params.m_Samples = 0;
    EXPECT_EQ((EGLConfig) & display.m_Configs[1], dmPlatform::ChooseWindowConfig(params, &samples, &pbuffer));
    EXPECT_EQ(1u, samples);

    g_Display = 0;
    g_EGL = {};
}
