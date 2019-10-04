#include "../../include/GL/glfw.h"
#include "platform.h"

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

static GLFWTouch* getTouchById(int id)
{
    int32_t i;
    GLFWTouch* freeTouch = 0x0;
    for (i = 0; i != GLFW_MAX_TOUCH; i++)
    {
        if (_glfwInput.Touch[i].Reference && _glfwInput.Touch[i].Id == i){
            return &_glfwInput.Touch[i];
        }

        if (freeTouch == 0x0 && _glfwInput.Touch[i].Reference == 0x0) {
            freeTouch = &_glfwInput.Touch[i];
        }
    }

    if (freeTouch != 0x0) {
    	freeTouch->Id = id;
        freeTouch->Reference = freeTouch;
    }

    return freeTouch;
}

static GLFWTouch* touchUpdate(GLFWTouch *touch, int32_t x, int32_t y, int phase)
{
    if (touch)
    {
        // We can only update previous touches that has been initialized (began, moved etc).
        if (touch->Phase == GLFW_PHASE_IDLE) {
            touch->Reference = 0x0;
            return 0x0;
        }

        int prevPhase = touch->Phase;
        int newPhase = phase;

        // If previous phase was TAPPED, we need to return early since we currently cannot buffer actions/phases.
        if (prevPhase == GLFW_PHASE_TAPPED) {
            return 0x0;
        }

        // This is an invalid touch order, we need to recieve a began or moved
        // phase before moving pushing any more move inputs.
        if (prevPhase == GLFW_PHASE_ENDED && newPhase == GLFW_PHASE_MOVED) {
            return touch;
        }

        touch->DX = x - touch->X;
        touch->DY = y - touch->Y;
        touch->X = x;
        touch->Y = y;

        // If we recieved both a began and moved for the same touch during one frame/update,
        // just update the coordinates but leave the phase as began.
        if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_MOVED) {
            return touch;

        // If a touch both began and ended during one frame/update, set the phase as
        // tapped and we will send the released event during next update (see input.c).
        } else if (prevPhase == GLFW_PHASE_BEGAN && newPhase == GLFW_PHASE_ENDED) {
            touch->Phase = GLFW_PHASE_TAPPED;
            return touch;
        }

        touch->Phase = phase;

        return touch;
    }
    return 0;
}

static GLFWTouch* touchStart(GLFWTouch *touch, int32_t x, int32_t y)
{
    if (touch)
    {
        // We can't start/begin a new touch if it already has an ongoing phase (ie not idle).
        if (touch->Phase != GLFW_PHASE_IDLE) {
            return 0x0;
        }

        touch->Phase = GLFW_PHASE_BEGAN;
        touch->X = x;
        touch->Y = y;
        touch->DX = 0;
        touch->DY = 0;

        return touch;
    }

    return 0;
}

static void GLFWCALL handleTouches(int id, float x, float y, int phase)
{
    if (phase == GLFW_PHASE_BEGAN) {
        GLFWTouch* glfwt = getTouchById(id);

        // We can only update previous touches that has been initialized (began, moved etc).
        if (glfwt->Phase == GLFW_PHASE_IDLE) {
            glfwt->Reference = 0x0;
            return;
        }
        touchUpdate(glfwt, x, y, phase);
    } else {
    	GLFWTouch* glfwt = getTouchById(id);
    	touchStart(glfwt, x, y);
    }
}

GLFWAPI int GLFWAPIENTRY glfwInit()
{
    _glfwInitJS();
    glfwSetTouchCallback( handleTouches );
    return 1;
}
