#ifndef DM_MOUSE_CAPTURE_H
#define DM_MOUSE_CAPTURE_H

#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
    typedef HWND WindowHandle;
#elif __APPLE__
    typedef unsigned long WindowHandle;
#else
    #include <X11/Xlib.h>
    typedef unsigned long WindowHandle;
#endif

namespace dmMouseCapture
{
    struct MouseDelta
    {
        double dx;
        double dy;
    };

    typedef struct Context* HContext;

    HContext CreateContext();
    bool StartCapture(HContext context, WindowHandle window);
    void StopCapture(HContext context);
    bool PollDelta(HContext context, MouseDelta* out_delta);
    void DestroyContext(HContext context);
}

#endif
