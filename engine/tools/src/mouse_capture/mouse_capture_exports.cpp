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

#include <dmsdk/dlib/shared_library.h>
#include "mouse_capture.h"

extern "C"
{
    DM_DLLEXPORT void MouseCapture_WarpCursor(int x, int y)
    {
        dmMouseCapture::WarpCursor(x, y);
    }

    DM_DLLEXPORT HContext* MouseCapture_StartCapture(int save_cursor_x, int save_cursor_y)
    {
        return dmMouseCapture::StartCapture(save_cursor_x, save_cursor_y);
    }

    DM_DLLEXPORT void MouseCapture_StopCapture(dmMouseCapture::HContext context)
    {
        dmMouseCapture::StopCapture((dmMouseCapture::HContext)context);
    }

    DM_DLLEXPORT bool MouseCapture_PollDelta(dmMouseCapture::HContext context, dmMouseCapture::MouseDelta* delta)
    {
        return dmMouseCapture::PollDelta((dmMouseCapture::HContext)context, delta);
    }
}
