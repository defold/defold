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
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

namespace dmMouseCapture
{
    struct Context
    {
        bool m_Capturing;
        double m_AccDx;
        double m_AccDy;
        id m_LocalMonitor;
    };

    static NSEventMask const kMotionMask =
        NSEventMaskMouseMoved |
        NSEventMaskLeftMouseDragged |
        NSEventMaskRightMouseDragged |
        NSEventMaskOtherMouseDragged;

    // We don't need to save the cursor positions because the cursor never moves
    HContext CreateContext(int save_cursor_x, int save_cursor_y)
    {
        Context* ctx = new Context();
        ctx->m_Capturing = false;
        ctx->m_AccDx = 0.0;
        ctx->m_AccDy = 0.0;
        ctx->m_LocalMonitor = nil;
        return ctx;
    }

    void DestroyContext(HContext context)
    {
        if (!context)
            return;

        if (context->m_Capturing)
            StopCapture(context);

        delete context;
    }

    void WarpCursor(int x, int y)
    {
        CGWarpMouseCursorPosition(CGPointMake(x, y));
        // CGWarpMouseCursorPosition causes a short delay, calling this prevents it
        CGAssociateMouseAndMouseCursorPosition(YES);
    }

    bool StartCapture(HContext context)
    {
        if (!context || context->m_Capturing)
            return false;

        CGAssociateMouseAndMouseCursorPosition(NO);

        Context* ctx = context;

        ctx->m_AccDx = 0.0;
        ctx->m_AccDy = 0.0;

        ctx->m_LocalMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:kMotionMask
            handler:^NSEvent*(NSEvent* event) {
                ctx->m_AccDx += [event deltaX];
                ctx->m_AccDy += [event deltaY];
                return event;
            }];

        if (!ctx->m_LocalMonitor)
        {
            CGAssociateMouseAndMouseCursorPosition(YES);
            return false;
        }

        ctx->m_Capturing = true;
        return true;
    }

    void StopCapture(HContext context)
    {
        if (!context || !context->m_Capturing)
            return;

        if (context->m_LocalMonitor)
        {
            [NSEvent removeMonitor:context->m_LocalMonitor];
            context->m_LocalMonitor = nil;
        }

        CGAssociateMouseAndMouseCursorPosition(YES);

        context->m_Capturing = false;
    }

    bool PollDelta(HContext context, MouseDelta* out_delta)
    {
        if (!context || !context->m_Capturing || !out_delta)
            return false;

        // On macOS the event monitor callback fires on the main run loop,
        // which JavaFX pumps. No manual event processing needed here —
        // deltas have already been accumulated by the time we poll.

        out_delta->dx = context->m_AccDx;
        out_delta->dy = context->m_AccDy;

        bool had_motion = (context->m_AccDx != 0.0 || context->m_AccDy != 0.0);

        context->m_AccDx = 0.0;
        context->m_AccDy = 0.0;

        return had_motion;
    }
}
