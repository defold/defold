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
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
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
    if (!strcmp(name, "XGetXCBConnection") && getenv("DM_TEST_X11_NO_XCB"))
    {
        return 0;
    }

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

static int g_ResizeFd = -1;
static unsigned g_ResizeWaits;
extern "C" int __real_poll(struct pollfd*, nfds_t, int);
extern "C" int __wrap_poll(struct pollfd* fds, nfds_t count, int timeout)
{
    if (count == 1 && fds[0].fd == g_ResizeFd && timeout > 0)
    {
        ++g_ResizeWaits;
    }

    return __real_poll(fds, count, timeout);
}

// The game thread submits a resize even when the manager chooses different dimensions.
TEST(LinuxWindow, X11ConstrainedResize)
{
    if (!getenv("DISPLAY"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
    p.m_Hidden = true;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
    dmPlatform::SetWindowSize(w, 375, 255);
    EXPECT_EQ(375u, dmPlatform::GetWindowWidth(w));
    EXPECT_EQ(255u, dmPlatform::GetWindowHeight(w));
    Display* manager = XOpenDisplay(0);
    ASSERT_TRUE(manager);
    Window parent = XCreateSimpleWindow(manager, DefaultRootWindow(manager), 0, 0, 640, 480, 0, 0, 0);
    XSetWindowAttributes attributes = {};
    attributes.override_redirect = True;
    XChangeWindowAttributes(manager, parent, CWOverrideRedirect, &attributes);
    XMapWindow(manager, parent);
    XReparentWindow(manager, w->m_Xid, parent, 0, 0);
    XSync(manager, False);
    dmPlatform::ShowWindow(w);
    XSelectInput(manager, parent, SubstructureRedirectMask);
    XSync(manager, False);
    g_ResizeWaits = 0;
    g_ResizeFd = ConnectionNumber(w->m_Display);
    dmPlatform::SetWindowSize(w, 427, 311);
    g_ResizeFd = -1;
    EXPECT_EQ(0u, g_ResizeWaits);
    XSync(w->m_Display, False);
    XEvent request;
    ASSERT_TRUE(XCheckTypedWindowEvent(manager, parent, ConfigureRequest, &request));
    EXPECT_EQ(427, request.xconfigurerequest.width);
    XResizeWindow(manager, w->m_Xid, 420, 300);
    XSync(manager, False);
    dmPlatform::PollEvents(w);
    EXPECT_EQ(420u, dmPlatform::GetWindowWidth(w));
    EXPECT_EQ(300u, dmPlatform::GetWindowHeight(w));
    dmPlatform::DeleteWindow(w);
    XDestroyWindow(manager, parent);
    XCloseDisplay(manager);
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
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
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

struct TestClipboardManager
{
    Display* m_Display;
    Window   m_Window;
};

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
    Atom pairs[] = { utf8, utf8, XA_STRING, XA_STRING };
    XChangeProperty(d, window, multiple, XInternAtom(d, "ATOM_PAIR", False), 32, PropModeReplace, (unsigned char*)pairs, 4);
    XConvertSelection(d, clipboard, multiple, multiple, window, CurrentTime);
    XEvent converted;
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

// Closing the game transfers the full clipboard to the manager, including text larger than a core X11 request.
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
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
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
    pthread_t worker;
    ASSERT_EQ(0, pthread_create(&worker, 0, SaveClipboardSelection, &manager));
    dmPlatform::DeleteWindow(w);
    void* saved = 0;
    ASSERT_EQ(0, pthread_join(worker, &saved));
    EXPECT_TRUE(saved == &manager);
    EXPECT_EQ(manager.m_Window, XGetSelectionOwner(d, XInternAtom(d, "CLIPBOARD", False)));
    Atom formats[] = { XInternAtom(d, "UTF8_STRING", False), XA_STRING };
    for (unsigned i = 0; i < 2; ++i)
    {
        Atom type;
        int format;
        unsigned long size, left;
        unsigned char* bytes = 0;
        EXPECT_EQ(Success, XGetWindowProperty(d, manager.m_Window, formats[i], 0, (text.Size() + 3) / 4, False, formats[i], &type, &format, &size, &left, &bytes));
        EXPECT_TRUE(type == formats[i] && format == 8 && size == text.Size() && !left && !memcmp(bytes, text.Begin(), size));
        XFree(bytes);
    }

    XCloseDisplay(d);
}

// ICCCM-only window managers signal minimize and restore through WM_STATE.
TEST(LinuxWindow, X11IconicState)
{
    if (!getenv("DISPLAY"))
    {
        SKIP();
    }

    HWindow w = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, w);
    WindowCreateParams p = Params();
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
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
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
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
    p.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;
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

static void FrameDone(void* data, wl_callback*, uint32_t)
{
    *(bool*)data = true;
}

// A visible fullscreen window must present its first EGL buffer directly after WindowOpen.
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

static void CountRepeatedKey(void* data, int character)
{
    EXPECT_EQ('a', character);
    ++*(unsigned*)data;
}

// A slow frame must deliver each elapsed Wayland repeat and keep the next deadline on the keyboard's schedule.
TEST(LinuxWindow, WaylandRepeatAcrossSlowFrames)
{
    ASSERT_TRUE(LoadWaylandClient());
    LinuxWayland wl = {};
    wl.m_Xkb = g_WaylandClient.xkb_context_new_(XKB_CONTEXT_NO_FLAGS);
    const char* keymap = "xkb_keymap { xkb_keycodes { include \"evdev\" }; xkb_types { include \"complete\" }; "
                         "xkb_compatibility { include \"complete\" }; xkb_symbols { include \"pc+us\" }; };";
    wl.m_Keymap = g_WaylandClient.xkb_keymap_new_from_string_(wl.m_Xkb, keymap, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    ASSERT_TRUE(wl.m_Keymap);
    wl.m_KeyState = g_WaylandClient.xkb_state_new_(wl.m_Keymap);
    ASSERT_TRUE(wl.m_KeyState);
    wl.m_RepeatKey = KEY_A;
    wl.m_RepeatRate = 25;
    wl.m_NextRepeat = 100000;
    dmWindow w = {};
    w.m_Wayland = &wl;
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

static void CheckMultisampleFallback(HWindow w, WindowsGraphicsApi api)
{
    WindowCreateParams p = Params();
    p.m_GraphicsApi = api;
    p.m_Hidden = true;
    const unsigned requests[] = { 4, 1, 1024 };
    GLint samples[3] = {};
    for (unsigned i = 0; i < 3; ++i)
    {
        p.m_Samples = requests[i];
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(w, p));
        __typeof__(&::glGetIntegerv) get_integer = (__typeof__(&::glGetIntegerv)) dmPlatform::GetProcAddress(w, "glGetIntegerv");
        ASSERT_TRUE(get_integer);
        get_integer(GL_SAMPLES, &samples[i]);
        dmPlatform::CloseWindow(w);
    }

    if (samples[0] > 0)
    {
        EXPECT_GT(samples[1], 0);
        EXPECT_LE(samples[1], samples[0]);
    }

    EXPECT_GE(samples[2], samples[0]);
}

// EGL must retain available multisampling for requests outside the driver's sample range.
TEST(LinuxWindow, EGLMultisampleFallback)
{
    HWindow w = dmPlatform::NewWindow();
    ASSERT_TRUE(w);
    CheckMultisampleFallback(w, WINDOW_GRAPHICS_API_OPENGLES);
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
        deadline = dmTime::GetMonotonicTime() + 3000000;
        do
        {
            dmPlatform::PollEvents(w);
        } while (dmPlatform::GetWindowWidth(w) != 421u * expected_scale && dmTime::GetMonotonicTime() < deadline);
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
        if (step == POINTER && (!w->m_Wayland || w->m_Wayland->m_PointerSurface == w->m_Surface))
        {
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
                         XGetSelectionOwner(w->m_Display, XInternAtom(w->m_Display, "CLIPBOARD", False)) == w->m_Xid;
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
    if (argc == 2 && !strcmp(argv[1], "--reject-glx-context"))
    {
        HWindow w = dmPlatform::NewWindow();
        if (!w || !w->m_X11)
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
