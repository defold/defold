#ifndef DM_MOUSE_CAPTURE_H
#define DM_MOUSE_CAPTURE_H

#include <stdint.h>

namespace dmMouseCapture
{
    struct MouseDelta
    {
        double dx;
        double dy;
    };

    typedef struct Context* HContext;

    HContext                CreateContext(int save_cursor_x, int save_cursor_y);
    void                    WarpCursor(int x, int y);
    bool                    StartCapture(HContext context);
    void                    StopCapture(HContext context);
    bool                    PollDelta(HContext context, MouseDelta* out_delta);
    void                    DestroyContext(HContext context);
} // namespace dmMouseCapture

#endif
