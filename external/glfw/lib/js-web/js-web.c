// Copyright 2020-2023 The Defold Foundation
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

#include "../../include/GL/glfw.h"
#include "platform.h"

#include <string.h>

GLFWAPI int GLFWAPIENTRY glfwGetTouch(GLFWTouch* touch, int count, int* out_count)
{
    int i, touchCount;

    touchCount = 0;
    for (i = 0; i < GLFW_MAX_TOUCH; ++i) {
        GLFWTouch* t = &_glfwInput.Touch[i];
        if (t->Reference) {
            touch[touchCount] = *t;

            int phase = t->Phase;
            if (phase == GLFW_PHASE_ENDED || phase == GLFW_PHASE_CANCELLED) {
                // Clear reference since this touch has ended.
                t->Reference = 0x0;
                t->Phase = GLFW_PHASE_IDLE;
            } else if (phase == GLFW_PHASE_BEGAN) {
                // Touches that has begun will change to stationary until moved or released.
                t->Phase = GLFW_PHASE_STATIONARY;
            }

            // If this was a tap (began and ended on same frame), we need to
            // make sure this touch results in an ended action next frame.
            if (t->Phase == GLFW_PHASE_TAPPED) {
                touch[touchCount].Phase = GLFW_PHASE_BEGAN;
                t->Phase = GLFW_PHASE_ENDED;
            }

            touchCount++;
        }
    }

    if (count < touchCount)
        touchCount = count;

    *out_count = touchCount;

    touchCount = 0;

    return 1;
}

static GLFWTouch* touchById(int id)
{
    int32_t i;

    GLFWTouch* freeTouch = 0x0;
    for (i = 0; i != GLFW_MAX_TOUCH; i++)
    {
        if (_glfwInput.Touch[i].Reference != 0x0 && _glfwInput.Touch[i].Id == id){
            return &_glfwInput.Touch[i];
        }

        if (freeTouch == 0x0 && _glfwInput.Touch[i].Reference == 0x0) {
            freeTouch = &_glfwInput.Touch[i];
        }
    }

    if (freeTouch != 0x0) {
        freeTouch->Reference = freeTouch;
    }

    return freeTouch;
}

static void touchUpdate(GLFWTouch *touch, int x, int y, int phase)
{
        // We can only update previous touches that has been initialized (began, moved etc).
        if (touch->Phase == GLFW_PHASE_IDLE) {
            touch->Reference = 0x0;
            return;
        }

        int prevPhase = touch->Phase;
        int newPhase = phase;
        if (phase == GLFW_PHASE_CANCELLED) {
            newPhase = GLFW_PHASE_ENDED;
        }

        // If previous phase was TAPPED, we need to return early since we currently cannot buffer actions/phases.
        if (prevPhase == GLFW_PHASE_TAPPED) {
            return;
        }

        // This is an invalid touch order, we need to recieve a began or moved
        // phase before moving pushing any more move inputs.
        if (prevPhase == GLFW_PHASE_ENDED && newPhase == GLFW_PHASE_MOVED) {
            return;
        }

        touch->DX = x - touch->X;
        touch->DY = y - touch->Y;
        touch->X = x;
        touch->Y = y;

        // If we recieved both a began and moved for the same touch during one frame/update,
        // just update the coordinates but leave the phase as began.
        if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_MOVED) {
            return;

        // If a touch both began and ended during one frame/update, set the phase as
        // tapped and we will send the released event during next update (see input.c).
        } else if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_ENDED) {
            touch->Phase = GLFW_PHASE_TAPPED;
            return;
        }

        touch->Phase = phase;
}

static void touchStart(GLFWTouch *touch, int id, int x, int y)
{
    if (touch->Phase != GLFW_PHASE_IDLE) {
        return;
    }

    touch->Phase = GLFW_PHASE_BEGAN;
    touch->Id = id;
    touch->X = x;
    touch->Y = y;
    touch->DX = 0;
    touch->DY = 0;
}

static void GLFWCALL handleTouches(int id, int x, int y, int phase)
{
    GLFWTouch* glfwt = touchById(id);
    if (glfwt != 0x0) {
        if (phase == GLFW_PHASE_BEGAN) {
            touchStart(glfwt, id, x, y);
        } else {
            touchUpdate(glfwt, x, y, phase);
        }
    }
}

void _glfwClearInput( void )
{
    int i;
    for (i = 0; i < GLFW_MAX_TOUCH; ++i) {
        memset(&_glfwInput.Touch[i], 0, sizeof(_glfwInput.Touch[i]));
        _glfwInput.Touch[i].Id = i;
        _glfwInput.Touch[i].Reference = 0x0;
        _glfwInput.Touch[i].Phase = GLFW_PHASE_IDLE;
    }
}

GLFWAPI int GLFWAPIENTRY glfwInit()
{
    _glfwClearInput();
    glfwInitJS();
    glfwSetTouchCallback( handleTouches );
    return 1;
}
