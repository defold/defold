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
#include <EGL/egl.h>

struct TestConfig
{
    EGLint m_DepthBits;
    EGLint m_StencilBits;
};

static struct TestConfig g_Configs[] = {
    {
        24, 8
    },
    {
        0, 8
    },
    {
        16, 0
    },
    {
        0, 0
    },
    {
        0, 0
    }
};
static EGLint g_ConfigCount;

static EGLBoolean TestChooseConfig(EGLDisplay display, const EGLint* attributes, EGLConfig* configs, EGLint capacity, EGLint* count)
{
    *count = g_ConfigCount;
    if (configs)
    {
        if (*count > capacity)
        {
            *count = capacity;
        }

        for (EGLint i = 0; i < *count; ++i)
        {
            configs[i] = &g_Configs[i];
        }
    }

    return EGL_TRUE;
}

static EGLBoolean TestGetConfigAttrib(EGLDisplay display, EGLConfig config, EGLint attribute, EGLint* value)
{
    const struct TestConfig* match = config;
    if (attribute == EGL_DEPTH_SIZE)
    {
        *value = match->m_DepthBits;
    }
    else if (attribute == EGL_STENCIL_SIZE)
    {
        *value = match->m_StencilBits;
    }
    else
    {
        return EGL_FALSE;
    }

    return EGL_TRUE;
}

// Exercise the window's config selection with a fixed driver preference order.
#define eglChooseConfig TestChooseConfig
#define eglGetConfigAttrib TestGetConfigAttrib
#include "../native/android/android_util.c"
#undef eglChooseConfig
#undef eglGetConfigAttrib

// Skip depth-only and stencil-only configs and take the first exact color-only match.
TEST(AndroidEGLConfig, PreferColorOnly)
{
    dmNativeWin.depthBits = 0;
    dmNativeWin.stencilBits = 0;
    g_ConfigCount = 5;
    EGLConfig config = NULL;
    ASSERT_EQ(1, choose_egl_config(EGL_NO_DISPLAY, false, &config));
    ASSERT_EQ(&g_Configs[3], config);
}

// A driver with no color-only config must still be able to open the window with its preferred config.
TEST(AndroidEGLConfig, FallBackToFirstMatch)
{
    dmNativeWin.depthBits = 0;
    dmNativeWin.stencilBits = 0;
    g_ConfigCount = 3;
    EGLConfig config = NULL;
    ASSERT_EQ(1, choose_egl_config(EGL_NO_DISPLAY, false, &config));
    ASSERT_EQ(&g_Configs[0], config);
}

// A window requesting depth and stencil keeps the driver's preferred match.
TEST(AndroidEGLConfig, DepthStencilEnabled)
{
    dmNativeWin.depthBits = 24;
    dmNativeWin.stencilBits = 8;
    g_ConfigCount = 5;
    EGLConfig config = NULL;
    ASSERT_EQ(1, choose_egl_config(EGL_NO_DISPLAY, false, &config));
    ASSERT_EQ(&g_Configs[0], config);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
