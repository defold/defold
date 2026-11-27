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

#include "window_win32.h"
#include "../platform_window_opengl.h"
#include <dlib/log.h>

// EGL is loaded at runtime so Windows builds without ANGLE can still use WGL, DX12 and Vulkan.
typedef void*        EGLDisplay;
typedef void*        EGLConfig;
typedef void*        EGLContext;
typedef void*        EGLSurface;
typedef int          EGLint;
typedef unsigned int EGLenum;

enum
{
    EGL_NONE                   = 0x3038,
    EGL_RED_SIZE               = 0x3024,
    EGL_GREEN_SIZE             = 0x3023,
    EGL_BLUE_SIZE              = 0x3022,
    EGL_ALPHA_SIZE             = 0x3021,
    EGL_DEPTH_SIZE             = 0x3025,
    EGL_STENCIL_SIZE           = 0x3026,
    EGL_RENDERABLE_TYPE        = 0x3040,
    EGL_OPENGL_ES2_BIT         = 0x0004,
    EGL_SURFACE_TYPE           = 0x3033,
    EGL_WINDOW_BIT             = 0x0004,
    EGL_PBUFFER_BIT            = 0x0001,
    EGL_SAMPLES                = 0x3031,
    EGL_CONTEXT_CLIENT_VERSION = 0x3098,
    EGL_OPENGL_ES_API          = 0x30A0,
    EGL_WIDTH                  = 0x3057,
    EGL_HEIGHT                 = 0x3056
};

struct EGLFunctions
{
    HMODULE m_Library, m_GLES;
    EGLDisplay(WINAPI * m_GetDisplay)(void*);
    int(WINAPI * m_Initialize)(EGLDisplay, EGLint*, EGLint*);
    int(WINAPI * m_BindAPI)(EGLenum);
    int(WINAPI * m_ChooseConfig)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
    int(WINAPI * m_GetConfigAttrib)(EGLDisplay, EGLConfig, EGLint, EGLint*);
    EGLContext(WINAPI * m_CreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
    EGLSurface(WINAPI * m_CreateWindowSurface)(EGLDisplay, EGLConfig, void*, const EGLint*);
    EGLSurface(WINAPI * m_CreatePbufferSurface)(EGLDisplay, EGLConfig, const EGLint*);
    int(WINAPI * m_MakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    EGLContext(WINAPI * m_GetCurrentContext)();
    int(WINAPI * m_DestroySurface)(EGLDisplay, EGLSurface);
    int(WINAPI * m_DestroyContext)(EGLDisplay, EGLContext);
    int(WINAPI * m_Terminate)(EGLDisplay);
    void*(WINAPI* m_GetProcAddress)(const char*);
    int(WINAPI * m_SwapBuffers)(EGLDisplay, EGLSurface);
    int(WINAPI * m_SwapInterval)(EGLDisplay, EGLint);
};

struct Win32EGL
{
    EGLConfig  m_Config;
    EGLContext m_Context, m_AuxContext;
    EGLSurface m_Surface, m_AuxSurface;
};

static EGLFunctions g_EGL;
static EGLDisplay g_Display;
static uint32_t g_Users;

static bool LoadEGL()
{
    if (g_Users)
    {
        ++g_Users;
        return true;
    }

    g_EGL.m_Library = LoadLibraryW(L"libEGL.dll");
    g_EGL.m_GLES = LoadLibraryW(L"libGLESv2.dll");
    if (!g_EGL.m_Library || !g_EGL.m_GLES)
    {
        goto fail;
    }

#define LOAD(name) \
    g_EGL.m_##name = (decltype(g_EGL.m_##name))GetProcAddress(g_EGL.m_Library, "egl" #name); \
    if (!g_EGL.m_##name) goto fail;
    LOAD(GetDisplay)
    LOAD(Initialize)
    LOAD(BindAPI)
    LOAD(ChooseConfig)
    LOAD(GetConfigAttrib)
    LOAD(CreateContext)
    LOAD(CreateWindowSurface)
    LOAD(CreatePbufferSurface)
    LOAD(MakeCurrent)
    LOAD(GetCurrentContext)
    LOAD(DestroySurface)
    LOAD(DestroyContext)
    LOAD(Terminate)
    LOAD(GetProcAddress)
    LOAD(SwapBuffers)
    LOAD(SwapInterval)
#undef LOAD

    g_Display = g_EGL.m_GetDisplay(0);
    if (g_Display && g_EGL.m_Initialize(g_Display, 0, 0) && g_EGL.m_BindAPI(EGL_OPENGL_ES_API))
    {
        g_Users = 1;
        return true;
    }

    if (g_Display)
    {
        g_EGL.m_Terminate(g_Display);
    }

fail:
    if (g_EGL.m_GLES)
    {
        FreeLibrary(g_EGL.m_GLES);
    }

    if (g_EGL.m_Library)
    {
        FreeLibrary(g_EGL.m_Library);
    }

    g_EGL = {};
    g_Display = 0;
    return false;
}

static void ReleaseEGL()
{
    if (--g_Users == 0)
    {
        g_EGL.m_Terminate(g_Display);
        FreeLibrary(g_EGL.m_GLES);
        FreeLibrary(g_EGL.m_Library);
        g_EGL = {};
        g_Display = 0;
    }
}

namespace dmPlatform
{
    static EGLConfig ChooseWindowConfig(const WindowCreateParams& params, uint32_t* sample_count)
    {
        const EGLint format[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, params.m_ContextAlphabits ? 8 : 0,
            EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE
        };
        EGLint count = 0;
        if (!g_EGL.m_ChooseConfig(g_Display, format, 0, 0, &count) || !count)
        {
            return 0;
        }

        dmArray<EGLConfig> configs;
        configs.SetCapacity(count);
        if (!g_EGL.m_ChooseConfig(g_Display, format, configs.Begin(), count, &count))
        {
            return 0;
        }

        EGLConfig best = 0;
        uint32_t best_error = UINT32_MAX;
        for (EGLint i = 0; i < count; ++i)
        {
            EGLConfig config = configs.Begin()[i];
            EGLint samples;
            if (g_EGL.m_GetConfigAttrib(g_Display, config, EGL_SAMPLES, &samples))
            {
                uint32_t error = OpenGLSampleCountError(params.m_Samples, samples);
                if (!best || error < best_error)
                {
                    best = config;
                    best_error = error;
                    *sample_count = samples > 1 ? samples : 1;
                    if (!error)
                    {
                        break;
                    }
                }
            }
        }

        return best;
    }

    bool Win32EGLCreate(HWindow window)
    {
        if (!LoadEGL())
        {
            dmLogError("Failed to load or initialize ANGLE EGL.");
            return false;
        }

        Win32EGL* egl = new Win32EGL();
        *egl = {};
        window->m_EGL = egl;
        if (!g_EGL.m_BindAPI(EGL_OPENGL_ES_API))
        {
            return false;
        }

        egl->m_Config = ChooseWindowConfig(window->m_Params, &window->m_Samples);
        if (!egl->m_Config)
        {
            return false;
        }

        const EGLint version[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        egl->m_Context = g_EGL.m_CreateContext(g_Display, egl->m_Config, 0, version);
        egl->m_Surface = g_EGL.m_CreateWindowSurface(g_Display, egl->m_Config, window->m_Window, 0);
        if (!egl->m_Context || !egl->m_Surface ||
            !g_EGL.m_MakeCurrent(g_Display, egl->m_Surface, egl->m_Surface, egl->m_Context))
        {
            return false;
        }

        const EGLint pbuffer[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
        egl->m_AuxSurface = g_EGL.m_CreatePbufferSurface(g_Display, egl->m_Config, pbuffer);
        if (egl->m_AuxSurface)
        {
            egl->m_AuxContext = g_EGL.m_CreateContext(g_Display, egl->m_Config, egl->m_Context, version);
            if (!egl->m_AuxContext)
            {
                g_EGL.m_DestroySurface(g_Display, egl->m_AuxSurface);
                egl->m_AuxSurface = 0;
            }
        }

        return true;
    }

    void Win32EGLDestroy(HWindow window)
    {
        Win32EGL* egl = window->m_EGL;
        if (!egl)
        {
            return;
        }

        EGLContext current = g_EGL.m_GetCurrentContext();
        if (current == egl->m_Context || current == egl->m_AuxContext)
        {
            g_EGL.m_MakeCurrent(g_Display, 0, 0, 0);
        }

        if (egl->m_AuxContext)
        {
            g_EGL.m_DestroyContext(g_Display, egl->m_AuxContext);
        }

        if (egl->m_AuxSurface)
        {
            g_EGL.m_DestroySurface(g_Display, egl->m_AuxSurface);
        }

        if (egl->m_Context)
        {
            g_EGL.m_DestroyContext(g_Display, egl->m_Context);
        }

        if (egl->m_Surface)
        {
            g_EGL.m_DestroySurface(g_Display, egl->m_Surface);
        }

        delete egl;
        window->m_EGL = 0;
        ReleaseEGL();
    }

    uintptr_t Win32EGLGetProcAddress(HWindow window, const char* name)
    {
        if (!window->m_EGL || !g_EGL.m_GetCurrentContext())
        {
            return 0;
        }

        void* proc = g_EGL.m_GetProcAddress(name);
        return (uintptr_t)(proc ? proc : ::GetProcAddress(g_EGL.m_GLES, name));
    }

    void Win32EGLSetSwapInterval(HWindow window, uint32_t interval)
    {
        if (window->m_EGL)
        {
            g_EGL.m_SwapInterval(g_Display, interval);
        }
    }

    void Win32EGLSwapBuffers(HWindow window)
    {
        g_EGL.m_SwapBuffers(g_Display, window->m_EGL->m_Surface);
    }

    void* Win32EGLAcquireAuxContext(HWindow window)
    {
        Win32EGL* egl = window->m_EGL;
        return egl->m_AuxContext && g_EGL.m_MakeCurrent(g_Display, egl->m_AuxSurface, egl->m_AuxSurface, egl->m_AuxContext)
                   ? egl->m_AuxContext : 0;
    }

    void Win32EGLUnacquireAuxContext(HWindow window, void* context)
    {
        if (context && context == window->m_EGL->m_AuxContext && context == g_EGL.m_GetCurrentContext())
        {
            g_EGL.m_MakeCurrent(g_Display, 0, 0, 0);
        }
    }

    bool Win32EGLHasAuxContext(HWindow window)
    {
        return window->m_EGL && window->m_EGL->m_AuxContext;
    }

    int Win32EGLGetConfigParam(HWindow window, WindowState state)
    {
        EGLint attribute = 0;
        switch (state)
        {
            case WINDOW_STATE_RED_BITS:
            {
                attribute = EGL_RED_SIZE;
                break;
            }
            case WINDOW_STATE_GREEN_BITS:
            {
                attribute = EGL_GREEN_SIZE;
                break;
            }
            case WINDOW_STATE_BLUE_BITS:
            {
                attribute = EGL_BLUE_SIZE;
                break;
            }
            case WINDOW_STATE_ALPHA_BITS:
            {
                attribute = EGL_ALPHA_SIZE;
                break;
            }
            case WINDOW_STATE_DEPTH_BITS:
            {
                attribute = EGL_DEPTH_SIZE;
                break;
            }
            case WINDOW_STATE_STENCIL_BITS:
            {
                attribute = EGL_STENCIL_SIZE;
                break;
            }
            default:
            {
                return 0;
            }
        }

        EGLint value = 0;
        g_EGL.m_GetConfigAttrib(g_Display, window->m_EGL->m_Config, attribute, &value);
        return value;
    }

}
