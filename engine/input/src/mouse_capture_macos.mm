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

    HContext CreateContext()
    {
        Context* ctx = new Context();
        ctx->m_Capturing = false;
        ctx->m_AccDx = 0.0;
        ctx->m_AccDy = 0.0;
        ctx->m_LocalMonitor = nil;
        return ctx;
    }

    bool StartCapture(HContext context, unsigned long window)
    {
        if (!context || context->m_Capturing)
            return false;

        // Dissociate mouse movement from cursor position
        CGAssociateMouseAndMouseCursorPosition(false);

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
            CGAssociateMouseAndMouseCursorPosition(true);
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

        // Re-associate mouse and cursor
        CGAssociateMouseAndMouseCursorPosition(true);

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

    void DestroyContext(HContext context)
    {
        if (!context)
            return;

        if (context->m_Capturing)
            StopCapture(context);

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
