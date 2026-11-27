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

#include "window_wayland.h"
#include "../platform_window_opengl.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glxext.h>
#include <wayland-egl.h>
#include <dlfcn.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <pthread.h>
#include <string.h>

#ifndef EGL_PRESENT_OPAQUE_EXT
#define EGL_PRESENT_OPAQUE_EXT 0x31DF
#endif
#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif
#ifndef EGL_DEVICE_TYPE_EXT
#define EGL_DEVICE_TYPE_EXT 0x3590
#define EGL_DEVICE_TYPE_INTEGRATED_GPU_EXT 0x3592
#define EGL_DEVICE_TYPE_DISCRETE_GPU_EXT 0x3593
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
    F(eglGetError) \
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
    bool       m_PresentOpaque, m_Accelerated;
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

    static int ConfigAttribute(HWindow w, GLXFBConfig glx, EGLConfig egl, int glx_attr, int egl_attr)
    {
        int value = 0;
        if (glx)
        {
            w->m_GL->m_glXGetFBConfigAttrib(w->m_Display, glx, glx_attr, &value);
        }
        else
        {
            w->m_GL->m_eglGetConfigAttrib(w->m_GL->m_EDisplay, egl, egl_attr, &value);
        }

        return value;
    }

    static OpenGLFormatScore ScoreFormat(HWindow w, GLXFBConfig glx, EGLConfig egl, bool pbuffer)
    {
        // Bit depths are preferences. A 16-bit depth buffer or an RGB565 visual can still run the game.
        const int channels[][3] = {
            { GLX_RED_SIZE, EGL_RED_SIZE, 8 }, { GLX_GREEN_SIZE, EGL_GREEN_SIZE, 8 },
            { GLX_BLUE_SIZE, EGL_BLUE_SIZE, 8 }, { GLX_ALPHA_SIZE, EGL_ALPHA_SIZE, w->m_Params.m_ContextAlphabits },
            { GLX_DEPTH_SIZE, EGL_DEPTH_SIZE, 24 }, { GLX_STENCIL_SIZE, EGL_STENCIL_SIZE, 8 }
        };
        int samples = ConfigAttribute(w, glx, egl, GLX_SAMPLES, EGL_SAMPLES);
        OpenGLFormatScore score = ScoreOpenGLSamples(w->m_Params.m_Samples, samples, pbuffer);
        for (size_t i = 0; i < sizeof(channels) / sizeof(channels[0]); ++i)
        {
            int bits = ConfigAttribute(w, glx, egl, channels[i][0], channels[i][1]);
            ScoreOpenGLChannel(score, channels[i][2], bits);
        }

        return score;
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
        const int attrs[] = { GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DOUBLEBUFFER, True, None };
        GLXFBConfig* configs = g->m_glXChooseFBConfig(w->m_Display, DefaultScreen(w->m_Display), attrs, &count);
        if (!configs || !count)
        {
            XFree(configs);
            return false;
        }

        OpenGLFormatScore best = {};
        for (int i = 0; i < count; ++i)
        {
            int types = 0;
            g->m_glXGetFBConfigAttrib(w->m_Display, configs[i], GLX_DRAWABLE_TYPE, &types);
            OpenGLFormatScore score = ScoreFormat(w, configs[i], 0, (types & GLX_PBUFFER_BIT) != 0);
            if (!g->m_XConfig || BetterOpenGLFormat(score, best))
            {
                g->m_XConfig = configs[i];
                best = score;
                if (!best.m_MissingBits && !best.m_SampleError && best.m_Pbuffer)
                {
                    break;
                }
            }
        }

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

    static bool EGLFailure(LinuxGL* g, const char* operation, uint32_t major = 0, uint32_t minor = 0)
    {
        // EGL errors are per-thread. Capture the failing call's result before any cleanup calls can replace it.
        EGLint error = g->m_eglGetError();
        static const char* const literals[] = {
            "EGL_SUCCESS", "EGL_NOT_INITIALIZED", "EGL_BAD_ACCESS", "EGL_BAD_ALLOC", "EGL_BAD_ATTRIBUTE",
            "EGL_BAD_CONFIG", "EGL_BAD_CONTEXT", "EGL_BAD_CURRENT_SURFACE", "EGL_BAD_DISPLAY", "EGL_BAD_MATCH",
            "EGL_BAD_NATIVE_PIXMAP", "EGL_BAD_NATIVE_WINDOW", "EGL_BAD_PARAMETER", "EGL_BAD_SURFACE", "EGL_CONTEXT_LOST"
        };
        char unknown[32];
        const char* reason;
        if (error >= EGL_SUCCESS && error <= EGL_CONTEXT_LOST)
        {
            reason = literals[error - EGL_SUCCESS];
        }
        else
        {
            dmSnPrintf(unknown, sizeof(unknown), "EGL error 0x%x", (unsigned)error);
            reason = unknown;
        }

        const char* api = g->m_Api == EGL_OPENGL_ES_API ? "OpenGL ES" : "OpenGL";
        if (major)
        {
            dmLogError("%s failed for %s %u.%u: %s", operation, api, major, minor, reason);
        }
        else
        {
            dmLogError("%s failed for %s: %s", operation, api, reason);
        }

        return false;
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

        g->m_Api = w->m_Params.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGLES ? EGL_OPENGL_ES_API : EGL_OPENGL_API;
        if (!g->m_EDisplay)
        {
            return EGLFailure(g, "eglGetDisplay");
        }

        if (!g->m_eglInitialize(g->m_EDisplay, 0, 0))
        {
            return EGLFailure(g, "eglInitialize");
        }

        if (!g->m_eglBindAPI(g->m_Api))
        {
            return EGLFailure(g, "eglBindAPI");
        }

        const char* client = g->m_Api == EGL_OPENGL_ES_API ? "libGLESv2.so.2" : "libOpenGL.so.0";
        g->m_EClient = dlopen(client, RTLD_LAZY | RTLD_LOCAL);
        if (!g->m_EClient && g->m_Api == EGL_OPENGL_API)
        {
            g->m_EClient = dlopen("libGL.so.1", RTLD_LAZY | RTLD_LOCAL);
        }

        g->m_PresentOpaque = w->m_Wayland && Extension(g->m_eglQueryString(g->m_EDisplay, EGL_EXTENSIONS), "EGL_EXT_present_opaque");
        const EGLint attrs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
                                 EGL_RENDERABLE_TYPE, g->m_Api == EGL_OPENGL_ES_API ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_BIT, EGL_NONE };
        EGLint count = 0;
        if (!g->m_eglChooseConfig(g->m_EDisplay, attrs, 0, 0, &count))
        {
            return EGLFailure(g, "eglChooseConfig");
        }

        if (!count)
        {
            dmLogError("EGL returned no window framebuffer configurations for the requested graphics API.");
            return false;
        }

        dmArray<EGLConfig> configs;
        configs.SetCapacity(count);
        if (!g->m_eglChooseConfig(g->m_EDisplay, attrs, configs.Begin(), count, &count))
        {
            return EGLFailure(g, "eglChooseConfig");
        }

        OpenGLFormatScore best = {};
        for (EGLint i = 0; i < count; ++i)
        {
            EGLConfig config = configs.Begin()[i];
            // The Wayland opaque region is a repaint hint. Without opaque presentation the buffer itself must have no alpha.
            EGLint alpha;
            if (w->m_Wayland && !g->m_PresentOpaque && (!g->m_eglGetConfigAttrib(g->m_EDisplay, config, EGL_ALPHA_SIZE, &alpha) || alpha))
            {
                continue;
            }

            EGLint id = 0;
            if (w->m_X11 && (!g->m_eglGetConfigAttrib(g->m_EDisplay, config, EGL_NATIVE_VISUAL_ID, &id) || !id))
            {
                continue;
            }

            EGLint surface_types = 0;
            g->m_eglGetConfigAttrib(g->m_EDisplay, config, EGL_SURFACE_TYPE, &surface_types);
            OpenGLFormatScore score = ScoreFormat(w, 0, config, (surface_types & EGL_PBUFFER_BIT) != 0);
            if (!g->m_EConfig || BetterOpenGLFormat(score, best))
            {
                best = score;
                g->m_EConfig = config;
                *visual = id;
                if (!best.m_MissingBits && !best.m_SampleError && best.m_Pbuffer)
                {
                    break;
                }
            }
        }

        if (!g->m_EConfig)
        {
            dmLogError("EGL returned no framebuffer configuration compatible with the native window.");
            return false;
        }

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
                        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, None };
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
        else if (Extension(g->m_glXQueryExtensionsString(d, DefaultScreen(d)), "GLX_MESA_query_renderer"))
        {
            // This query describes the current context. Keep its answer with the window that owns it.
            PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC query = (PFNGLXQUERYCURRENTRENDERERINTEGERMESAPROC)
                                                              g->m_glXGetProcAddressARB((const GLubyte*)"glXQueryCurrentRendererIntegerMESA");
            unsigned int accelerated = 0;
            g->m_Accelerated = query && query(GLX_RENDERER_ACCELERATED_MESA, &accelerated) && accelerated;
        }

        return ok;
    }

    static bool EGLRendererAccelerated(LinuxGL* g)
    {
        const char* extensions = g->m_eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        if (!Extension(extensions, "EGL_EXT_device_query") && !Extension(extensions, "EGL_EXT_device_base"))
        {
            return false;
        }

        PFNEGLQUERYDISPLAYATTRIBEXTPROC query_display = (PFNEGLQUERYDISPLAYATTRIBEXTPROC)g->m_eglGetProcAddress("eglQueryDisplayAttribEXT");
        PFNEGLQUERYDEVICESTRINGEXTPROC query_string = (PFNEGLQUERYDEVICESTRINGEXTPROC)g->m_eglGetProcAddress("eglQueryDeviceStringEXT");
        EGLAttrib value = 0;
        if (!query_display || !query_string || !query_display(g->m_EDisplay, EGL_DEVICE_EXT, &value) || !value)
        {
            return false;
        }

        EGLDeviceEXT device = (EGLDeviceEXT)value;
        const char* device_extensions = query_string(device, EGL_EXTENSIONS);
        if (Extension(device_extensions, "EGL_EXT_device_type"))
        {
            PFNEGLQUERYDEVICEATTRIBEXTPROC query = (PFNEGLQUERYDEVICEATTRIBEXTPROC)g->m_eglGetProcAddress("eglQueryDeviceAttribEXT");
            EGLAttrib type = 0;
            if (query && query(device, EGL_DEVICE_TYPE_EXT, &type))
            {
                return type == EGL_DEVICE_TYPE_INTEGRATED_GPU_EXT || type == EGL_DEVICE_TYPE_DISCRETE_GPU_EXT;
            }
        }

        // Older Linux drivers can identify a hardware device by its DRM node.
        if (Extension(device_extensions, "EGL_MESA_device_software"))
        {
            return false;
        }

        return (Extension(device_extensions, "EGL_EXT_device_drm_render_node") && query_string(device, EGL_DRM_RENDER_NODE_FILE_EXT)) ||
               (Extension(device_extensions, "EGL_EXT_device_drm") && query_string(device, EGL_DRM_DEVICE_FILE_EXT));
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
                        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                        EGL_CONTEXT_FLAGS_KHR, EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR, EGL_NONE };
        if (g->m_Api == EGL_OPENGL_ES_API)
        {
            major = 2;
            minor = 0;
            attrs[1] = major;
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
        g->m_EMain = g->m_eglCreateContext(g->m_EDisplay, g->m_EConfig, EGL_NO_CONTEXT, attrs);
        if (!g->m_EMain)
        {
            return EGLFailure(g, "eglCreateContext", major, minor);
        }

        g->m_ESurface = g->m_eglCreateWindowSurface(g->m_EDisplay, g->m_EConfig, native, g->m_PresentOpaque ? opaque : 0);
        if (!g->m_ESurface)
        {
            return EGLFailure(g, "eglCreateWindowSurface", major, minor);
        }

        if (!g->m_eglMakeCurrent(g->m_EDisplay, g->m_ESurface, g->m_ESurface, g->m_EMain))
        {
            return EGLFailure(g, "eglMakeCurrent", major, minor);
        }

        g->m_Accelerated = EGLRendererAccelerated(g);
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
            return g->m_glXGetCurrentContext() ? (uintptr_t)g->m_glXGetProcAddressARB((const GLubyte*)name) : 0;
        }

        if (!g->m_eglGetCurrentContext())
        {
            return 0;
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
        // Opening a visible Wayland window configures its surface before the first explicit show.
        if (!g || (w->m_Wayland && w->m_Wayland->m_Unmapped))
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
                return g->m_Accelerated;
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
            case WINDOW_STATE_SAMPLE_COUNT:
            {
                // GL reports zero multisamples for a framebuffer with one sample per pixel.
                int samples = ConfigAttribute(w, g->m_XConfig, g->m_EConfig, GLX_SAMPLES, EGL_SAMPLES);
                return samples > 0 ? samples : 1;
            }
            default:
            {
                return 0;
            }
        }

        return ConfigAttribute(w, g->m_XConfig, g->m_EConfig, gx, eg);
    }

}
