//========================================================================
// GLFW - An OpenGL framework
// Platform:    Android EGL window backend
// API version: 2.7
//========================================================================


// Modified for Defold: private mobile/web backend, without the GLFW API.
#include "android_window_backend.h"

#include <jni.h>

#include "android_log.h"
#include "android_util.h"
#include "android_jni.h"

extern struct android_app* g_AndroidApp;

static int g_PendingResize = 0;
static int g_PendingResizeBecauseOfInsets = 0;

static GlfwAndroidEglResult HandleGLSurfaceFailure(GlfwAndroidEglResult result)
{
    result = limit_egl_failure_retries(&dmNativeWinAndroid, result);
    destroy_gl_surface(&dmNativeWinAndroid);
    dmNativeWinAndroid.should_recreate_surface = is_egl_result_retryable(result);
    dmNativeWin.iconified = 1;

    if (result == NATIVE_ANDROID_EGL_RESULT_FATAL)
    {
        LOGE("Fatal EGL failure. Closing the window.");
        androidDestroyWindow();
    }

    return result;
}

static GlfwAndroidEglResult CreateGLSurface()
{
    GlfwAndroidEglResult result = create_gl_surface(&dmNativeWinAndroid);
    if (result != NATIVE_ANDROID_EGL_RESULT_READY)
    {
        result = HandleGLSurfaceFailure(result);
        return result;
    }

    // This thread attachment is a workaround for this crash
    // https://github.com/defold/defold/issues/6956
    // only on Android 13
    int did_attach = 0;
    JNIAttachCurrentThreadIfNeeded(&did_attach);

    result = make_current(&dmNativeWinAndroid);

    JNIDetachCurrentThreadIfNeeded(did_attach);
    if (result == NATIVE_ANDROID_EGL_RESULT_READY)
        result = update_width_height_info(&dmNativeWin, &dmNativeWinAndroid, 1);

    if (result != NATIVE_ANDROID_EGL_RESULT_READY)
    {
        result = HandleGLSurfaceFailure(result);
        return result;
    }

    reset_egl_failure_retries(&dmNativeWinAndroid);
    dmNativeWinAndroid.should_recreate_surface = 0;
    computeIconifiedState();
    return NATIVE_ANDROID_EGL_RESULT_READY;
}

int dmNativeAndroidPlatformGetWindowRefreshRate(void)
{
    // Source: http://irrlicht.sourceforge.net/forum/viewtopic.php?f=9&t=50206
    if (dmNativeWinAndroid.display == EGL_NO_DISPLAY || dmNativeWinAndroid.surface == EGL_NO_SURFACE || dmNativeWin.iconified == 1)
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

int dmNativeAndroidPlatformOpenWindow(int width, int height, const Nativewndconfig* wndconfig, const Nativefbconfig* fbconfig)
{
    (void)width;
    (void)height;
    (void)fbconfig;

    dmNativeWin.clientAPI = wndconfig->clientAPI;

    if (dmNativeWin.clientAPI == NATIVE_OPENGL_API)
    {
        GlfwAndroidEglResult result;
        uint32_t retry_count = 0;
        reset_egl_failure_retries(&dmNativeWinAndroid);
        do
        {
            if (init_gl(&dmNativeWinAndroid) == 0)
                return GL_FALSE;

            result = make_current(&dmNativeWinAndroid);
            if (result == NATIVE_ANDROID_EGL_RESULT_READY)
                result = update_width_height_info(&dmNativeWin, &dmNativeWinAndroid, 1);

            if (result != NATIVE_ANDROID_EGL_RESULT_READY)
            {
                result = HandleGLSurfaceFailure(result);
                final_gl(&dmNativeWinAndroid);
            }
            if (is_egl_result_retryable(result))
                wait_for_egl_retry(retry_count++);
        }
        while (is_egl_result_retryable(result));

        if (result != NATIVE_ANDROID_EGL_RESULT_READY)
            return GL_FALSE;

        reset_egl_failure_retries(&dmNativeWinAndroid);
        dmNativeWinAndroid.should_recreate_surface = 0;
        computeIconifiedState();
    }

    return GL_TRUE;
}

void dmNativeAndroidPlatformCloseWindow(void)
{
    if (dmNativeWin.opened && dmNativeWin.clientAPI != NATIVE_NO_API)
    {
        destroy_gl_surface(&dmNativeWinAndroid);
        final_gl(&dmNativeWinAndroid);
        reset_egl_failure_retries(&dmNativeWinAndroid);
        dmNativeWin.opened = 0;
    }
}

void dmNativeAndroidPlatformSwapBuffers(void)
{
    if (dmNativeWinAndroid.display == EGL_NO_DISPLAY || dmNativeWinAndroid.surface == EGL_NO_SURFACE || dmNativeWin.iconified == 1)
    {
        return;
    }

    if (!eglSwapBuffers(dmNativeWinAndroid.display, dmNativeWinAndroid.surface))
    {
        // Error checking inspired by Android implementation of GLSurfaceView:
        // https://android.googlesource.com/platform/frameworks/base/+/master/opengl/java/android/opengl/GLSurfaceView.java
        EGLint error = eglGetError();
        if (error != EGL_SUCCESS)
        {
            if (error == EGL_CONTEXT_LOST)
            {
                LOGE("eglSwapBuffers failed due to EGL_CONTEXT_LOST!");
                assert(0);
                return;
            }
            else if (error == EGL_BAD_SURFACE)
            {
                LOGE("eglSwapBuffers failed due to EGL_BAD_SURFACE, destroy surface and wait for recreation.");
                destroy_gl_surface(&dmNativeWinAndroid);
                dmNativeWinAndroid.should_recreate_surface = 1;
                dmNativeWin.iconified = 1;
                return;
            }
            else
            {
                LOGW("eglSwapBuffers failed, eglGetError: %X", error);
                return;
            }
        }
    }

    /*
     Handle orientation/size changes when signaled by APP_CMD_CONFIG_CHANGED,
     APP_CMD_WINDOW_RESIZED or APP_CMD_CONTENT_RECT_CHANGED. Some devices
     report the old EGL size at the time of the event, so we defer the query
     until a swap occurs.
     */
    if (g_PendingResize || g_PendingResizeBecauseOfInsets)
    {
        GlfwAndroidEglResult result = update_width_height_info(&dmNativeWin, &dmNativeWinAndroid, 1);
        if (result != NATIVE_ANDROID_EGL_RESULT_READY)
        {
            HandleGLSurfaceFailure(result);
            return;
        }
        g_PendingResize = 0;
        g_PendingResizeBecauseOfInsets = 0;
    }
}

void dmNativeAndroidPlatformSwapInterval(int interval)
{
    if (dmNativeWin.clientAPI != NATIVE_NO_API)
    {
        // eglSwapInterval is not supported on all devices, so clear the error here
        // (yields EGL_BAD_PARAMETER when not supported for kindle and HTC desire)
        // https://groups.google.com/forum/#!topic/android-developers/HvMZRcp3pt0
        eglSwapInterval(dmNativeWinAndroid.display, interval);
        EGLint error = eglGetError();
        assert(error == EGL_SUCCESS || error == EGL_BAD_PARAMETER);
        (void)error;
    }
}

int32_t dmNativeAndroidPlatformVerifySurface(void)
{
    // Although it's the wrong place to do a eglSwapbuffers, we're already handling a bad state from the last opengl error
    // Verifying the state of the surface is worth it.
    if (!eglSwapBuffers(dmNativeWinAndroid.display, dmNativeWinAndroid.surface))
    {
        EGLint error = eglGetError();
        int32_t result = dmNativeAndroidVerifySurfaceError(error);
        if (!result)
        {
            destroy_gl_surface(&dmNativeWinAndroid);
            dmNativeWinAndroid.should_recreate_surface = 1;
            dmNativeWin.iconified = 1;
            return result;
        }
    }
    return 1; // surface is ok
}

void dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets(void)
{
    g_PendingResizeBecauseOfInsets = 1;
}

void dmNativeAndroidPlatformOnTermWindow(void)
{
    reset_egl_failure_retries(&dmNativeWinAndroid);
    if (dmNativeWin.clientAPI != NATIVE_NO_API)
    {
        spinlock_lock(&dmNativeWinAndroid.m_RenderLock);

        destroy_gl_surface(&dmNativeWinAndroid);
        dmNativeWinAndroid.surface = EGL_NO_SURFACE;

        spinlock_unlock(&dmNativeWinAndroid.m_RenderLock);
    }
}

void dmNativeAndroidPlatformOnInitWindow(void)
{
    reset_egl_failure_retries(&dmNativeWinAndroid);
    // We don't get here the first time around, but from the second and onwards
    // The first time, the create_gl_surface() is called from the dmNativeOSOpenWindow function
    if (dmNativeWin.opened && dmNativeWinAndroid.display != EGL_NO_DISPLAY && dmNativeWinAndroid.surface == EGL_NO_SURFACE)
    {
        CreateGLSurface();
    }
}

void dmNativeAndroidPlatformOnGainedFocus(void)
{
    // If we failed to create the window in APP_CMD_INIT_WINDOW, let's try again
    if (dmNativeWin.clientAPI != NATIVE_NO_API && dmNativeWinAndroid.surface == EGL_NO_SURFACE)
    {
        CreateGLSurface();
    }
}

void dmNativeAndroidPlatformOnResize(void)
{
    g_PendingResize = 1;
}

void dmNativeAndroidPlatformAfterFlushEvents(void)
{
    // Still, there seem to be room for the surface to not be ready when the rendering restarts (Issue 5358)
    if (dmNativeWin.clientAPI != NATIVE_NO_API && dmNativeWinAndroid.should_recreate_surface && dmNativeWinAndroid.surface == EGL_NO_SURFACE)
    {
        LOGV("Recreating surface");
        CreateGLSurface();
    }
}

void dmNativeAndroidPlatformDestroyWindow(void)
{
    final_gl(&dmNativeWinAndroid);
}

int dmNativeAndroidPlatformQueryAuxContext(void)
{
    return dmNativeWin.clientAPI == NATIVE_NO_API ? 0 : query_gl_aux_context(&dmNativeWinAndroid);
}

void* dmNativeAndroidPlatformAcquireAuxContext(void)
{
    return dmNativeWin.clientAPI == NATIVE_NO_API ? 0 : acquire_gl_aux_context(&dmNativeWinAndroid);
}

void dmNativeAndroidPlatformUnacquireAuxContext(void* context)
{
    (void)context;
    if (dmNativeWin.clientAPI != NATIVE_NO_API)
    {
        unacquire_gl_aux_context(&dmNativeWinAndroid);
    }
}
