#ifndef DM_MOUSE_CAPTURE_H
#define DM_MOUSE_CAPTURE_H

#include <stdint.h>
#include <X11/Xlib.h>

namespace dmMouseCapture
{
    struct MouseDelta
    {
        double dx;
        double dy;
    };

    typedef struct Context* HContext;

    /**
     * Create mouse capture context for an X11 window.
     * Opens connection to default X11 display.
     * Returns null on failure.
     */
    HContext CreateContext(Window window);

    bool StartCapture(HContext context);
    void StopCapture(HContext context);
    bool PollDelta(HContext context, MouseDelta* out_delta);
    void DestroyContext(HContext context);
}

#endif // DM_MOUSE_CAPTURE_H
