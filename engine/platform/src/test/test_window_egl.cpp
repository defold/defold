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

static struct
{
    char       m_Main, m_Aux, m_Window, m_Pbuffer;
    EGLContext m_Current;
    int        m_LiveObjects, m_PbufferAttempts;
    bool       m_AllowPbuffer, m_AllowSurfaceless, m_RejectRestore;
} g_Worker;

static int EGL_CALL BindAPI(EGLenum api)
{
    return api == EGL_OPENGL_ES_API;
}

static EGLContext EGL_CALL CreateContext(EGLDisplay, EGLConfig, EGLContext share, const EGLint*)
{
    if (share && share != &g_Worker.m_Main)
    {
        return 0;
    }

    ++g_Worker.m_LiveObjects;
    return share ? &g_Worker.m_Aux : &g_Worker.m_Main;
}

static EGLSurface EGL_CALL CreateWindowSurface(EGLDisplay, EGLConfig, void* native, const EGLint*)
{
    ++g_Worker.m_LiveObjects;
    return native;
}

static EGLSurface EGL_CALL CreatePbufferSurface(EGLDisplay, EGLConfig, const EGLint*)
{
    ++g_Worker.m_PbufferAttempts;
    if (!g_Worker.m_AllowPbuffer)
    {
        return 0;
    }

    ++g_Worker.m_LiveObjects;
    return &g_Worker.m_Pbuffer;
}

static int EGL_CALL MakeCurrent(EGLDisplay, EGLSurface draw, EGLSurface read, EGLContext context)
{
    bool restore_failed = g_Worker.m_RejectRestore && g_Worker.m_Current == &g_Worker.m_Aux;
    bool main = context == &g_Worker.m_Main && draw == &g_Worker.m_Window && !restore_failed;
    bool aux = context == &g_Worker.m_Aux && (draw ? draw == &g_Worker.m_Pbuffer : g_Worker.m_AllowSurfaceless);
    if (draw != read || !(main || aux || (!context && !draw)))
    {
        return 0;
    }

    g_Worker.m_Current = context;
    return 1;
}

static EGLContext EGL_CALL GetCurrentContext()
{
    return g_Worker.m_Current;
}

static int EGL_CALL DestroyObject(EGLDisplay, void*)
{
    --g_Worker.m_LiveObjects;
    return 1;
}

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

// Worker uploads can use a pbuffer or a surfaceless context. Window creation must leave the render context current.
TEST(WindowEGL, SharedWorkerContext)
{
    TestDisplay display = { { { 0, EGL_WINDOW_BIT, 24, 8 } }, 1 };
    g_Display = &display;
    g_Users = 1;
    g_EGL.m_BindAPI = BindAPI;
    g_EGL.m_ChooseConfig = ChooseConfigs;
    g_EGL.m_GetConfigAttrib = GetConfigAttrib;
    g_EGL.m_CreateContext = CreateContext;
    g_EGL.m_CreateWindowSurface = CreateWindowSurface;
    g_EGL.m_CreatePbufferSurface = CreatePbufferSurface;
    g_EGL.m_MakeCurrent = MakeCurrent;
    g_EGL.m_GetCurrentContext = GetCurrentContext;
    g_EGL.m_DestroyContext = g_EGL.m_DestroySurface = DestroyObject;
    const struct
    {
        bool config_pbuffer, allow_pbuffer, surfaceless, reject_restore;
    } cases[] = {
        { true, true, false, false },    // Working pbuffer.
        { false, false, true, false },   // Window-only config.
        { true, false, true, false },    // Advertised pbuffer fails to allocate.
        { false, false, false, false },  // Only the render context is usable.
        { false, false, true, true }     // Render context cannot be restored.
    };
    WindowCreateParams params = {};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        g_Worker = {};
        g_Worker.m_AllowPbuffer = cases[i].allow_pbuffer;
        g_Worker.m_AllowSurfaceless = cases[i].surfaceless;
        g_Worker.m_RejectRestore = cases[i].reject_restore;
        display.m_Configs[0].m_SurfaceTypes = EGL_WINDOW_BIT | (cases[i].config_pbuffer ? EGL_PBUFFER_BIT : 0);
        uint32_t samples;
        WindowEGL* window = dmPlatform::EGLCreateWindow(&g_Worker.m_Window, params, &samples);
        EXPECT_EQ(!cases[i].reject_restore, window != 0);
        EXPECT_EQ((int)cases[i].config_pbuffer, g_Worker.m_PbufferAttempts);
        if (window)
        {
            bool expected = cases[i].allow_pbuffer || cases[i].surfaceless;
            EXPECT_EQ((EGLContext) & g_Worker.m_Main, g_Worker.m_Current);
            EXPECT_EQ(expected, dmPlatform::EGLHasAuxContext(window));
            void* worker = dmPlatform::EGLAcquireAuxContext(window);
            EXPECT_EQ(expected ? &g_Worker.m_Aux : (EGLContext)0, worker);
            dmPlatform::EGLUnacquireAuxContext(window, worker);
            dmPlatform::EGLDestroyWindow(window);
        }

        EXPECT_EQ((EGLContext)0, g_Worker.m_Current);
        EXPECT_EQ(0, g_Worker.m_LiveObjects);
        EXPECT_EQ(1u, g_Users);
    }

    g_Users = 0;
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
