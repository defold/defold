#include "mouse_capture.h"
#include <string.h>
#include <X11/extensions/XInput2.h>
#include <X11/cursorfont.h>

namespace dmMouseCapture
{
    struct Context
    {
        Display* m_Display;
        Window m_Window;
        Cursor m_BlankCursor;
        Cursor m_OriginalCursor;
        int m_XIOpcode;
        bool m_Capturing;
        bool m_HasOriginalCursor;
        MouseDelta m_AccumulatedDelta;
    };

    static Cursor CreateBlankCursor(Display* display, Window window)
    {
        static char data[1] = {0};
        Pixmap blank = XCreateBitmapFromData(display, window, data, 1, 1);
        XColor dummy;
        memset(&dummy, 0, sizeof(dummy));
        Cursor cursor = XCreatePixmapCursor(display, blank, blank, &dummy, &dummy, 0, 0);
        XFreePixmap(display, blank);
        return cursor;
    }

    HContext CreateContext()
    {
        Display* display = XOpenDisplay(NULL);
        if (!display)
            return nullptr;

        int xi_opcode, xi_event, xi_error;
        if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &xi_event, &xi_error))
        {
            XCloseDisplay(display);
            return nullptr;
        }

        int major = 2, minor = 2;
        if (XIQueryVersion(display, &major, &minor) != Success)
        {
            XCloseDisplay(display);
            return nullptr;
        }

        Context* ctx = new Context();
        ctx->m_Display = display;
        ctx->m_Window = None;
        ctx->m_BlankCursor = None;
        ctx->m_OriginalCursor = None;
        ctx->m_XIOpcode = xi_opcode;
        ctx->m_Capturing = false;
        ctx->m_HasOriginalCursor = false;
        ctx->m_AccumulatedDelta.dx = 0.0;
        ctx->m_AccumulatedDelta.dy = 0.0;

        return ctx;
    }

    // NOTE: We tried using XGrabPointer, however, according to a JDK issue, JavaFX Glass is indeed using XGrabPointer
    // so when testing, the function would return `AlreadyGrabbed`, so we have to use this recentering technique
    // https://bugs.openjdk.org/browse/JDK-8096597?focusedId=13791606&page=com.atlassian.jira.plugin.system.issuetabpanels:comment-tabpanel#comment-13791606
    bool StartCapture(HContext context, unsigned long window)
    {
        if (!context || context->m_Capturing || window == None)
            return false;

        context->m_Window = (Window)window;

        Window root = DefaultRootWindow(context->m_Display);
        unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8];
        memset(mask_bytes, 0, sizeof(mask_bytes));

        XIEventMask evmask;
        evmask.deviceid = XIAllMasterDevices;
        evmask.mask_len = sizeof(mask_bytes);
        evmask.mask = mask_bytes;
        XISetMask(mask_bytes, XI_RawMotion);
        XISelectEvents(context->m_Display, root, &evmask, 1);

        context->m_BlankCursor = CreateBlankCursor(context->m_Display, context->m_Window);
        XDefineCursor(context->m_Display, context->m_Window, context->m_BlankCursor);

        XWindowAttributes attrs;
        XGetWindowAttributes(context->m_Display, context->m_Window, &attrs);
        XWarpPointer(context->m_Display, None, context->m_Window, 0, 0, 0, 0,
                     attrs.width / 2, attrs.height / 2);

        XFlush(context->m_Display);

        context->m_Capturing = true;
        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;

        return true;
    }

    void StopCapture(HContext context)
    {
        if (!context || !context->m_Capturing)
            return;

        Window root = DefaultRootWindow(context->m_Display);
        unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8];
        memset(mask_bytes, 0, sizeof(mask_bytes));

        XIEventMask evmask;
        evmask.deviceid = XIAllMasterDevices;
        evmask.mask_len = sizeof(mask_bytes);
        evmask.mask = mask_bytes;
        XISelectEvents(context->m_Display, root, &evmask, 1);

        XUndefineCursor(context->m_Display, context->m_Window);
        if (context->m_BlankCursor != None)
        {
            XFreeCursor(context->m_Display, context->m_BlankCursor);
            context->m_BlankCursor = None;
        }

        XFlush(context->m_Display);

        context->m_Window = None;
        context->m_Capturing = false;
    }

    bool PollDelta(HContext context, MouseDelta* out_delta)
    {
        if (!context || !context->m_Capturing || !out_delta)
            return false;

        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;

        while (XPending(context->m_Display) > 0)
        {
            XEvent ev;
            XNextEvent(context->m_Display, &ev);

            if (ev.type == GenericEvent && ev.xcookie.extension == context->m_XIOpcode)
            {
                if (XGetEventData(context->m_Display, &ev.xcookie))
                {
                    if (ev.xcookie.evtype == XI_RawMotion)
                    {
                        XIRawEvent* raw = (XIRawEvent*)ev.xcookie.data;
                        double* raw_vals = raw->raw_values;

                        int val_idx = 0;
                        for (int i = 0; i < raw->valuators.mask_len * 8; i++)
                        {
                            if (XIMaskIsSet(raw->valuators.mask, i))
                            {
                                if (i == 0)
                                    context->m_AccumulatedDelta.dx += raw_vals[val_idx];
                                else if (i == 1)
                                    context->m_AccumulatedDelta.dy += raw_vals[val_idx];
                                val_idx++;
                            }
                        }
                    }
                    XFreeEventData(context->m_Display, &ev.xcookie);
                }
            }
        }

        XWindowAttributes attrs;
        XGetWindowAttributes(context->m_Display, context->m_Window, &attrs);
        XWarpPointer(context->m_Display, None, context->m_Window, 0, 0, 0, 0,
                     attrs.width / 2, attrs.height / 2);
        XFlush(context->m_Display);

        *out_delta = context->m_AccumulatedDelta;

        return (context->m_AccumulatedDelta.dx != 0.0 ||
                context->m_AccumulatedDelta.dy != 0.0);
    }

    void DestroyContext(HContext context)
    {
        if (!context)
            return;

        if (context->m_Capturing)
            StopCapture(context);

        XCloseDisplay(context->m_Display);
        delete context;
    }
}

extern "C" {
    void* MouseCapture_CreateContext() {
        return dmMouseCapture::CreateContext();
    }

    void MouseCapture_DestroyContext(void* context) {
        dmMouseCapture::DestroyContext((dmMouseCapture::HContext)context);
    }

    bool MouseCapture_StartCapture(void* context, unsigned long window) {
        return dmMouseCapture::StartCapture((dmMouseCapture::HContext)context, window);
    }

    void MouseCapture_StopCapture(void* context) {
        dmMouseCapture::StopCapture((dmMouseCapture::HContext)context);
    }

    bool MouseCapture_PollDelta(void* context, dmMouseCapture::MouseDelta* delta) {
        return dmMouseCapture::PollDelta((dmMouseCapture::HContext)context, delta);
    }
}
