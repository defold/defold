#include <dmsdk/dlib/shared_library.h>
#include "mouse_capture.h"

extern "C"
{
    DM_DLLEXPORT void* MouseCapture_CreateContext(int save_cursor_x, int save_cursor_y)
    {
        return dmMouseCapture::CreateContext(save_cursor_x, save_cursor_y);
    }

    DM_DLLEXPORT void MouseCapture_DestroyContext(void* context)
    {
        dmMouseCapture::DestroyContext((dmMouseCapture::HContext)context);
    }

    DM_DLLEXPORT void MouseCapture_WarpCursor(int x, int y)
    {
        dmMouseCapture::WarpCursor(x, y);
    }

    DM_DLLEXPORT bool MouseCapture_StartCapture(void* context, WindowHandle window)
    {
        return dmMouseCapture::StartCapture((dmMouseCapture::HContext)context, window);
    }

    DM_DLLEXPORT void MouseCapture_StopCapture(void* context)
    {
        dmMouseCapture::StopCapture((dmMouseCapture::HContext)context);
    }

    DM_DLLEXPORT bool MouseCapture_PollDelta(void* context, dmMouseCapture::MouseDelta* delta)
    {
        return dmMouseCapture::PollDelta((dmMouseCapture::HContext)context, delta);
    }
}
