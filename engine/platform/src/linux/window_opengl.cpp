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

#include "window_linux.h"
#include "../platform_window_opengl.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glxext.h>
#include <wayland-egl.h>
#include <dlfcn.h>
#include <dlib/log.h>
#include <pthread.h>
#include <string.h>

#ifndef EGL_PRESENT_OPAQUE_EXT
#define EGL_PRESENT_OPAQUE_EXT 0x31DF
#endif

#define GLX_FUNCTIONS(F) \
    F(glXChooseFBConfig) \
    F(glXGetVisualFromFBConfig) \
    F(glXCreateNewContext) \
    F(glXGetProcAddressARB) \
    F(glXCreatePbuffer) \
    F(glXDestroyPbuffer) \
    F(glXDestroyContext) \
    F(glXMakeContextCurrent) \
    F(glXSwapBuffers) \
    F(glXGetCurrentContext) \
    F(glXGetFBConfigAttrib) \
    F(glXQueryExtensionsString)
#define EGL_FUNCTIONS(F) \
    F(eglGetDisplay) \
    F(eglInitialize) \
    F(eglBindAPI) \
    F(eglChooseConfig) \
    F(eglGetCurrentContext) \
    F(eglGetConfigAttrib) \
    F(eglCreateContext) \
    F(eglCreateWindowSurface) \
    F(eglCreatePbufferSurface) \
    F(eglMakeCurrent) \
    F(eglDestroySurface) \
    F(eglDestroyContext) \
    F(eglTerminate) \
    F(eglGetProcAddress) \
    F(eglSwapBuffers) \
    F(eglSwapInterval) \
    F(eglQueryString)
struct LinuxGL
{
    void*                              m_WaylandEGL;
    wl_egl_window*                     m_Native;
    __typeof__(&wl_egl_window_create)  m_CreateNative;
    __typeof__(&wl_egl_window_destroy) m_DestroyNative;
    __typeof__(&wl_egl_window_resize)  m_ResizeNative;
#define FIELD(f) __typeof__(&f) m_##f;
    GLX_FUNCTIONS(FIELD)
    EGL_FUNCTIONS(FIELD)
#undef FIELD
    GLXFBConfig m_XConfig;
    GLXContext m_XMain, m_XAux;
    GLXPbuffer m_XPbuffer;
    EGLDisplay m_EDisplay;
    EGLConfig  m_EConfig;
    void*      m_EClient;
    EGLContext m_EMain, m_EAux;
    EGLSurface m_ESurface, m_EPbuffer;
    EGLenum    m_Api;
};

namespace dmPlatform
{
    static bool Extension(const char* list, const char* name)
    {
        if (!list)
        {
            return false;
        }

        size_t n = strlen(name);
        for (const char* p = list; (p = strstr(p, name)); p += n)
        {
            if ((p == list || p[-1] == ' ') && (!p[n] || p[n] == ' '))
            {
                return true;
            }
        }

        return false;
    }

    static bool SelectGLX(HWindow w, VisualID* visual)
    {
        LinuxGL* g = w->m_GL;
        // GLX installs Xlib callbacks that outlive individual windows and contexts.
        static void* library = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!library)
        {
            return false;
        }

#define LOAD(f) if (!(g->m_##f = (__typeof__(&f))dlsym(library,#f))) return false;
        GLX_FUNCTIONS(LOAD)
#undef LOAD
        int count = 0;
        int attrs[] = { GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT | GLX_PBUFFER_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DOUBLEBUFFER, True,
                        GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, (int)w->m_Params.m_ContextAlphabits,
                        GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, GLX_SAMPLES, (int)(w->m_Params.m_Samples > 1 ? w->m_Params.m_Samples : 0), None };
        GLXFBConfig* configs = g->m_glXChooseFBConfig(w->m_Display, DefaultScreen(w->m_Display), attrs, &count);
        if (!count)
        {
            XFree(configs);
            attrs[sizeof(attrs) / sizeof(*attrs) - 3] = None;
            configs = g->m_glXChooseFBConfig(w->m_Display, DefaultScreen(w->m_Display), attrs, &count);
        }

        if (!count)
        {
            XFree(configs);
            attrs[1] = GLX_WINDOW_BIT;
            configs = g->m_glXChooseFBConfig(w->m_Display, DefaultScreen(w->m_Display), attrs, &count);
        }

        if (!configs || !count)
        {
            XFree(configs);
            return false;
        }

        g->m_XConfig = configs[0];
        XFree(configs);
        XVisualInfo* vi = g->m_glXGetVisualFromFBConfig(w->m_Display, g->m_XConfig);
        if (!vi)
        {
            return false;
        }

        *visual = vi->visualid;
        XFree(vi);
        return true;
    }

    static bool SelectEGL(HWindow w, VisualID* visual)
    {
        LinuxGL* g = w->m_GL;
        static void* library = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!library)
        {
            return false;
        }

#define LOAD(f) if (!(g->m_##f = (__typeof__(&f))dlsym(library,#f))) return false;
        EGL_FUNCTIONS(LOAD)
#undef LOAD
        void* display = w->m_X11 ? (void*)w->m_Display : (void*)w->m_WlDisplay;
        PFNEGLGETPLATFORMDISPLAYEXTPROC platform = (PFNEGLGETPLATFORMDISPLAYEXTPROC)g->m_eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (platform)
        {
            g->m_EDisplay = platform(w->m_X11 ? EGL_PLATFORM_X11_EXT : EGL_PLATFORM_WAYLAND_EXT, display, 0);
        }

        if (!g->m_EDisplay)
        {
            g->m_EDisplay = g->m_eglGetDisplay((EGLNativeDisplayType)display);
        }

        if (!g->m_EDisplay || !g->m_eglInitialize(g->m_EDisplay, 0, 0))
        {
            return false;
        }

        g->m_Api = w->m_Params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGLES ? EGL_OPENGL_ES_API : EGL_OPENGL_API;
        if (!g->m_eglBindAPI(g->m_Api))
        {
            return false;
        }

        const char* client = g->m_Api == EGL_OPENGL_ES_API ? "libGLESv2.so.2" : "libOpenGL.so.0";
        g->m_EClient = dlopen(client, RTLD_LAZY | RTLD_LOCAL);
        if (!g->m_EClient && g->m_Api == EGL_OPENGL_API)
        {
            g->m_EClient = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        }

        int attrs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, g->m_Api == EGL_OPENGL_ES_API ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT,
                        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, (int)w->m_Params.m_ContextAlphabits, EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
                        EGL_SAMPLES, (int)(w->m_Params.m_Samples > 1 ? w->m_Params.m_Samples : 0), EGL_NONE };
        int count = 0;
        g->m_eglChooseConfig(g->m_EDisplay, attrs, &g->m_EConfig, 1, &count);
        if (!count)
        {
            attrs[sizeof(attrs) / sizeof(*attrs) - 2] = 0;
            if (!g->m_eglChooseConfig(g->m_EDisplay, attrs, &g->m_EConfig, 1, &count) || !count)
            {
                return false;
            }
        }

        int id = 0;
        if (w->m_X11 && !g->m_eglGetConfigAttrib(g->m_EDisplay, g->m_EConfig, EGL_NATIVE_VISUAL_ID, &id))
        {
            return false;
        }

        *visual = id;
        return true;
    }

    bool LinuxPrepareGL(HWindow w, VisualID* visual)
    {
        *visual = 0;
        if (w->m_Params.m_GraphicsApi != WINDOW_GRAPHICS_API_OPENGL && w->m_Params.m_GraphicsApi != WINDOW_GRAPHICS_API_OPENGLES)
        {
            return true;
        }

        w->m_GL = new LinuxGL();
        return w->m_X11 && w->m_Params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL ? SelectGLX(w, visual) : SelectEGL(w, visual);
    }

    // GLX reports a rejected profile/version via the display error handler.
    static pthread_mutex_t g_ContextLock = PTHREAD_MUTEX_INITIALIZER;
    static Display* g_Creating;
    static int g_ContextError;
    static int (*g_PreviousError)(Display*, XErrorEvent*);
    static int ContextError(Display* display, XErrorEvent* event)
    {
        if (display != g_Creating)
        {
            return g_PreviousError(display, event);
        }

        if (!g_ContextError)
        {
            g_ContextError = event->error_code;
        }

        return 0;
    }

    static GLXContext CreateContext(HWindow w, int major, int minor, GLXContext share)
    {
        LinuxGL* g = w->m_GL;
        if (!major)
        {
            return g->m_glXCreateNewContext(w->m_Display, g->m_XConfig, GLX_RGBA_TYPE, share, True);
        }

        PFNGLXCREATECONTEXTATTRIBSARBPROC create = (PFNGLXCREATECONTEXTATTRIBSARBPROC)g->m_glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
        int attrs[] = { GLX_CONTEXT_MAJOR_VERSION_ARB, major, GLX_CONTEXT_MINOR_VERSION_ARB, minor,
                        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, None };
        if (!w->m_Params.m_OpenGLUseCoreProfileHint)
        {
            attrs[4] = None;
        }

        return create ? create(w->m_Display, g->m_XConfig, share, True, attrs) : 0;
    }

    static bool CreateGLX(HWindow w, int major, int minor)
    {
        LinuxGL* g = w->m_GL;
        Display* d = w->m_Display;
        pthread_mutex_lock(&g_ContextLock);
        XSync(d, False);
        g_Creating = d;
        g_ContextError = 0;
        g_PreviousError = XSetErrorHandler(ContextError);
        g->m_XMain = CreateContext(w, major, minor, 0);
        bool ok = g->m_XMain && g->m_glXMakeContextCurrent(d, w->m_Xid, w->m_Xid, g->m_XMain);
        XSync(d, False);
        const int main_error = g_ContextError;
        ok = ok && !main_error;
        int drawable_types = 0;
        if (ok && g->m_glXGetFBConfigAttrib(d, g->m_XConfig, GLX_DRAWABLE_TYPE, &drawable_types) == Success && (drawable_types & GLX_PBUFFER_BIT))
        {
            int attrs[] = { GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, None };
            g->m_XPbuffer = g->m_glXCreatePbuffer(d, g->m_XConfig, attrs);
            if (g->m_XPbuffer)
            {
                g->m_XAux = CreateContext(w, major, minor, g->m_XMain);
            }
        }

        XSync(d, False);
        XSetErrorHandler(g_PreviousError);
        g_Creating = 0;
        pthread_mutex_unlock(&g_ContextLock);
        if (!ok)
        {
            char reason[256];
            const char* operation = g->m_XMain ? "make current" : "create context";
            if (main_error)
            {
                XGetErrorText(d, main_error, reason, sizeof(reason));
            }

            const char* detail = main_error ? reason : "the GLX driver rejected the request";
            if (major)
            {
                dmLogError("GLX failed to %s for OpenGL %d.%d (%s profile): %s", operation, major, minor,
                           w->m_Params.m_OpenGLUseCoreProfileHint ? "core" : "driver-selected", detail);
            }
            else
            {
                dmLogError("GLX failed to %s for the default OpenGL context: %s", operation, detail);
            }
        }
        return ok;
    }

    bool LinuxOpenGL(HWindow w)
    {
        LinuxGL* g = w->m_GL;
        if (!g)
        {
            return true;
        }

        uint32_t major = 3, minor = 3;
        OpenGLGetVersion(w->m_Params.m_GraphicsApiVersionHint, &major, &minor);
        if (g->m_XConfig)
        {
            return CreateGLX(w, major, minor);
        }

        EGLNativeWindowType native = (EGLNativeWindowType)w->m_Xid;
        if (w->m_WlDisplay)
        {
            g->m_WaylandEGL = dlopen("libwayland-egl.so.1", RTLD_NOW | RTLD_LOCAL);
            if (!g->m_WaylandEGL)
            {
                return false;
            }

            g->m_CreateNative = (__typeof__(g->m_CreateNative))dlsym(g->m_WaylandEGL, "wl_egl_window_create");
            g->m_DestroyNative = (__typeof__(g->m_DestroyNative))dlsym(g->m_WaylandEGL, "wl_egl_window_destroy");
            g->m_ResizeNative = (__typeof__(g->m_ResizeNative))dlsym(g->m_WaylandEGL, "wl_egl_window_resize");
            if (!g->m_CreateNative || !g->m_DestroyNative || !g->m_ResizeNative)
            {
                return false;
            }

            g->m_Native = g->m_CreateNative(w->m_Surface, w->m_PixelWidth, w->m_PixelHeight);
            if (!g->m_Native)
            {
                return false;
            }

            native = (EGLNativeWindowType)g->m_Native;
        }

        int attrs[] = { EGL_CONTEXT_MAJOR_VERSION_KHR, (int)major, EGL_CONTEXT_MINOR_VERSION_KHR, (int)minor,
                        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR, EGL_NONE };
        if (g->m_Api == EGL_OPENGL_ES_API)
        {
            attrs[1] = 2;
            attrs[2] = EGL_NONE;
        }
        else if (!major)
        {
            attrs[0] = EGL_NONE;
        }
        else if (!w->m_Params.m_OpenGLUseCoreProfileHint)
        {
            attrs[4] = EGL_NONE;
        }

        const char* extensions = g->m_eglQueryString(g->m_EDisplay, EGL_EXTENSIONS);
        const EGLint opaque[] = { EGL_PRESENT_OPAQUE_EXT, EGL_TRUE, EGL_NONE };
        const bool present_opaque = w->m_Wayland && Extension(extensions, "EGL_EXT_present_opaque");
        g->m_EMain = g->m_eglCreateContext(g->m_EDisplay, g->m_EConfig, EGL_NO_CONTEXT, attrs);
        g->m_ESurface = g->m_eglCreateWindowSurface(g->m_EDisplay, g->m_EConfig, native, present_opaque ? opaque : 0);
        if (!g->m_EMain || !g->m_ESurface || !g->m_eglMakeCurrent(g->m_EDisplay, g->m_ESurface, g->m_ESurface, g->m_EMain))
        {
            return false;
        }

        int pbuffer[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
        g->m_EPbuffer = g->m_eglCreatePbufferSurface(g->m_EDisplay, g->m_EConfig, pbuffer);
        if (g->m_EPbuffer || Extension(extensions, "EGL_KHR_surfaceless_context"))
        {
            g->m_EAux = g->m_eglCreateContext(g->m_EDisplay, g->m_EConfig, g->m_EMain, attrs);
        }

        return true;
    }

    void LinuxResizeGL(HWindow w)
    {
        LinuxGL* g = w->m_GL;
        if (g && g->m_Native)
        {
            g->m_ResizeNative(g->m_Native, w->m_PixelWidth, w->m_PixelHeight, 0, 0);
        }
    }

    void LinuxCloseGL(HWindow w)
    {
        LinuxGL* g = w->m_GL;
        if (!g)
        {
            return;
        }

        if (g->m_XConfig)
        {
            if (g->m_XMain && g->m_glXGetCurrentContext() == g->m_XMain)
            {
                g->m_glXMakeContextCurrent(w->m_Display, None, None, 0);
            }

            if (g->m_XAux)
            {
                g->m_glXDestroyContext(w->m_Display, g->m_XAux);
            }

            if (g->m_XMain)
            {
                g->m_glXDestroyContext(w->m_Display, g->m_XMain);
            }

            if (g->m_XPbuffer)
            {
                g->m_glXDestroyPbuffer(w->m_Display, g->m_XPbuffer);
            }
        }

        if (g->m_EDisplay)
        {
            if (g->m_EMain && g->m_eglGetCurrentContext() == g->m_EMain)
            {
                g->m_eglMakeCurrent(g->m_EDisplay, 0, 0, 0);
            }

            if (g->m_EAux)
            {
                g->m_eglDestroyContext(g->m_EDisplay, g->m_EAux);
            }

            if (g->m_EMain)
            {
                g->m_eglDestroyContext(g->m_EDisplay, g->m_EMain);
            }

            if (g->m_EPbuffer)
            {
                g->m_eglDestroySurface(g->m_EDisplay, g->m_EPbuffer);
            }

            if (g->m_ESurface)
            {
                g->m_eglDestroySurface(g->m_EDisplay, g->m_ESurface);
            }

            g->m_eglTerminate(g->m_EDisplay);
        }

        if (g->m_EClient)
        {
            dlclose(g->m_EClient);
        }

        if (g->m_Native)
        {
            g->m_DestroyNative(g->m_Native);
        }

        if (g->m_WaylandEGL)
        {
            dlclose(g->m_WaylandEGL);
        }

        delete g;
        w->m_GL = 0;
    }

    GLXContext GetX11GLXContext(HWindow w)
    {
        return w && w->m_GL ? w->m_GL->m_XMain : 0;
    }

    int32_t OpenGLGetDefaultFramebufferId()
    {
        return 0;
    }

    uintptr_t GetProcAddress(HWindow w, const char* name)
    {
        LinuxGL* g = w->m_GL;
        if (!g)
        {
            return 0;
        }

        if (g->m_XConfig)
        {
            return (uintptr_t)g->m_glXGetProcAddressARB((const GLubyte*)name);
        }

        // EGL 1.4 only guarantees extension entry points from eglGetProcAddress.
        void* core = g->m_EClient ? dlsym(g->m_EClient, name) : 0;
        return core ? (uintptr_t)core : (uintptr_t)g->m_eglGetProcAddress(name);
    }

    void* AcquireAuxContext(HWindow w)
    {
        LinuxGL* g = w->m_GL;
        if (!g)
        {
            return 0;
        }

        if (g->m_XAux && g->m_glXMakeContextCurrent(w->m_Display, g->m_XPbuffer, g->m_XPbuffer, g->m_XAux))
        {
            return g->m_XAux;
        }

        if (g->m_EAux && g->m_eglBindAPI(g->m_Api) && g->m_eglMakeCurrent(g->m_EDisplay, g->m_EPbuffer, g->m_EPbuffer, g->m_EAux))
        {
            return g->m_EAux;
        }

        return 0;
    }

    void UnacquireAuxContext(HWindow w, void*)
    {
        LinuxGL* g = w->m_GL;
        if (g->m_XConfig)
        {
            g->m_glXMakeContextCurrent(w->m_Display, None, None, 0);
        }
        else
        {
            g->m_eglMakeCurrent(g->m_EDisplay, 0, 0, 0);
        }
    }

    void SwapBuffers(HWindow w)
    {
        LinuxGL* g = w->m_GL;
        if (!g || (w->m_Wayland && !w->m_Shown))
        {
            return;
        }

        if (g->m_XConfig)
        {
            g->m_glXSwapBuffers(w->m_Display, w->m_Xid);
        }
        else
        {
            g->m_eglSwapBuffers(g->m_EDisplay, g->m_ESurface);
        }
    }

    void SetSwapInterval(HWindow w, uint32_t interval)
    {
        LinuxGL* g = w->m_GL;
        if (!g)
        {
            return;
        }

        if (!g->m_XConfig)
        {
            g->m_eglSwapInterval(g->m_EDisplay, interval);
            return;
        }

        const char* extensions = g->m_glXQueryExtensionsString(w->m_Display, DefaultScreen(w->m_Display));
        if (Extension(extensions, "GLX_EXT_swap_control"))
        {
            PFNGLXSWAPINTERVALEXTPROC set = (PFNGLXSWAPINTERVALEXTPROC)GetProcAddress(w, "glXSwapIntervalEXT");
            if (set)
            {
                set(w->m_Display, w->m_Xid, interval);
            }
        }
        else if (Extension(extensions, "GLX_MESA_swap_control"))
        {
            PFNGLXSWAPINTERVALMESAPROC set = (PFNGLXSWAPINTERVALMESAPROC)GetProcAddress(w, "glXSwapIntervalMESA");
            if (set)
            {
                set(interval);
            }
        }
        else if (interval && Extension(extensions, "GLX_SGI_swap_control"))
        {
            PFNGLXSWAPINTERVALSGIPROC set = (PFNGLXSWAPINTERVALSGIPROC)GetProcAddress(w, "glXSwapIntervalSGI");
            if (set)
            {
                set(interval);
            }
        }
    }

    int LinuxGLState(HWindow w, WindowState state)
    {
        LinuxGL* g = w->m_GL;
        if (!g)
        {
            return 0;
        }

        int gx, eg;
        switch (state)
        {
            case WINDOW_STATE_AUX_CONTEXT:
            {
                return g->m_XAux || g->m_EAux;
            }
            case WINDOW_STATE_ACCELERATED:
            {
                return 1;
            }
            case WINDOW_STATE_RED_BITS:
            {
                gx = GLX_RED_SIZE;
                eg = EGL_RED_SIZE;
                break;
            }
            case WINDOW_STATE_GREEN_BITS:
            {
                gx = GLX_GREEN_SIZE;
                eg = EGL_GREEN_SIZE;
                break;
            }
            case WINDOW_STATE_BLUE_BITS:
            {
                gx = GLX_BLUE_SIZE;
                eg = EGL_BLUE_SIZE;
                break;
            }
            case WINDOW_STATE_ALPHA_BITS:
            {
                gx = GLX_ALPHA_SIZE;
                eg = EGL_ALPHA_SIZE;
                break;
            }
            case WINDOW_STATE_STENCIL_BITS:
            {
                gx = GLX_STENCIL_SIZE;
                eg = EGL_STENCIL_SIZE;
                break;
            }
            case WINDOW_STATE_DEPTH_BITS:
            {
                gx = GLX_DEPTH_SIZE;
                eg = EGL_DEPTH_SIZE;
                break;
            }
            case WINDOW_STATE_FSAA_SAMPLES:
            {
                gx = GLX_SAMPLES;
                eg = EGL_SAMPLES;
                break;
            }
            default:
            {
                return 0;
            }
        }

        int value = 0;
        if (g->m_XConfig)
        {
            g->m_glXGetFBConfigAttrib(w->m_Display, g->m_XConfig, gx, &value);
        }
        else
        {
            g->m_eglGetConfigAttrib(g->m_EDisplay, g->m_EConfig, eg, &value);
        }

        return value;
    }

}
