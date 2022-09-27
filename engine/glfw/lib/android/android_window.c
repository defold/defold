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
#include "android_joystick.h"

extern struct android_app* g_AndroidApp;
extern int g_AppCommands[MAX_APP_COMMANDS];
extern int g_NumAppCommands;
extern struct InputEvent g_AppInputEvents[MAX_APP_INPUT_EVENTS];
extern int g_NumAppInputEvents;
extern uint32_t g_EventLock;
extern const char* _glfwGetAndroidCmdName(int32_t cmd);

int g_KeyboardActive = 0;
int g_autoCloseKeyboard = 0;
// TODO: Hack. PRESS AND RELEASE is sent the same frame. Similar hack on iOS for handling of special keys
int g_SpecialKeyActive = -1;

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_FakeBackspace(JNIEnv* env, jobject obj)
{
    g_SpecialKeyActive = 10;
    _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_PRESS );
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_FakeEnter(JNIEnv* env, jobject obj)
{
    g_SpecialKeyActive = 10;
    _glfwInputKey( GLFW_KEY_ENTER, GLFW_PRESS );
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_glfwInputCharNative(JNIEnv* env, jobject obj, jint unicode)
{
    struct Command cmd;
    cmd.m_Command = CMD_INPUT_CHAR;
    cmd.m_Data = (void*)(uintptr_t)unicode;
    if (write(_glfwWinAndroid.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
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
    if (write(_glfwWinAndroid.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        LOGF("Failed to write command");
    }

    (*env)->ReleaseStringUTFChars(env, text, text_chrs);
}

int _glfwPlatformGetWindowRefreshRate( void )
{
    // Source: http://irrlicht.sourceforge.net/forum/viewtopic.php?f=9&t=50206
    if (_glfwWinAndroid.display == EGL_NO_DISPLAY || _glfwWinAndroid.surface == EGL_NO_SURFACE || _glfwWin.iconified == 1)
    {
        return 0;
    }

    float refresh_rate = 0.0f;
    jint result;

    JavaVM* lJavaVM = g_AndroidApp->activity->vm;
    JNIEnv* lJNIEnv = g_AndroidApp->activity->env;

    JavaVMAttachArgs lJavaVMAttachArgs;
    lJavaVMAttachArgs.version = JNI_VERSION_1_6;
    lJavaVMAttachArgs.name = "NativeThread";
    lJavaVMAttachArgs.group = NULL;

    result = (*lJavaVM)->AttachCurrentThread(lJavaVM, &lJNIEnv, &lJavaVMAttachArgs);
    if (result == JNI_ERR) {
         return 0;
    }

    jobject native_activity = g_AndroidApp->activity->clazz;
    jclass native_activity_class = (*lJNIEnv)->GetObjectClass(lJNIEnv, native_activity);
    jclass native_window_manager_class = (*lJNIEnv)->FindClass(lJNIEnv, "android/view/WindowManager");
    jclass native_display_class = (*lJNIEnv)->FindClass(lJNIEnv, "android/view/Display");

    if (native_window_manager_class)
    {
        jmethodID get_window_manager = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "getWindowManager", "()Landroid/view/WindowManager;");
        jmethodID get_default_display = (*lJNIEnv)->GetMethodID(lJNIEnv, native_window_manager_class, "getDefaultDisplay", "()Landroid/view/Display;");
        jmethodID get_refresh_rate = (*lJNIEnv)->GetMethodID(lJNIEnv, native_display_class, "getRefreshRate", "()F");
        if (get_refresh_rate)
        {
            jobject window_manager = (*lJNIEnv)->CallObjectMethod(lJNIEnv, native_activity, get_window_manager);

            if (window_manager)
            {
                jobject display = (*lJNIEnv)->CallObjectMethod(lJNIEnv, window_manager, get_default_display);

                if (display)
                {
                    refresh_rate = (*lJNIEnv)->CallFloatMethod(lJNIEnv, display, get_refresh_rate);
                }
            }
        }
    }
    (*lJavaVM)->DetachCurrentThread(lJavaVM);

    return (int)(refresh_rate + 0.5f);
}

int _glfwPlatformOpenWindow( int width__, int height__,
                             const _GLFWwndconfig* wndconfig__,
                             const _GLFWfbconfig* fbconfig__ )
{
    LOGV("_glfwPlatformOpenWindow");

    _glfwWin.clientAPI = wndconfig__->clientAPI;

    if (_glfwWin.clientAPI == GLFW_OPENGL_API)
    {
        if (init_gl(&_glfwWinAndroid) == 0)
        {
            return GL_FALSE;
        }
        make_current(&_glfwWinAndroid);
        update_width_height_info(&_glfwWin, &_glfwWinAndroid, 1);
        computeIconifiedState();
    }

    return GL_TRUE;
}

//========================================================================
// Properly kill the window/video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    LOGV("_glfwPlatformCloseWindow");

    if (_glfwWin.opened && _glfwWin.clientAPI != GLFW_NO_API) {
        destroy_gl_surface(&_glfwWinAndroid);
        final_gl(&_glfwWinAndroid);
        _glfwWin.opened = 0;
    }
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

static void _glfwPlatformSwapBuffersNoLock( void )
{
    if (_glfwWinAndroid.display == EGL_NO_DISPLAY || _glfwWinAndroid.surface == EGL_NO_SURFACE || _glfwWin.iconified == 1)
    {
        return;
    }

    if (!eglSwapBuffers(_glfwWinAndroid.display, _glfwWinAndroid.surface))
    {
        // Error checking inspired by Android implementation of GLSurfaceView:
        // https://android.googlesource.com/platform/frameworks/base/+/master/opengl/java/android/opengl/GLSurfaceView.java
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS) {

            if (error == EGL_CONTEXT_LOST) {
                LOGE("eglSwapBuffers failed due to EGL_CONTEXT_LOST!");
                assert(0);
                return;
            } else if (error == EGL_BAD_SURFACE) {
                // Recreate surface
                LOGE("eglSwapBuffers failed due to EGL_BAD_SURFACE, destroy surface and wait for recreation.");
                destroy_gl_surface(&_glfwWinAndroid);
                _glfwWinAndroid.should_recreate_surface = 1;
                _glfwWin.iconified = 1;
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
    update_width_height_info(&_glfwWin, &_glfwWinAndroid, 0);
}

void _glfwPlatformSwapBuffers( void )
{
    _glfwPlatformSwapBuffersNoLock();
    spinlock_unlock(&_glfwWinAndroid.m_RenderLock);
}


//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    if (_glfwWin.clientAPI != GLFW_NO_API)
    {
        // eglSwapInterval is not supported on all devices, so clear the error here
        // (yields EGL_BAD_PARAMETER when not supported for kindle and HTC desire)
        // https://groups.google.com/forum/#!topic/android-developers/HvMZRcp3pt0
        eglSwapInterval(_glfwWinAndroid.display, interval);
        EGLint error = eglGetError();
        assert(error == EGL_SUCCESS || error == EGL_BAD_PARAMETER);
        (void)error;
    }
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


void glfwAndroidBeginFrame()
{
    spinlock_lock(&_glfwWinAndroid.m_RenderLock);
}

static void CreateGLSurface()
{
    create_gl_surface(&_glfwWinAndroid);

    // We might have tried to create the surface just as we received an APP_CMD_TERM_WINDOW on the looper thread
    if (_glfwWinAndroid.surface != EGL_NO_SURFACE)
    {
        // This thread attachment is a workaround for this crash 
        // https://github.com/defold/defold/issues/6956
        // only on Android 13 
        JNIEnv* env = g_AndroidApp->activity->env;
        JavaVM* vm = g_AndroidApp->activity->vm;
        (*vm)->AttachCurrentThread(vm, &env, NULL);

        make_current(&_glfwWinAndroid);
        
        if (vm != 0)
        {
            (*vm)->DetachCurrentThread(vm);
        }
        update_width_height_info(&_glfwWin, &_glfwWinAndroid, 1);

        computeIconifiedState();
    }
}

void glfwAndroidFlushEvents()
{
    spinlock_lock(&g_EventLock);

    for (int i = 0; i < g_NumAppCommands; ++i)
    {
        int cmd = g_AppCommands[i];

        LOGV("handleCommand (main thread): %s", _glfwGetAndroidCmdName(cmd));

        switch(cmd)
        {
        case APP_CMD_INIT_WINDOW:
            // We don't get here the first time around, but from the second and onwards
            // The first time, the create_gl_surface() is called from the _glfwPlatformOpenWindow function
            if (_glfwWin.opened && _glfwWinAndroid.display != EGL_NO_DISPLAY && _glfwWinAndroid.surface == EGL_NO_SURFACE)
            {
                CreateGLSurface();
            }
            computeIconifiedState();
            break;

        case APP_CMD_GAINED_FOCUS:
            // If we failed to create the window in APP_CMD_INIT_WINDOW, let's try again
            if (_glfwWinAndroid.surface == EGL_NO_SURFACE) {
                CreateGLSurface();
            }
            break;

        case APP_CMD_PAUSE:
            if(_glfwWin.windowFocusCallback)
                _glfwWin.windowFocusCallback(0); // invokes Lua callbacks
            break;
        case APP_CMD_RESUME:
            if(_glfwWin.windowFocusCallback)
                _glfwWin.windowFocusCallback(1); // invokes Lua callbacks
            break;

        }
    }
    g_NumAppCommands = 0;

    // Still, there seem to be room for the surface to not be ready when the rendering restarts (Issue 5358)
    if (_glfwWinAndroid.should_recreate_surface && _glfwWinAndroid.surface == EGL_NO_SURFACE)
    {
        LOGV("Recreating surface");
        CreateGLSurface();
        _glfwWinAndroid.should_recreate_surface = 0;
    }

    JNIEnv* env = 0;
    JavaVM* vm = 0;
    if (g_NumAppInputEvents > 0)
    {
        env = g_AndroidApp->activity->env;
        vm = g_AndroidApp->activity->vm;
        (*vm)->AttachCurrentThread(vm, &env, NULL);
    }

    for (int i = 0; i < g_NumAppInputEvents; ++i)
    {
        struct InputEvent* event = &g_AppInputEvents[i];
        _glfwAndroidHandleInput(_glfwWinAndroid.app, env, event);
    }
    g_NumAppInputEvents = 0;

    if (vm != 0)
    {
        (*vm)->DetachCurrentThread(vm);
    }

    spinlock_unlock(&g_EventLock);
}

void androidDestroyWindow( void )
{
    if (_glfwWin.opened) {
        _glfwWin.opened = 0;
        final_gl(&_glfwWinAndroid);
        computeIconifiedState();
    }
}

// Called from the engine thread
void _glfwPlatformPollEvents( void )
{
    // TODO: Terrible hack. See comment in top of file
    if (g_SpecialKeyActive > 0) {
       g_SpecialKeyActive--;
       if (g_SpecialKeyActive == 0) {
           _glfwInputKey( GLFW_KEY_BACKSPACE, GLFW_RELEASE );
           _glfwInputKey( GLFW_KEY_ENTER, GLFW_RELEASE );
       }
    }

    glfwAndroidDiscoverJoysticks();

    glfwAndroidFlushEvents();
}

// Called from the looper thread
void glfwAndroidPollEvents()
{
    int ident;
    int events;
    struct android_poll_source* source;

    int timeoutMillis = 0;
    if (_glfwWin.iconified) {
        timeoutMillis = 1000 * 2;
    }
    while ((ident=ALooper_pollAll(timeoutMillis, NULL, &events, (void**)&source)) >= 0)
    {
        timeoutMillis = 0;
        // Process this event.
        if (source != NULL) {
            source->process(_glfwWinAndroid.app, source);
        }

        if (_glfwWinAndroid.app->destroyRequested) {
            androidDestroyWindow();
            // OS is destroyng the app. All the other events doesn't matter in this case.
            return;
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

void _glfwAndroidSetFullscreenParameters(int immersive_mode, int display_cutout)
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

    jmethodID set_immersive_mode = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "setFullscreenParameters", "(ZZ)V");
    (*lJNIEnv)->CallVoidMethod(lJNIEnv, native_activity, set_immersive_mode, immersive_mode, display_cutout);

    (*lJavaVM)->DetachCurrentThread(lJavaVM);
}

//========================================================================
// Defold extension: Get native references (window, view and context)
//========================================================================
GLFWAPI EGLContext glfwGetAndroidEGLContext()
{
    return _glfwWinAndroid.context;
}

GLFWAPI EGLSurface glfwGetAndroidEGLSurface()
{
    return _glfwWinAndroid.surface;
}

GLFWAPI JavaVM* glfwGetAndroidJavaVM()
{
    return g_AndroidApp->activity->vm;
}

GLFWAPI jobject glfwGetAndroidActivity()
{
    return g_AndroidApp->activity->clazz;
}

GLFWAPI struct android_app* glfwGetAndroidApp(void)
{
    return g_AndroidApp;
}

//========================================================================
// Query auxillary context
//========================================================================
int _glfwPlatformQueryAuxContext()
{
    return _glfwWin.clientAPI == GLFW_NO_API ? 0 : query_gl_aux_context(&_glfwWinAndroid);
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
void* _glfwPlatformAcquireAuxContext()
{
    return _glfwWin.clientAPI == GLFW_NO_API ? 0 : acquire_gl_aux_context(&_glfwWinAndroid);
}

//========================================================================
// Unacquire auxillary context for current thread
//========================================================================
void _glfwPlatformUnacquireAuxContext(void* context)
{
    if (_glfwWin.clientAPI != GLFW_NO_API)
    {
        unacquire_gl_aux_context(&_glfwWinAndroid);
    }
}

void _glfwPlatformSetViewType(int view_type)
{
}

void _glfwPlatformSetWindowBackgroundColor(unsigned int color)
{
}

float _glfwPlatformGetDisplayScaleFactor()
{
    return 1.0f;
}

#define MAX_ACTIVITY_LISTENERS (32)
static glfwactivityresultfun g_Listeners[MAX_ACTIVITY_LISTENERS];
static int g_ListenersCount = 0;

GLFWAPI void glfwAndroidRegisterOnActivityResultListener(glfwactivityresultfun listener)
{
    if (g_ListenersCount >= MAX_ACTIVITY_LISTENERS) {
        LOGW("Max activity listeners reached (%d)", MAX_ACTIVITY_LISTENERS);
    } else {
        g_Listeners[g_ListenersCount++] = listener;
    }
}

GLFWAPI void glfwAndroidUnregisterOnActivityResultListener(glfwactivityresultfun listener)
{
    for (int i = 0; i < g_ListenersCount; ++i)
    {
        if (g_Listeners[i] == listener)
        {
            g_Listeners[i] = g_Listeners[g_ListenersCount - 1];
            g_ListenersCount--;
            return;
        }
    }
    LOGW("activity listener not found");
}

JNIEXPORT void
Java_com_dynamo_android_DefoldActivity_nativeOnActivityResult(
    JNIEnv *env, jobject thiz, jobject activity, jint requestCode,
    jint resultCode, jobject data) {

    for (int i = g_ListenersCount - 1; i >= 0 ; --i)
    {
        g_Listeners[i](env, activity, requestCode, resultCode, data);
    }
}

#define MAX_ONCREATE_LISTENERS (32)
static glfwoncreatefun g_onCreate_Listeners[MAX_ONCREATE_LISTENERS];
static int g_onCreate_ListenersCount = 0;

GLFWAPI void glfwAndroidRegisterOnCreateListener(glfwoncreatefun listener)
{
    if (g_onCreate_ListenersCount >= MAX_ONCREATE_LISTENERS) {
        LOGW("Max onCreate listeners reached (%d)", MAX_ONCREATE_LISTENERS);
    } else {
        g_onCreate_Listeners[g_onCreate_ListenersCount++] = listener;
    }
}

GLFWAPI void glfwAndroidUnregisterOnCreateListener(glfwoncreatefun listener)
{
    for (int i = 0; i < g_onCreate_ListenersCount; ++i)
    {
        if (g_onCreate_Listeners[i] == listener)
        {
            g_onCreate_Listeners[i] = g_onCreate_Listeners[g_onCreate_ListenersCount - 1];
            g_onCreate_ListenersCount--;
            return;
        }
    }
    LOGW("onCreate listener not found");
}

JNIEXPORT void
Java_com_dynamo_android_DefoldActivity_nativeOnCreate(
    JNIEnv *env, jobject thiz, jobject activity) {

    for (int i = g_onCreate_ListenersCount - 1; i >= 0 ; --i)
    {
        g_onCreate_Listeners[i](env, activity);
    }
}
