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

//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Initialize GLFW thread package
//========================================================================

struct android_app* g_AndroidApp;

extern int main(int argc, char** argv);

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
        }
        _glfwWin.opened = 1;
        break;
    case APP_CMD_TERM_WINDOW:
        if (_glfwWin.opened)
        {
            destroy_gl_surface(&_glfwWin);
        }
        break;
    case APP_CMD_GAINED_FOCUS:
        _glfwWin.active = 1;
        break;
    case APP_CMD_LOST_FOCUS:
        _glfwWin.active = 0;
        break;
    case APP_CMD_START:
        break;
    case APP_CMD_STOP:
        break;
    case APP_CMD_RESUME:
        _glfwWin.iconified = 0;
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        break;
    case APP_CMD_PAUSE:
        _glfwWin.iconified = 1;
        break;
    case APP_CMD_DESTROY:
        final_gl(&_glfwWin);
        break;
    }
}

static int32_t handleInput(struct android_app* app, AInputEvent* event)
{
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
    {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK ;
        int32_t x = AMotionEvent_getX(event, 0);
        int32_t y = AMotionEvent_getY(event, 0);
        _glfwInput.MousePosX = x;
        _glfwInput.MousePosY = y;

        switch (action)
        {
        case AMOTION_EVENT_ACTION_DOWN:
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS );
            break;
        case AMOTION_EVENT_ACTION_UP:
            _glfwInputMouseClick( GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE );
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            if( _glfwWin.mousePosCallback )
            {
                _glfwWin.mousePosCallback(x, y);
            }
            break;
        }
        return 1;
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
    while (_glfwWin.opened == 0)
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
    }

    char* argv[] = {0};
    argv[0] = strdup("defold-app");
    main(1, argv);
    free(argv[0]);
}

int _glfwPlatformInit( void )
{
    LOGV("_glfwPlatformInit");

    _glfwWin.display = EGL_NO_DISPLAY;
    _glfwWin.context = EGL_NO_CONTEXT;
    _glfwWin.surface = EGL_NO_SURFACE;
    _glfwWin.iconified = 1;

    // Initialize thread package
    initThreads();

    // Install atexit() routine
    atexit( glfw_atexit );

    // Start the timer
    _glfwInitTimer();

    _glfwWin.app = g_AndroidApp;

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

    // Kill thread package
    terminateThreads();

    return GL_TRUE;
}

