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
int g_autoCloseKeyboard = 0;
// TODO: Hack. PRESS AND RELEASE is sent the same frame. Similar hack on iOS for handling of special keys
int g_SpecialKeyActive = -1;

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_FakeBackspace(JNIEnv* env, jobject obj)
{
    g_SpecialKeyActive = 10;
    _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_PRESS );
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_glfwInputCharNative(JNIEnv* env, jobject obj, jint unicode)
{
    struct Command cmd;
    cmd.m_Command = CMD_INPUT_CHAR;
    cmd.m_Data = (void*)unicode;
    if (write(_glfwWin.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        LOGF("Failed to write command");
    }
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_glfwSetMarkedTextNative(JNIEnv* env, jobject obj, jstring text)
{
    const jsize len = (*env)->GetStringUTFLength(env, text);
    const char* text_chrs = (*env)->GetStringUTFChars(env, text, (jboolean *)0);

    char* cmd_text = (char*)malloc(len * sizeof(char) + 1);
    memcpy(cmd_text, text_chrs, (int)len*sizeof(char) );
    cmd_text[len] = '\0';

    struct Command cmd;
    cmd.m_Command = CMD_INPUT_MARKED_TEXT;
    cmd.m_Data = (void*)cmd_text;
    if (write(_glfwWin.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        LOGF("Failed to write command");
    }

    (*env)->ReleaseStringUTFChars(env, text, text_chrs);
}

int _glfwPlatformOpenWindow( int width__, int height__,
                             const _GLFWwndconfig* wndconfig__,
                             const _GLFWfbconfig* fbconfig__ )
{
    LOGV("_glfwPlatformOpenWindow");
    RestoreWin(&_glfwWin);
    create_gl_surface(&_glfwWin);
    return GL_TRUE;
}

//========================================================================
// Properly kill the window/video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    LOGV("_glfwPlatformCloseWindow");

    if (_glfwWin.opened) {
        destroy_gl_surface(&_glfwWin);
        _glfwWin.opened = 0;
    }
    SaveWin(&_glfwWin);
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
    if (_glfwWin.display == EGL_NO_DISPLAY || _glfwWin.surface == EGL_NO_SURFACE || _glfwWin.iconified == 1)
    {
        return;
    }

    if (!eglSwapBuffers(_glfwWin.display, _glfwWin.surface))
    {
        // Error checking inspired by Android implementation of GLSurfaceView:
        // http://grepcode.com/file/repository.grepcode.com/java/ext/com.google.android/android/5.1.0_r1/android/opengl/GLSurfaceView.java?av=f
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS) {

            if (error == EGL_CONTEXT_LOST) {
                LOGE("eglSwapBuffers failed due to EGL_CONTEXT_LOST!");
                assert(0);
                return;
            } else if (error == EGL_BAD_SURFACE) {
                // Recreate surface
                LOGE("eglSwapBuffers failed due to EGL_BAD_SURFACE, destroy surface and wait for recreation.");
                destroy_gl_surface(&_glfwWin);
                _glfwWin.iconified = 1;
                _glfwWin.hasSurface = 0;
                return;
            } else {
                // Other errors typically mean that the current surface is bad,
                // probably because the SurfaceView surface has been destroyed,
                // but we haven't been notified yet.
                // Ignore error, but log for debugging purpose.
                LOGW("eglSwapBuffers failed, eglGetError: %X", error);
                return;
            }
        }
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
    assert(error == EGL_SUCCESS || error == EGL_BAD_PARAMETER);
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
        jmethodID show_soft_input_method = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "showSoftInput", "(I)V");
        (*lJNIEnv)->CallVoidMethod(lJNIEnv, native_activity, show_soft_input_method, type);
    } else {
        jmethodID hide_soft_input_method = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "hideSoftInput", "()V");
        (*lJNIEnv)->CallVoidMethod(lJNIEnv, native_activity, hide_soft_input_method);
    }

    (*lJavaVM)->DetachCurrentThread(lJavaVM);
}

void _glfwResetKeyboard( void )
{
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

    jmethodID reset_soft_input_method = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "resetSoftInput", "()V");
    (*lJNIEnv)->CallVoidMethod(lJNIEnv, native_activity, reset_soft_input_method);

    (*lJavaVM)->DetachCurrentThread(lJavaVM);
}


void _glfwAndroidSetInputMethod(int use_hidden_input)
{
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

    jmethodID reset_soft_input_method = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "setUseHiddenInputField", "(Z)V");
    (*lJNIEnv)->CallVoidMethod(lJNIEnv, native_activity, reset_soft_input_method, use_hidden_input);

    (*lJavaVM)->DetachCurrentThread(lJavaVM);
}

void _glfwAndroidSetImmersiveMode(int immersive_mode)
{
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

    jmethodID set_immersive_mode = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "setImmersiveMode", "(Z)V");
    (*lJNIEnv)->CallVoidMethod(lJNIEnv, native_activity, set_immersive_mode, immersive_mode);

    (*lJavaVM)->DetachCurrentThread(lJavaVM);
}

//========================================================================
// Defold extension: Get native references (window, view and context)
//========================================================================
GLFWAPI EGLContext glfwGetAndroidEGLContext()
{
    return _glfwWin.context;
}

GLFWAPI EGLSurface glfwGetAndroidEGLSurface()
{
    return _glfwWin.surface;
}

GLFWAPI JavaVM* glfwGetAndroidJavaVM()
{
    return g_AndroidApp->activity->vm;
}

GLFWAPI jobject glfwGetAndroidActivity()
{
    return g_AndroidApp->activity->clazz;
}


//========================================================================
// Query auxillary context
//========================================================================
int _glfwPlatformQueryAuxContext()
{
    return query_gl_aux_context(&_glfwWin);
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
void* _glfwPlatformAcquireAuxContext()
{
    return acquire_gl_aux_context(&_glfwWin);
}

//========================================================================
// Unacquire auxillary context for current thread
//========================================================================
void _glfwPlatformUnacquireAuxContext(void* context)
{
    unacquire_gl_aux_context(&_glfwWin);
}



