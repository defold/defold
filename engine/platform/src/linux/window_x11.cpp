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

#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <dlfcn.h>
#include <dlib/hash.h>
#include <dlib/math.h>
#include <cmath>
#include <dlib/array.h>
#include <jc/ringbuffer.h>
#include <limits.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <locale.h>
#include <dlib/time.h>

struct X11ResizeRequest
{
    unsigned long m_Serial;
    int           m_Width, m_Height;
    uint32_t      m_Count;
};

struct LinuxX11
{
    Colormap      m_ColorMap;
    XIM           m_IM;
    XIC           m_IC;
    bool          m_IMRegistered;
    Cursor        m_Blank, m_Arrow, m_Hand;
    int           m_RandREventBase;
    int           m_XkbEventBase;
    int16_t       m_KeyCodes[256];
    int           m_LastCursorX, m_LastCursorY, m_RestoreCursorX, m_RestoreCursorY;
    int           m_WarpX, m_WarpY;
    unsigned long m_GrabSerial, m_WarpSerial;
    unsigned long m_ConfigureSerial;
    unsigned long m_SizeReadSerial;
    uint64_t      m_LayoutHash;
    int           m_RequestX, m_RequestY;
    bool          m_RequestIsSize, m_ClampAfterResize;
    Atom          m_Delete, m_Ping, m_Protocols, m_State, m_Clipboard, m_Targets, m_Utf8;
    Atom          m_ClipboardManager, m_SaveTargets, m_Multiple, m_AtomPair;
    Window        m_ClipboardWindow;
    Atom          m_WmState, m_HiddenAtom, m_MaxVert, m_MaxHorz;
    Atom          m_WorkArea, m_CurrentDesktop;
    dmArray<char> m_Text;

    jc::RingBuffer<X11ResizeRequest> m_ResizeRequests;
};

namespace dmPlatform
{
#define RANDR_FUNCTIONS(F) \
    F(QueryExtension) \
    F(QueryVersion) \
    F(SelectInput) \
    F(UpdateConfiguration) \
    F(GetScreenResourcesCurrent) \
    F(FreeScreenResources) \
    F(GetOutputPrimary) \
    F(GetOutputInfo) \
    F(FreeOutputInfo) \
    F(GetCrtcInfo) \
    F(FreeCrtcInfo)

    struct RandRFunctions
    {
#define FIELD(name) __typeof__(&XRR##name) name;
        RANDR_FUNCTIONS(FIELD)
#undef FIELD
    };
    static RandRFunctions g_RandR;

    static bool LoadRandR()
    {
        void* library = dlopen("libXrandr.so.2", RTLD_NOW | RTLD_LOCAL);
        if (!library)
        {
            return false;
        }

        RandRFunctions functions;
        bool complete = true;
#define LOAD(name) \
        functions.name = (__typeof__(functions.name))dlsym(library, "XRR" #name); \
        complete &= functions.name != 0;
        RANDR_FUNCTIONS(LOAD)
#undef LOAD
#undef RANDR_FUNCTIONS
        if (!complete)
        {
            dlclose(library);
            return false;
        }

        // Xlib keeps RandR callbacks on each display. Keep their library loaded across windows.
        g_RandR = functions;
        return true;
    }

    static Atom AtomFor(HWindow w, const char* name)
    {
        return XInternAtom(w->m_Display, name, False);
    }

    static Window Root(HWindow w)
    {
        return DefaultRootWindow(w->m_Display);
    }

    static bool ReadProperty(HWindow w, Window owner, Atom atom, Atom type, long limit, dmArray<unsigned long>& result)
    {
        Atom actual;
        int format;
        unsigned long length, left;
        unsigned char* data = 0;
        result.SetSize(0);
        bool valid = XGetWindowProperty(w->m_Display, owner, atom, 0, limit, False, type, &actual, &format, &length, &left, &data) == Success &&
                     data && actual == type && format == 32;
        if (valid)
        {
            result.EnsureSize(length);
            if (length)
            {
                memcpy(result.Begin(), data, length * sizeof(unsigned long));
            }
        }

        XFree(data);
        return valid && !left;
    }

    static void Message(HWindow w, Atom type, long a, long b = 0, long c = 0)
    {
        XEvent event = {};
        event.xclient.type = ClientMessage;
        event.xclient.window = w->m_Xid;
        event.xclient.message_type = type;
        event.xclient.format = 32;
        event.xclient.data.l[0] = a;
        event.xclient.data.l[1] = b;
        event.xclient.data.l[2] = c;
        XSendEvent(w->m_Display, Root(w), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }

    static float Scale(HWindow w)
    {
        char* resources = XResourceManagerString(w->m_Display);
        XrmDatabase db = resources ? XrmGetStringDatabase(resources) : 0;
        float scale = 1.0f;
        XrmValue value;
        char* type;
        if (db && XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr)
        {
            float dpi = strtof(value.addr, 0);
            if (dpi >= 24 && dpi <= 1536)
            {
                scale = dpi / 96.0f;
            }
        }

        if (db)
        {
            XrmDestroyDatabase(db);
        }

        return scale;
    }

    struct Output
    {
        dmWindowState::Monitor m_Monitor;
        dmWindowState::Rect    m_Bounds;
        int                    m_MM, m_Refresh;
        bool                   m_Primary;
    };
    static void Outputs(HWindow w, dmArray<Output>& outputs)
    {
        Display* display = w->m_Display;
        Window root = Root(w);
        float scale = Scale(w);
        XRRScreenResources* resources = w->m_X11->m_RandREventBase ? g_RandR.GetScreenResourcesCurrent(display, root) : 0;
        outputs.SetCapacity(resources ? resources->noutput + 1 : 1);
        if (resources)
        {
            RROutput primary = g_RandR.GetOutputPrimary(display, root);
            for (int i = 0; i < resources->noutput; ++i)
            {
                RROutput handle = resources->outputs[i];
                XRROutputInfo* info = g_RandR.GetOutputInfo(display, resources, handle);
                XRRCrtcInfo* crtc = info && info->connection == RR_Connected && info->crtc ? g_RandR.GetCrtcInfo(display, resources, info->crtc) : 0;
                if (crtc && crtc->width && crtc->height)
                {
                    Output o = {};
                    o.m_Bounds = { crtc->x, crtc->y, (int)crtc->width, (int)crtc->height };
                    o.m_Monitor.m_NameHash = dmHashBuffer64(info->name, info->nameLen);
                    o.m_Primary = handle == primary;
                    o.m_MM = crtc->rotation & (RR_Rotate_90 | RR_Rotate_270) ? info->mm_height : info->mm_width;
                    for (int m = 0; m < resources->nmode; ++m)
                    {
                        XRRModeInfo& mode = resources->modes[m];
                        if (mode.id == crtc->mode && mode.hTotal && mode.vTotal)
                        {
                            o.m_Refresh = (int)lround((double)mode.dotClock / mode.hTotal / mode.vTotal);
                        }
                    }

                    outputs.Push(o);
                    if (o.m_Primary)
                    {
                        outputs[outputs.Size() - 1] = outputs[0];
                        outputs[0] = o;
                    }
                }

                if (crtc)
                {
                    g_RandR.FreeCrtcInfo(crtc);
                }

                if (info)
                {
                    g_RandR.FreeOutputInfo(info);
                }
            }

            g_RandR.FreeScreenResources(resources);
        }

        if (outputs.Empty())
        {
            Output o = {};
            int screen = DefaultScreen(display);
            o.m_Bounds = { 0, 0, DisplayWidth(display, screen), DisplayHeight(display, screen) };
            o.m_MM = DisplayWidthMM(display, screen);
            o.m_Monitor.m_NameHash = dmHashString64("Display");
            outputs.Push(o);
        }

        dmArray<unsigned long> areas;
        ReadProperty(w, root, w->m_X11->m_WorkArea, XA_CARDINAL, 4096, areas);
        dmArray<unsigned long> desktop;
        ReadProperty(w, root, w->m_X11->m_CurrentDesktop, XA_CARDINAL, 1, desktop);
        size_t index = desktop.Empty() ? 0 : desktop[0];
        for (uint32_t i = 0; i < outputs.Size(); ++i)
        {
            Output& o = outputs[i];
            dmWindowState::Rect area = o.m_Bounds;
            if (index < areas.Size() / 4)
            {
                const unsigned long* work = &areas[index * 4];
                int x = dmMath::Max(area.m_X, (int)work[0]), y = dmMath::Max(area.m_Y, (int)work[1]);
                int right = dmMath::Min(area.m_X + area.m_Width, (int)(work[0] + work[2]));
                int bottom = dmMath::Min(area.m_Y + area.m_Height, (int)(work[1] + work[3]));
                if (right > x && bottom > y)
                {
                    area = { x, y, right - x, bottom - y };
                }
            }

            o.m_Monitor.m_WorkArea = area;
            o.m_Monitor.m_ScaleX = o.m_Monitor.m_ScaleY = scale;
        }
    }

    void X11Monitors(HWindow w, dmArray<dmWindowState::Monitor>& monitors)
    {
        dmArray<Output> outputs;
        Outputs(w, outputs);
        monitors.SetCapacity(outputs.Size());
        for (uint32_t i = 0; i < outputs.Size(); ++i)
        {
            const Output& o = outputs[i];
            monitors.Push(o.m_Monitor);
        }
    }

    void X11FrameExtents(HWindow w, int* left, int* top, int* right, int* bottom)
    {
        dmArray<unsigned long> frame;
        ReadProperty(w, w->m_Xid, AtomFor(w, "_NET_FRAME_EXTENTS"), XA_CARDINAL, 4, frame);
        if (frame.Size() == 4)
        {
            *left = frame[0];
            *right = frame[1];
            *top = frame[2];
            *bottom = frame[3];
        }
    }

    void X11GetPosition(HWindow w, int* x, int* y)
    {
        Window child;
        XTranslateCoordinates(w->m_Display, w->m_Xid, Root(w), 0, 0, x, y, &child);
    }

    static bool WaitForMap(HWindow w)
    {
        uint64_t end = dmTime::GetMonotonicTime() + 250000;
        XWindowAttributes attrs;
        while (XGetWindowAttributes(w->m_Display, w->m_Xid, &attrs))
        {
            if (attrs.map_state == IsViewable)
            {
                return true;
            }

            uint64_t now = dmTime::GetMonotonicTime();
            if (now >= end)
            {
                break;
            }

            pollfd descriptor = { ConnectionNumber(w->m_Display), POLLIN, 0 };
            poll(&descriptor, 1, dmMath::Min(10, (int)((end - now) / 1000) + 1));
        }

        return false;
    }

    static void RecordGeometryRequest(HWindow w, bool size, int x, int y)
    {
        if (w->m_Placement && w->m_Shown)
        {
            LinuxX11* state = w->m_X11;
            state->m_ConfigureSerial = NextRequest(w->m_Display);
            state->m_RequestIsSize = size;
            state->m_RequestX = x;
            state->m_RequestY = y;
            state->m_ClampAfterResize |= size;
        }
    }

    static Bool ObserveConfigure(Display*, XEvent* event, XPointer data)
    {
        HWindow w = (HWindow)data;
        if (event->type == ConfigureNotify && event->xconfigure.window == w->m_Xid &&
            event->xconfigure.serial >= w->m_X11->m_ConfigureSerial)
        {
            w->m_X11->m_ConfigureSerial = 0;
        }

        // Leave the event in order for normal input and resize callback dispatch.
        return False;
    }

    static void CompleteGeometryRequest(HWindow w)
    {
        LinuxX11* state = w->m_X11;
        state->m_ConfigureSerial = 0;
        if (state->m_ClampAfterResize)
        {
            state->m_ClampAfterResize = false;
            ClampWindowPlacement(w);
        }
    }

    void X11FlushPlacement(HWindow w)
    {
        LinuxX11* state = w->m_X11;
        if (!state->m_ConfigureSerial)
        {
            return;
        }

        // Only a forced placement save (for instance, on close) needs to wait for
        // the WM. A ConfigureNotify with a constrained size or position is final.
        int x, y;
        if (state->m_RequestIsSize)
        {
            X11ReadSize(w);
            x = w->m_Width;
            y = w->m_Height;
        }
        else
        {
            X11GetPosition(w, &x, &y);
        }

        if (x != state->m_RequestX || y != state->m_RequestY)
        {
            uint64_t end = dmTime::GetMonotonicTime() + 250000;
            for (;;)
            {
                XEvent unused;
                XCheckIfEvent(w->m_Display, &unused, ObserveConfigure, (XPointer)w);
                uint64_t now = dmTime::GetMonotonicTime();
                if (!state->m_ConfigureSerial || now >= end)
                {
                    break;
                }

                pollfd fd = { ConnectionNumber(w->m_Display), POLLIN, 0 };
                poll(&fd, 1, (int)((end - now) / 1000) + 1);
            }
        }

        X11ReadSize(w);
        CompleteGeometryRequest(w);
        if (state->m_ConfigureSerial)
        {
            // Clamping a resized window can issue one final move before saving.
            X11FlushPlacement(w);
        }
    }

    void X11SetPosition(HWindow w, int x, int y)
    {
        if (!w->m_Shown)
        {
            XSizeHints hints = {};
            long supplied;
            XGetWMNormalHints(w->m_Display, w->m_Xid, &hints, &supplied);
            hints.flags |= PPosition;
            hints.x = x;
            hints.y = y;
            XSetWMNormalHints(w->m_Display, w->m_Xid, &hints);
        }

        RecordGeometryRequest(w, false, x, y);
        XMoveWindow(w->m_Display, w->m_Xid, x, y);
        XFlush(w->m_Display);
        WindowPlacementChanged(w);
    }

    static void ReportSize(HWindow w, int width, int height, unsigned long serial, bool synthetic)
    {
        jc::RingBuffer<X11ResizeRequest>& requests = w->m_X11->m_ResizeRequests;
        bool changed = width != w->m_Width || height != w->m_Height;
        uint32_t acknowledgements = 0;
        // A manager that declines a resize sends a synthetic ConfigureNotify with the retained size.
        if ((changed || synthetic) && !requests.Empty() && serial >= requests[0].m_Serial)
        {
            acknowledgements = requests.Pop().m_Count;
            if (changed)
            {
                --acknowledgements; // LinuxUpdateSize reports the size change itself.
            }
        }

        const uint32_t generation = w->m_OpenGeneration;
        LinuxUpdateSize(w, width, height, w->m_Scale);
        for (uint32_t i = 0; i < acknowledgements; ++i)
        {
            if (!w->m_Open || w->m_DeletePending || w->m_OpenGeneration != generation)
            {
                break;
            }

            NotifyWindowResize(w, w->m_Params, w->m_ResizeCallbackCount);
        }
    }

    void X11ReadSize(HWindow w)
    {
        XWindowAttributes attrs;
        if (w->m_Xid && XGetWindowAttributes(w->m_Display, w->m_Xid, &attrs))
        {
            ReportSize(w, attrs.width, attrs.height, LastKnownRequestProcessed(w->m_Display), false);
        }
    }

    static void DisplayChanged(HWindow w, bool check_layout)
    {
        dmArray<Output> outputs;
        Outputs(w, outputs);
        if (check_layout && w->m_Placement)
        {
            uint64_t hash = outputs.Size();
            for (uint32_t i = 0; i < outputs.Size(); ++i)
            {
                const Output& o = outputs[i];
                // Only geometry and output identity affect placement. RandR may enumerate outputs in a different order.
                struct Geometry
                {
                    uint64_t            m_Name, m_Primary;
                    dmWindowState::Rect m_Bounds, m_WorkArea;
                } geometry = { o.m_Monitor.m_NameHash, o.m_Primary, o.m_Bounds, o.m_Monitor.m_WorkArea };
                hash += dmHashBufferNoReverse64(&geometry, sizeof(geometry));
            }

            if (w->m_Open && w->m_X11->m_LayoutHash != hash)
            {
                WindowPlacementMonitorsChanged(w);
            }

            w->m_X11->m_LayoutHash = hash;
        }

        int x, y;
        X11GetPosition(w, &x, &y);
        const Output* best = &outputs[0];
        int64_t overlap = 0;
        for (uint32_t i = 0; i < outputs.Size(); ++i)
        {
            const Output& o = outputs[i];
            const dmWindowState::Rect& r = o.m_Bounds;
            int width = dmMath::Min(x + w->m_Width, r.m_X + r.m_Width) - dmMath::Max(x, r.m_X);
            int height = dmMath::Min(y + w->m_Height, r.m_Y + r.m_Height) - dmMath::Max(y, r.m_Y);
            if (width > 0 && height > 0 && (int64_t)width * height > overlap)
            {
                overlap = (int64_t)width * height;
                best = &o;
            }
        }

        float scale = best->m_Monitor.m_ScaleX;
        w->m_Density = CalculateDisplayDensity(best->m_MM, best->m_Bounds.m_Width * scale);
        w->m_RefreshRate = best->m_Refresh;
        LinuxUpdateSize(w, w->m_Width, w->m_Height, scale);
    }

    static void SetClientIdentity(HWindow w)
    {
        char hostname[HOST_NAME_MAX + 1];
        if (gethostname(hostname, sizeof(hostname)) != 0 || !hostname[0])
        {
            return;
        }

        const char* name = hostname;
        addrinfo hints = {};
        hints.ai_flags = AI_CANONNAME;
        addrinfo* address = 0;
        if (!strchr(hostname, '.') && getaddrinfo(hostname, 0, &hints, &address) == 0 &&
            address->ai_canonname && strchr(address->ai_canonname, '.'))
        {
            name = address->ai_canonname;
        }

        XTextProperty machine = { (unsigned char*)name, XA_STRING, 8, strlen(name) };
        XSetWMClientMachine(w->m_Display, w->m_Xid, &machine);
        if (address)
        {
            freeaddrinfo(address);
        }

        unsigned long pid = getpid();
        XChangeProperty(w->m_Display, w->m_Xid, AtomFor(w, "_NET_WM_PID"), XA_CARDINAL, 32, PropModeReplace, (unsigned char*)&pid, 1);
    }

    static void Title(HWindow w, const char* title)
    {
        if (!title)
        {
            title = "";
        }

        XTextProperty utf8 = { (unsigned char*)title, w->m_X11->m_Utf8, 8, strlen(title) };
        XTextProperty legacy = {};
        char* text = (char*)title;
        int result = Xutf8TextListToTextProperty(w->m_Display, &text, 1, XStdICCTextStyle, &legacy);
        XTextProperty* name = result == Success ? &legacy : &utf8;
        XSetWMName(w->m_Display, w->m_Xid, name);
        XSetWMIconName(w->m_Display, w->m_Xid, name);
        if (legacy.value)
        {
            XFree(legacy.value);
        }

        XSetTextProperty(w->m_Display, w->m_Xid, &utf8, AtomFor(w, "_NET_WM_NAME"));
        XSetTextProperty(w->m_Display, w->m_Xid, &utf8, AtomFor(w, "_NET_WM_ICON_NAME"));
        XFlush(w->m_Display);
    }

    bool X11QueryPointer(HWindow w, int* x, int* y)
    {
        Window root, child;
        int root_x, root_y;
        unsigned int mask;
        return XQueryPointer(w->m_Display, w->m_Xid, &root, &child, &root_x, &root_y, x, y, &mask);
    }

    static void WarpCursor(HWindow w, int cursor_x, int cursor_y)
    {
        LinuxX11* x = w->m_X11;
        x->m_WarpX = cursor_x;
        x->m_WarpY = cursor_y;
        x->m_WarpSerial = NextRequest(w->m_Display);
        XWarpPointer(w->m_Display, None, w->m_Xid, 0, 0, 0, 0, cursor_x, cursor_y);
    }

    static void ReleaseCursor(HWindow w)
    {
        LinuxX11* x = w->m_X11;
        XUngrabPointer(w->m_Display, CurrentTime);
        // A restore outside the window may send no motion event. Its serial still marks the end of grabbed input.
        WarpCursor(w, x->m_RestoreCursorX, x->m_RestoreCursorY);
        x->m_GrabSerial = 0;
        if (!w->m_Locked)
        {
            w->m_MouseX = x->m_RestoreCursorX;
            w->m_MouseY = x->m_RestoreCursorY;
        }
    }

    static void CursorMode(HWindow w)
    {
        LinuxX11* x = w->m_X11;
        if (!w->m_Xid)
        {
            return;
        }

        bool grab = w->m_Focused && w->m_Locked;
        if (grab && !x->m_GrabSerial)
        {
            X11QueryPointer(w, &x->m_RestoreCursorX, &x->m_RestoreCursorY);
            if (XGrabPointer(w->m_Display, w->m_Xid, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                             GrabModeAsync, GrabModeAsync, w->m_Xid, x->m_Blank, CurrentTime) == GrabSuccess)
            {
                WarpCursor(w, w->m_Width / 2, w->m_Height / 2);
                // Ignore motion from before capture, including any warp caused by confinement.
                x->m_GrabSerial = x->m_WarpSerial;
            }
        }
        else if (!grab && x->m_GrabSerial)
        {
            ReleaseCursor(w);
        }

        XDefineCursor(w->m_Display, w->m_Xid, grab ? x->m_Blank : w->m_LinkCursor ? x->m_Hand : x->m_Arrow);
        XFlush(w->m_Display);
    }

    static bool WriteSelection(HWindow w, Window requestor, Atom target, Atom dest)
    {
        LinuxX11* x = w->m_X11;
        if (target == x->m_Targets)
        {
            Atom formats[] = { x->m_Targets, x->m_Multiple, x->m_SaveTargets, x->m_Utf8, XA_STRING };
            XChangeProperty(w->m_Display, requestor, dest, XA_ATOM, 32, PropModeReplace, (unsigned char*)formats, DM_ARRAY_SIZE(formats));
            return true;
        }

        if (target == x->m_SaveTargets)
        {
            // On CLIPBOARD, SAVE_TARGETS is a capability marker with an empty result.
            XChangeProperty(w->m_Display, requestor, dest, AtomFor(w, "NULL"), 32, PropModeReplace, 0, 0);
            return true;
        }

        if (target != x->m_Utf8 && target != XA_STRING)
        {
            return false;
        }

        // BIG-REQUESTS adds one length word to the regular ChangeProperty header.
        long extended_words = XExtendedMaxRequestSize(w->m_Display);
        unsigned long request_words = extended_words ? extended_words - 1 : XMaxRequestSize(w->m_Display);
        if (x->m_Text.Size() > INT_MAX || x->m_Text.Size() > request_words * 4 - sz_xChangePropertyReq)
        {
            return false;
        }

        XChangeProperty(w->m_Display, requestor, dest, target, 8, PropModeReplace, (const unsigned char*)x->m_Text.Begin(), x->m_Text.Size());
        return true;
    }

    static bool Selection(HWindow w, const XSelectionRequestEvent& req)
    {
        LinuxX11* x = w->m_X11;
        XEvent reply = {};
        reply.xselection.type = SelectionNotify;
        reply.xselection.requestor = req.requestor;
        reply.xselection.selection = req.selection;
        reply.xselection.target = req.target;
        reply.xselection.time = req.time;
        Atom dest = req.property ? req.property : req.target;
        if (req.target == x->m_Multiple)
        {
            dmArray<Atom> pairs;
            if (req.property && ReadProperty(w, req.requestor, req.property, x->m_AtomPair, XMaxRequestSize(w->m_Display) - 16, pairs) && !(pairs.Size() % 2))
            {
                for (unsigned i = 0; i < pairs.Size(); i += 2)
                {
                    if (!pairs[i + 1] || !WriteSelection(w, req.requestor, pairs[i], pairs[i + 1]))
                    {
                        pairs[i + 1] = None;
                    }
                }

                XChangeProperty(w->m_Display, req.requestor, dest, x->m_AtomPair, 32, PropModeReplace, (unsigned char*)pairs.Begin(), pairs.Size());
                reply.xselection.property = dest;
            }
        }
        else if (WriteSelection(w, req.requestor, req.target, dest))
        {
            reply.xselection.property = dest;
        }

        XSendEvent(w->m_Display, req.requestor, False, 0, &reply);
        XFlush(w->m_Display);
        return reply.xselection.property != None;
    }

    static Bool ClipboardHandoffEvent(Display*, XEvent* e, XPointer data)
    {
        HWindow w = (HWindow)data;
        LinuxX11* x = w->m_X11;
        if (e->type == SelectionRequest)
        {
            return e->xselectionrequest.owner == x->m_ClipboardWindow && e->xselectionrequest.selection == x->m_Clipboard;
        }

        return e->type == SelectionNotify && e->xselection.requestor == x->m_ClipboardWindow &&
               e->xselection.selection == x->m_ClipboardManager && e->xselection.target == x->m_SaveTargets;
    }

    static void SaveClipboard(HWindow w)
    {
        Display* d = w->m_Display;
        LinuxX11* x = w->m_X11;
        if (XGetSelectionOwner(d, x->m_Clipboard) != x->m_ClipboardWindow || !XGetSelectionOwner(d, x->m_ClipboardManager))
        {
            return;
        }

        Atom formats[] = { x->m_Utf8, XA_STRING };
        XChangeProperty(d, x->m_ClipboardWindow, x->m_SaveTargets, XA_ATOM, 32, PropModeReplace, (unsigned char*)formats, 2);
        XConvertSelection(d, x->m_ClipboardManager, x->m_SaveTargets, x->m_SaveTargets, x->m_ClipboardWindow, CurrentTime);
        XFlush(d);
        // The manager can request several formats before acknowledging SAVE_TARGETS.
        const uint64_t idle_timeout = 1000000;
        uint64_t deadline = dmTime::GetMonotonicTime() + idle_timeout;
        uint64_t now;
        while ((now = dmTime::GetMonotonicTime()) < deadline)
        {
            XEvent event;
            if (XCheckIfEvent(d, &event, ClipboardHandoffEvent, (XPointer)w))
            {
                if (event.type == SelectionNotify)
                {
                    return;
                }

                if (Selection(w, event.xselectionrequest))
                {
                    deadline = dmTime::GetMonotonicTime() + idle_timeout;
                }
            }
            else
            {
                pollfd fd = { ConnectionNumber(d), POLLIN, 0 };
                poll(&fd, 1, (int)((deadline - now) / 1000) + 1);
            }
        }
    }

    static void Clipboard(HWindow w, const char* text, unsigned length)
    {
        LinuxX11* x = w->m_X11;
        if (!x->m_ClipboardWindow)
        {
            // Selection requests must still have an owner after the game window closes.
            x->m_ClipboardWindow = XCreateSimpleWindow(w->m_Display, Root(w), 0, 0, 1, 1, 0, 0, 0);
        }

        x->m_Text.EnsureSize(length);
        if (length)
        {
            memcpy(x->m_Text.Begin(), text, length);
        }

        XSetSelectionOwner(w->m_Display, x->m_Clipboard, x->m_ClipboardWindow, CurrentTime);
        XFlush(w->m_Display);
    }

    unsigned X11LookupCharacter(XKeyEvent* event)
    {
        // XLookupString returns Latin-1, including keypad digits, regardless of the locale.
        char byte;
        KeySym sym = NoSymbol;
        unsigned c = 0;
        if (XLookupString(event, &byte, 1, &sym, 0) == 1)
        {
            c = (unsigned char)byte;
        }
        else if ((sym & 0xff000000) == 0x01000000)
        {
            c = sym & 0xffffff;
        }
        else
        {
            // Use Xlib's legacy keysym table even on installations without libxkbcommon.
            typedef unsigned (*KeysymToUnicode)(KeySym);
            static KeysymToUnicode lookup = (KeysymToUnicode)dlsym(RTLD_DEFAULT, "KeySymToUcs4");
            if (lookup)
            {
                c = lookup(sym);
            }
        }

        return c >= 32 && !(c >= 127 && c < 160) && !(c >= 0xd800 && c <= 0xdfff) && c <= 0x10ffff ? c : 0;
    }

    static void KeyEvent(HWindow w, unsigned keycode, XKeyEvent& e, bool down, bool filtered)
    {
        int code = keycode < 256 ? w->m_X11->m_KeyCodes[keycode] : -1;
        if (code >= 0)
        {
            w->m_Keys[code] = down;
        }

        if (!down || filtered || !w->m_Char || (e.state & (ControlMask | Mod1Mask)))
        {
            return;
        }

        if (!w->m_X11->m_IC)
        {
            unsigned c = X11LookupCharacter(&e);
            if (c)
            {
                w->m_Char(w->m_CharData, c);
            }

            return;
        }

        KeySym sym;
        Status status;
        char buf[64];
        int n = Xutf8LookupString(w->m_X11->m_IC, &e, buf, sizeof(buf), &sym, &status);
        if (status == XBufferOverflow)
        {
            dmArray<char> large;
            large.EnsureSize(n + 1);
            n = Xutf8LookupString(w->m_X11->m_IC, &e, large.Begin(), large.Size(), &sym, &status);
            if (n > 0)
            {
                LinuxText(w, large.Begin(), n);
            }
        }
        else if (n > 0)
        {
            LinuxText(w, buf, n);
        }
    }

    static void RefreshKeymap(HWindow w)
    {
        int16_t* codes = w->m_X11->m_KeyCodes;
        X11ReadKeymap(w->m_Display, codes);
        if (w->m_Focused)
        {
            char down[32];
            XQueryKeymap(w->m_Display, down);
            memset(w->m_Keys, 0, sizeof(w->m_Keys));
            for (unsigned i = 0; i < 256; ++i)
            {
                if (codes[i] >= 0 && (down[i / 8] & (1 << (i % 8))))
                {
                    w->m_Keys[codes[i]] = 1;
                }
            }
        }
    }

    static void Poll(HWindow w)
    {
        Display* d = w->m_Display;
        LinuxX11* x = w->m_X11;
        bool metrics = false;
        bool check_layout = false;
        while (XPending(d))
        {
            XEvent e;
            XNextEvent(d, &e);
            // Some input methods clear the keycode when they consume an event for text composition.
            unsigned keycode = (e.type == KeyPress || e.type == KeyRelease) ? e.xkey.keycode : 0;
            bool filtered = XFilterEvent(&e, None);
            if (x->m_XkbEventBase && e.type == x->m_XkbEventBase)
            {
                XkbEvent* event = (XkbEvent*)&e;
                if (event->any.xkb_type == XkbMapNotify)
                {
                    XkbRefreshKeyboardMapping(&event->map);
                }

                if (event->any.xkb_type == XkbMapNotify || event->any.xkb_type == XkbNewKeyboardNotify ||
                    event->any.xkb_type == XkbNamesNotify)
                {
                    RefreshKeymap(w);
                }

                continue;
            }

            if (e.type == MappingNotify && e.xmapping.request != MappingPointer)
            {
                XRefreshKeyboardMapping(&e.xmapping);
                RefreshKeymap(w);
                continue;
            }

            if (x->m_RandREventBase && (e.type == x->m_RandREventBase + RRScreenChangeNotify || e.type == x->m_RandREventBase + RRNotify))
            {
                g_RandR.UpdateConfiguration(&e);
                check_layout = true;
            }

            if (e.type == PropertyNotify && e.xproperty.window == Root(w) &&
                (e.xproperty.atom == x->m_WorkArea || e.xproperty.atom == x->m_CurrentDesktop))
            {
                check_layout = true;
            }

            if (x->m_ClipboardWindow && e.xany.window == x->m_ClipboardWindow)
            {
                if (e.type == SelectionRequest && e.xselectionrequest.selection == x->m_Clipboard)
                {
                    Selection(w, e.xselectionrequest);
                }
                else if (e.type == SelectionClear && e.xselectionclear.selection == x->m_Clipboard &&
                         XGetSelectionOwner(d, x->m_Clipboard) != x->m_ClipboardWindow)
                {
                    x->m_Text.SetSize(0);
                }

                continue;
            }

            if (!w->m_Xid || e.xany.window != w->m_Xid)
            {
                continue;
            }

            switch (e.type)
            {
                case KeyPress:
                case KeyRelease:
                {
                    KeyEvent(w, keycode, e.xkey, e.type == KeyPress, filtered);
                    break;
                }
                case ButtonPress:
                case ButtonRelease:
                {
                    unsigned b = e.xbutton.button;
                    bool down = e.type == ButtonPress;
                    if (b == 4 || b == 5)
                    {
                        if (down)
                        {
                            w->m_Wheel += b == 4 ? 1 : -1;
                        }
                    }
                    else if (b == 1 || b == 2 || b == 3 || (b >= 8 && b <= 12))
                    {
                        w->m_Buttons[b == 1 ? 0 : b == 2 ? 2 : b == 3 ? 1 : b - 5] = down;
                    }

                    break;
                }
                case MotionNotify:
                {
                    if (x->m_GrabSerial && e.xmotion.serial >= x->m_GrabSerial)
                    {
                        // Motion already queued before our warp still uses the old pointer position.
                        if (x->m_WarpSerial && e.xmotion.serial >= x->m_WarpSerial)
                        {
                            x->m_LastCursorX = x->m_WarpX;
                            x->m_LastCursorY = x->m_WarpY;
                            x->m_WarpSerial = 0;
                        }

                        w->m_MouseX += e.xmotion.x - x->m_LastCursorX;
                        w->m_MouseY += e.xmotion.y - x->m_LastCursorY;
                        x->m_LastCursorX = e.xmotion.x;
                        x->m_LastCursorY = e.xmotion.y;
                    }
                    else if (!w->m_Locked && e.xmotion.serial >= x->m_WarpSerial)
                    {
                        x->m_WarpSerial = 0;
                        w->m_MouseX = e.xmotion.x;
                        w->m_MouseY = e.xmotion.y;
                    }

                    break;
                }
                case FocusIn:
                case FocusOut:
                {
                    if (e.xfocus.mode != NotifyGrab && e.xfocus.mode != NotifyUngrab)
                    {
                        if (x->m_IC)
                        {
                            if (e.type == FocusIn)
                            {
                                XSetICFocus(x->m_IC);
                            }
                            else
                            {
                                XUnsetICFocus(x->m_IC);
                            }
                        }

                        LinuxFocus(w, e.type == FocusIn);
                    }

                    break;
                }
                case ConfigureNotify:
                {
                    if (e.xconfigure.serial >= x->m_SizeReadSerial)
                    {
                        ReportSize(w, e.xconfigure.width, e.xconfigure.height, e.xconfigure.serial, e.xconfigure.send_event);
                        if (w->m_DeletePending || e.xconfigure.window != w->m_Xid)
                        {
                            break;
                        }
                    }

                    if (x->m_ConfigureSerial && e.xconfigure.serial >= x->m_ConfigureSerial)
                    {
                        CompleteGeometryRequest(w);
                    }

                    WindowPlacementChanged(w);
                    metrics = true;
                    break;
                }
                case PropertyNotify:
                {
                    if (e.xproperty.atom == x->m_State || e.xproperty.atom == x->m_WmState)
                    {
                        dmArray<unsigned long> state;
                        ReadProperty(w, w->m_Xid, x->m_State, XA_ATOM, 64, state);
                        bool hidden = false;
                        w->m_Maximized = false;
                        for (uint32_t i = 0; i < state.Size(); ++i)
                        {
                            hidden |= state[i] == x->m_HiddenAtom;
                            w->m_Maximized |= state[i] == x->m_MaxVert || state[i] == x->m_MaxHorz;
                        }

                        ReadProperty(w, w->m_Xid, x->m_WmState, x->m_WmState, 2, state);
                        LinuxIconify(w, hidden || (state.Size() && state[0] == IconicState));
                    }

                    break;
                }
                case ClientMessage:
                {
                    if (e.xclient.message_type == x->m_Protocols)
                    {
                        if ((Atom)e.xclient.data.l[0] == x->m_Delete)
                        {
                            TriggerCloseCallback(w);
                        }

                        if ((Atom)e.xclient.data.l[0] == x->m_Ping)
                        {
                            e.xclient.window = Root(w);
                            XSendEvent(d, Root(w), False, SubstructureNotifyMask | SubstructureRedirectMask, &e);
                        }
                    }

                    break;
                }
            }
        }

        const int cx = w->m_Width / 2, cy = w->m_Height / 2;
        if (x->m_GrabSerial && !x->m_WarpSerial && (x->m_LastCursorX != cx || x->m_LastCursorY != cy))
        {
            WarpCursor(w, cx, cy);
            XFlush(d);
        }

        if ((metrics || check_layout) && w->m_Xid)
        {
            DisplayChanged(w, check_layout);
        }
    }

    static void CreateInputContext(HWindow w)
    {
        LinuxX11* x = w->m_X11;
        if (!x->m_IM || !w->m_Xid || x->m_IC)
        {
            return;
        }

        x->m_IC = XCreateIC(x->m_IM, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                            XNClientWindow, w->m_Xid, XNFocusWindow, w->m_Xid, NULL);
        if (x->m_IC)
        {
            long mask = 0;
            XWindowAttributes attrs;
            if (!XGetICValues(x->m_IC, XNFilterEvents, &mask, NULL) && mask && XGetWindowAttributes(w->m_Display, w->m_Xid, &attrs))
            {
                XSelectInput(w->m_Display, w->m_Xid, attrs.your_event_mask | mask);
            }

            if (w->m_Focused)
            {
                XSetICFocus(x->m_IC);
            }
        }
    }

    static void InputMethodDestroyed(XIM, XPointer data, XPointer)
    {
        LinuxX11* x = ((HWindow)data)->m_X11;
        // Xlib destroys the method and all its contexts after this callback.
        x->m_IM = 0;
        x->m_IC = 0;
    }

    static void OpenInputMethod(Display* display, XPointer data, XPointer)
    {
        HWindow w = (HWindow)data;
        if (w->m_X11->m_IM)
        {
            return;
        }

        XIM im = XOpenIM(display, 0, 0, 0);
        if (im)
        {
            XIMCallback destroy = { data, InputMethodDestroyed };
            if (XSetIMValues(im, XNDestroyCallback, &destroy, NULL))
            {
                XCloseIM(im);
                return;
            }

            w->m_X11->m_IM = im;
            CreateInputContext(w);
        }
    }

    static bool Open(HWindow w)
    {
        Display* d = w->m_Display;
        LinuxX11* x = w->m_X11;
        int screen = DefaultScreen(d);
        VisualID visual;
        if (!LinuxPrepareGL(w, &visual))
        {
            return false;
        }

        XVisualInfo pattern = {};
        pattern.visualid = visual ? visual : XVisualIDFromVisual(DefaultVisual(d, screen));
        int count;
        XVisualInfo* vi = XGetVisualInfo(d, VisualIDMask, &pattern, &count);
        if (!vi)
        {
            return false;
        }

        x->m_ColorMap = XCreateColormap(d, Root(w), vi->visual, AllocNone);
        XSetWindowAttributes attrs = {};
        attrs.colormap = x->m_ColorMap;
        attrs.event_mask = StructureNotifyMask | FocusChangeMask | KeyPressMask | KeyReleaseMask | PointerMotionMask |
                           ButtonPressMask | ButtonReleaseMask | PropertyChangeMask;
        int left = 0, top = 0;
        float scale = Scale(w);
        int width = dmMath::Max(1, (int)lround(w->m_Width * scale)), height = dmMath::Max(1, (int)lround(w->m_Height * scale));
        if (w->m_Params.m_Fullscreen)
        {
            dmArray<Output> outputs;
            Outputs(w, outputs);
            dmWindowState::Rect r = outputs[0].m_Bounds;
            left = r.m_X;
            top = r.m_Y;
            width = r.m_Width;
            height = r.m_Height;
        }

        w->m_Xid = XCreateWindow(d, Root(w), left, top, width, height, 0, vi->depth, InputOutput, vi->visual,
                                 CWColormap | CWBorderPixel | CWEventMask, &attrs);
        XFree(vi);
        if (!w->m_Xid)
        {
            return false;
        }

        Atom protocols[] = { x->m_Delete, x->m_Ping };
        XSetWMProtocols(d, w->m_Xid, protocols, 2);
        XSizeHints hints = {};
        hints.flags = PWinGravity;
        hints.win_gravity = StaticGravity;
        if (w->m_Params.m_Fullscreen)
        {
            // Window managers use PPosition to honor the primary output's origin when first mapping a fullscreen game.
            hints.flags |= PPosition;
            hints.x = left;
            hints.y = top;
        }

        XSetWMNormalHints(d, w->m_Xid, &hints);
        const char* title = w->m_Params.m_Title;
        const char* resource = getenv("RESOURCE_NAME");
        XClassHint class_hint = { (char*)(title && *title ? title : "defold"), (char*)(title && *title ? title : "Defold") };
        if (resource && *resource)
        {
            class_hint.res_name = (char*)resource;
        }

        XSetClassHint(d, w->m_Xid, &class_hint);
        if (!w->m_Params.m_FocusOnShow)
        {
            // EWMH uses a zero user time for windows that should not take focus when mapped.
            unsigned long user_time = 0;
            XChangeProperty(d, w->m_Xid, AtomFor(w, "_NET_WM_USER_TIME"), XA_CARDINAL, 32,
                            PropModeReplace, (unsigned char*)&user_time, 1);
        }

        SetClientIdentity(w);
        Title(w, w->m_Params.m_Title);
        if (w->m_Params.m_Fullscreen)
        {
            Atom fullscreen = AtomFor(w, "_NET_WM_STATE_FULLSCREEN");
            XChangeProperty(d, w->m_Xid, x->m_State, XA_ATOM, 32, PropModeReplace, (unsigned char*)&fullscreen, 1);
            // Motif window managers use the decorations field even without EWMH fullscreen support.
            const unsigned long MWM_HINTS_DECORATIONS = 1ul << 1;
            const unsigned long motif_hints[] = { MWM_HINTS_DECORATIONS, 0, 0, 0, 0 };
            Atom motif = AtomFor(w, "_MOTIF_WM_HINTS");
            XChangeProperty(d, w->m_Xid, motif, motif, 32, PropModeReplace, (const unsigned char*)motif_hints, 5);
        }

        CreateInputContext(w);
        LinuxUpdateSize(w, width, height, scale);
        DisplayChanged(w, true);
        return true;
    }

    static void Close(HWindow w)
    {
        LinuxX11* x = w->m_X11;
        if (x->m_GrabSerial)
        {
            ReleaseCursor(w);
        }

        if (x->m_IC)
        {
            XDestroyIC(x->m_IC);
            x->m_IC = 0;
        }

        if (w->m_Xid)
        {
            XDestroyWindow(w->m_Display, w->m_Xid);
            w->m_Xid = 0;
        }

        if (x->m_ColorMap)
        {
            XFreeColormap(w->m_Display, x->m_ColorMap);
            x->m_ColorMap = 0;
        }

        x->m_ResizeRequests.Clear();
        x->m_ConfigureSerial = 0;
        x->m_ClampAfterResize = false;
        XSync(w->m_Display, False);
    }

    static void Focus(HWindow w, bool viewable)
    {
        Atom active = AtomFor(w, "_NET_ACTIVE_WINDOW");
        dmArray<Atom> supported;
        ReadProperty(w, Root(w), AtomFor(w, "_NET_SUPPORTED"), XA_ATOM, XMaxRequestSize(w->m_Display), supported);
        for (uint32_t i = 0; i < supported.Size(); ++i)
        {
            if (supported[i] == active)
            {
                Message(w, active, 1, CurrentTime);
                return;
            }
        }

        // Bare X servers and older window managers need an explicit focus change on a viewable window.
        if (viewable)
        {
            XRaiseWindow(w->m_Display, w->m_Xid);
            XSetInputFocus(w->m_Display, w->m_Xid, RevertToParent, CurrentTime);
        }
    }

    static bool Show(HWindow w, bool on)
    {
        if (on)
        {
            XMapWindow(w->m_Display, w->m_Xid);
            bool viewable = WaitForMap(w);
            if (w->m_Params.m_FocusOnShow)
            {
                Focus(w, viewable);
            }
        }
        else
        {
            XUnmapWindow(w->m_Display, w->m_Xid);
        }

        XFlush(w->m_Display);
        return true;
    }

    static void ReadDrawableSize(HWindow w)
    {
        XWindowAttributes attrs;
        if (XGetWindowAttributes(w->m_Display, w->m_Xid, &attrs))
        {
            // Keep the last dispatched size until ConfigureNotify acknowledges each request.
            w->m_PixelWidth = attrs.width;
            w->m_PixelHeight = attrs.height;
        }
    }

    static void Size(HWindow w, int width, int height)
    {
        if (!w->m_Shown)
        {
            // A hidden renderer needs the new drawable before its first frame.
            // Apply the resize directly while the window is withdrawn.
            XSetWindowAttributes attributes = {};
            attributes.override_redirect = True;
            XChangeWindowAttributes(w->m_Display, w->m_Xid, CWOverrideRedirect, &attributes);
            XResizeWindow(w->m_Display, w->m_Xid, width, height);
            attributes.override_redirect = False;
            XChangeWindowAttributes(w->m_Display, w->m_Xid, CWOverrideRedirect, &attributes);
            // The synchronous read includes all earlier ConfigureNotify events, including placement restoration.
            w->m_X11->m_SizeReadSerial = NextRequest(w->m_Display);
            X11ReadSize(w);
            return;
        }

        RecordGeometryRequest(w, true, width, height);
        XResizeWindow(w->m_Display, w->m_Xid, width, height);
        ReadDrawableSize(w);
        WindowPlacementChanged(w);
    }

    void X11RequestWindowSize(HWindow w, int width, int height)
    {
        if (w->m_Params.m_ResizeCallback)
        {
            jc::RingBuffer<X11ResizeRequest>& requests = w->m_X11->m_ResizeRequests;
            if (requests.Empty())
            {
                // An external resize may still be in the event queue. Only skip the native request
                // when the server already has the requested size.
                XWindowAttributes attrs;
                if (XGetWindowAttributes(w->m_Display, w->m_Xid, &attrs) && attrs.width == width && attrs.height == height)
                {
                    const Window xid = w->m_Xid;
                    const uint32_t count = w->m_ResizeCallbackCount;
                    LinuxUpdateSize(w, width, height, w->m_Scale);
                    if (w->m_DeletePending || w->m_Xid != xid)
                    {
                        return;
                    }

                    // A pending move must settle before clamping the final client position.
                    w->m_X11->m_ClampAfterResize = true;
                    if (!w->m_X11->m_ConfigureSerial)
                    {
                        CompleteGeometryRequest(w);
                    }

                    if (w->m_ResizeCallbackCount == count)
                    {
                        NotifyWindowResize(w, w->m_Params, w->m_ResizeCallbackCount);
                    }

                    return;
                }
            }

            if (!requests.Empty())
            {
                X11ResizeRequest& last = requests[requests.Size() - 1];
                if (last.m_Width == width && last.m_Height == height)
                {
                    // X11 can omit a second event for two requests for the same dimensions.
                    ++last.m_Count;
                    ReadDrawableSize(w);
                    return;
                }
            }

            if (requests.Full())
            {
                requests.OffsetCapacity(4);
            }

            X11ResizeRequest request = { NextRequest(w->m_Display), width, height, 1 };
            requests.Push(request);
        }

        Size(w, width, height);
    }

    static void Iconify(HWindow w)
    {
        XIconifyWindow(w->m_Display, w->m_Xid, DefaultScreen(w->m_Display));
        XFlush(w->m_Display);
    }

    static void Disconnect(HWindow w)
    {
        LinuxX11* x = w->m_X11;
        if (x->m_ClipboardWindow)
        {
            SaveClipboard(w);
            XDestroyWindow(w->m_Display, x->m_ClipboardWindow);
        }

        XFreeCursor(w->m_Display, x->m_Blank);
        XFreeCursor(w->m_Display, x->m_Arrow);
        XFreeCursor(w->m_Display, x->m_Hand);
        if (x->m_IMRegistered)
        {
            XUnregisterIMInstantiateCallback(w->m_Display, 0, 0, 0, OpenInputMethod, (XPointer)w);
        }

        if (x->m_IM)
        {
            XCloseIM(x->m_IM);
        }

        XCloseDisplay(w->m_Display);
        delete x;
    }

    bool LinuxConnectX11(HWindow w)
    {
        static const bool threads = XInitThreads();
        (void)threads;
        Display* d = XOpenDisplay(0);
        if (!d)
        {
            return false;
        }

        static const LinuxWindowOps ops = { "x11", Open, Close, Disconnect, Poll, Show, Iconify, Title, Size, CursorMode, Clipboard };
        w->m_Display = d;
        w->m_Ops = &ops;
        LinuxX11* x = w->m_X11 = new LinuxX11();
        x->m_WorkArea = AtomFor(w, "_NET_WORKAREA");
        x->m_CurrentDesktop = AtomFor(w, "_NET_CURRENT_DESKTOP");
        XSelectInput(d, Root(w), PropertyChangeMask);
        int error, event, major = 1, minor = 3;
        static const bool has_randr = LoadRandR();
        if (has_randr && g_RandR.QueryExtension(d, &event, &error) && g_RandR.QueryVersion(d, &major, &minor) &&
            (major > 1 || (major == 1 && minor >= 3)))
        {
            x->m_RandREventBase = event;
            g_RandR.SelectInput(d, Root(w), RRScreenChangeNotifyMask | RROutputChangeNotifyMask | RRCrtcChangeNotifyMask);
        }

        int opcode, xkb_major = XkbMajorVersion, xkb_minor = XkbMinorVersion, xkb_event;
        if (XkbQueryExtension(d, &opcode, &xkb_event, &error, &xkb_major, &xkb_minor))
        {
            unsigned int events = XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbNamesNotifyMask;
            XkbSelectEvents(d, XkbUseCoreKbd, events, events);
            x->m_XkbEventBase = xkb_event;
        }

        RefreshKeymap(w);
        Bool repeat;
        XkbSetDetectableAutoRepeat(d, True, &repeat);
        const char* locale = setlocale(LC_CTYPE, 0);
        if (locale && !strcmp(locale, "C"))
        {
            setlocale(LC_CTYPE, "");
        }

        if (XSupportsLocale())
        {
            XSetLocaleModifiers("");
            x->m_IMRegistered = XRegisterIMInstantiateCallback(d, 0, 0, 0, OpenInputMethod, (XPointer)w);
            if (!x->m_IMRegistered)
            {
                OpenInputMethod(d, (XPointer)w, 0);
            }
        }

        x->m_Delete = AtomFor(w, "WM_DELETE_WINDOW");
        x->m_Protocols = AtomFor(w, "WM_PROTOCOLS");
        x->m_Ping = AtomFor(w, "_NET_WM_PING");
        x->m_State = AtomFor(w, "_NET_WM_STATE");
        x->m_WmState = AtomFor(w, "WM_STATE");
        x->m_HiddenAtom = AtomFor(w, "_NET_WM_STATE_HIDDEN");
        x->m_MaxVert = AtomFor(w, "_NET_WM_STATE_MAXIMIZED_VERT");
        x->m_MaxHorz = AtomFor(w, "_NET_WM_STATE_MAXIMIZED_HORZ");
        x->m_Clipboard = AtomFor(w, "CLIPBOARD");
        x->m_ClipboardManager = AtomFor(w, "CLIPBOARD_MANAGER");
        x->m_SaveTargets = AtomFor(w, "SAVE_TARGETS");
        x->m_Multiple = AtomFor(w, "MULTIPLE");
        x->m_AtomPair = AtomFor(w, "ATOM_PAIR");
        x->m_Targets = AtomFor(w, "TARGETS");
        x->m_Utf8 = AtomFor(w, "UTF8_STRING");
        x->m_Arrow = XCreateFontCursor(d, XC_left_ptr);
        x->m_Hand = XCreateFontCursor(d, XC_hand2);
        const char empty = 0;
        Pixmap pixmap = XCreateBitmapFromData(d, Root(w), &empty, 1, 1);
        XColor color = {};
        x->m_Blank = XCreatePixmapCursor(d, pixmap, pixmap, &color, &color, 0, 0);
        XFreePixmap(d, pixmap);
        return true;
    }

}
