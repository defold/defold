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


// Modified for Defold: private mobile/web backend, without the GLFW API.
#include "internal.h"

#include <limits.h>
#include <jni.h>
#include <unistd.h>

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
#include "android_jni.h"
#include "android_window_backend.h"

extern struct android_app* g_AndroidApp;
extern int g_AppCommands[MAX_APP_COMMANDS];
extern int g_NumAppCommands;
extern struct InputEvent* g_AppInputEvents;
extern int g_NumAppInputEvents;
extern uint32_t g_EventLock;
extern const char* dmNativeGetAndroidCmdName(int32_t cmd);

int g_KeyboardActive = 0;
int g_autoCloseKeyboard = 0;
// TODO: Hack. PRESS AND RELEASE is sent the same frame. Similar hack on iOS for handling of special keys
int g_SpecialKeyActive = -1;

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_FakeBackspace(JNIEnv* env, jobject obj)
{
    g_SpecialKeyActive = 10;
    dmNativeInputKey( NATIVE_KEY_BACKSPACE, NATIVE_PRESS );
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_FakeEnter(JNIEnv* env, jobject obj)
{
    g_SpecialKeyActive = 10;
    dmNativeInputKey( NATIVE_KEY_ENTER, NATIVE_PRESS );
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_dmNativeInputBackButton(JNIEnv* env, jobject obj)
{
    g_SpecialKeyActive = 10;
    dmNativeInputKey( NATIVE_KEY_BACK, NATIVE_PRESS );
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_dmNativeInputCharNative(JNIEnv* env, jobject obj, jint unicode)
{
    struct Command cmd;
    cmd.m_Command = CMD_INPUT_CHAR;
    cmd.m_Data = (void*)(uintptr_t)unicode;
    if (write(dmNativeWinAndroid.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        LOGF("Failed to write command");
    }
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_dmNativeSetPendingResizeBecauseOfInsets(JNIEnv* env, jobject obj)
{
    dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets();
}

JNIEXPORT void JNICALL Java_com_dynamo_android_DefoldActivity_dmNativeSetMarkedTextNative(JNIEnv* env, jobject obj, jstring text)
{
    const jsize len = (*env)->GetStringUTFLength(env, text);
    const char* text_chrs = (*env)->GetStringUTFChars(env, text, (jboolean *)0);

    char* cmd_text = (char*)malloc(len * sizeof(char) + 1);
    memcpy(cmd_text, text_chrs, (int)len*sizeof(char) );
    cmd_text[len] = '\0';

    struct Command cmd;
    cmd.m_Command = CMD_INPUT_MARKED_TEXT;
    cmd.m_Data = (void*)cmd_text;
    if (write(dmNativeWinAndroid.m_Pipefd[1], &cmd, sizeof(cmd)) != sizeof(cmd)) {
        LOGF("Failed to write command");
    }

    (*env)->ReleaseStringUTFChars(env, text, text_chrs);
}

int dmNativeOSGetWindowRefreshRate( void )
{
    return dmNativeAndroidPlatformGetWindowRefreshRate();
}

int dmNativeOSOpenWindow( int width__, int height__,
                             const Nativewndconfig* wndconfig__,
                             const Nativefbconfig* fbconfig__ )
{
    LOGV("dmNativeOSOpenWindow");

    if (!dmNativeAndroidPlatformOpenWindow(width__, height__, wndconfig__, fbconfig__))
    {
        return GL_FALSE;
    }

    dmNativeTerminateJoysticks();

    return GL_TRUE;
}

//========================================================================
// Properly kill the window/video display
//========================================================================

void dmNativeOSCloseWindow( void )
{
    LOGV("dmNativeOSCloseWindow");
    dmNativeAndroidPlatformCloseWindow();
}

int dmNativeOSGetDefaultFramebuffer( )
{
    return 0;
}

//========================================================================
// Set the window title
//========================================================================

void dmNativeOSSetWindowTitle( const char *title )
{
}

//========================================================================
// Set the window size
//========================================================================

void dmNativeOSSetWindowSize( int width, int height )
{
    if (dmNativeWin.opened && dmNativeWin.clientAPI == NATIVE_NO_API)
    {
        dmNativeWin.width = width;
        dmNativeWin.height = height;
    }
}

//========================================================================
// Set the window position.
//========================================================================

void dmNativeOSSetWindowPos( int x, int y )
{
}

//========================================================================
// Window iconification
//========================================================================

void dmNativeOSIconifyWindow( void )
{
    // Call finish and let Android life cycle take care of the iconification
    ANativeActivity_finish(g_AndroidApp->activity);
}

//========================================================================
// Window un-iconification
//========================================================================

void dmNativeOSRestoreWindow( void )
{
}

void dmNativeOSSwapBuffers( void )
{
    dmNativeAndroidPlatformSwapBuffers();
    spinlock_unlock(&dmNativeWinAndroid.m_RenderLock);
}


//========================================================================
// Set double buffering swap interval
//========================================================================

void dmNativeOSSwapInterval( int interval )
{
    dmNativeAndroidPlatformSwapInterval(interval);
}

//========================================================================
// Write back window parameters into Native window structure
//========================================================================

void dmNativeOSRefreshWindowParams( void )
{
}

//========================================================================
// Poll for new window and input events
//========================================================================


void dmNativeAndroidBeginFrame()
{
    spinlock_lock(&dmNativeWinAndroid.m_RenderLock);
}

void dmNativeAndroidFlushEvents()
{
    int app_commands[MAX_APP_COMMANDS];
    static struct InputEvent* flush_input_events = 0;
    static int flush_input_events_capacity = 0;
    struct InputEvent* input_events = 0;

    int num_app_commands = 0;
    int num_input_events = 0;
    int events_copied = 0;
    while (!events_copied)
    {
        spinlock_lock(&g_EventLock);

        num_app_commands = g_NumAppCommands;
        num_input_events = g_NumAppInputEvents;
        if (num_input_events <= flush_input_events_capacity)
        {
            memcpy(app_commands, g_AppCommands, num_app_commands * sizeof(int));
            g_NumAppCommands = 0;

            if (num_input_events > 0)
            {
                memcpy(flush_input_events, g_AppInputEvents, num_input_events * sizeof(struct InputEvent));
                input_events = flush_input_events;
            }
            g_NumAppInputEvents = 0;
            events_copied = 1;
        }

        spinlock_unlock(&g_EventLock);

        if (!events_copied)
        {
            int new_flush_input_events_capacity = flush_input_events_capacity > 0 ? flush_input_events_capacity : APP_INPUT_EVENTS_SIZE_INCREASE_STEP;
            while (new_flush_input_events_capacity < num_input_events)
            {
                new_flush_input_events_capacity *= 2;
            }

            // Grow the local copy buffer outside g_EventLock. The queues are not drained
            // until the next locked pass can snapshot commands and input events together.
            struct InputEvent* new_flush_input_events = (struct InputEvent*) realloc(flush_input_events, new_flush_input_events_capacity * sizeof(struct InputEvent));
            if (new_flush_input_events == 0)
            {
                LOGE("dmNativeAndroidFlushEvents: failed to allocate %d input events", new_flush_input_events_capacity);
                num_app_commands = 0;
                num_input_events = 0;
                events_copied = 1;
            }
            else
            {
                flush_input_events = new_flush_input_events;
                flush_input_events_capacity = new_flush_input_events_capacity;
            }
        }
    }

    for (int i = 0; i < num_app_commands; ++i)
    {
        int cmd = app_commands[i];

        LOGV("handleCommand (main thread): %s", dmNativeGetAndroidCmdName(cmd));

        switch(cmd)
        {
        case APP_CMD_TERM_WINDOW:
            dmNativeAndroidPlatformOnTermWindow();
            computeIconifiedState();
            break;

        case APP_CMD_INIT_WINDOW:
            dmNativeAndroidPlatformOnInitWindow();
            computeIconifiedState();
            break;

        case APP_CMD_GAINED_FOCUS:
            dmNativeAndroidPlatformOnGainedFocus();
            break;

        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONFIG_CHANGED:
        case APP_CMD_CONTENT_RECT_CHANGED:
            dmNativeAndroidPlatformOnResize();
            computeIconifiedState();
            break;

        case APP_CMD_WINDOW_REDRAW_NEEDED:
            break;

        case APP_CMD_PAUSE:
            if(dmNativeWin.windowFocusCallback)
                dmNativeWin.windowFocusCallback(0); // invokes Lua callbacks
            break;
        case APP_CMD_RESUME:
            if(dmNativeWin.windowFocusCallback)
                dmNativeWin.windowFocusCallback(1); // invokes Lua callbacks
            break;

        }
    }

    dmNativeAndroidPlatformAfterFlushEvents();

    JNIEnv* env = 0;
    JavaVM* vm = 0;
    if (num_input_events > 0)
    {
        env = g_AndroidApp->activity->env;
        vm = g_AndroidApp->activity->vm;
        (*vm)->AttachCurrentThread(vm, &env, NULL);
    }

    for (int i = 0; i < num_input_events; ++i)
    {
        struct InputEvent* event = &input_events[i];
        dmNativeAndroidDispatchInput(dmNativeWinAndroid.app, env, event);
    }

    if (vm != 0)
    {
        (*vm)->DetachCurrentThread(vm);
    }
}

void androidDestroyWindow( void )
{
    if (dmNativeWin.opened) {
        dmNativeWin.opened = 0;
        dmNativeAndroidPlatformDestroyWindow();
        computeIconifiedState();
    }
}

// Called from the engine thread
void dmNativeOSPollEvents( void )
{
    // TODO: Terrible hack. See comment in top of file
    if (g_SpecialKeyActive > 0) {
       g_SpecialKeyActive--;
       if (g_SpecialKeyActive == 0) {
           dmNativeInputKey( NATIVE_KEY_BACKSPACE, NATIVE_RELEASE );
           dmNativeInputKey( NATIVE_KEY_ENTER, NATIVE_RELEASE );
           dmNativeInputKey( NATIVE_KEY_BACK, NATIVE_RELEASE );
       }
    }

    dmNativeAndroidDiscoverJoysticks();

    dmNativeAndroidFlushEvents();
}

// Called from the looper thread
void dmNativeAndroidPollEvents()
{
    int timeoutMillis = 0;
    if (dmNativeWin.iconified) {
        timeoutMillis = 300;
    }
    void* data = NULL;
    int ident = ALooper_pollOnce(timeoutMillis, NULL, NULL, &data);

    if (ident >= 0 && data != NULL) {
        struct android_poll_source* source = (struct android_poll_source*)data;
        source->process(dmNativeWinAndroid.app, source);
    }
    if (ident == ALOOPER_POLL_ERROR) {
        LOGF("ALooper_pollOnce returned an error");
        return;
    }

    if (dmNativeWinAndroid.app->destroyRequested) {
        androidDestroyWindow();
        // OS is destroyng the app. All the other events doesn't matter in this case.
        return;
    }
}

//========================================================================
// Wait for new window and input events
//========================================================================

void dmNativeOSWaitEvents( void )
{
}

//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void dmNativeOSHideMouseCursor( void )
{
}

//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void dmNativeOSShowMouseCursor( void )
{
}

//========================================================================
// Set physical mouse cursor position
//========================================================================

void dmNativeOSSetMouseCursorPos( int x, int y )
{
}

void dmNativeShowKeyboard( int show, int type, int auto_close )
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

void dmNativeResetKeyboard( void )
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


void dmNativeAndroidSetInputMethod(int use_hidden_input)
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

void dmNativeAndroidSetFullscreenParameters(int immersive_mode, int display_cutout)
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

int dmNativeAndroidGetSafeAreaInsets(int* left, int* top, int* right, int* bottom)
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
        return 0;
    }

    jobject native_activity = g_AndroidApp->activity->clazz;
    jclass native_activity_class = (*lJNIEnv)->GetObjectClass(lJNIEnv, native_activity);

    jmethodID get_safe_area_insets = (*lJNIEnv)->GetMethodID(lJNIEnv, native_activity_class, "getSafeAreaInsets", "()[I");
    if (!get_safe_area_insets)
    {
        (*lJavaVM)->DetachCurrentThread(lJavaVM);
        return 0;
    }

    jintArray array = (jintArray)(*lJNIEnv)->CallObjectMethod(lJNIEnv, native_activity, get_safe_area_insets);
    if (!array)
    {
        (*lJavaVM)->DetachCurrentThread(lJavaVM);
        return 0;
    }

    jsize len = (*lJNIEnv)->GetArrayLength(lJNIEnv, array);
    if (len < 4)
    {
        (*lJavaVM)->DetachCurrentThread(lJavaVM);
        return 0;
    }

    jint* values = (*lJNIEnv)->GetIntArrayElements(lJNIEnv, array, NULL);
    if (!values)
    {
        (*lJavaVM)->DetachCurrentThread(lJavaVM);
        return 0;
    }

    *left = values[0];
    *top = values[1];
    *right = values[2];
    *bottom = values[3];

    (*lJNIEnv)->ReleaseIntArrayElements(lJNIEnv, array, values, JNI_ABORT);
    (*lJavaVM)->DetachCurrentThread(lJavaVM);
    return 1;
}

//========================================================================
// Defold extension: Get native references (window, view and context)
//========================================================================
EGLContext dmNativeGetAndroidEGLContext()
{
    return dmNativeWinAndroid.context;
}

EGLSurface dmNativeGetAndroidEGLSurface()
{
    return dmNativeWinAndroid.surface;
}

JavaVM* dmNativeGetAndroidJavaVM()
{
    return g_AndroidApp->activity->vm;
}

jobject dmNativeGetAndroidActivity()
{
    return g_AndroidApp->activity->clazz;
}

struct android_app* dmNativeGetAndroidApp(void)
{
    return g_AndroidApp;
}

ANativeWindow* dmNativeAndroidAcquireWindow(void)
{
    struct android_app* app = g_AndroidApp;
    if (app == NULL)
        return NULL;

    int did_attach = 0;
    JNIAttachCurrentThreadIfNeeded(&did_attach);
    pthread_mutex_lock(&app->mutex);
    ANativeWindow* window = app->window;
    if (app->pendingWindow != window)
        window = NULL;
    if (window)
        ANativeWindow_acquire(window);
    pthread_mutex_unlock(&app->mutex);
    JNIDetachCurrentThreadIfNeeded(did_attach);

    return window;
}

ANativeWindow* dmNativeAcquireAndroidWindow(void)
{
    return dmNativeAndroidAcquireWindow();
}

ANativeWindow* dmNativeWaitForAndroidWindow(void)
{
    const useconds_t wait_period = 50 * 1000;
    int logged_wait = 0;
    while (g_AndroidApp != NULL && !g_AndroidApp->destroyRequested)
    {
        if (dmNativeAndroidIsAppResumed())
        {
            ANativeWindow* window = dmNativeAndroidAcquireWindow();
            if (window)
            {
                LOGI("ENGINE THREAD: Window ready!");
                return window;
            }
        }

        if (!logged_wait)
        {
            LOGI("ENGINE THREAD: Window not ready. Waiting...");
            logged_wait = 1;
        }
        usleep(wait_period);
    }

    LOGI("ENGINE THREAD: App is being destroyed. Exiting!");
    return NULL;
}

int dmNativeAndroidIsWindowCurrent(ANativeWindow* window)
{
    struct android_app* app = g_AndroidApp;
    if (app == NULL || window == NULL || !dmNativeAndroidIsAppResumed())
        return 0;

    pthread_mutex_lock(&app->mutex);
    int is_current = app->window == window && app->pendingWindow == window;
    pthread_mutex_unlock(&app->mutex);
    return is_current;
}

void dmNativeReleaseAndroidWindow(ANativeWindow* window)
{
    if (window)
    {
        int did_attach = 0;
        JNIAttachCurrentThreadIfNeeded(&did_attach);
        ANativeWindow_release(window);
        JNIDetachCurrentThreadIfNeeded(did_attach);
    }
}

//========================================================================
// Query auxillary context
//========================================================================
int dmNativeOSQueryAuxContext()
{
    return dmNativeAndroidPlatformQueryAuxContext();
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
void* dmNativeOSAcquireAuxContext()
{
    return dmNativeAndroidPlatformAcquireAuxContext();
}

//========================================================================
// Unacquire auxillary context for current thread
//========================================================================
void dmNativeOSUnacquireAuxContext(void* context)
{
    dmNativeAndroidPlatformUnacquireAuxContext(context);
}

void dmNativeOSSetViewType(int view_type)
{
}

void dmNativeOSSetWindowBackgroundColor(unsigned int color)
{
}

float dmNativeOSGetDisplayScaleFactor()
{
    return 1.0f;
}

#define MAX_ACTIVITY_LISTENERS (32)
static dmNativeactivityresultfun g_Listeners[MAX_ACTIVITY_LISTENERS];
static int g_ListenersCount = 0;

void dmNativeAndroidRegisterOnActivityResultListener(dmNativeactivityresultfun listener)
{
    if (g_ListenersCount >= MAX_ACTIVITY_LISTENERS) {
        LOGW("Max activity listeners reached (%d)", MAX_ACTIVITY_LISTENERS);
    } else {
        g_Listeners[g_ListenersCount++] = listener;
    }
}

void dmNativeAndroidUnregisterOnActivityResultListener(dmNativeactivityresultfun listener)
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
static dmNativeoncreatefun g_onCreate_Listeners[MAX_ONCREATE_LISTENERS];
static int g_onCreate_ListenersCount = 0;

void dmNativeAndroidRegisterOnCreateListener(dmNativeoncreatefun listener)
{
    if (g_onCreate_ListenersCount >= MAX_ONCREATE_LISTENERS) {
        LOGW("Max onCreate listeners reached (%d)", MAX_ONCREATE_LISTENERS);
    } else {
        g_onCreate_Listeners[g_onCreate_ListenersCount++] = listener;
    }
}

void dmNativeAndroidUnregisterOnCreateListener(dmNativeoncreatefun listener)
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
