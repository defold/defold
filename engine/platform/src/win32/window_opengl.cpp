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

namespace dmPlatform
{
    // WGL extensions are queried from a current context after its pixel format is set.
    enum
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB             = 0x2091,
        WGL_CONTEXT_MINOR_VERSION_ARB             = 0x2092,
        WGL_CONTEXT_FLAGS_ARB                     = 0x2094,
        WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB    = 0x0002,
        WGL_CONTEXT_PROFILE_MASK_ARB              = 0x9126,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB          = 0x0001
    };

    static PROC GetWglProc(const char* name)
    {
        PROC proc = wglGetProcAddress(name);
        // Some ICDs return these sentinel values for an unknown entry point.
        if ((uintptr_t)proc <= 3 || proc == (PROC)-1)
        {
            return 0;
        }

        return proc;
    }

    static HWND CreateHelperWindow(HWND main)
    {
        RECT bounds = {};
        GetWindowRect(main, &bounds);
        return CreateWindowExW(0, WIN32_WINDOW_CLASS, L"", WS_POPUP, bounds.left, bounds.top, 1, 1, 0, 0, GetModuleHandleW(0), 0);
    }

    static bool SetFormat(HDC dc, int index)
    {
        PIXELFORMATDESCRIPTOR format;
        return index && DescribePixelFormat(dc, index, sizeof(format), &format) && SetPixelFormat(dc, index, &format);
    }

    static int ChooseBasicFormat(HDC dc)
    {
        PIXELFORMATDESCRIPTOR format = {};
        format.nSize = sizeof(format);
        format.nVersion = 1;
        format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        format.iPixelType = PFD_TYPE_RGBA;
        format.cColorBits = 24;
        format.cAlphaBits = 8;
        format.cDepthBits = 24;
        format.cStencilBits = 8;
        return ChoosePixelFormat(dc, &format);
    }

    static bool LoadWgl(HWND main, WglFunctions* wgl)
    {
        HWND helper = CreateHelperWindow(main);
        if (!helper)
        {
            return false;
        }

        HDC dc = GetDC(helper);
        HGLRC context = dc && SetFormat(dc, ChooseBasicFormat(dc)) ? wglCreateContext(dc) : 0;
        HDC previous_dc = wglGetCurrentDC();
        HGLRC previous = wglGetCurrentContext();
        bool bound = context && wglMakeCurrent(dc, context);
        if (bound)
        {
            wgl->m_ChooseFormat = (decltype(wgl->m_ChooseFormat))GetWglProc("wglChoosePixelFormatARB");
            wgl->m_GetFormat = (decltype(wgl->m_GetFormat))GetWglProc("wglGetPixelFormatAttribivARB");
            wgl->m_CreateContext = (decltype(wgl->m_CreateContext))GetWglProc("wglCreateContextAttribsARB");
        }

        if (context)
        {
            wglMakeCurrent(previous_dc, previous);
            wglDeleteContext(context);
        }

        if (dc)
        {
            ReleaseDC(helper, dc);
        }

        DestroyWindow(helper);
        return bound;
    }

    int Win32ChoosePixelFormat(HDC dc, const WglFunctions& wgl, int format_count, uint32_t requested, uint32_t* samples)
    {
        *samples = 1;
        if (!requested || !wgl.m_ChooseFormat || !wgl.m_GetFormat || format_count <= 0)
        {
            return 0;
        }

        const int required[] = { WGL_DRAW_TO_WINDOW_ARB, TRUE, WGL_SUPPORT_OPENGL_ARB, TRUE, WGL_DOUBLE_BUFFER_ARB, TRUE,
                                 WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB, 0 };
        dmArray<int> formats;
        formats.SetCapacity(format_count);
        UINT count = 0;
        if (!wgl.m_ChooseFormat(dc, required, 0, format_count, formats.Begin(), &count))
        {
            return 0;
        }

        const int attributes[] = { WGL_COLOR_BITS_ARB, WGL_ALPHA_BITS_ARB, WGL_DEPTH_BITS_ARB, WGL_STENCIL_BITS_ARB,
                                   WGL_SAMPLE_BUFFERS_ARB, WGL_SAMPLES_ARB };
        const int preferred_bits[] = { 24, 8, 24, 8 };
        const uint32_t channel_count = DM_ARRAY_SIZE(preferred_bits);
        int best = 0;
        OpenGLFormatScore best_score = {};
        for (UINT i = 0; i < count; ++i)
        {
            int values[DM_ARRAY_SIZE(attributes)];
            int index = formats.Begin()[i];
            if (!wgl.m_GetFormat(dc, index, 0, DM_ARRAY_SIZE(attributes), attributes, values))
            {
                continue;
            }

            const uint32_t available = values[channel_count] ? values[channel_count + 1] : 0;
            OpenGLFormatScore score = ScoreOpenGLSamples(requested, available, false);
            for (uint32_t j = 0; j < channel_count; ++j)
            {
                ScoreOpenGLChannel(score, preferred_bits[j], values[j]);
            }

            if (!best || BetterOpenGLFormat(score, best_score))
            {
                best = index;
                *samples = available > 1 ? available : 1;
                best_score = score;
            }
        }

        return best;
    }

    static HGLRC CreateContext(HDC dc, HGLRC share, const WglFunctions& wgl, uint32_t major, uint32_t minor, bool core)
    {
        if (major)
        {
            int attributes[] = { WGL_CONTEXT_MAJOR_VERSION_ARB, (int)major, WGL_CONTEXT_MINOR_VERSION_ARB, (int)minor,
                                 WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                 WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, 0 };
            if (!core)
            {
                // A version hint alone leaves profile selection to the driver.
                attributes[4] = 0;
            }

            return wgl.m_CreateContext ? wgl.m_CreateContext(dc, share, attributes) : 0;
        }

        // A legacy WGL creation request selects the driver's highest backwards-compatible context.
        HGLRC context = wglCreateContext(dc);
        if (context && share && !wglShareLists(share, context))
        {
            wglDeleteContext(context);
            return 0;
        }

        return context;
    }

    bool Win32OpenGLCreate(HWindow window)
    {
        WglFunctions wgl = {};
        if (!LoadWgl(window->m_Window, &wgl))
        {
            return false;
        }

        uint32_t major = 3, minor = 3;
        if (!OpenGLGetVersion(window->m_Params.m_GraphicsApiVersionHint, &major, &minor))
        {
            dmLogWarning("Unsupported OpenGL version hint %d. Using OpenGL 3.3.", window->m_Params.m_GraphicsApiVersionHint);
        }

        window->m_DC = GetDC(window->m_Window);
        if (!window->m_DC)
        {
            return false;
        }

        int format_count = DescribePixelFormat(window->m_DC, 1, 0, 0);
        int format = Win32ChoosePixelFormat(window->m_DC, wgl, format_count, window->m_Params.m_Samples, &window->m_Samples);
        if (!format)
        {
            format = ChooseBasicFormat(window->m_DC);
        }

        if (!SetFormat(window->m_DC, format))
        {
            return false;
        }

        bool core = window->m_Params.m_OpenGLUseCoreProfileHint;
        window->m_Context = CreateContext(window->m_DC, 0, wgl, major, minor, core);
        if (!window->m_Context)
        {
            return false;
        }

        HDC previous_dc = wglGetCurrentDC();
        HGLRC previous = wglGetCurrentContext();
        if (!wglMakeCurrent(window->m_DC, window->m_Context))
        {
            wglMakeCurrent(previous_dc, previous);
            return false;
        }

        window->m_SwapInterval = (decltype(window->m_SwapInterval))GetWglProc("wglSwapIntervalEXT");
        window->m_AuxWindow = CreateHelperWindow(window->m_Window);
        if (window->m_AuxWindow)
        {
            window->m_AuxDC = GetDC(window->m_AuxWindow);
            if (window->m_AuxDC && SetFormat(window->m_AuxDC, format))
            {
                window->m_AuxContext = CreateContext(window->m_AuxDC, window->m_Context, wgl, major, minor, core);
            }
        }

        return true;
    }

    void Win32OpenGLDestroy(HWindow window)
    {
        HGLRC current = wglGetCurrentContext();
        if (current && (current == window->m_Context || current == window->m_AuxContext))
        {
            wglMakeCurrent(0, 0);
        }

        if (window->m_AuxContext)
        {
            wglDeleteContext(window->m_AuxContext);
        }

        if (window->m_AuxDC)
        {
            ReleaseDC(window->m_AuxWindow, window->m_AuxDC);
        }

        if (window->m_AuxWindow)
        {
            DestroyWindow(window->m_AuxWindow);
        }

        if (window->m_Context)
        {
            wglDeleteContext(window->m_Context);
        }

        if (window->m_DC)
        {
            ReleaseDC(window->m_Window, window->m_DC);
        }
    }

    int32_t OpenGLGetDefaultFramebufferId()
    {
        return 0;
    }

    uintptr_t GetProcAddress(HWindow window, const char* name)
    {
        if (window->m_EGL)
        {
            return EGLGetProcAddress(window->m_EGL, name);
        }

        if (!window->m_Context || !wglGetCurrentContext())
        {
            return 0;
        }

        PROC proc = GetWglProc(name);
        return (uintptr_t)(proc ? proc : ::GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name));
    }

    void SetSwapInterval(HWindow window, uint32_t interval)
    {
        if (window->m_EGL)
        {
            EGLSetSwapInterval(interval);
            return;
        }

        if (window->m_SwapInterval)
        {
            window->m_SwapInterval(interval);
        }
    }

    void SwapBuffers(HWindow window)
    {
        if (window->m_EGL)
        {
            EGLSwapBuffers(window->m_EGL);
            return;
        }

        if (window->m_Context)
        {
            ::SwapBuffers(window->m_DC);
        }
    }

    void* AcquireAuxContext(HWindow window)
    {
        if (window->m_EGL)
        {
            return EGLAcquireAuxContext(window->m_EGL);
        }

        return window->m_AuxContext && wglMakeCurrent(window->m_AuxDC, window->m_AuxContext) ? window->m_AuxContext : 0;
    }

    void UnacquireAuxContext(HWindow window, void* context)
    {
        if (window->m_EGL)
        {
            EGLUnacquireAuxContext(window->m_EGL, context);
            return;
        }

        if (context && context == window->m_AuxContext && context == wglGetCurrentContext())
        {
            wglMakeCurrent(0, 0);
        }
    }

}
