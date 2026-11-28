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

#include "window_egl.h"
#include "platform_window_opengl.h"
#include <dlib/array.h>
#include <dmsdk/dlib/log.h>

// Load ANGLE only for windows that request OpenGL ES.
#if defined(_WIN32)
#include <windows.h>
typedef HMODULE EGLLibrary;
#define EGL_CALL WINAPI
#define EGL_OPEN(name) LoadLibraryA(name ".dll")
#define EGL_SYMBOL(library, name) ::GetProcAddress(library, name)
#define EGL_CLOSE(library) FreeLibrary(library)
#else
#include <dlfcn.h>
typedef void* EGLLibrary;
#define EGL_CALL
// ANGLE registers Objective-C classes that AppKit can use after the last EGL window closes.
#define EGL_OPEN(name) dlopen(name ".dylib", RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE)
#define EGL_SYMBOL(library, name) dlsym(library, name)
#define EGL_CLOSE(library) dlclose(library)
#endif
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
    EGLLibrary m_Library, m_GLES;
    EGLDisplay(EGL_CALL * m_GetDisplay)(void*);
    int(EGL_CALL * m_Initialize)(EGLDisplay, EGLint*, EGLint*);
    int(EGL_CALL * m_BindAPI)(EGLenum);
    int(EGL_CALL * m_ChooseConfig)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
    int(EGL_CALL * m_GetConfigAttrib)(EGLDisplay, EGLConfig, EGLint, EGLint*);
    EGLContext(EGL_CALL * m_CreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
    EGLSurface(EGL_CALL * m_CreateWindowSurface)(EGLDisplay, EGLConfig, void*, const EGLint*);
    EGLSurface(EGL_CALL * m_CreatePbufferSurface)(EGLDisplay, EGLConfig, const EGLint*);
    int(EGL_CALL * m_MakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    EGLContext(EGL_CALL * m_GetCurrentContext)();
    int(EGL_CALL * m_DestroySurface)(EGLDisplay, EGLSurface);
    int(EGL_CALL * m_DestroyContext)(EGLDisplay, EGLContext);
    int(EGL_CALL * m_Terminate)(EGLDisplay);
    void*(EGL_CALL* m_GetProcAddress)(const char*);
    int(EGL_CALL * m_SwapBuffers)(EGLDisplay, EGLSurface);
    int(EGL_CALL * m_SwapInterval)(EGLDisplay, EGLint);
};

struct WindowEGL
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

    g_EGL.m_Library = EGL_OPEN("libEGL");
    g_EGL.m_GLES = EGL_OPEN("libGLESv2");
    if (!g_EGL.m_Library || !g_EGL.m_GLES)
    {
        goto fail;
    }

#define LOAD(name) \
    g_EGL.m_##name = (decltype(g_EGL.m_##name))EGL_SYMBOL(g_EGL.m_Library, "egl" #name); \
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
        EGL_CLOSE(g_EGL.m_GLES);
    }

    if (g_EGL.m_Library)
    {
        EGL_CLOSE(g_EGL.m_Library);
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
        EGL_CLOSE(g_EGL.m_GLES);
        EGL_CLOSE(g_EGL.m_Library);
        g_EGL = {};
        g_Display = 0;
    }
}

namespace dmPlatform
{
    static EGLConfig ChooseWindowConfig(const WindowCreateParams& params, uint32_t* sample_count, bool* has_pbuffer)
    {
        const EGLint format[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE
        };
        const EGLint channels[][2] = {
            { EGL_RED_SIZE, 8 }, { EGL_GREEN_SIZE, 8 }, { EGL_BLUE_SIZE, 8 },
            { EGL_ALPHA_SIZE, params.m_ContextAlphabits }, { EGL_DEPTH_SIZE, 24 }, { EGL_STENCIL_SIZE, 8 }
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
        OpenGLFormatScore best_score = {};
        *has_pbuffer = false;
        for (EGLint i = 0; i < count; ++i)
        {
            EGLConfig config = configs.Begin()[i];
            EGLint samples;
            if (!g_EGL.m_GetConfigAttrib(g_Display, config, EGL_SAMPLES, &samples))
            {
                continue;
            }

            EGLint surfaces = 0;
            g_EGL.m_GetConfigAttrib(g_Display, config, EGL_SURFACE_TYPE, &surfaces);
            OpenGLFormatScore score = ScoreOpenGLSamples(params.m_Samples, samples, (surfaces & EGL_PBUFFER_BIT) != 0);
            for (size_t j = 0; j < sizeof(channels) / sizeof(channels[0]); ++j)
            {
                EGLint bits = 0;
                g_EGL.m_GetConfigAttrib(g_Display, config, channels[j][0], &bits);
                ScoreOpenGLChannel(score, channels[j][1], bits);
            }

            if (!best || BetterOpenGLFormat(score, best_score))
            {
                best = config;
                best_score = score;
                *sample_count = samples > 1 ? samples : 1;
                *has_pbuffer = score.m_Pbuffer;
                if (!score.m_MissingBits && !score.m_SampleError && score.m_Pbuffer)
                {
                    break;
                }
            }
        }

        return best;
    }

    WindowEGL* EGLCreateWindow(void* native_window, const WindowCreateParams& params, uint32_t* sample_count)
    {
        if (!LoadEGL())
        {
            dmLogError("Failed to load or initialize ANGLE EGL.");
            return 0;
        }

        WindowEGL* egl = new WindowEGL();
        if (!g_EGL.m_BindAPI(EGL_OPENGL_ES_API))
        {
            EGLDestroyWindow(egl);
            return 0;
        }

        bool has_pbuffer;
        egl->m_Config = ChooseWindowConfig(params, sample_count, &has_pbuffer);
        if (!egl->m_Config)
        {
            EGLDestroyWindow(egl);
            return 0;
        }

        const EGLint version[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        egl->m_Context = g_EGL.m_CreateContext(g_Display, egl->m_Config, 0, version);
        egl->m_Surface = g_EGL.m_CreateWindowSurface(g_Display, egl->m_Config, native_window, 0);
        if (!egl->m_Context || !egl->m_Surface ||
            !g_EGL.m_MakeCurrent(g_Display, egl->m_Surface, egl->m_Surface, egl->m_Context))
        {
            EGLDestroyWindow(egl);
            return 0;
        }

        egl->m_AuxContext = g_EGL.m_CreateContext(g_Display, egl->m_Config, egl->m_Context, version);
        if (!egl->m_AuxContext)
        {
            return egl;
        }

        if (has_pbuffer)
        {
            const EGLint pbuffer[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
            egl->m_AuxSurface = g_EGL.m_CreatePbufferSurface(g_Display, egl->m_Config, pbuffer);
            if (egl->m_AuxSurface)
            {
                return egl;
            }
        }

        // Surfaceless binding requires support from both the EGL display and this ES context.
        if (!g_EGL.m_MakeCurrent(g_Display, 0, 0, egl->m_AuxContext))
        {
            g_EGL.m_DestroyContext(g_Display, egl->m_AuxContext);
            egl->m_AuxContext = 0;
        }
        else if (!g_EGL.m_MakeCurrent(g_Display, egl->m_Surface, egl->m_Surface, egl->m_Context))
        {
            EGLDestroyWindow(egl);
            return 0;
        }

        return egl;
    }

    void EGLDestroyWindow(WindowEGL* egl)
    {
        if (!egl)
        {
            return;
        }

        EGLContext current = g_EGL.m_GetCurrentContext();
        if (current && (current == egl->m_Context || current == egl->m_AuxContext))
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
        ReleaseEGL();
    }

    uintptr_t EGLGetProcAddress(WindowEGL* egl, const char* name)
    {
        if (!egl || !g_EGL.m_GetCurrentContext())
        {
            return 0;
        }

        void* proc = g_EGL.m_GetProcAddress(name);
        return (uintptr_t)(proc ? proc : (void*)EGL_SYMBOL(g_EGL.m_GLES, name));
    }

    void EGLSetSwapInterval(uint32_t interval)
    {
        g_EGL.m_SwapInterval(g_Display, interval);
    }

    void EGLSwapBuffers(WindowEGL* egl)
    {
        g_EGL.m_SwapBuffers(g_Display, egl->m_Surface);
    }

    void* EGLAcquireAuxContext(WindowEGL* egl)
    {
        return egl->m_AuxContext && g_EGL.m_MakeCurrent(g_Display, egl->m_AuxSurface, egl->m_AuxSurface, egl->m_AuxContext)
                   ? egl->m_AuxContext : 0;
    }

    void EGLUnacquireAuxContext(WindowEGL* egl, void* context)
    {
        if (context && context == egl->m_AuxContext && context == g_EGL.m_GetCurrentContext())
        {
            g_EGL.m_MakeCurrent(g_Display, 0, 0, 0);
        }
    }

    bool EGLHasAuxContext(WindowEGL* egl)
    {
        return egl && egl->m_AuxContext;
    }

    int EGLGetConfigParam(WindowEGL* egl, WindowState state)
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
        g_EGL.m_GetConfigAttrib(g_Display, egl->m_Config, attribute, &value);
        return value;
    }

}
