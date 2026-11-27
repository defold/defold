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
    EGLint m_Samples, m_SurfaceTypes;
};

struct TestDisplay
{
    TestConfig m_Configs[3];
    EGLint     m_Count;
};

static int EGL_CALL ChooseConfigs(EGLDisplay handle, const EGLint* attributes, EGLConfig* configs, EGLint capacity, EGLint* count)
{
    TestDisplay* display = (TestDisplay*)handle;
    EGLint surfaces = 0;
    for (; *attributes != EGL_NONE; attributes += 2)
    {
        if (attributes[0] == EGL_SURFACE_TYPE)
        {
            surfaces = attributes[1];
        }
    }

    *count = 0;
    for (int i = 0; i < display->m_Count; ++i)
    {
        TestConfig* format = &display->m_Configs[i];
        if ((format->m_SurfaceTypes & surfaces) == surfaces)
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

static int EGL_CALL GetConfigAttrib(EGLDisplay, EGLConfig config, EGLint attribute, EGLint* value)
{
    const TestConfig* format = (const TestConfig*)config;
    *value = attribute == EGL_SAMPLES ? format->m_Samples : format->m_SurfaceTypes;
    return 1;
}

// A window-only format can be the sole or closest match. Pbuffer support breaks ties between equivalent window formats.
TEST(WindowEGL, OptionalPbuffer)
{
    TestDisplay display = { { { 4, EGL_WINDOW_BIT }, { 0, EGL_WINDOW_BIT | EGL_PBUFFER_BIT }, { 4, EGL_WINDOW_BIT | EGL_PBUFFER_BIT } }, 0 };
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
