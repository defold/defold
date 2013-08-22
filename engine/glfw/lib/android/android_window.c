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
static int g_KeyboardActive = 0;
// TODO: Hack. PRESS AND RELEASE is sent the same frame. Similar hack on iOS for handling of special keys
static int g_SpecialKeyActive = -1;

static int32_t handleInput(struct android_app* app, AInputEvent* event)
{

    struct engine* engine = (struct engine*)app->userData;
    int32_t event_type = AInputEvent_getType(event);

    if (event_type == AINPUT_EVENT_TYPE_MOTION)
    {
        if (g_KeyboardActive) {
            // Implicitly hide keyboard
            _glfwShowKeyboard(0, 0);
            return;
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
        int glfw_action;
        if (action == AKEY_EVENT_ACTION_DOWN)
            glfw_action = GLFW_PRESS;
        else
            glfw_action = GLFW_RELEASE;


        if (glfw_action == GLFW_PRESS) {
            switch (code) {
            case AKEYCODE_DEL:
                g_SpecialKeyActive = 10;
                _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_PRESS );
                return;
            case AKEYCODE_ENTER:
                g_SpecialKeyActive = 10;
                _glfwInputKey( GLFW_KEY_ENTER, GLFW_PRESS );
                return;
            }
        }

        JNIEnv* env = g_AndroidApp->activity->env;
        JavaVM* vm = g_AndroidApp->activity->vm;
        (*vm)->AttachCurrentThread(vm, &env, NULL);

        jclass KeyEventClass = (*env)->FindClass(env, "android/view/KeyEvent");
        jmethodID KeyEventConstructor = (*env)->GetMethodID(env, KeyEventClass, "<init>", "(II)V");
        jobject keyEvent = (*env)->NewObject(env, KeyEventClass, KeyEventConstructor, AKeyEvent_getAction(event), AKeyEvent_getKeyCode(event));
        jmethodID KeyEvent_getUnicodeChar = (*env)->GetMethodID(env, KeyEventClass, "getUnicodeChar", "(I)I");
        // NOTE: For certain special characters zero is returned. Something else required to KeyEvent?
        int unicode = (*env)->CallIntMethod(env, keyEvent, KeyEvent_getUnicodeChar, AKeyEvent_getMetaState(event));
        (*env)->DeleteLocalRef( env, keyEvent );

        (*vm)->DetachCurrentThread(vm);

        _glfwInputChar( unicode, glfw_action );
    }

    return 0;
}

int _glfwPlatformOpenWindow( int width__, int height__,
                             const _GLFWwndconfig* wndconfig__,
                             const _GLFWfbconfig* fbconfig__ )
{
    LOGV("_glfwPlatformOpenWindow");

    _glfwWin.app = g_AndroidApp;

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
        (*env)->CallIntMethod(env, activity->clazz, setOrientationMethod, o);
    }
    (*activity->vm)->DetachCurrentThread(activity->vm);

    create_gl_surface(&_glfwWin);

    return GL_TRUE;
}

//========================================================================
// Properly kill the window/video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    LOGV("_glfwPlatformCloseWindow");

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

   while ((ident=ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0)
   {
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

void _glfwShowKeyboard( int show, int type )
{
    // JNI implemntation as ANativeActivity_showSoftInput seems to be broken...
    // https://code.google.com/p/android/issues/detail?id=35991

    g_KeyboardActive = show;

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

    jclass context = (*lJNIEnv)->FindClass(lJNIEnv, "android/content/Context");
    jfieldID input_service_field = (*lJNIEnv)->GetStaticFieldID(lJNIEnv, context, "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
    jobject input_service = (*lJNIEnv)->GetStaticObjectField(lJNIEnv, context, input_service_field);

    jclass input_manager_class = (*lJNIEnv)->FindClass(lJNIEnv, "android/view/inputmethod/InputMethodManager");
    jmethodID get_system_service = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject input_manager = (*lJNIEnv)->CallObjectMethod(lJNIEnv, native_activity, get_system_service, input_service);

    jmethodID get_window_method = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "getWindow", "()Landroid/view/Window;");
    jobject window = (*lJNIEnv)->CallObjectMethod(lJNIEnv, native_activity, get_window_method);
    jclass window_class = (*lJNIEnv)->FindClass(lJNIEnv, "android/view/Window");
    jmethodID get_decor_view_method = (*lJNIEnv)->GetMethodID(lJNIEnv, window_class, "getDecorView", "()Landroid/view/View;");
    jobject decor_view = (*lJNIEnv)->CallObjectMethod(lJNIEnv, window, get_decor_view_method);

    if (show) {
        jmethodID show_soft_input_method = (*lJNIEnv)->GetMethodID(lJNIEnv, input_manager_class, "showSoftInput", "(Landroid/view/View;I)Z");
        (*lJNIEnv)->CallBooleanMethod(lJNIEnv, input_manager, show_soft_input_method, decor_view, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
    } else {
        jclass view_class = (*lJNIEnv)->FindClass(lJNIEnv, "android/view/View");
        jmethodID get_window_token_method = (*lJNIEnv)->GetMethodID(lJNIEnv, view_class, "getWindowToken", "()Landroid/os/IBinder;");
        jobject binder = (*lJNIEnv)->CallObjectMethod(lJNIEnv, decor_view, get_window_token_method);

        jmethodID hide_soft_input_method = (*lJNIEnv)->GetMethodID(lJNIEnv, input_manager_class, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
        (*lJNIEnv)->CallBooleanMethod(lJNIEnv, input_manager, hide_soft_input_method, binder, ANATIVEACTIVITY_HIDE_SOFT_INPUT_NOT_ALWAYS);
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
