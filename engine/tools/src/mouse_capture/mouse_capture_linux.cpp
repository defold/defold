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

#include "mouse_capture.h"
#include <string.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xfixes.h>

namespace dmMouseCapture
{
    struct Context
    {
        Display*   m_Display;
        Window     m_Window;
        int        m_XIOpcode;
        bool       m_Capturing;
        MouseDelta m_AccumulatedDelta;
        int        m_SavedCursorX;
        int        m_SavedCursorY;
    };

    HContext CreateContext(int save_cursor_x, int save_cursor_y)
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
        ctx->m_XIOpcode = xi_opcode;
        ctx->m_Capturing = false;
        ctx->m_AccumulatedDelta.dx = 0.0;
        ctx->m_AccumulatedDelta.dy = 0.0;
        ctx->m_SavedCursorX = save_cursor_x;
        ctx->m_SavedCursorY = save_cursor_y;

        return ctx;
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

    void WarpCursor(int x, int y)
    {
        static Display* display = XOpenDisplay(NULL);
        if (!display)
            return;
        Window root = DefaultRootWindow(display);
        XWarpPointer(display, None, root, 0, 0, 0, 0, x, y);
        XFlush(display);
    }

    bool StartCapture(HContext context)
    {
        if (!context || context->m_Capturing)
            return false;


        Window       root = DefaultRootWindow(context->m_Display);
        Window       dummy;
        int          root_x, root_y, dummy_int;
        unsigned int dummy_uint;
        XQueryPointer(context->m_Display, root, &dummy, &dummy, &root_x, &root_y, &dummy_int, &dummy_int, &dummy_uint);

        unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8];
        memset(mask_bytes, 0, sizeof(mask_bytes));

        XIEventMask evmask;
        evmask.deviceid = XIAllMasterDevices;
        evmask.mask_len = sizeof(mask_bytes);
        evmask.mask = mask_bytes;
        XISetMask(mask_bytes, XI_RawMotion);
        XISelectEvents(context->m_Display, root, &evmask, 1);

        XFixesHideCursor(context->m_Display, root);

        XFlush(context->m_Display);

        context->m_Window = root;
        context->m_Capturing = true;
        context->m_AccumulatedDelta.dx = 0.0;
        context->m_AccumulatedDelta.dy = 0.0;

        return true;
    }

    void StopCapture(HContext context)
    {
        if (!context || !context->m_Capturing)
            return;

        Window        root = DefaultRootWindow(context->m_Display);
        unsigned char mask_bytes[(XI_LASTEVENT + 7) / 8];
        memset(mask_bytes, 0, sizeof(mask_bytes));

        XIEventMask evmask;
        evmask.deviceid = XIAllMasterDevices;
        evmask.mask_len = sizeof(mask_bytes);
        evmask.mask = mask_bytes;
        XISelectEvents(context->m_Display, root, &evmask, 1);

        XFixesShowCursor(context->m_Display, context->m_Window);
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
                        double*     raw_vals = raw->raw_values;

                        int         val_idx = 0;
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

        *out_delta = context->m_AccumulatedDelta;

        Window root = DefaultRootWindow(context->m_Display);
        XWarpPointer(context->m_Display, None, root, 0, 0, 0, 0, context->m_SavedCursorX, context->m_SavedCursorY);
        XFlush(context->m_Display);

        return (context->m_AccumulatedDelta.dx != 0.0 ||
                context->m_AccumulatedDelta.dy != 0.0);
    }
} // namespace dmMouseCapture
