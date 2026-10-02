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

#include "native.h"
#include "platform.h"

#include <string.h>

int dmNativeGetTouch(NativeTouch* touch, int count, int* out_count)
{
    *out_count = dmNativeReadTouches(dmNativeInput.Touch, touch, count, 0);
    return 1;
}

static NativeTouch* touchById(int id)
{
    int32_t i;

    NativeTouch* freeTouch = 0x0;
    for (i = 0; i != NATIVE_MAX_TOUCH; i++)
    {
        if (dmNativeInput.Touch[i].Reference != 0x0 && dmNativeInput.Touch[i].Id == id){
            return &dmNativeInput.Touch[i];
        }

        if (freeTouch == 0x0 && dmNativeInput.Touch[i].Reference == 0x0) {
            freeTouch = &dmNativeInput.Touch[i];
        }
    }

    if (freeTouch != 0x0) {
        freeTouch->Reference = freeTouch;
    }

    return freeTouch;
}

static void touchUpdate(NativeTouch *touch, int x, int y, int phase)
{
        // We can only update previous touches that has been initialized (began, moved etc).
        if (touch->Phase == NATIVE_PHASE_IDLE) {
            touch->Reference = 0x0;
            return;
        }

        int prevPhase = touch->Phase;
        int newPhase = phase;
        if (phase == NATIVE_PHASE_CANCELLED) {
            newPhase = NATIVE_PHASE_ENDED;
        }

        // If previous phase was TAPPED, we need to return early since we currently cannot buffer actions/phases.
        if (prevPhase == NATIVE_PHASE_TAPPED) {
            return;
        }

        // This is an invalid touch order, we need to recieve a began or moved
        // phase before moving pushing any more move inputs.
        if (prevPhase == NATIVE_PHASE_ENDED && newPhase == NATIVE_PHASE_MOVED) {
            return;
        }

        touch->DX = x - touch->X;
        touch->DY = y - touch->Y;
        touch->X = x;
        touch->Y = y;

        // If we recieved both a began and moved for the same touch during one frame/update,
        // just update the coordinates but leave the phase as began.
        if (prevPhase == NATIVE_PHASE_BEGAN && newPhase == NATIVE_PHASE_MOVED) {
            return;

        // If a touch both began and ended during one frame/update, set the phase as
        // tapped and we will send the released event during next update (see input.c).
        } else if (prevPhase == NATIVE_PHASE_BEGAN && newPhase == NATIVE_PHASE_ENDED) {
            touch->Phase = NATIVE_PHASE_TAPPED;
            return;
        }

        touch->Phase = phase;
}

static void touchStart(NativeTouch *touch, int id, int x, int y)
{
    if (touch->Phase != NATIVE_PHASE_IDLE) {
        return;
    }

    touch->Phase = NATIVE_PHASE_BEGAN;
    touch->Id = id;
    touch->X = x;
    touch->Y = y;
    touch->DX = 0;
    touch->DY = 0;
}

static void handleTouches(int id, int x, int y, int phase)
{
    NativeTouch* dmNativet = touchById(id);
    if (dmNativet != 0x0) {
        if (phase == NATIVE_PHASE_BEGAN) {
            touchStart(dmNativet, id, x, y);
        } else {
            touchUpdate(dmNativet, x, y, phase);
        }
    }
}

void dmNativeClearInput( void )
{
    int i;
    for (i = 0; i < NATIVE_MAX_TOUCH; ++i) {
        memset(&dmNativeInput.Touch[i], 0, sizeof(dmNativeInput.Touch[i]));
        dmNativeInput.Touch[i].Id = i;
        dmNativeInput.Touch[i].Reference = 0x0;
        dmNativeInput.Touch[i].Phase = NATIVE_PHASE_IDLE;
    }
}

int dmNativeInit()
{
    dmNativeClearInput();
    dmNativeInitJS();
    dmNativeSetTouchCallback( handleTouches );
    return 1;
}

int dmNativeOpenWindowJS(int width, int height, int alphabits, int samples, int fullscreen, int high_dpi, int opengl, int version);

int dmNativeOpenWindow(const WindowCreateParams* params)
{
    int opengl = params->m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL || params->m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGLES;
    return dmNativeOpenWindowJS(params->m_Width, params->m_Height, params->m_ContextAlphabits,
        params->m_Samples, params->m_Fullscreen, params->m_HighDPI, opengl, params->m_GraphicsApiVersionHint);
}
