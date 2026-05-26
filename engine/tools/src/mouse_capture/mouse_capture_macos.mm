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
        int m_IgnoreEvents;
        int m_RestoreCursorX;
        int m_RestoreCursorY;
    };

    static NSEventMask const kMotionMask =
        NSEventMaskMouseMoved |
        NSEventMaskLeftMouseDragged |
        NSEventMaskRightMouseDragged |
        NSEventMaskOtherMouseDragged;

    void WarpCursor(int x, int y)
    {
        CGWarpMouseCursorPosition(CGPointMake(x, y));
        // CGWarpMouseCursorPosition causes a short delay, calling this prevents it
        CGAssociateMouseAndMouseCursorPosition(YES);
    }

    static bool GetCursorPos(CursorPos* cursor_pos)
    {
        if (!cursor_pos)
            return false;

        CGEventRef event = CGEventCreate(NULL);
        if (!event)
            return NO;
        CGPoint p = CGEventGetLocation(event);
        CFRelease(event);
        cursor_pos->x = (int)p.x;
        cursor_pos->y = (int)p.y;
        return YES;
    }

    HContext StartCapture(int capture_cursor_x, int capture_cursor_y)
    {
        Context* context = new Context();
        context->m_Capturing = true;
        context->m_AccDx = 0.0;
        context->m_AccDy = 0.0;
        context->m_IgnoreEvents = 1;

        context->m_RestoreCursorX = capture_cursor_x;
        context->m_RestoreCursorY = capture_cursor_y;

        CursorPos restore_cursor_pos;
        if (GetCursorPos(&restore_cursor_pos))
        {
            context->m_RestoreCursorX = restore_cursor_pos.x;
            context->m_RestoreCursorY = restore_cursor_pos.y;
        }

        [NSCursor hide];
        // Because we are dissasociating the cursor, this leaves it at a random position, and JavaFX needs the cursor
        // to be over the Image Node in order to receive events
        WarpCursor(capture_cursor_x, capture_cursor_y);
        CGAssociateMouseAndMouseCursorPosition(NO);

        context->m_LocalMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:kMotionMask
            handler:^NSEvent*(NSEvent* event) {
                // We have to ignore the first event because of the previous WarpCursor creates
                // one huge jump to move to the center of the screen
                if (context->m_IgnoreEvents > 0) {
                    context->m_IgnoreEvents--;
                    return event;
                }
                context->m_AccDx += [event deltaX];
                context->m_AccDy += [event deltaY];
                return event;
            }];

        if (!context->m_LocalMonitor)
        {
            CGAssociateMouseAndMouseCursorPosition(YES);
            return nil;
        }

        return context;
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
        WarpCursor(context->m_RestoreCursorX, context->m_RestoreCursorY);
        [NSCursor unhide];

        delete context;
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
