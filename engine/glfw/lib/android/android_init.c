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

extern int g_KeyboardActive;
extern int g_autoCloseKeyboard;
extern int g_SpecialKeyActive;

static int g_appLaunchInterrupted = 0;

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
        break;
    case APP_CMD_GAINED_FOCUS:
        // We do not cancel iconified status when RESUME is received, as we can
        // see the following order of commands when returning from a locked state:
        // RESUME, TERM_WINDOW, INIT_WINDOW, GAINED_FOCUS
        // We can also encounter this order of commands:
        // RESUME, GAINED_FOCUS
        // Between RESUME and INIT_WINDOW, the application could attempt to perform
        // operations without a current GL context.
        _glfwWin.iconified = 0;
        _glfwWin.active = 1;
        break;
    case APP_CMD_LOST_FOCUS:
        if (g_KeyboardActive) {
            _glfwShowKeyboard(0, 0, 0);
        }
        _glfwWin.active = 0;
        break;
    case APP_CMD_START:
        break;
    case APP_CMD_STOP:
        break;
    case APP_CMD_RESUME:
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
        // See _glfwPlatformSwapBuffers for handling of orientation changes
        break;
    case APP_CMD_PAUSE:
        _glfwWin.iconified = 1;
        break;
    case APP_CMD_DESTROY:
        _glfwWin.opened = 0;
        final_gl(&_glfwWin);
        break;
    }
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

        if (glfw_action == GLFW_PRESS) {
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
    // Wait for window to become ready (APP_CMD_INIT_WINDOW in handleCommand)
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
            _glfwInputChar( (int)cmd.m_Data, GLFW_PRESS );
        }
    } else {
        LOGF("read error in looper callback");
    }
    return 1;
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
    _glfwWin.app = g_AndroidApp;

    int result = pipe(_glfwWin.m_Pipefd);
    if (result != 0) {
        LOGF("Could not open pipe for communication: %d", result);
    }
    result = ALooper_addFd(g_AndroidApp->looper, _glfwWin.m_Pipefd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, LooperCallback, &_glfwWin);
    if (result != 1) {
        LOGF("Could not add file descriptor to looper: %d", result);
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

