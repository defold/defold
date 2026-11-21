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
#include "../linux/window_wayland.h"
#include "../linux/joystick_linux.h"
#include <vulkan/vulkan.h>
#include "../platform_window_vulkan.h"
#include <dlib/hash.h>
#include <dlib/time.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Exercise EGL 1.4 drivers that export core GL functions only from their client library.
static PFNEGLGETPROCADDRESSPROC g_EGLGetProcAddress;
static __eglMustCastToProperFunctionPointerType EGLAPIENTRY ExtensionProcAddress(const char* name)
{
    return strcmp(name, "glGenTextures") ? g_EGLGetProcAddress(name) : 0;
}

extern "C" void* __real_dlsym(void* handle, const char* name);
extern "C" void* __wrap_dlsym(void* handle, const char* name)
{
    void* symbol = __real_dlsym(handle, name);
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
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmArray<dmWindowState::Monitor> monitors;
    dmPlatform::X11Monitors(w, monitors);
    dmPlatform::DeleteWindow(w);

    ASSERT_EQ(1u, monitors.Size());
    ASSERT_EQ(dmHashString64(expected), monitors[0].m_NameHash);
    ASSERT_GT(monitors[0].m_WorkArea.m_Width, 0);
    ASSERT_GT(monitors[0].m_WorkArea.m_Height, 0);
}

// Window managers match game launchers by the initial title and the optional X resource instance.
TEST(LinuxWindow, X11WindowClass)
{
    if (!getenv("DISPLAY"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
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

// A live XKB remap on the test runner's private X server must change the reported physical key.
TEST(LinuxWindow, X11PhysicalKeymapChanges)
{
    if (!getenv("DM_TEST_X11_REMAP_KEYS"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    Display* display = w->m_Display;
    KeyCode q = XKeysymToKeycode(display, XK_q), other = XKeysymToKeycode(display, XK_w);
    ASSERT_NE(0, q);
    ASSERT_NE(0, other);
    XkbDescPtr keyboard = XkbGetMap(display, 0, XkbUseCoreKbd);
    ASSERT_TRUE(keyboard);
    ASSERT_EQ(Success, XkbGetNames(display, XkbKeyNamesMask, keyboard));
    XkbKeyNameRec* names = keyboard->names->keys;
    XkbKeyNameRec original = names[q];
    names[q] = names[other];
    names[other] = original;
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

    names[other] = names[q];
    names[q] = original;
    EXPECT_TRUE(XkbSetNames(display, XkbKeyNamesMask, 0, 0, keyboard));
    XSync(display, False);
    XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
    dmPlatform::DeleteWindow(w);
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

// Locked X11 motion follows processed pointer coordinates, including batched moves and engine recentering.
TEST(LinuxWindow, X11LockedCursorMotion)
{
    if (!getenv("DISPLAY"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    ASSERT_TRUE(w->m_X11);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmPlatform::ShowWindow(w);
    Display* d = w->m_Display;
    Window window = w->m_Xid;
    XSetInputFocus(d, window, RevertToParent, CurrentTime);
    const int cx = w->m_Width / 2, cy = w->m_Height / 2;
    const int start_x = cx - 30, start_y = cy + 20;
    XWarpPointer(d, None, window, 0, 0, 0, 0, start_x, start_y);
    ExpectX11MousePosition(w, start_x, start_y);
    ASSERT_TRUE(w->m_Focused);

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
    ExpectX11MousePosition(w, start_x, start_y);
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

// Presenting into a hidden Vulkan surface must not map a desktop window until ShowWindow.
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
    dmPlatform::ShowWindow(w);
    EXPECT_TRUE(w->m_Wayland->m_Configured);
    EXPECT_TRUE(w->m_Wayland->m_Frame || w->m_Wayland->m_Toplevel);
    dmPlatform::HideWindow(w);
    EXPECT_FALSE(w->m_Wayland->m_Frame || w->m_Wayland->m_Toplevel);
    dmPlatform::DeleteWindow(w);
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

// The compositor can send different adjusted and unaccelerated deltas for the same Wayland motion.
TEST(LinuxWindow, WaylandLockedCursorMotion)
{
    dmWindow w = {};
    w.m_MouseX = 10.0;
    w.m_MouseY = 20.0;
    const wl_fixed_t dx = wl_fixed_from_double(2.5), dy = wl_fixed_from_double(-1.25);
    dmPlatform::g_WaylandRelativePointerListener.relative_motion(&w, 0, 0, 0, dx, dy, wl_fixed_from_int(9), wl_fixed_from_int(-7));
    ASSERT_EQ(12.5, w.m_MouseX);
    ASSERT_EQ(18.75, w.m_MouseY);
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
    ASSERT_STREQ(getenv("DM_TEST_WINDOW_BACKEND"), w->m_Ops->m_Name);
    const char** live = dmPlatform::VulkanGetRequiredInstanceExtensions(&count);
    ASSERT_EQ(2u, count);
    ASSERT_STREQ(early, live[1]);
    if (w->m_X11)
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
    if (strcmp(getenv("DM_TEST_WINDOW_BACKEND"), "x11"))
    {
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
        SKIP();
    }

    ASSERT_EQ(VK_SUCCESS, result);
    HWindow w = dmPlatform::NewWindow();
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
};

static void* UploadTexture(void* data)
{
    TextureUpload* upload = (TextureUpload*)data;
    void* aux = dmPlatform::AcquireAuxContext(upload->m_Window);
    if (aux)
    {
        upload->m_Generate(1, &upload->m_Texture);
        upload->m_Bind(GL_TEXTURE_2D, upload->m_Texture);
        upload->m_Parameter(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        upload->m_Image(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload->m_Pixel);
        upload->m_Finish();
        dmPlatform::UnacquireAuxContext(upload->m_Window, aux);
    }

    return 0;
}

// A texture uploaded on the resource worker must be readable by the render context after resize and reopen.
TEST(LinuxWindow, SharedOpenGLContext)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    const char* scale = getenv("DM_TEST_WAYLAND_SCALE");
    const unsigned expected_scale = w->m_Wayland && scale ? atoi(scale) : 1;
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        p.m_HighDPI = cycle != 0;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        dmPlatform::ShowWindow(w);
        dmPlatform::SetSwapInterval(w, 0);
        ASSERT_TRUE(dmPlatform::GetWindowStateParam(w, WINDOW_STATE_AUX_CONTEXT));
        ASSERT_EQ(w->m_X11 != 0 && p.m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL, dmPlatform::GetX11GLXContext(w) != 0);
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
        dmPlatform::SwapBuffers(w);
        uint64_t deadline = dmTime::GetMonotonicTime() + 3000000;
        while (dmPlatform::GetDisplayScaleFactor(w) != expected_scale && dmTime::GetMonotonicTime() < deadline)
        {
            dmPlatform::PollEvents(w);
        }

        ASSERT_EQ((float)expected_scale, dmPlatform::GetDisplayScaleFactor(w));
        dmPlatform::SetWindowSize(w, 421, 307);
        dmPlatform::SwapBuffers(w);
        dmPlatform::PollEvents(w);
        ASSERT_EQ(421u * expected_scale, dmPlatform::GetWindowWidth(w));
        ASSERT_EQ(307u * expected_scale, dmPlatform::GetWindowHeight(w));
        uint32_t width, height;
        ASSERT_TRUE(dmPlatform::GetWindowClientSize(w, &width, &height));
        ASSERT_EQ(421u, width);
        ASSERT_EQ(307u, height);
        dmPlatform::HideWindow(w);
        ASSERT_FALSE(w->m_Shown);
        dmPlatform::SwapBuffers(w);
        dmPlatform::ShowWindow(w);
        ASSERT_TRUE(w->m_Shown);
        dmPlatform::SwapBuffers(w);
        dmPlatform::PollEvents(w);
        ASSERT_EQ((float)expected_scale, dmPlatform::GetDisplayScaleFactor(w));
        ASSERT_EQ(421u * expected_scale, dmPlatform::GetWindowWidth(w));
        dmPlatform::CloseWindow(w);
        ASSERT_TRUE(dmPlatform::GetX11GLXContext(w) == 0);
    }

    dmPlatform::DeleteWindow(w);
}

static void Character(void* data, int character)
{
    *(int*)data = character;
}

// X11 can return a composed character in a reused buffer without terminating it.
TEST(LinuxWindow, TextUsesLookupLength)
{
    dmWindow window = {};
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

static int InputTest()
{
    HWindow w = dmPlatform::NewWindow();
    WindowCreateParams p = Params();
    p.m_Fullscreen = true;
    if (!w || strcmp(w->m_Ops->m_Name, getenv("DM_TEST_WINDOW_BACKEND")) || dmPlatform::OpenWindow(w, p) != WINDOW_RESULT_OK)
    {
        return 1;
    }

    int text = 0;
    dmPlatform::SetKeyboardCharCallback(w, Character, &text);
    dmPlatform::ShowWindow(w);
    dmPlatform::SwapBuffers(w);
    puts("READY");
    fflush(stdout);
    int step = 0;
    int32_t x = 0, y = 0;
    uint64_t deadline = dmTime::GetMonotonicTime() + 20000000;
    while (step < 7 && dmTime::GetMonotonicTime() < deadline)
    {
        dmPlatform::PollEvents(w);
        if (step == 0 && text == 'a' && dmPlatform::GetKey(w, 'A') && dmPlatform::GetMouseButton(w, 0))
        {
            puts("PRESSED");
            ++step;
        }
        else if (step == 1 && !dmPlatform::GetKey(w, 'A') && !dmPlatform::GetMouseButton(w, 0) && dmPlatform::GetMouseWheel(w) > 0)
        {
            dmPlatform::SetDeviceState(w, WINDOW_DEVICE_STATE_CURSOR, false);
            dmPlatform::GetMousePosition(w, &x, &y);
            puts("LOCKED");
            ++step;
        }
        else if (step == 2)
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
        else if (step == 3 && dmPlatform::GetKey(w, '\''))
        {
            puts("ACCENT_DOWN");
            ++step;
        }
        else if (step == 4 && text == 0xe9)
        {
            puts("COMPOSED");
            ++step;
        }
        else if (step == 5 && dmPlatform::GetKey(w, 'B'))
        {
            const char text[] = "Defold clipboard";
            dmPlatform::SetLinuxClipboard(w, text, sizeof(text) - 1);
            dmPlatform::SetLinuxLinkCursor(w, true);
            puts("COPIED");
            ++step;
        }
        else if (step == 6)
        {
            pollfd input = { STDIN_FILENO, POLLIN, 0 };
            if (poll(&input, 1, 0) == 1)
            {
                ++step;
            }
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
    return step == 7 && shown ? 0 : 1;
}

int main(int argc, char** argv)
{
    dmPlatform::VulkanSetLoader(vkGetInstanceProcAddr);
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
