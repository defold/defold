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

#include <limits.h>
#include <jni.h>

//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Here is where the window is created, and
// the OpenGL rendering context is created
//========================================================================

#include "android_log.h"
#include "android_util.h"

extern struct android_app* g_AndroidApp;
int g_KeyboardActive = 0;
static int g_autoCloseKeyboard = 0;
// TODO: Hack. PRESS AND RELEASE is sent the same frame. Similar hack on iOS for handling of special keys
static int g_SpecialKeyActive = -1;

#define CMD_INPUT_CHAR (0)

struct Command
{
    int m_Command;
    void* m_Data;
};

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_glfwInputCharNative(JNIEnv* env, jobject obj, jint unicode)
{
    struct Command cmd;
    cmd.m_Command = CMD_INPUT_CHAR;
    cmd.m_Data = (void*)unicode;
    if (write(_glfwWin.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        LOGF("Failed to write command");
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

int _glfwPlatformOpenWindow( int width__, int height__,
                             const _GLFWwndconfig* wndconfig__,
                             const _GLFWfbconfig* fbconfig__ )
{
    LOGV("_glfwPlatformOpenWindow");

    _glfwWin.app = g_AndroidApp;
    _glfwWin.app->onInputEvent = handleInput;

    // Initialize display
    init_gl(&_glfwWin.display, &_glfwWin.context, &_glfwWin.config);

    ANativeActivity* activity = g_AndroidApp->activity;
    JNIEnv* env = 0;
    (*activity->vm)->AttachCurrentThread(activity->vm, &env, 0);

    jclass activity_class = (*env)->FindClass(env, "android/app/NativeActivity");
    jmethodID setOrientationMethod = (*env)->GetMethodID(env, activity_class, "setRequestedOrientation", "(I)V");
    if (setOrientationMethod) {
        // See http://developer.android.com/reference/android/content/pm/ActivityInfo.html
        const int SCREEN_ORIENTATION_LANDSCAPE = 0;
        const int SCREEN_ORIENTATION_PORTRAIT = 1;

        int o = width__ > height__ ? SCREEN_ORIENTATION_LANDSCAPE : SCREEN_ORIENTATION_PORTRAIT;
        (*env)->CallVoidMethod(env, activity->clazz, setOrientationMethod, o);
    }
    (*activity->vm)->DetachCurrentThread(activity->vm);

    create_gl_surface(&_glfwWin);

    int result = pipe(_glfwWin.m_Pipefd);
    if (result != 0) {
        LOGF("Could not open pipe for communication: %d", result);
    }
    result = ALooper_addFd(g_AndroidApp->looper, _glfwWin.m_Pipefd[0], ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, LooperCallback, &_glfwWin);
    if (result != 1) {
        LOGF("Could not add file descriptor to looper: %d", result);
    }

    return GL_TRUE;
}

//========================================================================
// Properly kill the window/video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    LOGV("_glfwPlatformCloseWindow");

    int result = ALooper_removeFd(g_AndroidApp->looper, _glfwWin.m_Pipefd[0]);
    if (result != 1) {
        LOGF("Could not remove fd from looper: %d", result);
    }

    destroy_gl_surface(&_glfwWin);

    final_gl(&_glfwWin);
}

int _glfwPlatformGetDefaultFramebuffer( )
{
    return 0;
}

//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle( const char *title )
{
}

//========================================================================
// Set the window size
//========================================================================

void _glfwPlatformSetWindowSize( int width, int height )
{
}

//========================================================================
// Set the window position.
//========================================================================

void _glfwPlatformSetWindowPos( int x, int y )
{
}

//========================================================================
// Window iconification
//========================================================================

void _glfwPlatformIconifyWindow( void )
{
    // Call finish and let Android life cycle take care of the iconification
    ANativeActivity_finish(g_AndroidApp->activity);
}

//========================================================================
// Window un-iconification
//========================================================================

void _glfwPlatformRestoreWindow( void )
{
}

//========================================================================
// Swap OpenGL buffers and poll any new events
//========================================================================

void _glfwPlatformSwapBuffers( void )
{
    if (_glfwWin.surface != EGL_NO_SURFACE)
    {
        eglSwapBuffers(_glfwWin.display, _glfwWin.surface);
        CHECK_EGL_ERROR
    }

    /*
     The preferred way of handling orientation changes is probably
     in APP_CMD_CONFIG_CHANGED or APP_CMD_WINDOW_RESIZED but occasionally
     the wrong previous orientation is reported (Tested on Samsung S2 GTI9100 4.1.2).
     This might very well be a bug..
     */
    EGLint w, h;
    EGLDisplay display = _glfwWin.display;
    EGLSurface surface = _glfwWin.surface;

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    CHECK_EGL_ERROR
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    CHECK_EGL_ERROR

    if (_glfwWin.width != w || _glfwWin.height != h) {
        LOGV("window size changed from %dx%d to %dx%d", _glfwWin.width, _glfwWin.height, w, h);
        if (_glfwWin.windowSizeCallback) {
            _glfwWin.windowSizeCallback(w, h);
        }
    }

    _glfwWin.width = w;
    _glfwWin.height = h;
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    // eglSwapInterval is not supported on all devices, so clear the error here
    // (yields EGL_BAD_PARAMETER when not supported for kindle and HTC desire)
    // https://groups.google.com/forum/#!topic/android-developers/HvMZRcp3pt0
    eglSwapInterval(_glfwWin.display, interval);
    EGLint error = eglGetError();
    (void)error;
}

//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================

void _glfwPlatformRefreshWindowParams( void )
{
}

//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
   int ident;
   int events;
   struct android_poll_source* source;

   // TODO: Terrible hack. See comment in top of file
   if (g_KeyboardActive) {
       if (g_SpecialKeyActive == 0) {
           _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_RELEASE );
           _glfwInputKey( GLFW_KEY_ENTER, GLFW_RELEASE );
       } else {
           g_SpecialKeyActive--;
       }
   }

   int timeout = 0;
   if (_glfwWin.iconified) {
       timeout = 1000 * 3000;
   }
   while ((ident=ALooper_pollAll(timeout, NULL, &events, (void**)&source)) >= 0)
   {
       timeout = 0;
       // Process this event.
       if (source != NULL) {
           source->process(_glfwWin.app, source);
       }
   }
}

//========================================================================
// Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents( void )
{
}

//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor( void )
{
}

//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor( void )
{
}

//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
}

void _glfwShowKeyboard( int show, int type, int auto_close )
{
    // JNI implemntation as ANativeActivity_showSoftInput seems to be broken...
    // https://code.google.com/p/android/issues/detail?id=35991
    // The actual call is implemented in DefoldActivity#showSoftInput to ensure UI thread

    g_KeyboardActive = show;
    g_autoCloseKeyboard = auto_close;

    jint result;

    JavaVM* lJavaVM = g_AndroidApp->activity->vm;
    JNIEnv* lJNIEnv = g_AndroidApp->activity->env;

    JavaVMAttachArgs lJavaVMAttachArgs;
    lJavaVMAttachArgs.version = JNI_VERSION_1_6;
    lJavaVMAttachArgs.name = "NativeThread";
    lJavaVMAttachArgs.group = NULL;

    result = (*lJavaVM)->AttachCurrentThread(lJavaVM, &lJNIEnv, &lJavaVMAttachArgs);
    if (result == JNI_ERR) {
        return;
    }

    jobject native_activity = g_AndroidApp->activity->clazz;
    jclass native_activity_class = (*lJNIEnv)->GetObjectClass(lJNIEnv, native_activity);

    if (show) {
        jmethodID show_soft_input_method = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "showSoftInput", "()V");
        (*lJNIEnv)->CallVoidMethod(lJNIEnv, native_activity, show_soft_input_method);
    } else {
        jmethodID hide_soft_input_method = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "hideSoftInput", "()V");
        (*lJNIEnv)->CallVoidMethod(lJNIEnv, native_activity, hide_soft_input_method);
    }

    (*lJavaVM)->DetachCurrentThread(lJavaVM);
}

//========================================================================
// Get physical accelerometer
//========================================================================

int _glfwPlatformGetAcceleration(float* x, float* y, float* z)
{
	return 0;
}
