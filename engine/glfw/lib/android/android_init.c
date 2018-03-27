//========================================================================
// GLFW - An OpenGL framework
// Platform:    X11/GLX
// API version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"

#include "android_log.h"
#include "android_util.h"

#include <android/sensor.h>

//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Initialize GLFW thread package
//========================================================================

struct android_app* g_AndroidApp;

extern int main(int argc, char** argv);

extern int g_KeyboardActive;
extern int g_autoCloseKeyboard;
extern int g_SpecialKeyActive;

static int g_appLaunchInterrupted = 0;

static ASensorEventQueue* g_sensorEventQueue = 0;
static ASensorRef g_accelerometer = 0;
static int g_accelerometerEnabled = 0;
static uint32_t g_accelerometerFrequency = 1000000/60;
static GLFWTouch* g_MouseEmulationTouch = 0;

static void initThreads( void )
{
    // Initialize critical section handle
#ifdef _GLFW_HAS_PTHREAD
    (void) pthread_mutex_init( &_glfwThrd.CriticalSection, NULL );
#endif

    // The first thread (the main thread) has ID 0
    _glfwThrd.NextID = 0;

    // Fill out information about the main thread (this thread)
    _glfwThrd.First.ID       = _glfwThrd.NextID++;
    _glfwThrd.First.Function = NULL;
    _glfwThrd.First.Previous = NULL;
    _glfwThrd.First.Next     = NULL;
#ifdef _GLFW_HAS_PTHREAD
    _glfwThrd.First.PosixID  = pthread_self();
#endif
}


//========================================================================
// Terminate GLFW thread package
//========================================================================

static void terminateThreads( void )
{
#ifdef _GLFW_HAS_PTHREAD

    _GLFWthread *t, *t_next;

    // Enter critical section
    ENTER_THREAD_CRITICAL_SECTION

    // Kill all threads (NOTE: THE USER SHOULD WAIT FOR ALL THREADS TO
    // DIE, _BEFORE_ CALLING glfwTerminate()!!!)
    t = _glfwThrd.First.Next;
    while( t != NULL )
    {
        // Get pointer to next thread
        t_next = t->Next;

        // Simply murder the process, no mercy!
        pthread_kill( t->PosixID, SIGKILL );

        // Free memory allocated for this thread
        free( (void *) t );

        // Select next thread in list
        t = t_next;
    }

    // Leave critical section
    LEAVE_THREAD_CRITICAL_SECTION

    // Delete critical section handle
    pthread_mutex_destroy( &_glfwThrd.CriticalSection );

#endif // _GLFW_HAS_PTHREAD
}

//========================================================================
// Terminate GLFW when exiting application
//========================================================================

static void glfw_atexit( void )
{
    LOGV("glfw_atexit");
    glfwTerminate();
}

//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Initialize various GLFW state
//========================================================================

#define CASE_RETURN(cmd)\
    case cmd:\
        return #cmd;

static const char* GetCmdName(int32_t cmd)
{
    switch (cmd)
    {
    CASE_RETURN(APP_CMD_INPUT_CHANGED);
    CASE_RETURN(APP_CMD_INIT_WINDOW);
    CASE_RETURN(APP_CMD_TERM_WINDOW);
    CASE_RETURN(APP_CMD_WINDOW_RESIZED);
    CASE_RETURN(APP_CMD_WINDOW_REDRAW_NEEDED);
    CASE_RETURN(APP_CMD_CONTENT_RECT_CHANGED);
    CASE_RETURN(APP_CMD_GAINED_FOCUS);
    CASE_RETURN(APP_CMD_LOST_FOCUS);
    CASE_RETURN(APP_CMD_CONFIG_CHANGED);
    CASE_RETURN(APP_CMD_LOW_MEMORY);
    CASE_RETURN(APP_CMD_START);
    CASE_RETURN(APP_CMD_RESUME);
    CASE_RETURN(APP_CMD_SAVE_STATE);
    CASE_RETURN(APP_CMD_PAUSE);
    CASE_RETURN(APP_CMD_STOP);
    CASE_RETURN(APP_CMD_DESTROY);
    default:
        return "unknown";
    }
}

#undef CASE_RETURN

static void computeIconifiedState()
{
    // We do not cancel iconified status when RESUME is received, as we can
    // see the following order of commands when returning from a locked state:
    // RESUME, TERM_WINDOW, INIT_WINDOW, GAINED_FOCUS
    // We can also encounter this order of commands:
    // RESUME, GAINED_FOCUS
    // Between RESUME and INIT_WINDOW, the application could attempt to perform
    // operations without a current GL context.
    //
    // Therefore, base iconified status on both INIT_WINDOW and PAUSE/RESUME states
    // Iconified unless opened, resumed (not paused)
    _glfwWin.iconified = !(_glfwWin.opened && !_glfwWin.paused && _glfwWin.hasSurface);
}

static void handleCommand(struct android_app* app, int32_t cmd) {
    LOGV("handleCommand: %s", GetCmdName(cmd));

    switch (cmd)
    {
    case APP_CMD_SAVE_STATE:
        break;
    case APP_CMD_INIT_WINDOW:
        if (_glfwWin.opened)
        {
            create_gl_surface(&_glfwWin);
            _glfwWin.hasSurface = 1;
        }
        _glfwWin.opened = 1;
        computeIconifiedState();
        break;
    case APP_CMD_TERM_WINDOW:
        if (!_glfwInitialized) {
            // If TERM arrives before the GL context etc. have been created, (e.g.
            // if the user opens search in a narrow time window during app launch),
            // then we can be placed in an unrecoverable situation:
            // TERM can arrive before _glPlatformInit is called, and so creation of the
            // GL context will fail. Deferred creation is not effective either, as the
            // application will attempt to open the GL window before it has regained focus.
            g_appLaunchInterrupted = 1;
        }
        destroy_gl_surface(&_glfwWin);
        _glfwWin.hasSurface = 0;
        computeIconifiedState();
        break;
    case APP_CMD_GAINED_FOCUS:
        computeIconifiedState();
        break;
    case APP_CMD_LOST_FOCUS:
        if (g_KeyboardActive) {
            _glfwShowKeyboard(0, 0, 0);
        }
        computeIconifiedState();
        break;
    case APP_CMD_START:
        break;
    case APP_CMD_STOP:
        break;
    case APP_CMD_RESUME:
        _glfwWin.active = 1;
        _glfwWin.paused = 0;
        if (g_sensorEventQueue && g_accelerometer && g_accelerometerEnabled) {
            ASensorEventQueue_enableSensor(g_sensorEventQueue, g_accelerometer);
        }
        computeIconifiedState();
        if(_glfwWin.windowFocusCallback)
            _glfwWin.windowFocusCallback(1);
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        // See _glfwPlatformSwapBuffers for handling of orientation changes
        break;
    case APP_CMD_PAUSE:
        _glfwWin.paused = 1;
        _glfwWin.active = 0;
        if (g_sensorEventQueue && g_accelerometer && g_accelerometerEnabled) {
            ASensorEventQueue_disableSensor(g_sensorEventQueue, g_accelerometer);
        }
        computeIconifiedState();
        if(_glfwWin.windowFocusCallback)
            _glfwWin.windowFocusCallback(0);
        break;
    case APP_CMD_DESTROY:
        _glfwWin.opened = 0;
        final_gl(&_glfwWin);
        computeIconifiedState();
        break;
    }
}

static GLFWTouch* touchById(void *ref)
{
    int32_t i;

    GLFWTouch* freeTouch = 0x0;
    for (i=0;i!=GLFW_MAX_TOUCH;i++)
    {
        _glfwInput.Touch[i].Id = i;
        if (_glfwInput.Touch[i].Reference == ref)
            return &_glfwInput.Touch[i];

        // Save touch entry for later if we need to "alloc" one in case we don't find the current reference.
        if (freeTouch == 0x0 && _glfwInput.Touch[i].Reference == 0x0) {
            freeTouch = &_glfwInput.Touch[i];
        }
    }

    if (freeTouch != 0x0) {
        freeTouch->Reference = ref;
    }

    return freeTouch;
}

static GLFWTouch* touchStart(void *ref, int32_t x, int32_t y)
{
    GLFWTouch *touch = touchById(ref);
    if (touch)
    {
        // When a new touch starts, and there was no previous one, this will be our mouse emulation touch.
        if (g_MouseEmulationTouch == 0x0) {
            g_MouseEmulationTouch = touch;
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

static GLFWTouch* touchUpdate(void *ref, int32_t x, int32_t y, int phase)
{
    GLFWTouch *touch = touchById(ref);
    if (touch)
    {
        int prevPhase = touch->Phase;
        int newPhase = phase;

        // If this touch is currently used for mouse emulation, and it ended, unset the mouse emulation pointer.
        if (newPhase == GLFW_PHASE_ENDED && g_MouseEmulationTouch == touch) {
            g_MouseEmulationTouch = 0x0;
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

inline void *pointerIdToRef(int32_t id)
{
    return (void*)(0x1 + id);
}

static void updateGlfwMousePos(int32_t x, int32_t y)
{
    _glfwInput.MousePosX = x;
    _glfwInput.MousePosY = y;
}

// return 1 to handle the event, 0 for default handling
static int32_t handleInput(struct android_app* app, AInputEvent* event)
{
    int32_t event_type = AInputEvent_getType(event);

    if (event_type == AINPUT_EVENT_TYPE_MOTION)
    {
        if (g_KeyboardActive && g_autoCloseKeyboard) {
            // Implicitly hide keyboard
            _glfwShowKeyboard(0, 0, 0);
        }

        // touch_handling
        int32_t action = AMotionEvent_getAction(event);
        int32_t pointer_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int32_t pointer_id = AMotionEvent_getPointerId(event, pointer_index);

        void *pointer_ref = pointerIdToRef(pointer_id);

        int32_t x = AMotionEvent_getX(event, pointer_index);
        int32_t y = AMotionEvent_getY(event, pointer_index);

        int32_t action_action = action & AMOTION_EVENT_ACTION_MASK;

        switch (action_action)
        {
            case AMOTION_EVENT_ACTION_DOWN:
                if (touchStart(pointer_ref, x, y) == g_MouseEmulationTouch) {
                    updateGlfwMousePos(x,y);
                    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
                }
                break;
            case AMOTION_EVENT_ACTION_UP:
                if (touchUpdate(pointer_ref, x, y, GLFW_PHASE_ENDED) == g_MouseEmulationTouch || !g_MouseEmulationTouch) {
                    updateGlfwMousePos(x,y);
                    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
                }
                break;;
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                if (touchStart(pointer_ref, x, y) == g_MouseEmulationTouch) {
                    updateGlfwMousePos(x,y);
                    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
                }
                break;
            case AMOTION_EVENT_ACTION_POINTER_UP:
                if (touchUpdate(pointer_ref, x, y, GLFW_PHASE_ENDED) == g_MouseEmulationTouch || !g_MouseEmulationTouch) {
                    updateGlfwMousePos(x,y);
                    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
                }
                break;
            case AMOTION_EVENT_ACTION_CANCEL:
                if (touchUpdate(pointer_ref, x, y, GLFW_PHASE_CANCELLED) == g_MouseEmulationTouch || !g_MouseEmulationTouch) {
                    updateGlfwMousePos(x,y);
                    _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
                }
                break;
            case AMOTION_EVENT_ACTION_MOVE:
                {
                    // these events contain updates for all pointers.
                    int i, max = AMotionEvent_getPointerCount(event);
                    for (i=0;i<max;i++)
                    {
                        x = AMotionEvent_getX(event, i);
                        y = AMotionEvent_getY(event, i);
                        pointer_ref = pointerIdToRef(AMotionEvent_getPointerId(event, i));

                        if (touchUpdate(pointer_ref, x, y, GLFW_PHASE_MOVED) == g_MouseEmulationTouch)
                        {
                            updateGlfwMousePos(x,y);
                            if (_glfwWin.mousePosCallback) {
                                _glfwWin.mousePosCallback(x, y);
                            }
                        }
                    }
                }
                break;
        }

        return 1;
    }
    else if (event_type == AINPUT_EVENT_TYPE_KEY)
    {
        int32_t code = AKeyEvent_getKeyCode(event);
        int32_t action = AKeyEvent_getAction(event);
        int32_t flags = AKeyEvent_getFlags(event);
        int32_t meta = AKeyEvent_getMetaState(event);
        int32_t scane_code = AKeyEvent_getScanCode(event);
        int32_t repeat = AKeyEvent_getRepeatCount(event);
        int32_t device_id = AInputEvent_getDeviceId(event);
        int32_t source = AInputEvent_getSource(event);
        int64_t down_time = AKeyEvent_getDownTime(event);
        int64_t event_time = AKeyEvent_getEventTime(event);
        int glfw_action = -1;
        if (action == AKEY_EVENT_ACTION_DOWN)
        {
            glfw_action = GLFW_PRESS;
        }
        else if (action == AKEY_EVENT_ACTION_UP)
        {
            glfw_action = GLFW_RELEASE;
        }
        else if (action == AKEY_EVENT_ACTION_MULTIPLE && code == AKEYCODE_UNKNOWN)
        {
            // complex character, let DefoldActivity#dispatchKeyEvent handle it
            // such characters are not copied into AInputEvent due to NDK bug
            return 0;
        }

        // virtual keyboard enter and backspace needs to generate both a press
        // and release but we cannot create them both here in the same frame
        // There's an ugly hack in android_window.c that counts down the
        // g_SpecialKeyActive and when it reaches zero it generates a release
        // event for the keys checked below
        if (g_KeyboardActive && (glfw_action == GLFW_PRESS)) {
            switch (code) {
            case AKEYCODE_DEL:
                g_SpecialKeyActive = 10;
                _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_PRESS );
                return 1;
            case AKEYCODE_ENTER:
                g_SpecialKeyActive = 10;
                _glfwInputKey( GLFW_KEY_ENTER, GLFW_PRESS );
                return 1;
            }
        }

        // check for key events that should generate a key trigger
        switch (code) {
        case AKEYCODE_MENU:
            _glfwInputKey( GLFW_KEY_MENU, glfw_action );
            return 1;
        case AKEYCODE_BACK:
            if (g_KeyboardActive) {
                // Implicitly hide keyboard
                _glfwShowKeyboard(0, 0, 0);
            } else {
                _glfwInputKey( GLFW_KEY_BACK, glfw_action );
            }
            return 1;
        case AKEYCODE_ESCAPE: _glfwInputKey( GLFW_KEY_ESC, glfw_action ); return 1;
        case AKEYCODE_F1: _glfwInputKey( GLFW_KEY_F1, glfw_action ); return 1;
        case AKEYCODE_F2: _glfwInputKey( GLFW_KEY_F2, glfw_action ); return 1;
        case AKEYCODE_F3: _glfwInputKey( GLFW_KEY_F3, glfw_action ); return 1;
        case AKEYCODE_F4: _glfwInputKey( GLFW_KEY_F4, glfw_action ); return 1;
        case AKEYCODE_F5: _glfwInputKey( GLFW_KEY_F5, glfw_action ); return 1;
        case AKEYCODE_F6: _glfwInputKey( GLFW_KEY_F6, glfw_action ); return 1;
        case AKEYCODE_F7: _glfwInputKey( GLFW_KEY_F7, glfw_action ); return 1;
        case AKEYCODE_F8: _glfwInputKey( GLFW_KEY_F8, glfw_action ); return 1;
        case AKEYCODE_F9: _glfwInputKey( GLFW_KEY_F9, glfw_action ); return 1;
        case AKEYCODE_F10: _glfwInputKey( GLFW_KEY_F10, glfw_action ); return 1;
        case AKEYCODE_F11: _glfwInputKey( GLFW_KEY_F11, glfw_action ); return 1;
        case AKEYCODE_F12: _glfwInputKey( GLFW_KEY_F12, glfw_action ); return 1;
        case AKEYCODE_DPAD_UP: _glfwInputKey( GLFW_KEY_UP, glfw_action ); return 1;
        case AKEYCODE_DPAD_DOWN: _glfwInputKey( GLFW_KEY_DOWN, glfw_action ); return 1;
        case AKEYCODE_DPAD_LEFT: _glfwInputKey( GLFW_KEY_LEFT, glfw_action ); return 1;
        case AKEYCODE_DPAD_RIGHT: _glfwInputKey( GLFW_KEY_RIGHT, glfw_action ); return 1;
        case AKEYCODE_SHIFT_LEFT: _glfwInputKey( GLFW_KEY_LSHIFT, glfw_action ); return 1;
        case AKEYCODE_SHIFT_RIGHT: _glfwInputKey( GLFW_KEY_RSHIFT, glfw_action ); return 1;
        case AKEYCODE_CTRL_LEFT: _glfwInputKey( GLFW_KEY_LCTRL, glfw_action ); return 1;
        case AKEYCODE_CTRL_RIGHT: _glfwInputKey( GLFW_KEY_RCTRL, glfw_action ); return 1;
        // This key is not {@link AKEYCODE_NUM_LOCK}; it is more like {@link AKEYCODE_ALT_LEFT}.
        // https://android.googlesource.com/platform/frameworks/native/+/master/include/android/keycodes.h
        case AKEYCODE_NUM: _glfwInputKey( GLFW_KEY_LALT, glfw_action ); return 1;
        case AKEYCODE_ALT_LEFT: _glfwInputKey( GLFW_KEY_LALT, glfw_action ); return 1;
        case AKEYCODE_ALT_RIGHT: _glfwInputKey( GLFW_KEY_RALT, glfw_action ); return 1;
        case AKEYCODE_TAB: _glfwInputKey( GLFW_KEY_TAB, glfw_action ); return 1;
        case AKEYCODE_INSERT: _glfwInputKey( GLFW_KEY_INSERT, glfw_action ); return 1;
        case AKEYCODE_DEL: _glfwInputKey( GLFW_KEY_DEL, glfw_action ); return 1;
        case AKEYCODE_ENTER: _glfwInputKey( GLFW_KEY_ENTER, glfw_action ); return 1;
        case AKEYCODE_PAGE_UP: _glfwInputKey( GLFW_KEY_PAGEUP, glfw_action ); return 1;
        case AKEYCODE_PAGE_DOWN: _glfwInputKey( GLFW_KEY_PAGEDOWN, glfw_action ); return 1;
        case AKEYCODE_MOVE_HOME: _glfwInputKey( GLFW_KEY_HOME, glfw_action ); return 1;
        case AKEYCODE_MOVE_END: _glfwInputKey( GLFW_KEY_END, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_0: _glfwInputKey( GLFW_KEY_KP_0, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_1: _glfwInputKey( GLFW_KEY_KP_1, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_2: _glfwInputKey( GLFW_KEY_KP_2, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_3: _glfwInputKey( GLFW_KEY_KP_3, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_4: _glfwInputKey( GLFW_KEY_KP_4, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_5: _glfwInputKey( GLFW_KEY_KP_5, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_6: _glfwInputKey( GLFW_KEY_KP_6, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_7: _glfwInputKey( GLFW_KEY_KP_7, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_8: _glfwInputKey( GLFW_KEY_KP_8, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_9: _glfwInputKey( GLFW_KEY_KP_9, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_DIVIDE: _glfwInputKey( GLFW_KEY_KP_DIVIDE, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_MULTIPLY: _glfwInputKey( GLFW_KEY_KP_MULTIPLY, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_SUBTRACT: _glfwInputKey( GLFW_KEY_KP_SUBTRACT, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_ADD: _glfwInputKey( GLFW_KEY_KP_ADD, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_DOT: _glfwInputKey( GLFW_KEY_KP_DECIMAL, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_EQUALS: _glfwInputKey( GLFW_KEY_KP_EQUAL, glfw_action ); return 1;
        case AKEYCODE_NUMPAD_ENTER: _glfwInputKey( GLFW_KEY_KP_ENTER, glfw_action ); return 1;
        case AKEYCODE_NUM_LOCK: _glfwInputKey( GLFW_KEY_KP_NUM_LOCK, glfw_action ); return 1;
        case AKEYCODE_CAPS_LOCK: _glfwInputKey( GLFW_KEY_CAPS_LOCK, glfw_action ); return 1;
        case AKEYCODE_SCROLL_LOCK: _glfwInputKey( GLFW_KEY_SCROLL_LOCK, glfw_action ); return 1;
        case AKEYCODE_META_LEFT: _glfwInputKey( GLFW_KEY_LSUPER, glfw_action ); return 1;
        case AKEYCODE_META_RIGHT: _glfwInputKey( GLFW_KEY_RSUPER, glfw_action ); return 1;
        // Break / Pause key
        // https://developer.android.com/ndk/reference/group___input.html
        case AKEYCODE_BREAK: _glfwInputKey( GLFW_KEY_PAUSE, glfw_action ); return 1;

        // the key events below have no direct GLFW_KEY_* mapping - do a reasonable translation
        case AKEYCODE_DPAD_CENTER: _glfwInputKey( GLFW_KEY_ENTER, glfw_action ); return 1;
        }

        // check for key events that should generate both a text and key trigger
        switch (code) {
            case AKEYCODE_STAR: _glfwInputKey( '*', glfw_action ); break;
            case AKEYCODE_POUND: _glfwInputKey( '#', glfw_action ); break;
            case AKEYCODE_COMMA: _glfwInputKey( ',', glfw_action ); break;
            case AKEYCODE_PERIOD: _glfwInputKey( '.', glfw_action ); break;
            case AKEYCODE_SPACE: _glfwInputKey( GLFW_KEY_SPACE, glfw_action ); break;
            case AKEYCODE_GRAVE: _glfwInputKey( '`', glfw_action ); break;
            case AKEYCODE_MINUS: _glfwInputKey( '-', glfw_action ); break;
            case AKEYCODE_EQUALS: _glfwInputKey( "=", glfw_action ); break;
            case AKEYCODE_LEFT_BRACKET: _glfwInputKey( '[', glfw_action ); break;
            case AKEYCODE_RIGHT_BRACKET: _glfwInputKey( ']', glfw_action ); break;
            case AKEYCODE_BACKSLASH: _glfwInputKey( '\\', glfw_action ); break;
            case AKEYCODE_SEMICOLON: _glfwInputKey( ';', glfw_action ); break;
            case AKEYCODE_APOSTROPHE: _glfwInputKey( '\'', glfw_action ); break;
            case AKEYCODE_SLASH: _glfwInputKey( '/', glfw_action ); break;
            case AKEYCODE_AT: _glfwInputKey( '@', glfw_action ); break;
            case AKEYCODE_PLUS: _glfwInputKey( '+', glfw_action ); break;
            default:
                if ((code >= AKEYCODE_A) && (code <= AKEYCODE_Z)) {
                    const int key = 'A' + (code - AKEYCODE_A);
                    _glfwInputKey( key, glfw_action );
                }
                else if ((code >= AKEYCODE_0) && (code <= AKEYCODE_9)) {
                    const int key = '0' + (code - AKEYCODE_0);
                    _glfwInputKey( key, glfw_action );
                }
                break;
        }

        JNIEnv* env = g_AndroidApp->activity->env;
        JavaVM* vm = g_AndroidApp->activity->vm;
        (*vm)->AttachCurrentThread(vm, &env, NULL);

        jclass KeyEventClass = (*env)->FindClass(env, "android/view/KeyEvent");
        jmethodID KeyEventConstructor = (*env)->GetMethodID(env, KeyEventClass, "<init>", "(JJIIIIIIII)V");
        jobject keyEvent = (*env)->NewObject(env, KeyEventClass, KeyEventConstructor,
                down_time, event_time, action, code, repeat, meta, device_id, scane_code, flags, source);
        jmethodID KeyEvent_getUnicodeChar = (*env)->GetMethodID(env, KeyEventClass, "getUnicodeChar", "(I)I");

        int unicode = (*env)->CallIntMethod(env, keyEvent, KeyEvent_getUnicodeChar, meta);
        (*env)->DeleteLocalRef( env, keyEvent );

        (*vm)->DetachCurrentThread(vm);

        _glfwInputChar( unicode, glfw_action );
    }

    return 0;
}


void _glfwPreMain(struct android_app* state)
{
    LOGV("_glfwPreMain");

    g_AndroidApp = state;

    state->onAppCmd = handleCommand;
    state->onInputEvent = handleInput;

    _glfwWin.opened = 0;
    _glfwWin.hasSurface = 0;

    // Wait for window to become ready (APP_CMD_INIT_WINDOW in handleCommand)
    int java_startup_complete = 0;
    while (_glfwWin.opened == 0 || !java_startup_complete)
    {
        int ident;
        int events;
        struct android_poll_source* source;

        while ((ident=ALooper_pollAll(300, NULL, &events, (void**)&source)) >= 0)
        {
            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }
        }

        if (!java_startup_complete)
        {
            // Defold activity has isStartupDone which reports if good or not to start engine.
            JNIEnv* env = g_AndroidApp->activity->env;
            JavaVM* vm = g_AndroidApp->activity->vm;
            (*vm)->AttachCurrentThread(vm, &env, NULL);
            jclass def_activity_class = (*env)->GetObjectClass(env, g_AndroidApp->activity->clazz);
            jmethodID is_startup_complete = (*env)->GetMethodID(env, def_activity_class, "isStartupDone", "()Z");
            java_startup_complete = (*env)->CallBooleanMethod(env, g_AndroidApp->activity->clazz, is_startup_complete);
            (*vm)->DetachCurrentThread(vm);
        }
    }

    char* argv[] = {0};
    argv[0] = strdup("defold-app");
    int ret = main(1, argv);
    free(argv[0]);
    // NOTE: _exit due to a dead-lock in glue code.
    _exit(ret);
}

static int LooperCallback(int fd, int events, void* data)
{
    struct Command cmd;
    if (read(_glfwWin.m_Pipefd[0], &cmd, sizeof(cmd)) == sizeof(cmd)) {
        if (cmd.m_Command == CMD_INPUT_CHAR) {
            // Trick to "fool" glfw. Otherwise repeated characters will be filtered due to repeat
            _glfwInputChar( (int)cmd.m_Data, GLFW_RELEASE );
            _glfwInputChar( (int)cmd.m_Data, GLFW_PRESS );

        } else if (cmd.m_Command == CMD_INPUT_MARKED_TEXT) {
            _glfwSetMarkedText( (char*)cmd.m_Data );

            // Need to free marked text string thas was
            // allocated in android_window.c
            free(cmd.m_Data);
        }
    } else {
        LOGF("read error in looper callback");
    }
    return 1;
}

static int SensorCallback(int fd, int events, void* data)
{
    // clear sensor event queue
    ASensorEvent e;
    while (ASensorEventQueue_getEvents(g_sensorEventQueue, &e, 1) > 0)
    {
        _glfwInput.AccX = e.acceleration.x;
        _glfwInput.AccY = e.acceleration.y;
        _glfwInput.AccZ = e.acceleration.z;
    }
    return 1;
}

int _glfwPlatformGetAcceleration(float* x, float* y, float* z)
{
    if (g_accelerometerEnabled) {
        // This trickery is to align scale and axises to what
        // iOS outputs (as that was implemented first)
        const float scale = - 1.0 / ASENSOR_STANDARD_GRAVITY;
        *x = scale * _glfwInput.AccX;
        *y = scale * _glfwInput.AccY;
        *z = scale * _glfwInput.AccZ;
    }
    return g_accelerometerEnabled;
}

int _glfwPlatformInit( void )
{
    LOGV("_glfwPlatformInit");

    if (g_appLaunchInterrupted) {
        return GL_FALSE;
    }

    _glfwWin.display = EGL_NO_DISPLAY;
    _glfwWin.context = EGL_NO_CONTEXT;
    _glfwWin.surface = EGL_NO_SURFACE;
    _glfwWin.iconified = 1;
    _glfwWin.paused = 0;
    _glfwWin.app = g_AndroidApp;

    int result = pipe(_glfwWin.m_Pipefd);
    if (result != 0) {
        LOGF("Could not open pipe for communication: %d", result);
    }
    result = ALooper_addFd(g_AndroidApp->looper, _glfwWin.m_Pipefd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, LooperCallback, &_glfwWin);
    if (result != 1) {
        LOGF("Could not add file descriptor to looper: %d", result);
    }

    ASensorManager* sensorManager = ASensorManager_getInstance();
    if (!sensorManager) {
        LOGF("Could not get sensor manager");
    }

    g_sensorEventQueue = ASensorManager_createEventQueue(sensorManager, g_AndroidApp->looper, ALOOPER_POLL_CALLBACK, SensorCallback, &_glfwWin);
    if (!g_sensorEventQueue) {
        LOGF("Could not create event queue");
    }

    // Initialize thread package
    initThreads();

    // Install atexit() routine
    atexit( glfw_atexit );

    // Start the timer
    _glfwInitTimer();

    // Initialize display
    if (init_gl(&_glfwWin) == 0)
    {
        return GL_FALSE;
    }
    SaveWin(&_glfwWin);

    return GL_TRUE;
}

//========================================================================
// Close window and kill all threads
//========================================================================

int _glfwPlatformTerminate( void )
{
    LOGV("_glfwPlatformTerminate");
#ifdef _GLFW_HAS_PTHREAD
    // Only the main thread is allowed to do this...
    if( pthread_self() != _glfwThrd.First.PosixID )
    {
        return GL_FALSE;
    }
#endif // _GLFW_HAS_PTHREAD

    // Close OpenGL window
    glfwCloseWindow();


    int result = ALooper_removeFd(g_AndroidApp->looper, _glfwWin.m_Pipefd[0]);
    if (result != 1) {
        LOGF("Could not remove fd from looper: %d", result);
    }

    close(_glfwWin.m_Pipefd[0]);

    ASensorManager* sensorManager = ASensorManager_getInstance();
    ASensorManager_destroyEventQueue(sensorManager, g_sensorEventQueue);

    // Close the other pipe on the java thread
    JNIEnv* env = g_AndroidApp->activity->env;
    JavaVM* vm = g_AndroidApp->activity->vm;
    (*vm)->AttachCurrentThread(vm, &env, NULL);
    close(_glfwWin.m_Pipefd[1]);
    (*vm)->DetachCurrentThread(vm);

    // Call finish and let Android life cycle take care of the termination
    ANativeActivity_finish(g_AndroidApp->activity);

     // Wait for gl context destruction
    while (_glfwWin.display != EGL_NO_DISPLAY)
    {
        int ident;
        int events;
        struct android_poll_source* source;

        while ((ident=ALooper_pollAll(300, NULL, &events, (void**)&source)) >= 0)
        {
            // Process this event.
            if (source != NULL) {
                source->process(g_AndroidApp, source);
            }
        }
    }

    // Kill thread package
    terminateThreads();

    return GL_TRUE;
}

GLFWAPI void glfwAccelerometerEnable()
{
    if (g_accelerometer == 0) {
        ASensorManager* sensorManager = ASensorManager_getInstance();
        if (!sensorManager) {
            LOGF("Could not get sensor manager");
            return;
        }
        g_accelerometer = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ACCELEROMETER);
    }

    if (g_sensorEventQueue != 0 && g_accelerometer != 0 && !g_accelerometerEnabled) {
        g_accelerometerEnabled = 1;
        ASensorEventQueue_enableSensor(g_sensorEventQueue, g_accelerometer);
        ASensorEventQueue_setEventRate(g_sensorEventQueue, g_accelerometer, g_accelerometerFrequency);
    }
}
