
#include "mouse_capture.h"
#include <string.h>
#include <X11/extensions/XInput2.h>

namespace dmMouseCapture
{
    struct Context
    {
        Display* m_Display;
        Window m_Window;
        Cursor m_InvisibleCursor;
        int m_XIOpcode;
        bool m_Capturing;
        bool m_OwnsDisplay;
        MouseDelta m_AccumulatedDelta;
    };

    HContext CreateContext(Window window)
    {
        if (!window)
            return nullptr;

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
        ctx->m_Window = window;
        ctx->m_XIOpcode = xi_opcode;
        ctx->m_Capturing = false;
        ctx->m_OwnsDisplay = true;
        ctx->m_AccumulatedDelta.dx = 0.0;
        ctx->m_AccumulatedDelta.dy = 0.0;

        Pixmap blank = XCreatePixmap(display, window, 1, 1, 1);
        XColor dummy = {0};
        ctx->m_InvisibleCursor = XCreatePixmapCursor(display, blank, blank,
                                                      &dummy, &dummy, 0, 0);
        XFreePixmap(display, blank);

        return ctx;
    }

    bool StartCapture(HContext context)
    {
        if (!context || context->m_Capturing)
            return false;

        int result = XGrabPointer(context->m_Display,
                                  context->m_Window,
                                  True,
                                  PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                                  GrabModeAsync,
                                  GrabModeAsync,
                                  context->m_Window,
                                  context->m_InvisibleCursor,
                                  CurrentTime);

        if (result != GrabSuccess)
            return false;

        Window root = DefaultRootWindow(context->m_Display);
        unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8];
        memset(mask_bytes, 0, sizeof(mask_bytes));

        XIEventMask evmask;
        evmask.deviceid = XIAllMasterDevices;
        evmask.mask_len = sizeof(mask_bytes);
        evmask.mask = mask_bytes;
        XISetMask(mask_bytes, XI_RawMotion);
        XISelectEvents(context->m_Display, root, &evmask, 1);

        context->m_Capturing = true;
        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;

        return true;
    }

    void StopCapture(HContext context)
    {
        if (!context || !context->m_Capturing)
            return;

        XUngrabPointer(context->m_Display, CurrentTime);

        Window root = DefaultRootWindow(context->m_Display);
        unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8];
        memset(mask_bytes, 0, sizeof(mask_bytes));

        XIEventMask evmask;
        evmask.deviceid = XIAllMasterDevices;
        evmask.mask_len = sizeof(mask_bytes);
        evmask.mask = mask_bytes;
        XISelectEvents(context->m_Display, root, &evmask, 1);

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
                                if (i == 0) // X axis
                                    context->m_AccumulatedDelta.dx += raw_vals[val_idx];
                                else if (i == 1) // Y axis
                                    context->m_AccumulatedDelta.dy += raw_vals[val_idx];
                                val_idx++;
                            }
                        }
                    }
                    XFreeEventData(context->m_Display, &ev.xcookie);
                }
            }
        }

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

        XFreeCursor(context->m_Display, context->m_InvisibleCursor);

        if (context->m_OwnsDisplay)
            XCloseDisplay(context->m_Display);

        delete context;
    }
}

extern "C" {
    void* MouseCapture_CreateContext(Window window) {
        return dmMouseCapture::CreateContext(window);
    }

    void MouseCapture_DestroyContext(void* context) {
        dmMouseCapture::DestroyContext((dmMouseCapture::HContext)context);
    }

    bool MouseCapture_StartCapture(void* context) {
        return dmMouseCapture::StartCapture((dmMouseCapture::HContext)context);
    }

    void MouseCapture_StopCapture(void* context) {
        dmMouseCapture::StopCapture((dmMouseCapture::HContext)context);
    }

    bool MouseCapture_PollDelta(void* context, dmMouseCapture::MouseDelta* delta) {
        return dmMouseCapture::PollDelta((dmMouseCapture::HContext)context, delta);
    }
}
