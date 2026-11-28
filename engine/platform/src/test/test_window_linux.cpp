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
#include "test_window_focus.h"
#include "../linux/window_wayland.h"
#include "../linux/joystick_linux.h"
#include "../window_desktop_keys.h"
#include <vulkan/vulkan.h>
#include "../platform_window_vulkan.h"
#include <dlib/hash.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <GL/glext.h>
#include <GL/glxext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static bool g_TestHardwareRenderer;
static __typeof__(&glXGetProcAddressARB) g_GLXGetProcAddress;
static bool g_TestWindowOnlyGLX, g_TestGLXPbufferError;
static __typeof__(&glXGetFBConfigAttrib) g_GLXGetFBConfigAttrib;

static int WindowOnlyGLXConfig(Display* display, GLXFBConfig config, int attribute, int* value)
{
    int result = g_GLXGetFBConfigAttrib(display, config, attribute, value);
    if (result == Success && attribute == GLX_DRAWABLE_TYPE)
    {
        *value &= ~GLX_PBUFFER_BIT;
    }

    return result;
}

static GLXPbuffer RejectedGLXPbuffer(Display* display, GLXFBConfig, const int*)
{
    // Like a rejected pbuffer, this returns an XID before the server delivers the error.
    return XCreateSimpleWindow(display, None, 0, 0, 1, 1, 0, 0, 0);
}

static Bool QueryAcceleratedGLXRenderer(int attribute, unsigned* value)
{
    *value = 1;
    return attribute == GLX_RENDERER_ACCELERATED_MESA;
}

static __GLXextFuncPtr GLXProcAddress(const GLubyte* name)
{
    if (g_TestHardwareRenderer && !strcmp((const char*)name, "glXQueryCurrentRendererIntegerMESA"))
    {
        return (__GLXextFuncPtr)QueryAcceleratedGLXRenderer;
    }

    return g_GLXGetProcAddress(name);
}

static const char* EGLAPIENTRY HardwareDeviceString(EGLDeviceEXT, EGLint attribute)
{
    if (attribute == EGL_EXTENSIONS)
    {
        return "EGL_EXT_device_drm";
    }

    return attribute == EGL_DRM_DEVICE_FILE_EXT ? "/dev/dri/card0" : 0;
}

// Exercise EGL 1.4 drivers that export core GL functions only from their client library.
static PFNEGLGETPROCADDRESSPROC g_EGLGetProcAddress;
static __eglMustCastToProperFunctionPointerType EGLAPIENTRY ExtensionProcAddress(const char* name)
{
    if (g_TestHardwareRenderer && !strcmp(name, "eglQueryDeviceStringEXT"))
    {
        return (__eglMustCastToProperFunctionPointerType)HardwareDeviceString;
    }

    return strcmp(name, "glGenTextures") ? g_EGLGetProcAddress(name) : 0;
}

extern "C" void* __real_dlsym(void* handle, const char* name);
static const char* g_EGLDisplayExtensions;
static PFNEGLMAKECURRENTPROC g_TestEGLMakeCurrent;
static bool g_TestWindowOnlyEGL, g_RejectEGLPbuffer, g_RejectEGLAuxWindow;
static unsigned g_EGLWindowSurfaces;

static EGLBoolean EGLAPIENTRY WindowOnlyEGLConfig(EGLDisplay display, EGLConfig config, EGLint attribute, EGLint* value)
{
    EGLBoolean result = eglGetConfigAttrib(display, config, attribute, value);
    if (result && attribute == EGL_SURFACE_TYPE)
    {
        *value &= ~EGL_PBUFFER_BIT;
    }

    return result;
}

static EGLSurface EGLAPIENTRY RejectedEGLPbuffer(EGLDisplay, EGLConfig, const EGLint*)
{
    return EGL_NO_SURFACE;
}

static EGLSurface EGLAPIENTRY MainEGLWindowOnly(EGLDisplay display, EGLConfig config, EGLNativeWindowType native, const EGLint* attrs)
{
    return ++g_EGLWindowSurfaces == 1 ? eglCreateWindowSurface(display, config, native, attrs) : EGL_NO_SURFACE;
}

static const char* EGLAPIENTRY TestEGLExtensions(EGLDisplay display, EGLint query)
{
    if (g_EGLDisplayExtensions && query == EGL_EXTENSIONS)
    {
        return display ? g_EGLDisplayExtensions : "";
    }

    return eglQueryString(display, query);
}

static EGLBoolean EGLAPIENTRY RejectSurfacelessBind(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
{
    if (context && !draw && !read)
    {
        return EGL_FALSE;
    }

    return eglMakeCurrent(display, draw, read, context);
}

static EGLBoolean EGLAPIENTRY RejectRenderContextRestore(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context)
{
    if (context && eglGetCurrentContext())
    {
        if (draw)
        {
            // A window surface without a context makes EGL reject the restore with EGL_BAD_MATCH.
            return eglMakeCurrent(display, draw, read, EGL_NO_CONTEXT);
        }

        // Make the auxiliary bind succeed even on drivers that require a surface.
        draw = eglGetCurrentSurface(EGL_DRAW);
        read = eglGetCurrentSurface(EGL_READ);
    }

    return eglMakeCurrent(display, draw, read, context);
}

// Visual 2 adds a pbuffer. Visual 3 trades depth precision for multisampling.
static bool g_TestEGLConfigs;
static EGLint g_TestVisuals[] = { 1, 2, 3 };

static EGLBoolean EGLAPIENTRY ChooseTestEGLConfigs(EGLDisplay, const EGLint*, EGLConfig* configs, EGLint capacity, EGLint* count)
{
    const EGLint available = DM_ARRAY_SIZE(g_TestVisuals);
    *count = configs ? dmMath::Min(capacity, available) : available;
    for (EGLint i = 0; configs && i < *count; ++i)
    {
        configs[i] = &g_TestVisuals[i];
    }

    return EGL_TRUE;
}

static EGLBoolean EGLAPIENTRY GetTestEGLConfigAttrib(EGLDisplay, EGLConfig config, EGLint attribute, EGLint* value)
{
    const EGLint visual = *(EGLint*)config;
    switch (attribute)
    {
        case EGL_NATIVE_VISUAL_ID:
        {
            *value = visual;
            break;
        }
        case EGL_SURFACE_TYPE:
        {
            *value = EGL_WINDOW_BIT | (visual == 2 ? EGL_PBUFFER_BIT : 0);
            break;
        }
        case EGL_SAMPLES:
        {
            *value = visual == 3 ? 4 : 0;
            break;
        }
        case EGL_DEPTH_SIZE:
        {
            *value = visual == 3 ? 16 : 24;
            break;
        }
        default:
        {
            *value = 8;
            break;
        }
    }

    return EGL_TRUE;
}

extern "C" void* __wrap_dlsym(void* handle, const char* name)
{
    if (g_TestEGLConfigs)
    {
        if (!strcmp(name, "eglChooseConfig"))
        {
            return (void*)ChooseTestEGLConfigs;
        }

        if (!strcmp(name, "eglGetConfigAttrib"))
        {
            return (void*)GetTestEGLConfigAttrib;
        }
    }

    if (g_EGLDisplayExtensions && !strcmp(name, "eglQueryString"))
    {
        return (void*)TestEGLExtensions;
    }

    if (g_TestEGLMakeCurrent && !strcmp(name, "eglMakeCurrent"))
    {
        return (void*)g_TestEGLMakeCurrent;
    }

    if (g_TestWindowOnlyEGL && !strcmp(name, "eglGetConfigAttrib"))
    {
        return (void*)WindowOnlyEGLConfig;
    }

    if (g_RejectEGLPbuffer && !strcmp(name, "eglCreatePbufferSurface"))
    {
        return (void*)RejectedEGLPbuffer;
    }

    if (g_RejectEGLAuxWindow && !strcmp(name, "eglCreateWindowSurface"))
    {
        return (void*)MainEGLWindowOnly;
    }

    if (!strcmp(name, "XGetXCBConnection") && getenv("DM_TEST_X11_NO_XCB"))
    {
        return 0;
    }

    void* symbol = __real_dlsym(handle, name);
    if (symbol && g_TestWindowOnlyGLX && !strcmp(name, "glXGetFBConfigAttrib"))
    {
        g_GLXGetFBConfigAttrib = (__typeof__(g_GLXGetFBConfigAttrib)) symbol;
        return (void*)WindowOnlyGLXConfig;
    }

    if (symbol && g_TestGLXPbufferError && !strcmp(name, "glXCreatePbuffer"))
    {
        return (void*)RejectedGLXPbuffer;
    }

    if (symbol && g_TestHardwareRenderer && !strcmp(name, "glXGetProcAddressARB"))
    {
        g_GLXGetProcAddress = (__typeof__(g_GLXGetProcAddress)) symbol;
        return (void*)GLXProcAddress;
    }

    if (symbol && !strcmp(name, "eglGetProcAddress"))
    {
        g_EGLGetProcAddress = (PFNEGLGETPROCADDRESSPROC)symbol;
        return (void*)ExtensionProcAddress;
    }

    return symbol;
}

static WindowCreateParams Params()
{
    WindowCreateParams p;
    WindowCreateParamsInitialize(&p);
#if defined(__aarch64__)
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGLES;
#else
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
#endif
    p.m_Title = "Defold Linux input test";
    p.m_Width = 320;
    p.m_Height = 240;
    return p;
}

static int g_ResizeFd = -1;
static unsigned g_ResizeWaits;
static int g_ClipboardWriteFd = -1;
static unsigned g_ClipboardWriteWaits;
extern "C" int __real_poll(struct pollfd*, nfds_t, int);
extern "C" int __wrap_poll(struct pollfd* fds, nfds_t count, int timeout)
{
    if (count == 1 && fds[0].fd == g_ClipboardWriteFd && fds[0].events == POLLOUT)
    {
        ++g_ClipboardWriteWaits;
        if (g_ClipboardWriteWaits == 1 && timeout >= 0)
        {
            return 0;
        }

        if (g_ClipboardWriteWaits == 2)
        {
            errno = EINTR;
            return -1;
        }
    }

    if (count == 1 && fds[0].fd == g_ResizeFd && timeout > 0)
    {
        ++g_ResizeWaits;
    }

    return __real_poll(fds, count, timeout);
}

static HWindow NewX11TestWindow()
{
    // DISPLAY may point to an unreachable server while the platform successfully connects to Wayland.
    HWindow window = dmPlatform::NewWindow();
    if (window && !window->m_X11)
    {
        dmPlatform::DeleteWindow(window);
        return 0;
    }

    return window;
}

struct X11ResizeEvents
{
    uint32_t m_Count, m_Width, m_Height;
    uint32_t m_FirstWidth, m_FirstHeight;
    HWindow* m_DeleteOnResize;
};

static void OnX11Resize(void* data, uint32_t width, uint32_t height)
{
    X11ResizeEvents* events = (X11ResizeEvents*)data;
    if (events->m_Count == 0)
    {
        events->m_FirstWidth = width;
        events->m_FirstHeight = height;
    }

    ++events->m_Count;
    events->m_Width = width;
    events->m_Height = height;
    if (events->m_DeleteOnResize)
    {
        dmPlatform::DeleteWindow(*events->m_DeleteOnResize);
        *events->m_DeleteOnResize = 0;
    }
}

// Polling the main X11 connection must also dispatch external resizes on a secondary window's connection.
TEST(LinuxWindow, X11PollSecondaryWindow)
{
    HWindow main = NewX11TestWindow();
    if (!main)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(main, p));
    HWindow second = NewX11TestWindow();
    ASSERT_TRUE(second);
    X11ResizeEvents events = {};
    p.m_ResizeCallback = OnX11Resize;
    p.m_ResizeCallbackUserData = &events;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(second, p));
    // Bypass the window manager so the X server applies the resize before the only event poll.
    XSetWindowAttributes attributes = {};
    attributes.override_redirect = True;
    XChangeWindowAttributes(main->m_Display, second->m_Xid, CWOverrideRedirect, &attributes);
    XResizeWindow(main->m_Display, second->m_Xid, 450, 320);
    XWindowAttributes geometry = {};
    EXPECT_TRUE(XGetWindowAttributes(main->m_Display, second->m_Xid, &geometry));
    EXPECT_EQ(450, geometry.width);
    EXPECT_EQ(320, geometry.height);
    XSync(second->m_Display, False);
    EXPECT_EQ(0u, events.m_Count);
    dmPlatform::PollEvents(main);
    EXPECT_EQ(1u, events.m_Count);
    EXPECT_EQ(450u, dmPlatform::GetWindowWidth(second));
    EXPECT_EQ(320u, dmPlatform::GetWindowHeight(second));
    dmPlatform::DeleteWindow(second);
    dmPlatform::DeleteWindow(main);
}

// An applied X11 resize updates size queries and preserves pointer coordinates before the next event poll.
TEST(LinuxWindow, X11AppliedResizeBeforePoll)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    X11ResizeEvents events = {};
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    p.m_ResizeCallback = OnX11Resize;
    p.m_ResizeCallbackUserData = &events;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    XSetWindowAttributes attributes = {};
    attributes.override_redirect = True;
    XChangeWindowAttributes(w->m_Display, w->m_Xid, CWOverrideRedirect, &attributes);
    dmPlatform::ShowWindow(w);
    XSync(w->m_Display, False);
    dmPlatform::PollEvents(w);
    events.m_Count = 0;
    XWarpPointer(w->m_Display, None, w->m_Xid, 0, 0, 0, 0, 200, 100);

    for (uint32_t i = 0; i < 2; ++i)
    {
        const uint32_t width = 481 + i, height = 311 + i;
        dmPlatform::SetWindowSize(w, width, height);
        EXPECT_EQ(width, dmPlatform::GetWindowWidth(w));
        EXPECT_EQ(height, dmPlatform::GetWindowHeight(w));
        uint32_t client_width, client_height;
        ASSERT_TRUE(dmPlatform::GetWindowClientSize(w, &client_width, &client_height));
        EXPECT_EQ(width, client_width);
        EXPECT_EQ(height, client_height);
        int32_t x, y;
        dmPlatform::GetMousePosition(w, &x, &y);
        EXPECT_EQ(200, x);
        EXPECT_EQ(100, y);
    }

    XSync(w->m_Display, False);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(2u, events.m_Count);
    EXPECT_EQ(481u, events.m_FirstWidth);
    EXPECT_EQ(311u, events.m_FirstHeight);
    EXPECT_EQ(482u, events.m_Width);
    EXPECT_EQ(312u, events.m_Height);

    // An external resize between two identical requests must not supersede the second one.
    Display* manager = XOpenDisplay(0);
    ASSERT_TRUE(manager);
    events.m_Count = 0;
    dmPlatform::SetWindowSize(w, 481, 311);
    XResizeWindow(manager, w->m_Xid, 503, 327);
    XSync(manager, False);
    dmPlatform::SetWindowSize(w, 481, 311);
    XWindowAttributes geometry;
    ASSERT_TRUE(XGetWindowAttributes(manager, w->m_Xid, &geometry));
    EXPECT_EQ(481, geometry.width);
    EXPECT_EQ(311, geometry.height);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(3u, events.m_Count);
    EXPECT_EQ(481u, events.m_Width);
    EXPECT_EQ(311u, events.m_Height);

    // A different pending request must not strand a request for the size the server already has.
    for (int later_resize = 0; later_resize <= 1; ++later_resize)
    {
        events.m_Count = 0;
        dmPlatform::SetWindowSize(w, 492, 319);
        XResizeWindow(manager, w->m_Xid, 503, 327);
        XSync(manager, False);
        dmPlatform::SetWindowSize(w, 503, 327);
        dmPlatform::SetWindowSize(w, 503, 327);
        if (later_resize)
        {
            dmPlatform::SetWindowSize(w, 510, 330);
        }

        dmPlatform::PollEvents(w);
        EXPECT_EQ(4u + later_resize, events.m_Count);
        EXPECT_EQ(492u, events.m_FirstWidth);
        EXPECT_EQ(319u, events.m_FirstHeight);
        const uint32_t width = later_resize ? 510 : 503, height = later_resize ? 330 : 327;
        EXPECT_EQ(width, events.m_Width);
        EXPECT_EQ(height, events.m_Height);
        dmPlatform::SetWindowSize(w, width, height);
        EXPECT_EQ(5u + later_resize, events.m_Count);
    }

    XCloseDisplay(manager);
    dmPlatform::DeleteWindow(w);
}

struct X11CloseOnResize
{
    HWindow             m_Window;
    WindowCreateParams* m_Replacement;
    uint32_t            m_Count;
};

static void CloseX11WindowOnResize(void* data, uint32_t, uint32_t)
{
    X11CloseOnResize* action = (X11CloseOnResize*)data;
    ++action->m_Count;
    dmPlatform::CloseWindow(action->m_Window);
    if (action->m_Replacement)
    {
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(action->m_Window, *action->m_Replacement));
    }
}

// Coalesced resize acknowledgements end when the original X11 window closes, even if its handle is reused.
TEST(LinuxWindow, X11CloseDuringCoalescedResize)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    X11ResizeEvents replacement_events = {};
    WindowCreateParams replacement = p;
    replacement.m_ResizeCallback = OnX11Resize;
    replacement.m_ResizeCallbackUserData = &replacement_events;
    for (int reopen = 0; reopen <= 1; ++reopen)
    {
        X11CloseOnResize action = { w, reopen ? &replacement : 0, 0 };
        p.m_ResizeCallback = CloseX11WindowOnResize;
        p.m_ResizeCallbackUserData = &action;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        XSetWindowAttributes attributes = {};
        attributes.override_redirect = True;
        XChangeWindowAttributes(w->m_Display, w->m_Xid, CWOverrideRedirect, &attributes);
        dmPlatform::ShowWindow(w);
        XSync(w->m_Display, False);
        dmPlatform::PollEvents(w);
        ASSERT_EQ(0u, action.m_Count);

        dmPlatform::SetWindowSize(w, 481, 311);
        dmPlatform::SetWindowSize(w, 481, 311);
        XSync(w->m_Display, False);
        dmPlatform::PollEvents(w);
        EXPECT_EQ(1u, action.m_Count);
        EXPECT_EQ(0u, replacement_events.m_Count);
        EXPECT_EQ(reopen != 0, w->m_Open);
        dmPlatform::CloseWindow(w);
    }

    dmPlatform::DeleteWindow(w);
}

static Window RedirectX11WindowResizes(Display* manager, HWindow w)
{
    Window parent = XCreateSimpleWindow(manager, DefaultRootWindow(manager), 0, 0, 640, 480, 0, 0, 0);
    XSetWindowAttributes attributes = {};
    attributes.override_redirect = True;
    XChangeWindowAttributes(manager, parent, CWOverrideRedirect, &attributes);
    XMapWindow(manager, parent);
    XReparentWindow(manager, w->m_Xid, parent, 0, 0);
    XSync(manager, False);
    dmPlatform::ShowWindow(w);
    XSync(w->m_Display, False);
    XSelectInput(manager, parent, SubstructureRedirectMask);
    XSync(manager, False);
    dmPlatform::PollEvents(w);
    return parent;
}

// A manager can resize the window independently before processing the application's earlier request.
TEST(LinuxWindow, X11LatestResizeReachesManager)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    X11ResizeEvents events = {};
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    p.m_ResizeCallback = OnX11Resize;
    p.m_ResizeCallbackUserData = &events;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    Display* manager = XOpenDisplay(0);
    ASSERT_TRUE(manager);
    Window parent = RedirectX11WindowResizes(manager, w);
    for (int poll_first = 0; poll_first <= 1; ++poll_first)
    {
        dmPlatform::SetWindowSize(w, 481, 311);
        XEvent first;
        ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &first));
        ASSERT_EQ(481, first.xconfigurerequest.width);

        const int width = 503 + poll_first, height = 327 + poll_first;
        XResizeWindow(manager, w->m_Xid, width, height);
        XSync(manager, False);
        if (poll_first)
        {
            dmPlatform::PollEvents(w);
        }

        dmPlatform::SetWindowSize(w, width, height);
        dmPlatform::SetWindowSize(w, width, height);
        XSync(w->m_Display, False);
        XEvent last;
        ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &last));
        EXPECT_EQ(width, last.xconfigurerequest.width);
        EXPECT_EQ(height, last.xconfigurerequest.height);
        XEvent extra;
        EXPECT_FALSE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &extra));
        XResizeWindow(manager, w->m_Xid, first.xconfigurerequest.width, first.xconfigurerequest.height);
        XResizeWindow(manager, w->m_Xid, last.xconfigurerequest.width, last.xconfigurerequest.height);
        XSync(manager, False);
        dmPlatform::PollEvents(w);
        EXPECT_EQ((uint32_t)width, events.m_Width);
        EXPECT_EQ((uint32_t)height, events.m_Height);
        EXPECT_EQ((uint32_t)width, dmPlatform::GetWindowWidth(w));
        EXPECT_EQ((uint32_t)height, dmPlatform::GetWindowHeight(w));
    }

    dmPlatform::DeleteWindow(w);
    XDestroyWindow(manager, parent);
    XCloseDisplay(manager);
}

// Each explicit resize must report the dimensions the manager accepts, including when it declines a request.
TEST(LinuxWindow, X11ConstrainedResize)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    X11ResizeEvents events = {};
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    p.m_ResizeCallback = OnX11Resize;
    p.m_ResizeCallbackUserData = &events;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmPlatform::SetWindowSize(w, 375, 255);
    EXPECT_EQ(375u, dmPlatform::GetWindowWidth(w));
    EXPECT_EQ(255u, dmPlatform::GetWindowHeight(w));
    Display* manager = XOpenDisplay(0);
    ASSERT_TRUE(manager);
    Window parent = RedirectX11WindowResizes(manager, w);
    events.m_Count = 0;
    g_ResizeWaits = 0;
    g_ResizeFd = ConnectionNumber(w->m_Display);
    dmPlatform::SetWindowSize(w, 427, 311);
    g_ResizeFd = -1;
    EXPECT_EQ(0u, g_ResizeWaits);
    EXPECT_EQ(375u, dmPlatform::GetWindowWidth(w));
    EXPECT_EQ(255u, dmPlatform::GetWindowHeight(w));
    XSync(w->m_Display, False);
    XEvent request;
    ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &request));
    EXPECT_EQ(427, request.xconfigurerequest.width);
    XResizeWindow(manager, w->m_Xid, 420, 300);
    XSync(manager, False);
    XSync(w->m_Display, False);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(420u, dmPlatform::GetWindowWidth(w));
    EXPECT_EQ(300u, dmPlatform::GetWindowHeight(w));
    EXPECT_EQ(1u, events.m_Count);
    EXPECT_EQ(420u, events.m_Width);
    EXPECT_EQ(300u, events.m_Height);

    dmPlatform::SetWindowSize(w, 420, 300);
    EXPECT_EQ(2u, events.m_Count);
    XSync(w->m_Display, False);
    ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &request));
    EXPECT_EQ(420, request.xconfigurerequest.width);
    dmPlatform::SetWindowSize(w, 450, 320);
    dmPlatform::SetWindowSize(w, 450, 320);
    dmPlatform::SetWindowSize(w, 460, 320);
    EXPECT_EQ(2u, events.m_Count);
    XSync(w->m_Display, False);
    XEvent reply = {};
    reply.xconfigure.type = ConfigureNotify;
    reply.xconfigure.event = reply.xconfigure.window = w->m_Xid;
    reply.xconfigure.width = 420;
    reply.xconfigure.height = 300;
    for (int width = 450; width <= 460; width += 10)
    {
        ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &request));
        EXPECT_EQ(width, request.xconfigurerequest.width);
        ASSERT_TRUE(XSendEvent(manager, w->m_Xid, False, StructureNotifyMask, &reply));
    }

    XSync(manager, False);
    XSync(w->m_Display, False);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(5u, events.m_Count);
    EXPECT_EQ(420u, events.m_Width);
    EXPECT_EQ(300u, events.m_Height);

    // A later position-only notification must not acknowledge any of those requests again.
    ASSERT_TRUE(XSendEvent(manager, w->m_Xid, False, StructureNotifyMask, &reply));
    XSync(manager, False);
    XSync(w->m_Display, False);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(5u, events.m_Count);
    // A synchronous acknowledgement supersedes older sizes, while later external changes still apply.
    XResizeWindow(manager, w->m_Xid, 448, 329);
    XResizeWindow(manager, w->m_Xid, 450, 320);
    XSync(manager, False);
    dmPlatform::SetWindowSize(w, 450, 320);
    EXPECT_EQ(6u, events.m_Count);
    EXPECT_EQ(450u, events.m_Width);
    EXPECT_EQ(320u, events.m_Height);
    XResizeWindow(manager, w->m_Xid, 455, 325);
    XSync(manager, False);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(7u, events.m_Count);
    EXPECT_EQ(455u, events.m_Width);
    EXPECT_EQ(325u, events.m_Height);
    dmPlatform::SetWindowSize(w, 455, 325);
    EXPECT_EQ(8u, events.m_Count);
    XSync(w->m_Display, False);
    ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &request));
    EXPECT_EQ(450, request.xconfigurerequest.width);
    ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &request));
    EXPECT_EQ(455, request.xconfigurerequest.width);
    EXPECT_FALSE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &request));

    // A manager that has only answered the first request still needs the final size request.
    dmPlatform::SetWindowSize(w, 475, 312);
    dmPlatform::SetWindowSize(w, 478, 315);
    XResizeWindow(manager, w->m_Xid, 465, 310);
    XSync(manager, False);
    dmPlatform::SetWindowSize(w, 465, 310);
    XSync(w->m_Display, False);
    const int requested_widths[] = { 475, 478, 465 };
    for (int width : requested_widths)
    {
        ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &request));
        EXPECT_EQ(width, request.xconfigurerequest.width);
    }

    XResizeWindow(manager, w->m_Xid, 478, 315);
    XResizeWindow(manager, w->m_Xid, 465, 310);
    XSync(manager, False);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(11u, events.m_Count);
    EXPECT_EQ(465u, events.m_Width);
    EXPECT_EQ(310u, events.m_Height);

    // Deleting from a synchronous acknowledgement releases the display once SetWindowSize returns.
    const int fd = ConnectionNumber(w->m_Display);
    events.m_DeleteOnResize = &w;
    XResizeWindow(manager, w->m_Xid, 470, 310);
    XSync(manager, False);
    dmPlatform::SetWindowSize(w, 470, 310);
    EXPECT_EQ(12u, events.m_Count);
    EXPECT_EQ((HWindow)0, w);
    EXPECT_EQ(-1, fcntl(fd, F_GETFD));
    XDestroyWindow(manager, parent);
    XCloseDisplay(manager);
}

// Both Linux backends must report held input to the focus-loss callback and release it before returning.
TEST(LinuxWindow, FocusLossReportsPressedInput)
{
    HWindow w = dmPlatform::NewWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    dmWindowTest::FocusLossInput observed = { w, 0, 0, 0 };
    p.m_FocusCallbackUserData = &observed;
    p.m_FocusCallback = dmWindowTest::FocusLossInput::Record;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmPlatform::LinuxFocus(w, true);
    w->m_Keys['A'] = w->m_Buttons[0] = 1;
    dmPlatform::LinuxFocus(w, false);
    EXPECT_EQ(1, observed.m_Losses);
    EXPECT_EQ(1, observed.m_Key);
    EXPECT_EQ(1, observed.m_Button);
    EXPECT_EQ(0, dmPlatform::GetKey(w, 'A'));
    EXPECT_EQ(0, dmPlatform::GetMouseButton(w, 0));
    dmPlatform::DeleteWindow(w);
}

// Verifies physical key identities are independent of the active keyboard layout.
TEST(LinuxWindow, PhysicalKeys)
{
    ASSERT_EQ('Q', dmPlatform::LinuxKeyCode(KEY_Q));
    ASSERT_EQ(340, dmPlatform::LinuxKeyCode(KEY_LEFTSHIFT));
    ASSERT_EQ(344, dmPlatform::LinuxKeyCode(KEY_RIGHTSHIFT));
    ASSERT_EQ(-1, dmPlatform::LinuxKeyCode(KEY_RESERVED));
}

// Preserves the GUID, normalized axes, button ordering and diagonal hats used by existing gamepad maps.
TEST(LinuxWindow, EvdevMapping)
{
    input_id id = { BUS_USB, 0x045e, 0x028e, 0x0114 };
    char guid[33];
    dmPlatform::EvdevGuid(id, "Xbox 360", guid);
    ASSERT_STREQ("030000005e0400008e02000014010000", guid);
    uint8_t keys[(KEY_CNT + 7) / 8] = {}, abs[(ABS_CNT + 7) / 8] = {};
    keys[BTN_A / 8] |= 1 << (BTN_A % 8);
    abs[ABS_X / 8] |= 1 << (ABS_X % 8);
    abs[ABS_HAT0X / 8] |= 3 << (ABS_HAT0X % 8);
    dmPlatform::EvdevPacket p;
    dmPlatform::InitEvdevPacket(&p, keys, abs);
    p.m_Ranges[ABS_X].minimum = -100;
    p.m_Ranges[ABS_X].maximum = 100;
    dmPlatform::ApplyEvdevEvent(&p, EV_ABS, ABS_X, 50);
    dmPlatform::ApplyEvdevEvent(&p, EV_KEY, BTN_A, 1);
    dmPlatform::ApplyEvdevEvent(&p, EV_ABS, ABS_HAT0X, 1);
    dmPlatform::ApplyEvdevEvent(&p, EV_ABS, ABS_HAT0Y, -1);
    ASSERT_EQ(1u, p.m_AxisCount);
    ASSERT_EQ(1u, p.m_ButtonCount);
    ASSERT_EQ(1u, p.m_HatCount);
    ASSERT_EQ(0.5f, p.m_Axes[0]);
    ASSERT_EQ(3u, p.m_Hats[0]);
    ASSERT_EQ(1u, p.m_Buttons[0]);
    ASSERT_EQ(1u, p.m_Buttons[1]);
    ASSERT_EQ(1u, p.m_Buttons[2]);
    dmPlatform::ApplyEvdevEvent(&p, EV_ABS, ABS_HAT0Y, 0);
    ASSERT_EQ(2u, p.m_Hats[0]);
    ASSERT_EQ(0u, p.m_Buttons[1]);
    dmPlatform::ApplyEvdevEvent(&p, EV_KEY, BTN_A, 0);
    ASSERT_EQ(0u, p.m_Buttons[0]);
}

// Xvfb exposes one named RandR output. Without the library, Xlib supplies the default screen.
TEST(LinuxWindow, X11RandRMonitors)
{
    const char* expected = getenv("DM_TEST_X11_MONITOR");
    if (!expected)
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    ASSERT_TRUE(w->m_X11);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmArray<dmWindowState::Monitor> monitors;
    dmPlatform::X11Monitors(w, monitors);
    dmPlatform::DeleteWindow(w);

    ASSERT_EQ(1u, monitors.Size());
    ASSERT_EQ(dmHashString64(expected), monitors[0].m_NameHash);
    ASSERT_GT(monitors[0].m_WorkArea.m_Width, 0);
    ASSERT_GT(monitors[0].m_WorkArea.m_Height, 0);
}

// A bare X server must focus a mapped game on request and preserve the current focus otherwise.
TEST(LinuxWindow, X11FocusWithoutWindowManager)
{
    if (!getenv("DM_TEST_X11_NO_WM"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w && w->m_X11);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    for (int focus = 0; focus <= 1; ++focus)
    {
        p.m_FocusOnShow = focus;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        Window before, after;
        int revert;
        XGetInputFocus(w->m_Display, &before, &revert);
        dmPlatform::ShowWindow(w);
        XGetInputFocus(w->m_Display, &after, &revert);
        EXPECT_EQ(focus ? w->m_Xid : before, after);
        dmPlatform::CloseWindow(w);
    }

    dmPlatform::DeleteWindow(w);
}

// Fullscreen placement and Motif's borderless hint must both be available before the window is mapped.
TEST(LinuxWindow, X11FullscreenHints)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    p.m_Fullscreen = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    XSizeHints hints = {};
    long supplied;
    ASSERT_TRUE(XGetWMNormalHints(w->m_Display, w->m_Xid, &hints, &supplied));
    EXPECT_TRUE(hints.flags & PPosition);

    Atom motif = XInternAtom(w->m_Display, "_MOTIF_WM_HINTS", False);
    Atom type;
    int format;
    unsigned long count, remaining;
    unsigned char* bytes = 0;
    ASSERT_EQ(Success, XGetWindowProperty(w->m_Display, w->m_Xid, motif, 0, 5, False, motif, &type, &format, &count, &remaining, &bytes));
    ASSERT_EQ(motif, type);
    ASSERT_EQ(32, format);
    ASSERT_EQ(5ul, count);
    const unsigned long* fields = (const unsigned long*)bytes;
    EXPECT_TRUE(fields[0] & (1ul << 1)); // MWM_HINTS_DECORATIONS
    EXPECT_EQ(0ul, fields[2]); // No decorations.
    XFree(bytes);
    dmPlatform::DeleteWindow(w);
}

// Window managers and task switchers must be able to decode both the initial and updated X11 titles.
TEST(LinuxWindow, X11WindowAndIconTitles)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    p.m_Title = "Étoiles 星";
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    const char* titles[] = { p.m_Title, "Café", "" };
    const char* names[] = { "WM_NAME", "WM_ICON_NAME", "_NET_WM_NAME", "_NET_WM_ICON_NAME" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(titles); ++i)
    {
        if (i)
        {
            dmPlatform::SetWindowTitle(w, titles[i]);
        }

        for (uint32_t j = 0; j < DM_ARRAY_SIZE(names); ++j)
        {
            XTextProperty property = {};
            ASSERT_TRUE(XGetTextProperty(w->m_Display, w->m_Xid, &property, XInternAtom(w->m_Display, names[j], False)));
            if (j >= 2)
            {
                EXPECT_EQ(XInternAtom(w->m_Display, "UTF8_STRING", False), property.encoding);
            }

            if (*titles[i])
            {
                char** strings;
                int count;
                ASSERT_EQ(Success, Xutf8TextPropertyToTextList(w->m_Display, &property, &strings, &count));
                ASSERT_EQ(1, count);
                EXPECT_STREQ(titles[i], strings[0]);
                XFreeStringList(strings);
            }
            else
            {
                EXPECT_EQ(0ul, property.nitems);
            }

            XFree(property.value);
        }
    }

    dmPlatform::DeleteWindow(w);
}

static const pthread_t g_TestThread = pthread_self();
static unsigned g_SynchronousHostnameLookups;
extern "C" int __real_getaddrinfo(const char*, const char*, const addrinfo*, addrinfo**);
extern "C" int __wrap_getaddrinfo(const char* node, const char* service, const addrinfo* hints, addrinfo** result)
{
    if (hints && (hints->ai_flags & AI_CANONNAME) && pthread_equal(pthread_self(), g_TestThread))
    {
        ++g_SynchronousHostnameLookups;
    }

    return __real_getaddrinfo(node, service, hints, result);
}

// A forwarded X11 window identifies its process and host immediately without resolving DNS on the event thread.
TEST(LinuxWindow, X11ClientProcessIdentity)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    Atom type;
    int format;
    unsigned long count, remaining;
    unsigned char* value;
    Atom pid = XInternAtom(w->m_Display, "_NET_WM_PID", False);
    ASSERT_EQ(Success, XGetWindowProperty(w->m_Display, w->m_Xid, pid, 0, 1, False, XA_CARDINAL, &type, &format, &count, &remaining, &value));
    ASSERT_EQ(XA_CARDINAL, type);
    ASSERT_EQ(32, format);
    ASSERT_EQ(1ul, count);
    EXPECT_EQ((unsigned long)getpid(), *(unsigned long*)value);
    XFree(value);

    XTextProperty machine = {};
    ASSERT_TRUE(XGetWMClientMachine(w->m_Display, w->m_Xid, &machine));
    EXPECT_EQ(0u, g_SynchronousHostnameLookups);
    EXPECT_EQ(XA_STRING, machine.encoding);
    EXPECT_EQ(8, machine.format);
    char hostname[HOST_NAME_MAX + 1];
    ASSERT_EQ(0, gethostname(hostname, sizeof(hostname)));
    addrinfo hints = {};
    hints.ai_flags = AI_CANONNAME;
    addrinfo* address = 0;
    const char* expected = hostname;
    if (!strchr(hostname, '.') && __real_getaddrinfo(hostname, 0, &hints, &address) == 0 &&
        address->ai_canonname && strchr(address->ai_canonname, '.'))
    {
        expected = address->ai_canonname;
    }

    EXPECT_TRUE(!strcmp(hostname, (const char*)machine.value) || !strcmp(expected, (const char*)machine.value));
    XFree(machine.value);
    // Wait for the qualified name when the resolver supplies one.
    if (expected != hostname)
    {
        uint64_t end = dmTime::GetMonotonicTime() + 5000000;
        for (;;)
        {
            dmPlatform::PollEvents(w);
            ASSERT_TRUE(XGetWMClientMachine(w->m_Display, w->m_Xid, &machine));
            bool resolved = !strcmp(expected, (const char*)machine.value);
            XFree(machine.value);
            if (resolved || dmTime::GetMonotonicTime() >= end)
            {
                EXPECT_TRUE(resolved);
                break;
            }
        }
    }

    if (address)
    {
        freeaddrinfo(address);
    }

    dmPlatform::DeleteWindow(w);
}

// Window managers match game launchers by the initial title and the optional X resource instance.
TEST(LinuxWindow, X11WindowClass)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    XClassHint hint = {};
    ASSERT_TRUE(XGetClassHint(w->m_Display, w->m_Xid, &hint));
    const char* resource = getenv("RESOURCE_NAME");
    EXPECT_STREQ(resource && *resource ? resource : p.m_Title, hint.res_name);
    EXPECT_STREQ(p.m_Title, hint.res_class);
    XFree(hint.res_name);
    XFree(hint.res_class);
    dmPlatform::DeleteWindow(w);
}

// A slow reader and an interrupted wait must still receive every byte of a Wayland clipboard offer.
TEST(LinuxWindow, WaylandClipboardBackpressure)
{
    int fds[2];
    ASSERT_EQ(0, pipe(fds));
    ASSERT_EQ(0, fcntl(fds[1], F_SETFL, O_NONBLOCK));
    const int capacity = fcntl(fds[1], F_GETPIPE_SZ);
    ASSERT_GT(capacity, 0);
    FILE* reader = fdopen(fds[0], "rb");
    ASSERT_NE((FILE*)0, reader);

    dmWindow window = {};
    LinuxWayland wl = {};
    window.m_Wayland = &wl;
    wl.m_Clipboard.EnsureSize(2 * capacity + 1);
    for (uint32_t i = 0; i < wl.m_Clipboard.Size(); ++i)
    {
        wl.m_Clipboard[i] = (char)(i % 127);
    }

    g_ClipboardWriteWaits = 0;
    g_ClipboardWriteFd = fds[1];
    dmPlatform::g_WaylandDataSourceListener.send(&window, 0, "text/plain", fds[1]);
    dmArray<char> received;
    received.EnsureSize(wl.m_Clipboard.Size() + 1);
    const size_t size = fread(received.Begin(), 1, received.Size(), reader);
    fclose(reader);
    g_ClipboardWriteFd = -1;
    ASSERT_EQ((size_t)wl.m_Clipboard.Size(), size);
    EXPECT_EQ(0, memcmp(wl.m_Clipboard.Begin(), received.Begin(), size));
}

struct TestClipboardManager
{
    Display* m_Display;
    Window   m_Window;
};

// Advance the owner's handoff clock at protocol boundaries while the manager waits on real X11 replies.
static thread_local bool g_UseClipboardClock;
static std::atomic<unsigned> g_ClipboardDelayMs(0);
extern "C" int __real_clock_gettime(clockid_t, struct timespec*);
extern "C" int __wrap_clock_gettime(clockid_t clock, struct timespec* time)
{
    int result = __real_clock_gettime(clock, time);
    if (result == 0 && clock == CLOCK_MONOTONIC && g_UseClipboardClock)
    {
        const uint64_t ns = time->tv_nsec + 1000000ULL * g_ClipboardDelayMs.load();
        time->tv_sec += ns / 1000000000;
        time->tv_nsec = ns % 1000000000;
    }

    return result;
}

static bool WaitForSelectionEvent(Display* display, int type, XEvent* event)
{
    const uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
    uint64_t now;
    while ((now = dmTime::GetMonotonicTime()) < deadline)
    {
        if (XCheckTypedEvent(display, type, event))
        {
            return true;
        }

        pollfd fd = { ConnectionNumber(display), POLLIN, 0 };
        poll(&fd, 1, (int)((deadline - now) / 1000) + 1);
    }

    return false;
}

static void* SaveClipboardSelection(void* data)
{
    TestClipboardManager* manager = (TestClipboardManager*)data;
    Display* d = manager->m_Display;
    Window window = manager->m_Window;
    XEvent request;
    if (!WaitForSelectionEvent(d, SelectionRequest, &request) || request.xselectionrequest.target != XInternAtom(d, "SAVE_TARGETS", False))
    {
        return 0;
    }

    Atom clipboard = XInternAtom(d, "CLIPBOARD", False);
    Atom multiple = XInternAtom(d, "MULTIPLE", False);
    Atom utf8 = XInternAtom(d, "UTF8_STRING", False);
    Atom targets[] = { XInternAtom(d, "TARGETS", False), XInternAtom(d, "SAVE_TARGETS", False), XA_STRING };
    XEvent converted;
    for (unsigned i = 0; i < DM_ARRAY_SIZE(targets); ++i)
    {
        g_ClipboardDelayMs += 600;
        XConvertSelection(d, clipboard, targets[i], targets[i], window, CurrentTime);
        if (!WaitForSelectionEvent(d, SelectionNotify, &converted) || converted.xselection.property != targets[i])
        {
            return 0;
        }
    }

    g_ClipboardDelayMs += 600;
    Atom pairs[] = { utf8, utf8, XA_STRING, XA_STRING };
    XChangeProperty(d, window, multiple, XInternAtom(d, "ATOM_PAIR", False), 32, PropModeReplace, (unsigned char*)pairs, 4);
    XConvertSelection(d, clipboard, multiple, multiple, window, CurrentTime);
    if (!WaitForSelectionEvent(d, SelectionNotify, &converted) || converted.xselection.property != multiple)
    {
        return 0;
    }

    XSetSelectionOwner(d, clipboard, window, CurrentTime);
    XEvent reply = {};
    reply.xselection.type = SelectionNotify;
    reply.xselection.requestor = request.xselectionrequest.requestor;
    reply.xselection.selection = request.xselectionrequest.selection;
    reply.xselection.target = request.xselectionrequest.target;
    reply.xselection.time = request.xselectionrequest.time;
    reply.xselection.property = request.xselectionrequest.property;
    XSendEvent(d, reply.xselection.requestor, False, 0, &reply);
    XFlush(d);
    return data;
}

// A separate X11 client can paste while the game window is closed and after it reopens without a clipboard manager.
TEST(LinuxWindow, X11ClipboardSurvivesCloseAndReopen)
{
    if (!getenv("DM_TEST_X11_REMAP_KEYS"))
    {
        SKIP();
    }

    HWindow w = NewX11TestWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(w, &p));
    Display* client = XOpenDisplay(0);
    ASSERT_NE((Display*)0, client);
    ASSERT_EQ((Window)None, XGetSelectionOwner(client, XInternAtom(client, "CLIPBOARD_MANAGER", False)));
    Window requestor = XCreateSimpleWindow(client, DefaultRootWindow(client), 0, 0, 1, 1, 0, 0, 0);
    Atom clipboard = XInternAtom(client, "CLIPBOARD", False);
    Atom utf8 = XInternAtom(client, "UTF8_STRING", False);
    const char text[] = "Defold clipboard";
    dmPlatform::SetLinuxClipboard(w, text, sizeof(text) - 1);
    WindowClose(w);

    for (int reopen = 0; reopen <= 1; ++reopen)
    {
        if (reopen)
        {
            ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(w, &p));
        }

        XConvertSelection(client, clipboard, utf8, utf8, requestor, CurrentTime);
        XSync(client, False);
        WindowPollEvents(w);
        XEvent reply;
        ASSERT_TRUE(WaitForSelectionEvent(client, SelectionNotify, &reply));
        ASSERT_EQ(utf8, reply.xselection.property);
        Atom type;
        int format;
        unsigned long size, left;
        unsigned char* bytes = 0;
        ASSERT_EQ(Success, XGetWindowProperty(client, requestor, utf8, 0, sizeof(text), True, utf8, &type, &format, &size, &left, &bytes));
        EXPECT_TRUE(type == utf8 && format == 8 && size == sizeof(text) - 1 && !left && !memcmp(bytes, text, size));
        XFree(bytes);
    }

    WindowDelete(w);
    EXPECT_EQ((Window)None, XGetSelectionOwner(client, clipboard));
    XCloseDisplay(client);
}

// The clipboard owner advertises and acknowledges SAVE_TARGETS and keeps serving text throughout a slow handoff.
TEST(LinuxWindow, X11ClipboardManagerKeepsText)
{
    // Only claim the manager selection on the harness's private X server.
    if (!getenv("DM_TEST_X11_REMAP_KEYS"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    const long core_words = XMaxRequestSize(w->m_Display);
    // Allow seven header words and one payload word beyond the core limit.
    const uint32_t text_size = XExtendedMaxRequestSize(w->m_Display) >= core_words + 8 ? core_words * 4 + 1 : 32;
    dmArray<char> text;
    text.SetCapacity(text_size);
    text.SetSize(text_size);
    for (unsigned i = 0; i < text.Size(); ++i)
    {
        text[i] = 'a' + i % 26;
    }

    dmPlatform::SetLinuxClipboard(w, text.Begin(), text.Size());
    Display* d = XOpenDisplay(0);
    ASSERT_TRUE(d);
    TestClipboardManager manager = { d, XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 1, 1, 0, 0, 0) };
    Atom selection = XInternAtom(d, "CLIPBOARD_MANAGER", False);
    XSetSelectionOwner(d, selection, manager.m_Window, CurrentTime);
    ASSERT_EQ(manager.m_Window, XGetSelectionOwner(d, selection));
    g_ClipboardDelayMs = 0;
    pthread_t worker;
    ASSERT_EQ(0, pthread_create(&worker, 0, SaveClipboardSelection, &manager));
    g_UseClipboardClock = true;
    dmPlatform::DeleteWindow(w);
    g_UseClipboardClock = false;
    void* saved = 0;
    ASSERT_EQ(0, pthread_join(worker, &saved));
    EXPECT_TRUE(saved == &manager);
    EXPECT_EQ(manager.m_Window, XGetSelectionOwner(d, XInternAtom(d, "CLIPBOARD", False)));

    Atom marker = XInternAtom(d, "SAVE_TARGETS", False);
    Atom type;
    int format;
    unsigned long size, left;
    unsigned char* bytes = 0;
    ASSERT_EQ(Success, XGetWindowProperty(d, manager.m_Window, XInternAtom(d, "TARGETS", False), 0, 256, True, XA_ATOM, &type, &format, &size, &left, &bytes));
    ASSERT_EQ(XA_ATOM, type);
    ASSERT_EQ(32, format);
    bool advertised = false;
    for (unsigned long i = 0; i < size; ++i)
    {
        advertised |= ((Atom*)bytes)[i] == marker;
    }

    XFree(bytes);
    EXPECT_TRUE(advertised);
    ASSERT_EQ(Success, XGetWindowProperty(d, manager.m_Window, marker, 0, 0, True, AnyPropertyType, &type, &format, &size, &left, &bytes));
    EXPECT_EQ(XInternAtom(d, "NULL", False), type);
    EXPECT_EQ(32, format);
    EXPECT_EQ(0ul, size);
    EXPECT_EQ(0ul, left);
    XFree(bytes);

    Atom formats[] = { XInternAtom(d, "UTF8_STRING", False), XA_STRING };
    for (unsigned i = 0; i < 2; ++i)
    {
        EXPECT_EQ(Success, XGetWindowProperty(d, manager.m_Window, formats[i], 0, (text.Size() + 3) / 4, False, formats[i], &type, &format, &size, &left, &bytes));
        EXPECT_TRUE(type == formats[i] && format == 8 && size == text.Size() && !left && !memcmp(bytes, text.Begin(), size));
        XFree(bytes);
    }

    XCloseDisplay(d);
}

// ICCCM-only window managers signal minimize and restore through WM_STATE.
TEST(LinuxWindow, X11IconicState)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    Atom wm_state = XInternAtom(w->m_Display, "WM_STATE", False);
    const long states[] = { IconicState, NormalState };
    for (unsigned i = 0; i < 2; ++i)
    {
        long property[] = { states[i], None };
        XChangeProperty(w->m_Display, w->m_Xid, wm_state, wm_state, 32, PropModeReplace, (unsigned char*)property, 2);
        XSync(w->m_Display, False);
        dmPlatform::PollEvents(w);
        EXPECT_EQ((uint32_t)(states[i] == IconicState), dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ICONIFIED));
    }

    Atom net_state = XInternAtom(w->m_Display, "_NET_WM_STATE", False);
    Atom hidden = XInternAtom(w->m_Display, "_NET_WM_STATE_HIDDEN", False);
    XChangeProperty(w->m_Display, w->m_Xid, net_state, XA_ATOM, 32, PropModeReplace, (unsigned char*)&hidden, 1);
    XSync(w->m_Display, False);
    dmPlatform::PollEvents(w);
    EXPECT_TRUE(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ICONIFIED));
    XDeleteProperty(w->m_Display, w->m_Xid, net_state);
    XSync(w->m_Display, False);
    dmPlatform::PollEvents(w);
    EXPECT_FALSE(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ICONIFIED));
    dmPlatform::DeleteWindow(w);
}

// Live XKB remapping must recognize physical keys and recover modifiers and keypad keys from their symbols.
TEST(LinuxWindow, X11PhysicalKeymapChanges)
{
    if (!getenv("DM_TEST_X11_REMAP_KEYS"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    Display* display = w->m_Display;
    KeyCode q = XKeysymToKeycode(display, XK_q), other = XKeysymToKeycode(display, XK_w);
    KeyCode level5 = XKeysymToKeycode(display, XK_e);
    ASSERT_NE(0, q);
    ASSERT_NE(0, other);
    ASSERT_NE(0, level5);
    XkbDescPtr keyboard = XkbGetMap(display, 0, XkbUseCoreKbd);
    ASSERT_TRUE(keyboard);
    ASSERT_EQ(Success, XkbGetNames(display, XkbKeyNamesMask, keyboard));
    int symbol_count;
    KeySym* original_symbols = XGetKeyboardMapping(display, level5, 1, &symbol_count);
    ASSERT_TRUE(original_symbols);
    KeySym level5_symbol = XK_ISO_Level5_Shift;
    XChangeKeyboardMapping(display, level5, 1, &level5_symbol, 1);
    XkbKeyNameRec* names = keyboard->names->keys;
    XkbKeyNameRec original = names[q], original_other = names[other], original_level5 = names[level5];
    names[q] = names[other];
    // One key has only the level-five physical name. The other has only the level-five keysym.
    memcpy(names[other].name, "LVL5", XkbKeyNameLength);
    memcpy(names[level5].name, "TST5", XkbKeyNameLength);
    ASSERT_TRUE(XkbSetNames(display, XkbKeyNamesMask, 0, 0, keyboard));
    XSync(display, False);
    dmPlatform::PollEvents(w);
    XEvent key = {};
    key.xkey.type = KeyPress;
    key.xkey.display = display;
    key.xkey.window = w->m_Xid;
    key.xkey.root = DefaultRootWindow(display);
    key.xkey.keycode = q;
    key.xkey.same_screen = True;
    XPutBackEvent(display, &key);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(1, dmPlatform::GetKey(w, 'W'));
    EXPECT_EQ(0, dmPlatform::GetKey(w, 'Q'));

    const KeyCode alt_keys[] = { other, level5 };
    const int right_alt = dmPlatform::LinuxKeyCode(KEY_RIGHTALT);
    for (KeyCode code : alt_keys)
    {
        key.xkey.keycode = code;
        key.type = KeyPress;
        XPutBackEvent(display, &key);
        dmPlatform::PollEvents(w);
        EXPECT_EQ(1, dmPlatform::GetKey(w, right_alt));
        key.type = KeyRelease;
        XPutBackEvent(display, &key);
        dmPlatform::PollEvents(w);
        EXPECT_EQ(0, dmPlatform::GetKey(w, right_alt));
    }

    // The unknown physical name now exposes only the Num Lock-off symbol.
    KeySym keypad[] = { XK_KP_Insert, XK_KP_End, XK_KP_Down, XK_KP_Page_Down, XK_KP_Left, XK_KP_Begin,
                        XK_KP_Right, XK_KP_Home, XK_KP_Up, XK_KP_Page_Up, XK_KP_Delete };
    key.xkey.keycode = level5;
    for (size_t i = 0; i < sizeof(keypad) / sizeof(keypad[0]); ++i)
    {
        XChangeKeyboardMapping(display, level5, 1, &keypad[i], 1);
        XSync(display, False);
        dmPlatform::PollEvents(w);
        int expected = i == 10 ? dmPlatform::PLATFORM_KEY_KP_DECIMAL : dmPlatform::PLATFORM_KEY_KP_0 + i;
        key.type = KeyPress;
        XPutBackEvent(display, &key);
        dmPlatform::PollEvents(w);
        EXPECT_EQ(1, dmPlatform::GetKey(w, expected));
        key.type = KeyRelease;
        XPutBackEvent(display, &key);
        dmPlatform::PollEvents(w);
        EXPECT_EQ(0, dmPlatform::GetKey(w, expected));
    }

    names[q] = original;
    names[other] = original_other;
    names[level5] = original_level5;
    EXPECT_TRUE(XkbSetNames(display, XkbKeyNamesMask, 0, 0, keyboard));
    XChangeKeyboardMapping(display, level5, symbol_count, original_symbols, 1);
    XSync(display, False);
    XFree(original_symbols);
    XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
    dmPlatform::DeleteWindow(w);
}

static XIDProc g_InstantiateIM;
static XPointer g_InstantiateData;
static XIC g_FocusedIC;
static unsigned g_FilterKeycode;

extern "C" Bool __real_XFilterEvent(XEvent*, Window);
extern "C" Bool __wrap_XFilterEvent(XEvent* event, Window window)
{
    if (g_FilterKeycode && (event->type == KeyPress || event->type == KeyRelease) && event->xkey.keycode == g_FilterKeycode)
    {
        event->xkey.keycode = 0;
        return True;
    }

    return __real_XFilterEvent(event, window);
}

static void Character(void* data, int character)
{
    *(int*)data = character;
}

// An XIM can clear a consumed event's keycode. Physical presses and releases must still reach the game.
TEST(LinuxWindow, X11FilteredPhysicalKey)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    if (!w->m_X11)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    int character = 0;
    dmPlatform::SetKeyboardCharCallback(w, Character, &character);
    XEvent key = {};
    key.xkey.display = w->m_Display;
    key.xkey.window = w->m_Xid;
    key.xkey.keycode = XKeysymToKeycode(w->m_Display, XK_a);
    ASSERT_NE(0u, key.xkey.keycode);
    int16_t codes[256];
    dmPlatform::X11ReadKeymap(w->m_Display, codes);
    int physical = codes[key.xkey.keycode];
    ASSERT_GE(physical, 0);
    g_FilterKeycode = key.xkey.keycode;
    key.type = KeyPress;
    XPutBackEvent(w->m_Display, &key);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(1, dmPlatform::GetKey(w, physical));
    EXPECT_EQ(0, character);
    key.type = KeyRelease;
    XPutBackEvent(w->m_Display, &key);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(0, dmPlatform::GetKey(w, physical));
    g_FilterKeycode = 0;
    dmPlatform::DeleteWindow(w);
}

extern "C" Bool __real_XRegisterIMInstantiateCallback(Display*, XrmDatabase, char*, char*, XIDProc, XPointer);
extern "C" Bool __wrap_XRegisterIMInstantiateCallback(Display* d, XrmDatabase db, char* name, char* cls, XIDProc callback, XPointer data)
{
    g_InstantiateIM = callback;
    g_InstantiateData = data;
    return __real_XRegisterIMInstantiateCallback(d, db, name, cls, callback, data);
}

extern "C" void __real_XSetICFocus(XIC);
extern "C" void __wrap_XSetICFocus(XIC ic)
{
    g_FocusedIC = ic;
    __real_XSetICFocus(ic);
}

// An input method that restarts while the game is focused must receive a new focused context.
TEST(LinuxWindow, X11InputMethodRestart)
{
    if (!getenv("DM_TEST_X11_REMAP_KEYS"))
    {
        SKIP();
    }

    g_InstantiateIM = 0;
    g_FocusedIC = 0;
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w && g_InstantiateIM);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    XEvent focus = {};
    focus.xfocus.type = FocusIn;
    focus.xfocus.window = w->m_Xid;
    focus.xfocus.display = w->m_Display;
    XPutBackEvent(w->m_Display, &focus);
    dmPlatform::PollEvents(w);
    ASSERT_TRUE(g_FocusedIC);
    XIM im = XIMOfIC(g_FocusedIC);
    XIMCallback* destroy = 0;
    ASSERT_TRUE(XGetIMValues(im, XNDestroyCallback, &destroy, NULL) == 0);
    ASSERT_TRUE(destroy && destroy->callback);
    destroy->callback(im, destroy->client_data, 0);
    XFree(destroy);
    XCloseIM(im);

    g_FocusedIC = 0;
    g_InstantiateIM(w->m_Display, g_InstantiateData, 0);
    EXPECT_TRUE(g_FocusedIC);
    dmPlatform::DeleteWindow(w);
}

// Without an input method, Xlib supplies Latin-1 bytes or a layout-specific keysym.
TEST(LinuxWindow, X11CharactersWithoutInputMethod)
{
    if (!getenv("DM_TEST_X11_REMAP_KEYS"))
    {
        SKIP();
    }

    Display* display = XOpenDisplay(0);
    ASSERT_TRUE(display);
    int first, last, count;
    XDisplayKeycodes(display, &first, &last);
    KeySym* original = XGetKeyboardMapping(display, last, 1, &count);
    ASSERT_TRUE(original);
    XKeyEvent key = {};
    key.type = KeyPress;
    key.display = display;
    key.window = key.root = DefaultRootWindow(display);
    key.keycode = last;
    key.same_screen = True;
    KeySym symbols[] = { XK_eacute, XK_Cyrillic_ya, 0x0101f600, XK_F2, XK_Return };
    unsigned expected[] = { 0xe9, 0x44f, 0x1f600, 0, 0 };
    for (unsigned i = 0; i < sizeof(symbols) / sizeof(symbols[0]); ++i)
    {
        XChangeKeyboardMapping(display, last, 1, &symbols[i], 1);
        XSync(display, False);
        XMappingEvent mapping = {};
        mapping.display = display;
        mapping.request = MappingKeyboard;
        mapping.first_keycode = last;
        mapping.count = 1;
        XRefreshKeyboardMapping(&mapping);
        EXPECT_EQ(expected[i], dmPlatform::X11LookupCharacter(&key));
    }

    XChangeKeyboardMapping(display, last, count, original, 1);
    XFree(original);
    XCloseDisplay(display);
}

static void ExpectX11MousePosition(HWindow w, int expected_x, int expected_y)
{
    XSync(w->m_Display, False);
    dmPlatform::PollEvents(w);
    int32_t x, y;
    dmPlatform::GetMousePosition(w, &x, &y);
    EXPECT_EQ(expected_x, x);
    EXPECT_EQ(expected_y, y);
}

// X11 cursor lock starts at the live pointer and preserves relative motion across batched moves and recentering.
TEST(LinuxWindow, X11LockedCursorMotion)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmPlatform::ShowWindow(w);
    Display* d = w->m_Display;
    Window window = w->m_Xid;
    // A reparenting manager can briefly unmap the client while showing its frame.
    uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
    while (!w->m_Focused && dmTime::GetMonotonicTime() < deadline)
    {
        dmPlatform::PollEvents(w);
    }

    ASSERT_TRUE(w->m_Focused);
    const int cx = w->m_Width / 2, cy = w->m_Height / 2;
    const int start_x = cx - 30, start_y = cy + 20;
    // Lock before the event loop has seen this motion.
    XWarpPointer(d, None, window, 0, 0, 0, 0, start_x, start_y);
    dmPlatform::SetDeviceState(w, WINDOW_DEVICE_STATE_CURSOR, false);
    ExpectX11MousePosition(w, start_x, start_y);
    XWarpPointer(d, None, window, 0, 0, 0, 0, cx + 4, cy - 3);
    XWarpPointer(d, None, window, 0, 0, 0, 0, cx + 11, cy - 8);
    ExpectX11MousePosition(w, start_x + 11, start_y - 8);
    ExpectX11MousePosition(w, start_x + 11, start_y - 8);
    XWarpPointer(d, None, window, 0, 0, 0, 0, cx - 6, cy + 5);
    XWarpPointer(d, None, window, 0, 0, 0, 0, cx, cy);
    ExpectX11MousePosition(w, start_x + 11, start_y - 8);

    dmPlatform::SetDeviceState(w, WINDOW_DEVICE_STATE_CURSOR, true);
    ExpectX11MousePosition(w, start_x, start_y);

    Window root, child;
    int root_x, root_y;
    XTranslateCoordinates(d, window, DefaultRootWindow(d), 0, 0, &root_x, &root_y, &child);
    const int outside_x = root_x > 0 ? -1 : w->m_Width;
    XWarpPointer(d, None, window, 0, 0, 0, 0, outside_x, cy);
    // An unlocked read outside the client must succeed without any motion event.
    int32_t live_x, live_y;
    dmPlatform::GetMousePosition(w, &live_x, &live_y);
    EXPECT_EQ(outside_x, live_x);
    EXPECT_EQ(cy, live_y);
    dmPlatform::SetDeviceState(w, WINDOW_DEVICE_STATE_CURSOR, false);
    ExpectX11MousePosition(w, outside_x, cy);
    XWarpPointer(d, None, window, 0, 0, 0, 0, cx + 7, cy + 4);
    dmPlatform::SetDeviceState(w, WINDOW_DEVICE_STATE_CURSOR, true);
    ExpectX11MousePosition(w, outside_x, cy);
    int actual_x, actual_y;
    unsigned int mask;
    ASSERT_TRUE(XQueryPointer(d, window, &root, &child, &root_x, &root_y, &actual_x, &actual_y, &mask));
    EXPECT_EQ(outside_x, actual_x);
    EXPECT_EQ(cy, actual_y);
    dmPlatform::DeleteWindow(w);
}

#ifdef EGL_PRESENT_OPAQUE_EXT
// A Wayland window must present opaque pixels while retaining the alpha channel used by shaders.
TEST(LinuxWindow, WaylandOpaqueEGLSurface)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    if (!w->m_Wayland)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_ContextAlphabits = 8;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    EGLDisplay display = eglGetCurrentDisplay();
    if (!strstr(eglQueryString(display, EGL_EXTENSIONS), "EGL_EXT_present_opaque"))
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    EGLint opaque = EGL_FALSE;
    EXPECT_TRUE(eglQuerySurface(display, eglGetCurrentSurface(EGL_DRAW), EGL_PRESENT_OPAQUE_EXT, &opaque));
    EXPECT_EQ(EGL_TRUE, opaque);
    EXPECT_GE(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ALPHA_BITS), 8u);
    dmPlatform::DeleteWindow(w);
}
#endif

// Wayland EGL drivers without opaque presentation must use an opaque buffer even when the engine requests alpha.
TEST(LinuxWindow, WaylandOpaqueEGLFallback)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    if (!w->m_Wayland)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_ContextAlphabits = 8;
    g_EGLDisplayExtensions = "";
    WindowResult result = dmPlatform::OpenWindow(w, p);
    g_EGLDisplayExtensions = 0;
    ASSERT_EQ(WINDOW_RESULT_OK, result);
    EXPECT_EQ(0u, dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ALPHA_BITS));
    EXPECT_EQ(0u, dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ACCELERATED));
    dmPlatform::DeleteWindow(w);
}

static void FrameDone(void* data, wl_callback*, uint32_t)
{
    *(bool*)data = true;
}

// The first EGL buffer must use the compositor's fullscreen geometry and present directly after WindowOpen.
TEST(LinuxWindow, WaylandFirstSwapAfterOpen)
{
    HWindow w = WindowNew();
    ASSERT_TRUE(w);
    if (!w->m_Wayland)
    {
        WindowDelete(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_Fullscreen = true;
    ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(w, &p));
    if (getenv("DM_TEST_WINDOW_BACKEND"))
    {
        // The private Weston compositor grants fullscreen requests on its only output.
        EXPECT_TRUE(w->m_Wayland->m_ShellFullscreen);
        LinuxOutput* output = w->m_Wayland->m_Outputs[0];
        EXPECT_EQ(output->m_Width / output->m_Scale, w->m_Width);
    }

    bool presented = false;
    static const wl_callback_listener listener = { FrameDone };
    wl_callback* frame = wl_surface_frame(w->m_Surface);
    wl_callback_add_listener(frame, &listener, &presented);
    WindowSwapBuffers(w);
    const uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
    while (!presented && dmTime::GetMonotonicTime() < deadline)
    {
        WindowPollEvents(w);
    }

    EXPECT_TRUE(presented);
    wl_callback_destroy(frame);
    WindowDelete(w);
}

// Density follows an entered output and falls back to the first one while the surface is unmapped.
TEST(LinuxWindow, WaylandDensityFollowsOutput)
{
    LinuxWayland wl = {};
    EXPECT_EQ(96.0f, dmPlatform::WaylandGetDisplayDensity(&wl));
    LinuxOutput first = {}, second = {};
    first.m_WidthMM = second.m_WidthMM = 254;
    first.m_Width = 1000;
    second.m_Width = 2000;
    wl.m_Outputs.SetCapacity(2);
    wl.m_Outputs.Push(&first);
    wl.m_Outputs.Push(&second);
    EXPECT_EQ(100.0f, dmPlatform::WaylandGetDisplayDensity(&wl));
    second.m_Entered = true;
    EXPECT_EQ(200.0f, dmPlatform::WaylandGetDisplayDensity(&wl));
    second.m_WidthMM = 0;
    EXPECT_EQ(96.0f, dmPlatform::WaylandGetDisplayDensity(&wl));
}

// Hidden fullscreen windows need a drawable before the compositor configures the toplevel, even with an unspecified size.
TEST(LinuxWindow, WaylandHiddenFullscreenSize)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    if (!w->m_Wayland)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_Hidden = p.m_Fullscreen = true;
    const uint32_t sizes[][2] = { { 0, 0 }, { 320, 0 }, { 0, 240 } };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(sizes); ++i)
    {
        p.m_Width = sizes[i][0];
        p.m_Height = sizes[i][1];
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        EXPECT_FALSE(w->m_Wayland->m_Configured);
        EXPECT_GT(dmPlatform::GetWindowWidth(w), 0u);
        EXPECT_GT(dmPlatform::GetWindowHeight(w), 0u);
        if (getenv("DM_TEST_WINDOW_BACKEND"))
        {
            // The test compositor exposes a single, unrotated output with integer scale.
            const LinuxOutput* output = w->m_Wayland->m_Outputs[0];
            EXPECT_EQ(output->m_Width, w->m_PixelWidth);
            EXPECT_EQ(output->m_Height, w->m_PixelHeight);
            EXPECT_EQ(output->m_Width / output->m_Scale, w->m_Width);
            EXPECT_EQ(output->m_Height / output->m_Scale, w->m_Height);
        }

        dmPlatform::ShowWindow(w);
        EXPECT_TRUE(w->m_Wayland->m_Configured);
        dmPlatform::CloseWindow(w);
    }

    dmPlatform::DeleteWindow(w);
}

static void AnnounceLegacyCompositor(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
    if (!strcmp(interface, "wl_compositor"))
    {
        dmPlatform::g_WaylandRegistryListener.global(data, registry, name, interface, dmMath::Min(version, 2u));
    }
}

// A surface without buffer scaling needs one pixel per logical unit both before and after it enters a HiDPI output.
TEST(LinuxWindow, WaylandLegacySurfaceScale)
{
    if (!getenv("DM_TEST_WAYLAND_SCALE"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w && w->m_Wayland);
    LinuxWayland* wl = w->m_Wayland;
    wl_compositor_destroy(wl->m_Compositor);
    wl->m_Compositor = 0;
    wl_registry* registry = wl_display_get_registry(w->m_WlDisplay);
    const wl_registry_listener listener = { AnnounceLegacyCompositor, dmPlatform::g_WaylandRegistryListener.global_remove };
    wl_registry_add_listener(registry, &listener, w);
    ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
    wl_registry_destroy(registry);
    ASSERT_TRUE(wl->m_Compositor);
    if (wl->m_ScaleManager)
    {
        wp_fractional_scale_manager_v1_destroy(wl->m_ScaleManager);
        wl->m_ScaleManager = 0;
    }

    WindowCreateParams p = Params();
    p.m_Hidden = p.m_Fullscreen = true;
    p.m_Width = p.m_Height = 0;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    ASSERT_LE(wl_surface_get_version(w->m_Surface), 2u);
    ASSERT_FALSE(wl->m_Outputs.Empty());
    const LinuxOutput* output = wl->m_Outputs[0];
    ASSERT_GT(output->m_Scale, 1);
    const uint32_t width = output->m_Width / output->m_Scale, height = output->m_Height / output->m_Scale;
    EXPECT_EQ((int)width, w->m_Width);
    EXPECT_EQ((int)height, w->m_Height);
    EXPECT_EQ(width, dmPlatform::GetWindowWidth(w));
    EXPECT_EQ(height, dmPlatform::GetWindowHeight(w));

    dmPlatform::ShowWindow(w);
    dmPlatform::SwapBuffers(w);
    const uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
    while (!wl->m_SurfaceEntered && dmTime::GetMonotonicTime() < deadline)
    {
        dmPlatform::PollEvents(w);
    }

    EXPECT_TRUE(wl->m_SurfaceEntered);
    EXPECT_EQ(1.0f, dmPlatform::GetDisplayScaleFactor(w));
    EXPECT_EQ(width, dmPlatform::GetWindowWidth(w));
    EXPECT_EQ(height, dmPlatform::GetWindowHeight(w));
    dmPlatform::DeleteWindow(w);
}

static void DeleteWaylandWindowOnResize(void* data, uint32_t, uint32_t)
{
    HWindow* window = (HWindow*)data;
    const int fd = g_WaylandClient.wl_display_get_fd_((*window)->m_WlDisplay);
    WindowDelete(*window);
    *window = 0;
    EXPECT_NE(-1, fcntl(fd, F_GETFD));
}

static void ResizeOnWaylandSync(void* data, wl_callback* callback, uint32_t)
{
    wl_callback_destroy(callback);
    HWindow window = *(HWindow*)data;
    if (window)
    {
        WindowSetSize(window, WindowGetWidth(window) + 1, WindowGetHeight(window));
    }
}

static void ShowOnWaylandSync(void* data, wl_callback* callback, uint32_t)
{
    wl_callback_destroy(callback);
    HWindow window = *(HWindow*)data;
    const int fd = g_WaylandClient.wl_display_get_fd_(window->m_WlDisplay);
    WindowShow(window);
    // The outer poll is still dispatching on the same display after ShowWindow returns.
    EXPECT_NE(-1, fcntl(fd, F_GETFD));
}

// Deleting a window during the initial show keeps its display alive until the outermost native dispatch returns.
TEST(LinuxWindow, WaylandDeleteDuringShow)
{
    for (int nested = 0; nested <= 1; ++nested)
    {
        HWindow window = WindowNew();
        ASSERT_NE((HWindow)0, window);
        if (!window->m_Wayland)
        {
            WindowDelete(window);
            SKIP();
        }

        WindowCreateParams p = Params();
        p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        p.m_Hidden = true;
        p.m_ResizeCallback = DeleteWaylandWindowOnResize;
        p.m_ResizeCallbackUserData = &window;
        ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(window, &p));
        wl_display* display = window->m_WlDisplay;
        const int fd = g_WaylandClient.wl_display_get_fd_(display);
        // The compositor replies in request order, so the resize is dispatched inside the map.
        if (nested)
        {
            static const wl_callback_listener show = { ShowOnWaylandSync };
            wl_callback_add_listener(wl_display_sync(display), &show, &window);
        }

        static const wl_callback_listener resize = { ResizeOnWaylandSync };
        wl_callback_add_listener(wl_display_sync(display), &resize, &window);
        if (nested)
        {
            const uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
            while (window && dmTime::GetMonotonicTime() < deadline)
            {
                WindowPollEvents(window);
            }
        }
        else
        {
            WindowShow(window);
        }

        EXPECT_EQ((HWindow)0, window);
        EXPECT_EQ(-1, fcntl(fd, F_GETFD));
        if (window)
        {
            WindowDelete(window);
        }
    }
}

struct WaylandOpenClose
{
    HWindow  m_Window;
    unsigned m_Callbacks;
    bool     m_RequestSent;
};

static void RequestCloseOnWaylandSync(void* data, wl_callback* callback, uint32_t)
{
    wl_callback_destroy(callback);
    WaylandOpenClose* action = (WaylandOpenClose*)data;
    EXPECT_FALSE(action->m_Window->m_Open);
    action->m_RequestSent = true;
    dmPlatform::g_WaylandToplevelListener.close(action->m_Window, action->m_Window->m_Wayland->m_Toplevel);
}

static int CloseFromWaylandRequest(void* data)
{
    WaylandOpenClose* action = (WaylandOpenClose*)data;
    ++action->m_Callbacks;
    EXPECT_TRUE(action->m_Window->m_Open);
    EXPECT_GT(action->m_Window->m_DispatchDepth, 0u);
    WindowClose(action->m_Window);
    return 1;
}

// A compositor close during WindowOpen runs on the first poll and expires if the window is replaced first.
TEST(LinuxWindow, WaylandCloseDuringOpen)
{
    HWindow w = WindowNew();
    ASSERT_TRUE(w);
    if (!w->m_Wayland)
    {
        WindowDelete(w);
        SKIP();
    }

    static const wl_callback_listener request = { RequestCloseOnWaylandSync };
    for (int replace = 0; replace <= 1; ++replace)
    {
        WaylandOpenClose action = { w, 0, false };
        WindowCreateParams p = Params();
        p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        p.m_CloseCallback = CloseFromWaylandRequest;
        p.m_CloseCallbackUserData = &action;
        wl_callback_add_listener(wl_display_sync(w->m_WlDisplay), &request, &action);
        ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(w, &p));
        EXPECT_TRUE(action.m_RequestSent);
        EXPECT_EQ(0u, action.m_Callbacks);
        if (replace)
        {
            WindowClose(w);
            ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(w, &p));
        }

        WindowPollEvents(w);
        EXPECT_EQ(replace ? 0u : 1u, action.m_Callbacks);
        EXPECT_EQ(replace != 0, w->m_Open);
        WindowClose(w);
    }

    WindowDelete(w);
}

struct WaylandShowClose
{
    HWindow m_Window;
    bool    m_Reopen, m_Called;
};

static void CloseWaylandWindowOnResize(void* data, uint32_t, uint32_t)
{
    WaylandShowClose* action = (WaylandShowClose*)data;
    WindowClose(action->m_Window);
    if (action->m_Reopen)
    {
        WindowCreateParams params = Params();
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        params.m_Hidden = true;
        ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(action->m_Window, &params));
    }

    action->m_Called = true;
    g_ResizeFd = g_WaylandClient.wl_display_get_fd_(action->m_Window->m_WlDisplay);
    g_ResizeWaits = 0;
}

// Closing or replacing a window during its initial show cancels the old map immediately.
TEST(LinuxWindow, WaylandCloseDuringShow)
{
    HWindow w = WindowNew();
    ASSERT_NE((HWindow)0, w);
    if (!w->m_Wayland)
    {
        WindowDelete(w);
        SKIP();
    }

    for (int reopen = 0; reopen <= 1; ++reopen)
    {
        WaylandShowClose action = { w, reopen != 0, false };
        WindowCreateParams p = Params();
        p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
        p.m_Hidden = true;
        p.m_ResizeCallback = CloseWaylandWindowOnResize;
        p.m_ResizeCallbackUserData = &action;
        ASSERT_EQ(WINDOW_RESULT_OK, WindowOpen(w, &p));
        static const wl_callback_listener resize = { ResizeOnWaylandSync };
        wl_callback_add_listener(wl_display_sync(w->m_WlDisplay), &resize, &w);
        WindowShow(w);
        g_ResizeFd = -1;
        ASSERT_TRUE(action.m_Called);
        EXPECT_EQ(0u, g_ResizeWaits);
        EXPECT_FALSE(w->m_Shown);
        EXPECT_FALSE(w->m_Wayland->m_Configured);
        EXPECT_EQ(reopen != 0, w->m_Open);
        if (reopen)
        {
            WindowShow(w);
            EXPECT_TRUE(w->m_Wayland->m_Configured);
        }

        WindowClose(w);
    }

    WindowDelete(w);
}

// A hidden Vulkan window can query the output density before polling and stays unmapped until shown.
TEST(LinuxWindow, WaylandHiddenVulkanWindow)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    if (!w->m_Wayland)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_Hidden = true;
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    ASSERT_TRUE(w->m_Surface);
    ASSERT_FALSE(w->m_Wayland->m_Frame || w->m_Wayland->m_Toplevel);
    const float initial_density = WindowGetDisplayDensity(w);
    ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
    if (!w->m_Wayland->m_Outputs.Empty())
    {
        const LinuxOutput* output = w->m_Wayland->m_Outputs[0];
        EXPECT_FALSE(output->m_Entered);
        const float expected = dmPlatform::CalculateDisplayDensity(output->m_WidthMM, output->m_Width);
        EXPECT_EQ(expected, initial_density);
        EXPECT_EQ(expected, WindowGetDisplayDensity(w));
    }

    dmPlatform::ShowWindow(w);
    EXPECT_TRUE(w->m_Wayland->m_Configured);
    EXPECT_TRUE(w->m_Wayland->m_Frame || w->m_Wayland->m_Toplevel);
    dmPlatform::HideWindow(w);
    EXPECT_FALSE(w->m_Wayland->m_Frame || w->m_Wayland->m_Toplevel);
    dmPlatform::DeleteWindow(w);
}

class WaylandSessionTest : public jc_test_base_class
{
    protected:
    HWindow m_Window;
    char*   m_PreviousPrefix;
    char    m_Directory[64];

    void SetUp()
    {
        m_Window = dmPlatform::NewWindow();
        const char* prefix = getenv("DM_WINDOW_STATE");
        m_PreviousPrefix = prefix ? strdup(prefix) : 0;
        m_Directory[0] = 0;
    }

    void TearDown()
    {
        dmPlatform::DeleteWindow(m_Window);
        if (m_PreviousPrefix)
        {
            setenv("DM_WINDOW_STATE", m_PreviousPrefix, 1);
            free(m_PreviousPrefix);
        }
        else
        {
            unsetenv("DM_WINDOW_STATE");
        }

        if (m_Directory[0])
        {
            const char* files[] = { "player-wayland.ini", "player-wayland.ini.session", "reset-size" };
            for (const char* file : files)
            {
                char path[128];
                snprintf(path, sizeof(path), "%s/%s", m_Directory, file);
                unlink(path);
            }

            rmdir(m_Directory);
        }
    }

    bool Present()
    {
        dmPlatform::SetSwapInterval(m_Window, 0);
        dmPlatform::SwapBuffers(m_Window);
        const uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
        while (!m_Window->m_Wayland->m_SurfaceEntered && dmTime::GetMonotonicTime() < deadline)
        {
            dmPlatform::PollEvents(m_Window);
        }

        return m_Window->m_Wayland->m_SurfaceEntered && g_WaylandClient.wl_display_roundtrip_(m_Window->m_WlDisplay) >= 0;
    }

    bool RecreateWindow()
    {
        dmPlatform::CloseWindow(m_Window);
        bool closed = g_WaylandClient.wl_display_roundtrip_(m_Window->m_WlDisplay) >= 0;
        dmPlatform::DeleteWindow(m_Window);
        m_Window = dmPlatform::NewWindow();
        return closed && m_Window && m_Window->m_Wayland;
    }

};

// The compositor restores the same player across connections and Reset Resolution still controls its size.
TEST_F(WaylandSessionTest, RestorePlayerAndResetSize)
{
    if (!m_Window || !m_Window->m_Wayland || !m_Window->m_Wayland->m_SessionManager)
    {
        SKIP();
    }

    strcpy(m_Directory, "test-wayland-session.XXXXXX");
    ASSERT_TRUE(mkdtemp(m_Directory));
    char path[128];
    snprintf(path, sizeof(path), "%s/player", m_Directory);
    ASSERT_EQ(0, setenv("DM_WINDOW_STATE", path, 1));
    WindowCreateParams p = Params();
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, p));
    EXPECT_FALSE(m_Window->m_Wayland->m_SessionRestored);
    dmPlatform::ShowWindow(m_Window);
    ASSERT_TRUE(Present());
    dmPlatform::SetWindowSize(m_Window, 450, 350);
    ASSERT_TRUE(Present());

    ASSERT_TRUE(RecreateWindow());
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, p));
    EXPECT_TRUE(m_Window->m_Wayland->m_SessionRestored);
    EXPECT_EQ(450, m_Window->m_Width);
    EXPECT_EQ(350, m_Window->m_Height);
    dmPlatform::ShowWindow(m_Window);
    ASSERT_TRUE(Present());
    ASSERT_TRUE(RecreateWindow());

    snprintf(path, sizeof(path), "%s/reset-size", m_Directory);
    FILE* reset = fopen(path, "wb");
    ASSERT_TRUE(reset);
    fputs("deadbeef-1234", reset);
    ASSERT_EQ(0, fclose(reset));
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, p));
    EXPECT_TRUE(m_Window->m_Wayland->m_SessionRestored);
    dmPlatform::ShowWindow(m_Window);
    ASSERT_TRUE(Present());
    EXPECT_EQ((int)p.m_Width, m_Window->m_Width);
    EXPECT_EQ((int)p.m_Height, m_Window->m_Height);
}

static int CountClose(void* data)
{
    ++*(unsigned*)data;
    return 0;
}

// A windowed game still has move, resize and close controls when no decoration provider is installed.
TEST(LinuxWindow, WaylandFallbackFrame)
{
    if (!getenv("DM_TEST_WAYLAND_FALLBACK"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w && w->m_Wayland);
    LinuxWayland* wl = w->m_Wayland;
    WindowCreateParams p = Params();
    unsigned closes = 0;
    p.m_CloseCallback = CountClose;
    p.m_CloseCallbackUserData = &closes;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmPlatform::ShowWindow(w);
    dmPlatform::SwapBuffers(w);
    ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
    EXPECT_FALSE(wl->m_Frame || wl->m_Decoration);
    ASSERT_TRUE(wl->m_FrameParts[0].m_Surface);
    dmPlatform::SetWindowSize(w, 321, 241);
    dmPlatform::SetWindowSize(w, 400, 300);
    dmPlatform::SwapBuffers(w);
    ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
    EXPECT_EQ(400, wl->m_FrameWidth);
    EXPECT_EQ(300, wl->m_FrameHeight);
    int width = 408, height = 330;
    dmPlatform::WaylandFrameContentSize(w, &width, &height);
    EXPECT_EQ(400, width);
    EXPECT_EQ(300, height);
    wl->m_PointerSurface = wl->m_FrameParts[0].m_Surface;
    wl->m_FramePointerX = 1;
    wl->m_FramePointerY = 1;
    EXPECT_STREQ("top_left_corner", dmPlatform::WaylandFrameCursor(wl));
    wl->m_FramePointerX = 400;
    wl->m_FramePointerY = 13;
    dmPlatform::WaylandFrameButton(w, 0, BTN_LEFT);
    EXPECT_EQ(1u, closes);
    dmPlatform::HideWindow(w);
    EXPECT_FALSE(wl->m_FrameParts[0].m_Surface);
    EXPECT_FALSE(wl->m_PointerSurface);
    dmPlatform::ShowWindow(w);
    EXPECT_TRUE(wl->m_FrameParts[0].m_Surface);
    dmPlatform::CloseWindow(w);
    p.m_Fullscreen = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmPlatform::ShowWindow(w);
    EXPECT_FALSE(wl->m_FrameParts[0].m_Surface);
    dmPlatform::DeleteWindow(w);
}

// The editor must only restore or capture floating geometry after a Wayland configure.
TEST(LinuxWindow, WaylandFloatingPlacement)
{
    dmWindow w = {};
    LinuxWayland wl = {};
    w.m_Wayland = &wl;
    uint32_t constrained[] = { XDG_TOPLEVEL_STATE_TILED_LEFT, XDG_TOPLEVEL_STATE_FULLSCREEN, XDG_TOPLEVEL_STATE_MAXIMIZED };
    for (uint32_t state : constrained)
    {
        wl_array states = { sizeof(state), sizeof(state), &state };
        dmPlatform::g_WaylandToplevelListener.configure(&w, 0, 1200, 900, &states);
        EXPECT_FALSE(dmPlatform::NormalPlacementWindow(&w));

        wl_array floating = {};
        dmPlatform::g_WaylandToplevelListener.configure(&w, 0, 640, 400, &floating);
        EXPECT_TRUE(dmPlatform::NormalPlacementWindow(&w));
    }
}

// A covered window can still be maximized. Suspending repaint must leave simulation and audio running.
TEST(LinuxWindow, WaylandSuspendedWindowIsNotIconified)
{
    dmWindow w = {};
    LinuxWayland wl = {};
    w.m_Wayland = &wl;
    // The suspended wire value is available even when building against xdg-shell headers older than v6.
    uint32_t states[] = { XDG_TOPLEVEL_STATE_MAXIMIZED, 9 };
    wl_array array = { sizeof(states), sizeof(states), states };
    dmPlatform::g_WaylandToplevelListener.configure(&w, 0, 640, 480, &array);
    EXPECT_TRUE(w.m_Maximized);
    EXPECT_FALSE(dmPlatform::GetWindowStateParam(&w, WINDOW_STATE_ICONIFIED));
}

// A compositor can ignore set_minimized, so the request alone must leave the game running.
TEST(LinuxWindow, WaylandMinimizeRequest)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    if (!w->m_Wayland)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmPlatform::ShowWindow(w);
    dmPlatform::SwapBuffers(w);
    ASSERT_FALSE(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ICONIFIED));
    dmPlatform::IconifyWindow(w);
    EXPECT_FALSE(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ICONIFIED));
    dmPlatform::DeleteWindow(w);
}

static void AnnounceLegacyClipboard(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
    if (!strcmp(interface, "wl_data_device_manager"))
    {
        version = 1;
    }
    else if (strcmp(interface, "wl_seat"))
    {
        return;
    }

    dmPlatform::g_WaylandRegistryListener.global(data, registry, name, interface, version);
}

// A v1 data-device manager must supply a clipboard device and disconnect without sending a v2 release.
TEST(LinuxWindow, WaylandLegacyClipboard)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    LinuxWayland* wl = w->m_Wayland;
    if (!wl || !wl->m_Seat)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    dmPlatform::WaylandReleaseSeat(w);
    if (wl->m_DataManager)
    {
        wl_data_device_manager_destroy(wl->m_DataManager);
        wl->m_DataManager = 0;
    }

    wl_registry* registry = wl_display_get_registry(w->m_WlDisplay);
    const wl_registry_listener listener = { AnnounceLegacyClipboard, dmPlatform::g_WaylandRegistryListener.global_remove };
    wl_registry_add_listener(registry, &listener, w);
    ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
    ASSERT_TRUE(wl->m_DataDevice);
    EXPECT_EQ(1u, wl_data_device_get_version(wl->m_DataDevice));
    dmPlatform::WaylandReleaseSeat(w);
    ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
    wl_registry_destroy(registry);
    dmPlatform::DeleteWindow(w);
}

// Seats predating the release requests must still supply a keyboard and pointer and disconnect cleanly.
TEST(LinuxWindow, WaylandLegacySeat)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    LinuxWayland* wl = w->m_Wayland;
    if (!wl || !wl->m_Seat)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
    if (!wl->m_Keyboard || !wl->m_Pointer)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    uint32_t name = wl->m_SeatName;
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    dmPlatform::WaylandReleaseSeat(w);
    for (uint32_t version = 1; version <= 2; ++version)
    {
        dmPlatform::WaylandBindSeat(w, name, version);
        ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
        ASSERT_TRUE(wl->m_Keyboard);
        ASSERT_TRUE(wl->m_Pointer);
        EXPECT_EQ(version, wl_keyboard_get_version(wl->m_Keyboard));
        EXPECT_EQ(version, wl_pointer_get_version(wl->m_Pointer));
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        dmPlatform::ShowWindow(w);
        EXPECT_TRUE(wl->m_Toplevel);
        EXPECT_FALSE(wl->m_Decor);
        dmPlatform::CloseWindow(w);
        dmPlatform::WaylandReleaseSeat(w);
        ASSERT_GE(g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay), 0);
    }

    dmPlatform::DeleteWindow(w);
}

// Only the content surface may deliver gameplay clicks, scrolling and client-relative positions.
TEST(LinuxWindow, WaylandDecorationPointerEvents)
{
    dmWindow w = {};
    LinuxWayland wl = {};
    w.m_Wayland = &wl;
    w.m_Surface = (wl_surface*)&w;
    const wl_pointer_listener& listener = dmPlatform::g_WaylandPointerListener;
    listener.enter(&w, 0, 1, w.m_Surface, wl_fixed_from_int(8), wl_fixed_from_int(9));
    listener.button(&w, 0, 2, 0, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    EXPECT_TRUE(w.m_Buttons[0]);
    listener.axis_discrete(&w, 0, WL_POINTER_AXIS_VERTICAL_SCROLL, 1);
    listener.frame(&w, 0);
    EXPECT_EQ(-1.0, w.m_Wheel);
    listener.leave(&w, 0, 3, w.m_Surface);
    listener.enter(&w, 0, 4, (wl_surface*)&wl, 0, 0);
    listener.button(&w, 0, 5, 0, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
    listener.motion(&w, 0, 0, 0, 0);
    listener.axis(&w, 0, 0, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(10));
    listener.axis_discrete(&w, 0, WL_POINTER_AXIS_VERTICAL_SCROLL, 1);
    listener.frame(&w, 0);
    EXPECT_FALSE(w.m_Buttons[0]);
    EXPECT_EQ(8.0, w.m_MouseX);
    EXPECT_EQ(9.0, w.m_MouseY);
    EXPECT_EQ(-1.0, w.m_Wheel);
}

static int g_PreviousCursorSize, g_CursorSize;
static wl_cursor_theme* ReadCursorTheme(const char* name, int size, wl_shm*)
{
    EXPECT_STREQ("Defold test cursor", name);
    g_PreviousCursorSize = g_CursorSize;
    g_CursorSize = size;
    return 0;
}

// A cursor must follow the player's theme and the current output scale, with a usable fallback.
TEST(LinuxWindow, WaylandCursorThemeSettings)
{
    char* old_theme = getenv("XCURSOR_THEME") ? strdup(getenv("XCURSOR_THEME")) : 0;
    char* old_size = getenv("XCURSOR_SIZE") ? strdup(getenv("XCURSOR_SIZE")) : 0;
    setenv("XCURSOR_THEME", "Defold test cursor", 1);
    setenv("XCURSOR_SIZE", "64", 1);
    dmWindow w = {};
    LinuxWayland wl = {};
    LinuxWindowOps ops = {};
    ops.m_SetCursor = dmPlatform::WaylandSetCursor;
    w.m_Ops = &ops;
    w.m_Scale = 1.0f;
    w.m_Wayland = &wl;
    w.m_Surface = wl.m_PointerSurface = (wl_surface*)&w;
    wl.m_Pointer = (wl_pointer*)&wl;
    wl.m_Shm = (wl_shm*)&wl;
    wl.m_EnterSerial = 1;
    WaylandClient client = g_WaylandClient;
    g_WaylandClient.wl_cursor_theme_load_ = ReadCursorTheme;
    g_WaylandClient.wl_proxy_get_version_ = [](wl_proxy*) -> uint32_t {
                                                return 3;
                                            };
    dmPlatform::WaylandSetCursor(&w);
    EXPECT_EQ(64, g_CursorSize);
    // The pointer stays on the window as it moves to a fractional-scale output.
    dmPlatform::LinuxUpdateSize(&w, 320, 240, 1.5f);
    EXPECT_EQ(128, g_PreviousCursorSize);
    EXPECT_EQ(64, g_CursorSize);
    setenv("XCURSOR_SIZE", "12px", 1);
    dmPlatform::LinuxUpdateSize(&w, 320, 240, 1.0f);
    EXPECT_EQ(24, g_CursorSize);
    g_WaylandClient = client;
    if (old_theme)
    {
        setenv("XCURSOR_THEME", old_theme, 1);
    }
    else
    {
        unsetenv("XCURSOR_THEME");
    }

    if (old_size)
    {
        setenv("XCURSOR_SIZE", old_size, 1);
    }
    else
    {
        unsetenv("XCURSOR_SIZE");
    }

    free(old_theme);
    free(old_size);
}

// Adjusted relative motion must affect the locked position only, including when the compositor revokes the lock.
TEST(LinuxWindow, WaylandLockedCursorMotion)
{
    dmWindow w = {};
    LinuxWayland wl = {};
    w.m_Wayland = &wl;
    w.m_MouseX = 10.0;
    w.m_MouseY = 20.0;
    const wl_fixed_t dx = wl_fixed_from_double(2.5), dy = wl_fixed_from_double(-1.25);
    dmPlatform::g_WaylandRelativePointerListener.relative_motion(&w, 0, 0, 0, dx, dy, 0, 0);
    ASSERT_EQ(10.0, w.m_MouseX);
    ASSERT_EQ(20.0, w.m_MouseY);
    dmPlatform::g_WaylandLockedPointerListener.locked(&w, 0);
    dmPlatform::g_WaylandRelativePointerListener.relative_motion(&w, 0, 0, 0, dx, dy, wl_fixed_from_int(9), wl_fixed_from_int(-7));
    ASSERT_EQ(12.5, w.m_MouseX);
    ASSERT_EQ(18.75, w.m_MouseY);
    dmPlatform::g_WaylandLockedPointerListener.unlocked(&w, 0);
    ASSERT_EQ(10.0, w.m_MouseX);
    ASSERT_EQ(20.0, w.m_MouseY);
    dmPlatform::g_WaylandLockedPointerListener.locked(&w, 0);
    dmPlatform::g_WaylandRelativePointerListener.relative_motion(&w, 0, 0, 0, dx, dy, 0, 0);
    dmPlatform::WaylandReleaseCursor(&w);
    ASSERT_EQ(10.0, w.m_MouseX);
    ASSERT_EQ(20.0, w.m_MouseY);
}

static void ReopenOnWaylandFocus(void* data, uint32_t focused)
{
    HWindow w = (HWindow)data;
    ASSERT_TRUE(focused);
    EXPECT_EQ(1, dmPlatform::GetKey(w, dmPlatform::LinuxKeyCode(KEY_LEFTSHIFT)));
    WindowCreateParams replacement = w->m_Params;
    replacement.m_FocusCallback = 0;
    dmPlatform::CloseWindow(w);
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, replacement));
}

// A Wayland focus listener sees keys held on entry, and reopening from that listener clears them for the new surface.
TEST(LinuxWindow, WaylandFocusRestoresHeldKeysBeforeCallback)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    if (!w->m_Wayland)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    p.m_FocusCallback = ReopenOnWaylandFocus;
    p.m_FocusCallbackUserData = w;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));

    uint32_t shift = KEY_LEFTSHIFT;
    wl_array keys = { sizeof(shift), sizeof(shift), &shift };
    dmPlatform::g_WaylandKeyboardListener.enter(w, 0, 1, w->m_Surface, &keys);
    EXPECT_TRUE(w->m_Open);
    EXPECT_FALSE(w->m_Focused);
    EXPECT_EQ(0, dmPlatform::GetKey(w, dmPlatform::LinuxKeyCode(shift)));
    dmPlatform::DeleteWindow(w);
}

// Wayland must expose its established physical key codes on focus entry and subsequent key events.
TEST(LinuxWindow, WaylandPhysicalKeyCodes)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    if (!w->m_Wayland)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    const wl_keyboard_listener& keyboard = dmPlatform::g_WaylandKeyboardListener;
    uint32_t iso = KEY_102ND;
    wl_array held = { sizeof(iso), sizeof(iso), &iso };
    keyboard.enter(w, 0, 1, w->m_Surface, &held);
    EXPECT_EQ(1, dmPlatform::GetKey(w, dmPlatform::PLATFORM_KEY_NON_US_2));
    EXPECT_EQ(0, dmPlatform::GetKey(w, dmPlatform::PLATFORM_KEY_NON_US_1));
    keyboard.key(w, 0, 2, 0, iso, WL_KEYBOARD_KEY_STATE_RELEASED);
    EXPECT_EQ(0, dmPlatform::GetKey(w, dmPlatform::PLATFORM_KEY_NON_US_2));
    keyboard.key(w, 0, 3, 0, iso, WL_KEYBOARD_KEY_STATE_PRESSED);
    EXPECT_EQ(1, dmPlatform::GetKey(w, dmPlatform::PLATFORM_KEY_NON_US_2));

    const uint32_t print_keys[] = { KEY_SYSRQ, KEY_PRINT };
    uint32_t serial = 4;
    for (uint32_t key : print_keys)
    {
        keyboard.key(w, 0, serial++, 0, key, WL_KEYBOARD_KEY_STATE_PRESSED);
        EXPECT_EQ(1, dmPlatform::GetKey(w, dmPlatform::PLATFORM_KEY_PRINT_SCREEN));
        keyboard.key(w, 0, serial++, 0, key, WL_KEYBOARD_KEY_STATE_RELEASED);
        EXPECT_EQ(0, dmPlatform::GetKey(w, dmPlatform::PLATFORM_KEY_PRINT_SCREEN));
    }

    dmPlatform::DeleteWindow(w);
}

static void CountRepeatedKey(void* data, int character)
{
    EXPECT_EQ('a', character);
    ++*(unsigned*)data;
}

static const char WAYLAND_TEST_KEYMAP[] = "xkb_keymap { xkb_keycodes { include \"evdev\" }; xkb_types { include \"complete\" }; "
                                          "xkb_compatibility { include \"complete\" }; xkb_symbols { include \"pc+us\" }; };";

// A slow frame must deliver each elapsed Wayland repeat and keep the next deadline on the keyboard's schedule.
TEST(LinuxWindow, WaylandRepeatAcrossSlowFrames)
{
    ASSERT_TRUE(LoadWaylandClient());
    LinuxWayland wl = {};
    wl.m_Xkb = g_WaylandClient.xkb_context_new_(XKB_CONTEXT_NO_FLAGS);
    wl.m_Keymap = g_WaylandClient.xkb_keymap_new_from_string_(wl.m_Xkb, WAYLAND_TEST_KEYMAP, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    ASSERT_TRUE(wl.m_Keymap);
    wl.m_KeyState = g_WaylandClient.xkb_state_new_(wl.m_Keymap);
    ASSERT_TRUE(wl.m_KeyState);
    wl.m_RepeatKey = KEY_A;
    wl.m_RepeatRate = 25;
    wl.m_NextRepeat = 100000;
    dmWindow w = {};
    w.m_Wayland = &wl;
    w.m_Open = true;
    unsigned count = 0;
    dmPlatform::SetKeyboardCharCallback(&w, CountRepeatedKey, &count);
    dmPlatform::WaylandRepeat(&w, 265000);
    EXPECT_EQ(5u, count);
    dmPlatform::WaylandRepeat(&w, 299999);
    EXPECT_EQ(5u, count);
    dmPlatform::WaylandRepeat(&w, 300000);
    EXPECT_EQ(6u, count);
    g_WaylandClient.xkb_state_unref_(wl.m_KeyState);
    g_WaylandClient.xkb_keymap_unref_(wl.m_Keymap);
    g_WaylandClient.xkb_context_unref_(wl.m_Xkb);
}

// A missing runtime loader must report no Vulkan extensions even when the system supports them.
TEST(LinuxWindow, VulkanMissingLoader)
{
    dmPlatform::VulkanSetLoader(0);
    uint32_t count = 99;
    const char** extensions = dmPlatform::VulkanGetRequiredInstanceExtensions(&count);
    dmPlatform::VulkanSetLoader(vkGetInstanceProcAddr);
    ASSERT_FALSE(extensions);
    ASSERT_EQ(0u, count);
}

// Probes Vulkan before the first window, then verifies the live window selects the same surface extension.
TEST(LinuxWindow, VulkanDisplaySelection)
{
    uint32_t count;
    const char** probe = dmPlatform::VulkanGetRequiredInstanceExtensions(&count);
    ASSERT_EQ(2u, count);
    const char* early = probe[1];
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    const char* expected = getenv("DM_TEST_WINDOW_BACKEND");
    if (expected)
    {
        ASSERT_STREQ(expected, w->m_Ops->m_Name);
    }

    const char** live = dmPlatform::VulkanGetRequiredInstanceExtensions(&count);
    ASSERT_EQ(2u, count);
    ASSERT_STREQ(early, live[1]);
    if (getenv("DM_TEST_X11_NO_XCB"))
    {
        ASSERT_STREQ("VK_KHR_xlib_surface", early);
    }
    else if (w->m_X11)
    {
        ASSERT_TRUE(!strcmp(early, "VK_KHR_xcb_surface") || !strcmp(early, "VK_KHR_xlib_surface"));
    }
    else
    {
        ASSERT_STREQ("VK_KHR_wayland_surface", early);
    }

    dmPlatform::DeleteWindow(w);
}

// An instance enabling only Xlib must still get a surface on drivers that also advertise XCB.
TEST(LinuxWindow, VulkanXlibFallback)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    if (!w->m_X11)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    const char* extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_xlib_surface" };
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.enabledExtensionCount = 2;
    info.ppEnabledExtensionNames = extensions;
    VkInstance instance;
    VkResult result = vkCreateInstance(&info, 0, &instance);
    if (result == VK_ERROR_EXTENSION_NOT_PRESENT)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    ASSERT_EQ(VK_SUCCESS, result);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    VkSurfaceKHR surface;
    ASSERT_EQ(VK_SUCCESS, dmPlatform::VulkanCreateWindowSurface(instance, w, &surface));
    vkDestroySurfaceKHR(instance, surface, 0);
    dmPlatform::DeleteWindow(w);
    vkDestroyInstance(instance, 0);
}

struct TextureUpload
{
    HWindow                      m_Window;
    GLuint                       m_Texture;
    const uint8_t*               m_Pixel;
    __typeof__(&glGenTextures)   m_Generate;
    __typeof__(&glBindTexture)   m_Bind;
    __typeof__(&glTexParameteri) m_Parameter;
    __typeof__(&glTexImage2D)    m_Image;
    __typeof__(&glFinish)        m_Finish;
    uintptr_t                    m_ProcBeforeAcquire, m_ProcWhileCurrent, m_ProcAfterRelease;
};

static void* UploadTexture(void* data)
{
    TextureUpload* upload = (TextureUpload*)data;
    upload->m_ProcBeforeAcquire = dmPlatform::GetProcAddress(upload->m_Window, "glGetString");
    void* aux = dmPlatform::AcquireAuxContext(upload->m_Window);
    if (aux)
    {
        upload->m_ProcWhileCurrent = dmPlatform::GetProcAddress(upload->m_Window, "glGetString");
        upload->m_Generate(1, &upload->m_Texture);
        upload->m_Bind(GL_TEXTURE_2D, upload->m_Texture);
        upload->m_Parameter(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        upload->m_Image(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload->m_Pixel);
        upload->m_Finish();
        dmPlatform::UnacquireAuxContext(upload->m_Window, aux);
        upload->m_ProcAfterRelease = dmPlatform::GetProcAddress(upload->m_Window, "glGetString");
    }

    return 0;
}

static void CheckMultisampleFallback(HWindow w, WindowsGraphicsApi api)
{
    WindowCreateParams p = Params();
    p.m_GraphicsApi = api;
    p.m_Hidden = true;
    const unsigned requests[] = { 4, 1, 1024, 0 };
    GLint samples[DM_ARRAY_SIZE(requests)] = {};
    for (unsigned i = 0; i < DM_ARRAY_SIZE(requests); ++i)
    {
        p.m_Samples = requests[i];
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        __typeof__(&::glGetIntegerv) get_integer = (__typeof__(&::glGetIntegerv)) dmPlatform::GetProcAddress(w, "glGetIntegerv");
        ASSERT_TRUE(get_integer);
        get_integer(GL_SAMPLES, &samples[i]);
        EXPECT_EQ(requests[i], dmPlatform::GetWindowStateParam(w, WINDOW_STATE_FSAA_SAMPLES));
        EXPECT_EQ((uint32_t)dmMath::Max(samples[i], 1), dmPlatform::GetWindowStateParam(w, WINDOW_STATE_SAMPLE_COUNT));
        dmPlatform::CloseWindow(w);
    }

    if (samples[0] > 0)
    {
        EXPECT_GT(samples[1], 0);
        EXPECT_LE(samples[1], samples[0]);
    }

    EXPECT_GE(samples[2], samples[0]);
}

// Vulkan negotiates its swapchain after the window opens, so it still needs the requested sample count.
TEST(LinuxWindow, VulkanRequestedSampleCount)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    p.m_Samples = 8;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    EXPECT_EQ(8u, dmPlatform::GetWindowStateParam(w, WINDOW_STATE_FSAA_SAMPLES));
    EXPECT_EQ(8u, dmPlatform::GetWindowStateParam(w, WINDOW_STATE_SAMPLE_COUNT));
    dmPlatform::DeleteWindow(w);
}

// A channel precision the driver cannot supply must still yield a usable GLX or EGL window.
TEST(LinuxWindow, FramebufferPrecisionFallback)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    WindowCreateParams p = Params();
    p.m_Hidden = true;
    p.m_ContextAlphabits = 255;
    const WindowsGraphicsApi apis[] = { WINDOW_GRAPHICS_API_OPENGL, WINDOW_GRAPHICS_API_OPENGLES };
    for (unsigned i = 0; i < sizeof(apis) / sizeof(apis[0]); ++i)
    {
        p.m_GraphicsApi = apis[i];
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        EXPECT_GT(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_RED_BITS), 0u);
        EXPECT_LT(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_ALPHA_BITS), 255u);
        dmPlatform::CloseWindow(w);
    }

    dmPlatform::DeleteWindow(w);
}

// Keep requested multisampling at reduced depth precision and prefer a pbuffer for otherwise equal formats.
TEST(LinuxWindow, EGLFramebufferSelection)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    w->m_Params = Params();
    w->m_Params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGLES;
    g_TestEGLConfigs = true;
    const uint32_t requests[] = { 0, 4 };
    const VisualID expected[] = { 2, 3 };
    for (unsigned i = 0; i < DM_ARRAY_SIZE(requests); ++i)
    {
        w->m_Params.m_Samples = requests[i];
        VisualID visual = 0;
        EXPECT_TRUE(dmPlatform::LinuxPrepareGL(w, &visual));
        EXPECT_EQ(expected[i], visual);
        dmPlatform::LinuxCloseGL(w);
    }

    g_TestEGLConfigs = false;
    dmPlatform::DeleteWindow(w);
}

// EGL must retain available multisampling for requests outside the driver's sample range.
TEST(LinuxWindow, EGLMultisampleFallback)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    CheckMultisampleFallback(w, WINDOW_GRAPHICS_API_OPENGLES);
    if (w->m_Wayland)
    {
        CheckMultisampleFallback(w, WINDOW_GRAPHICS_API_OPENGL);
    }

    dmPlatform::DeleteWindow(w);
}

// GLX must retain available multisampling for requests outside the driver's sample range.
TEST(LinuxWindow, GLXMultisampleFallback)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    if (!w->m_X11)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    CheckMultisampleFallback(w, WINDOW_GRAPHICS_API_OPENGL);
    dmPlatform::DeleteWindow(w);
}

// Both the main and shared desktop contexts must enforce the requested forward-compatible core profile.
TEST(LinuxWindow, ForwardCompatibleOpenGLContexts)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
    p.m_GraphicsApiVersionHint = 33;
    p.m_OpenGLUseCoreProfileHint = true;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    __typeof__(&::glGetIntegerv) get_integer = (__typeof__(&::glGetIntegerv)) dmPlatform::GetProcAddress(w, "glGetIntegerv");
    ASSERT_TRUE(get_integer);
    GLint flags = 0;
    get_integer(GL_CONTEXT_FLAGS, &flags);
    EXPECT_NE(0, flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT);
    if (dmPlatform::GetWindowStateParam(w, WINDOW_STATE_AUX_CONTEXT))
    {
        void* aux = dmPlatform::AcquireAuxContext(w);
        ASSERT_TRUE(aux);
        flags = 0;
        get_integer(GL_CONTEXT_FLAGS, &flags);
        EXPECT_NE(0, flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT);
        dmPlatform::UnacquireAuxContext(w, aux);
    }

    dmPlatform::DeleteWindow(w);
}

// Closing the owner must unbind its auxiliary context, while closing a different window must preserve it.
TEST(LinuxWindow, CloseWithAuxiliaryContextCurrent)
{
    HWindow owner = dmPlatform::NewWindow();
    HWindow other = dmPlatform::NewWindow();
    ASSERT_TRUE(owner);
    ASSERT_TRUE(other);
    WindowCreateParams p = Params();
    p.m_Hidden = true;
    const WindowsGraphicsApi apis[] = { WINDOW_GRAPHICS_API_OPENGL, WINDOW_GRAPHICS_API_OPENGLES };
    for (WindowsGraphicsApi api : apis)
    {
        p.m_GraphicsApi = api;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(owner, p));
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(other, p));
        void* aux = dmPlatform::AcquireAuxContext(owner);
        ASSERT_TRUE(aux);
        __typeof__(&glXGetCurrentContext) current_glx = 0;
        if (dmPlatform::GetX11GLXContext(owner))
        {
            current_glx = (__typeof__(current_glx)) dmPlatform::GetProcAddress(owner, "glXGetCurrentContext");
            ASSERT_TRUE(current_glx);
        }

        dmPlatform::CloseWindow(other);
        EXPECT_EQ(aux, current_glx ? current_glx() : eglGetCurrentContext());
        dmPlatform::CloseWindow(owner);
        EXPECT_EQ((void*)0, current_glx ? current_glx() : eglGetCurrentContext());
    }

    dmPlatform::DeleteWindow(other);
    dmPlatform::DeleteWindow(owner);
}

static void CheckDrawableSize(HWindow w, uint32_t client_width, uint32_t client_height)
{
    // X11 client dimensions are already pixels. Wayland scales logical dimensions, including fractional scales.
    float scale = w->m_Wayland ? dmPlatform::GetDisplayScaleFactor(w) : 1.0f;
    EXPECT_EQ((uint32_t)lround(client_width * scale), dmPlatform::GetWindowWidth(w));
    EXPECT_EQ((uint32_t)lround(client_height * scale), dmPlatform::GetWindowHeight(w));
}

// Software contexts report zero. A driver-confirmed GPU keeps its own result when another window becomes current.
TEST(LinuxWindow, RendererAcceleration)
{
    const char* driver = getenv("GALLIUM_DRIVER");
    if (!driver || strcmp(driver, "llvmpipe"))
    {
        SKIP();
    }

    const WindowsGraphicsApi apis[] = { WINDOW_GRAPHICS_API_OPENGL, WINDOW_GRAPHICS_API_OPENGLES };
    WindowCreateParams params = Params();
    params.m_Hidden = true;
    for (unsigned i = 0; i < sizeof(apis) / sizeof(apis[0]); ++i)
    {
        params.m_GraphicsApi = apis[i];
        HWindow hardware = dmPlatform::NewWindow();
        HWindow software = dmPlatform::NewWindow();
        ASSERT_NE((HWindow)0, hardware);
        ASSERT_NE((HWindow)0, software);
        g_TestHardwareRenderer = true;
        WindowResult result = dmPlatform::OpenWindow(hardware, params);
        g_TestHardwareRenderer = false;
        ASSERT_EQ(WINDOW_RESULT_OK, result);
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(software, params));
        EXPECT_EQ(0u, dmPlatform::GetWindowStateParam(software, WINDOW_STATE_ACCELERATED));
        EXPECT_EQ(1u, dmPlatform::GetWindowStateParam(hardware, WINDOW_STATE_ACCELERATED));
        dmPlatform::DeleteWindow(software);
        dmPlatform::DeleteWindow(hardware);
    }
}

static void CheckSharedTextureUpload(HWindow w)
{
#define GL_PROC(type, name) type name = (type)dmPlatform::GetProcAddress(w,#name); ASSERT_TRUE(name != 0)
    GL_PROC(__typeof__(&::glGenTextures), glGenTextures);
    GL_PROC(__typeof__(&::glBindTexture), glBindTexture);
    GL_PROC(__typeof__(&::glTexImage2D), glTexImage2D);
    GL_PROC(__typeof__(&::glTexParameteri), glTexParameteri);
    GL_PROC(__typeof__(&::glFinish), glFinish);
    GL_PROC(__typeof__(&::glReadPixels), glReadPixels);
    GL_PROC(__typeof__(&::glDeleteTextures), glDeleteTextures);
    GL_PROC(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers);
    GL_PROC(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
    GL_PROC(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D);
    GL_PROC(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus);
    GL_PROC(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers);
#undef GL_PROC
    const uint8_t pixel[] = { 17, 63, 129, 255 };
    TextureUpload upload = { w, 0, pixel, glGenTextures, glBindTexture, glTexParameteri, glTexImage2D, glFinish };
    pthread_t worker;
    ASSERT_EQ(0, pthread_create(&worker, 0, UploadTexture, &upload));
    ASSERT_EQ(0, pthread_join(worker, 0));
    EXPECT_EQ((uintptr_t)0, upload.m_ProcBeforeAcquire);
    EXPECT_NE((uintptr_t)0, upload.m_ProcWhileCurrent);
    EXPECT_EQ((uintptr_t)0, upload.m_ProcAfterRelease);
    GLuint texture = upload.m_Texture;
    ASSERT_NE(0u, texture);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_EQ((GLenum)GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    uint8_t actual[4] = {};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual);
    ASSERT_EQ(0, memcmp(pixel, actual, 4));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &texture);
}

// GLX must share textures with the worker even if the window's format or driver cannot provide a pbuffer.
TEST(LinuxWindow, X11SharedContextWithoutPbuffer)
{
    HWindow w = NewX11TestWindow();
    if (!w)
    {
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        g_TestWindowOnlyGLX = cycle == 0;
        g_TestGLXPbufferError = cycle == 1;
        WindowResult result = dmPlatform::OpenWindow(w, p);
        g_TestWindowOnlyGLX = g_TestGLXPbufferError = false;
        ASSERT_EQ(WINDOW_RESULT_OK, result);
        ASSERT_TRUE(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_AUX_CONTEXT));
        CheckSharedTextureUpload(w);
        dmPlatform::CloseWindow(w);
    }

    dmPlatform::DeleteWindow(w);
}

// Without a pbuffer, X11 and Wayland must preserve the render context and fall back when EGL rejects a surfaceless worker.
TEST(LinuxWindow, EGLSharedContextWithoutPbuffer)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGLES;
    p.m_Hidden = true;
    const char* surfaceless = "EGL_KHR_surfaceless_context";
    const struct
    {
        const char*           extensions;
        PFNEGLMAKECURRENTPROC make_current;
        bool                  window_only, reject_aux_window;
    } cases[] = {
        { "", 0, true, false },
        { "", 0, false, false },
        { "", 0, true, true },
        { surfaceless, RejectSurfacelessBind, false, false },
        { surfaceless, RejectSurfacelessBind, true, true },
        { surfaceless, RejectRenderContextRestore, true, false },
        { surfaceless, 0, true, false },
    };
    for (unsigned i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        g_TestWindowOnlyEGL = cases[i].window_only;
        g_RejectEGLAuxWindow = cases[i].reject_aux_window;
        g_EGLWindowSurfaces = 0;
        g_RejectEGLPbuffer = true;
        g_EGLDisplayExtensions = cases[i].extensions;
        g_TestEGLMakeCurrent = cases[i].make_current;
        WindowResult result = dmPlatform::OpenWindow(w, p);
        g_TestWindowOnlyEGL = g_RejectEGLPbuffer = g_RejectEGLAuxWindow = false;
        g_EGLDisplayExtensions = 0;
        g_TestEGLMakeCurrent = 0;
        if (cases[i].make_current == RejectRenderContextRestore)
        {
            ASSERT_EQ(WINDOW_RESULT_WINDOW_OPEN_ERROR, result);
            EXPECT_EQ((EGLContext)EGL_NO_CONTEXT, eglGetCurrentContext());
            continue;
        }

        ASSERT_EQ(WINDOW_RESULT_OK, result);
        EXPECT_NE((EGLSurface)EGL_NO_SURFACE, eglGetCurrentSurface(EGL_DRAW));
        ASSERT_EQ(!cases[i].reject_aux_window, dmPlatform::GetWindowStateParam(w, WINDOW_STATE_AUX_CONTEXT) != 0);
        if (cases[i].reject_aux_window)
        {
            EXPECT_EQ(2u, g_EGLWindowSurfaces);
        }
        else
        {
            CheckSharedTextureUpload(w);
        }

        dmPlatform::CloseWindow(w);
    }

    dmPlatform::DeleteWindow(w);
}

// A texture uploaded on the resource worker must be readable by the render context after resize and reopen.
// The worker can resolve GL functions only while its auxiliary context is current.
TEST(LinuxWindow, SharedOpenGLContext)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    const char* scale = w->m_Wayland ? getenv("DM_TEST_WAYLAND_SCALE") : 0;
    const float expected_scale = scale ? strtof(scale, 0) : 0.0f;
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        p.m_HighDPI = cycle != 0;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        dmPlatform::ShowWindow(w);
        dmPlatform::SetSwapInterval(w, 0);
        ASSERT_TRUE(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_AUX_CONTEXT));
        ASSERT_EQ(w->m_X11 != 0 && p.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL, dmPlatform::GetX11GLXContext(w) != 0);
        CheckSharedTextureUpload(w);
        dmPlatform::SwapBuffers(w);
        if (scale)
        {
            uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
            while (dmPlatform::GetDisplayScaleFactor(w) != expected_scale && dmTime::GetMonotonicTime() < deadline)
            {
                dmPlatform::PollEvents(w);
            }

            ASSERT_EQ(expected_scale, dmPlatform::GetDisplayScaleFactor(w));
        }

        dmPlatform::SetWindowSize(w, 421, 307);
        dmPlatform::SwapBuffers(w);
        uint32_t width, height;
        uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
        do
        {
            dmPlatform::PollEvents(w);
            ASSERT_TRUE(dmPlatform::GetWindowClientSize(w, &width, &height));
        } while ((width != 421 || height != 307) && dmTime::GetMonotonicTime() < deadline);

        ASSERT_EQ(421u, width);
        ASSERT_EQ(307u, height);
        CheckDrawableSize(w, width, height);
        dmPlatform::HideWindow(w);
        ASSERT_FALSE(w->m_Shown);
        dmPlatform::SwapBuffers(w);
        dmPlatform::ShowWindow(w);
        ASSERT_TRUE(w->m_Shown);
        dmPlatform::SwapBuffers(w);
        dmPlatform::PollEvents(w);
        if (scale)
        {
            ASSERT_EQ(expected_scale, dmPlatform::GetDisplayScaleFactor(w));
        }

        CheckDrawableSize(w, width, height);
        dmPlatform::CloseWindow(w);
        ASSERT_TRUE(dmPlatform::GetX11GLXContext(w) == 0);
    }

    dmPlatform::DeleteWindow(w);
}

// X11 can return a composed character in a reused buffer without terminating it.
TEST(LinuxWindow, TextUsesLookupLength)
{
    dmWindow window = {};
    window.m_Open = true;
    int character = 0;
    dmPlatform::SetKeyboardCharCallback(&window, Character, &character);

    char buffer[] = "\xf0\x9f\x98\x80";
    dmPlatform::LinuxText(&window, buffer, 4);
    ASSERT_EQ(0x1f600, character);
    memcpy(buffer, "\xc3\xa9", 2);
    dmPlatform::LinuxText(&window, buffer, 1);
    ASSERT_EQ(0x1f600, character);
    dmPlatform::LinuxText(&window, buffer, 2);
    ASSERT_EQ(0xe9, character);

    const char controls[] = { '\0', '\n', 'a' };
    dmPlatform::LinuxText(&window, controls, sizeof(controls));
    ASSERT_EQ('a', character);
}

struct TextCloseAction
{
    HWindow  m_Window;
    unsigned m_Count;
    bool     m_Reopen;
};

static void CloseOnFirstCharacter(void* data, int character)
{
    TextCloseAction* action = (TextCloseAction*)data;
    if (++action->m_Count == 1)
    {
        EXPECT_EQ('a', character);
        WindowCreateParams params = action->m_Window->m_Params;
        dmPlatform::CloseWindow(action->m_Window);
        if (action->m_Reopen)
        {
            ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(action->m_Window, params));
        }
    }
    else
    {
        EXPECT_EQ('c', character);
    }
}

// Closing or reopening during a multi-character commit stops delivery. A replacement can receive the next commit.
TEST(LinuxWindow, TextCommitStopsWhenWindowCloses)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    for (int reopen = 0; reopen <= 1; ++reopen)
    {
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        TextCloseAction action = { w, 0, reopen != 0 };
        dmPlatform::SetKeyboardCharCallback(w, CloseOnFirstCharacter, &action);
        dmPlatform::LinuxText(w, "ab", 2);
        EXPECT_EQ(1u, action.m_Count);
        dmPlatform::LinuxText(w, "c", 1);
        EXPECT_EQ(reopen ? 2u : 1u, action.m_Count);
        dmPlatform::SetKeyboardCharCallback(w, 0, 0);
    }

    dmPlatform::DeleteWindow(w);
}

// Reopening from a key callback cancels repeat and ignores queued keys until the new surface receives focus.
TEST(LinuxWindow, WaylandReopenCancelsStartingRepeat)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    if (!w->m_Wayland)
    {
        dmPlatform::DeleteWindow(w);
        SKIP();
    }

    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    // A headless compositor may have no keyboard. Supply a US keymap through the same listener as a real seat.
    FILE* keymap = tmpfile();
    ASSERT_NE((FILE*)0, keymap);
    ASSERT_EQ(sizeof(WAYLAND_TEST_KEYMAP), fwrite(WAYLAND_TEST_KEYMAP, 1, sizeof(WAYLAND_TEST_KEYMAP), keymap));
    ASSERT_EQ(0, fflush(keymap));
    const wl_keyboard_listener& keyboard = dmPlatform::g_WaylandKeyboardListener;
    keyboard.keymap(w, 0, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, dup(fileno(keymap)), sizeof(WAYLAND_TEST_KEYMAP));
    ASSERT_TRUE(w->m_Wayland->m_KeyState);
    keyboard.repeat_info(w, 0, 25, 200);

    TextCloseAction action = { w, 0, true };
    dmPlatform::SetKeyboardCharCallback(w, CloseOnFirstCharacter, &action);
    wl_array keys = {};
    keyboard.enter(w, 0, 1, w->m_Surface, &keys);
    keyboard.key(w, 0, 1, 0, KEY_A, WL_KEYBOARD_KEY_STATE_PRESSED);
    EXPECT_EQ(0u, w->m_Wayland->m_NextRepeat);
    dmPlatform::WaylandRepeat(w, w->m_Wayland->m_NextRepeat);
    EXPECT_EQ(1u, action.m_Count);

    keyboard.key(w, 0, 2, 0, KEY_C, WL_KEYBOARD_KEY_STATE_PRESSED);
    EXPECT_EQ(0, dmPlatform::GetKey(w, 'C'));
    EXPECT_EQ(0u, w->m_Wayland->m_NextRepeat);
    EXPECT_EQ(1u, action.m_Count);
    keyboard.enter(w, 0, 3, w->m_Surface, &keys);
    // A leave for a surface that was destroyed can arrive with a null surface.
    keyboard.leave(w, 0, 4, 0);
    keyboard.key(w, 0, 5, 0, KEY_C, WL_KEYBOARD_KEY_STATE_PRESSED);
    EXPECT_EQ(1, dmPlatform::GetKey(w, 'C'));
    EXPECT_NE(0u, w->m_Wayland->m_NextRepeat);
    EXPECT_EQ(2u, action.m_Count);

    keyboard.keymap(w, 0, WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP, dup(fileno(keymap)), 0);
    fclose(keymap);
    dmPlatform::DeleteWindow(w);
}

static void CopyOnWaylandFocus(void* data, uint32_t focused)
{
    HWindow w = (HWindow)data;
    if (focused && !w->m_Wayland->m_DataSource)
    {
        const char text[] = "Defold focus clipboard";
        dmPlatform::SetLinuxClipboard(w, text, sizeof(text) - 1);
    }
}

// The driver pastes the focus callback's selection before sending the first button or key event.
static int InputTest()
{
    HWindow w = dmPlatform::NewWindow();
    WindowCreateParams p = Params();
    // Older compositors grant focus during OpenWindow, before callbacks are enabled.
    p.m_Hidden = true;
    p.m_Fullscreen = true;
    if (w && w->m_Wayland)
    {
        p.m_FocusCallback = CopyOnWaylandFocus;
        p.m_FocusCallbackUserData = w;
    }

    const char* expected = getenv("DM_TEST_WINDOW_BACKEND");
    if (!w || (expected && strcmp(w->m_Ops->m_Name, expected)) || dmPlatform::OpenWindow(w, p) != WINDOW_RESULT_OK)
    {
        dmPlatform::DeleteWindow(w);
        return 1;
    }

    int text = 0;
    dmPlatform::SetKeyboardCharCallback(w, Character, &text);
    dmPlatform::ShowWindow(w);
    dmPlatform::SwapBuffers(w);
    puts("READY");
    fflush(stdout);
    enum
    {
        POINTER, FOCUS, PRESS, RELEASE, MOVE, ACCENT, COMPOSE, SHORTCUT, ALTGR, COPY, PASTE, DONE
    };
    int step = POINTER;
    int32_t x = 0, y = 0;
    pollfd input = { STDIN_FILENO, POLLIN, 0 };
    for (;;)
    {
        // The driver bounds each stage and keeps this pipe open until it acknowledges the paste.
        if (poll(&input, 1, 0) > 0)
        {
            char ack;
            if (step == PASTE && (input.revents & POLLIN) && read(input.fd, &ack, 1) == 1)
            {
                step = DONE;
            }

            break;
        }

        dmPlatform::PollEvents(w);
        if (step == POINTER && (!w->m_Wayland || (w->m_Focused && w->m_Wayland->m_PointerSurface == w->m_Surface)))
        {
            if (w->m_Wayland && g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay) < 0)
            {
                break;
            }

            puts("POINTER_READY");
            ++step;
        }
        else if (step == FOCUS && w->m_Focused && dmPlatform::GetMouseButton(w, 0))
        {
            puts("KEYBOARD_READY");
            ++step;
        }
        else if (step == PRESS && text == 'a' && dmPlatform::GetKey(w, 'A') && dmPlatform::GetMouseButton(w, 0))
        {
            puts("PRESSED");
            ++step;
        }
        else if (step == RELEASE && !dmPlatform::GetKey(w, 'A') && !dmPlatform::GetMouseButton(w, 0) && dmPlatform::GetMouseWheel(w) > 0)
        {
            dmPlatform::SetDeviceState(w, WINDOW_DEVICE_STATE_CURSOR, false);
            if (!w->m_Wayland || w->m_Wayland->m_PointerLocked)
            {
                dmPlatform::GetMousePosition(w, &x, &y);
                puts("LOCKED");
                ++step;
            }
        }
        else if (step == MOVE)
        {
            int32_t mx, my;
            dmPlatform::GetMousePosition(w, &mx, &my);
            if (mx != x || my != y)
            {
                dmPlatform::SetDeviceState(w, WINDOW_DEVICE_STATE_CURSOR, true);
                puts("MOVED");
                ++step;
            }
        }
        else if (step == ACCENT && dmPlatform::GetKey(w, '\''))
        {
            puts("ACCENT_DOWN");
            ++step;
        }
        else if (step == COMPOSE && text == 0xe9)
        {
            puts("COMPOSED");
            ++step;
        }
        else if (step == SHORTCUT && dmPlatform::GetKey(w, 'K') && text == 0xe9)
        {
            puts("SHORTCUT");
            ++step;
        }
        else if (step == ALTGR && text == 0x20ac)
        {
            puts("ALTGR");
            ++step;
        }
        else if (step == COPY && dmPlatform::GetKey(w, 'B'))
        {
            const char text[] = "Defold clipboard";
            dmPlatform::SetLinuxClipboard(w, text, sizeof(text) - 1);
            dmPlatform::SetLinuxLinkCursor(w, true);
            // The paste client opens a separate connection as soon as it sees COPIED.
            bool owned = w->m_WlDisplay ? g_WaylandClient.wl_display_roundtrip_(w->m_WlDisplay) >= 0 :
                         XGetSelectionOwner(w->m_Display, XInternAtom(w->m_Display, "CLIPBOARD", False)) != None;
            if (!owned)
            {
                dmPlatform::DeleteWindow(w);
                return 1;
            }

            puts("COPIED");
            ++step;
        }

        fflush(stdout);
    }

    dmPlatform::HideWindow(w);
    dmPlatform::ShowWindow(w);
    bool shown = w->m_Shown;
    if (shown)
    {
        dmPlatform::SwapBuffers(w);
    }

    dmPlatform::DeleteWindow(w);
    return step == DONE && shown ? 0 : 1;
}

int main(int argc, char** argv)
{
    dmPlatform::VulkanSetLoader(vkGetInstanceProcAddr);
    if (argc == 2 && (!strcmp(argv[1], "--reject-glx-context") || !strcmp(argv[1], "--reject-egl-context")))
    {
        HWindow w = dmPlatform::NewWindow();
        bool glx = !strcmp(argv[1], "--reject-glx-context");
        if (!w || (glx ? !w->m_X11 : !w->m_Wayland))
        {
            return 1;
        }

        WindowCreateParams p = Params();
        p.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
        p.m_GraphicsApiVersionHint = 46;
        p.m_OpenGLUseCoreProfileHint = true;
        bool rejected = dmPlatform::OpenWindow(w, p) == WINDOW_RESULT_WINDOW_OPEN_ERROR;
        dmPlatform::DeleteWindow(w);
        return rejected ? 0 : 1;
    }

    if (argc == 2 && !strcmp(argv[1], "--input"))
    {
        return InputTest();
    }

    if (argc == 2 && !strcmp(argv[1], "--no-display"))
    {
        uint32_t count = 99;
        if (dmPlatform::VulkanGetRequiredInstanceExtensions(&count) || count)
        {
            return 1;
        }

        return dmPlatform::NewWindow() ? 1 : 0;
    }

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
