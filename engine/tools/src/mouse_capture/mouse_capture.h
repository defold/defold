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

    void                    WarpCursor(int x, int y);
    HContext                StartCapture(int save_cursor_x, int save_cursor_y);
    void                    StopCapture(HContext context);
    bool                    PollDelta(HContext context, MouseDelta* out_delta);
} // namespace dmMouseCapture

#endif
