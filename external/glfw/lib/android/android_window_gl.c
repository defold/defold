//========================================================================
// GLFW - An OpenGL framework
// Platform:    Android EGL window backend
// API version: 2.7
//========================================================================

#include "android_window_backend.h"

#include <jni.h>

#include "android_log.h"
#include "android_util.h"
#include "android_jni.h"

extern struct android_app* g_AndroidApp;

static int g_PendingResize = 0;
static int g_PendingResizeBecauseOfInsets = 0;

static void CreateGLSurface()
{
    create_gl_surface(&_glfwWinAndroid);

    // We might have tried to create the surface just as we received an APP_CMD_TERM_WINDOW on the looper thread
    if (_glfwWinAndroid.surface != EGL_NO_SURFACE)
    {
        // This thread attachment is a workaround for this crash
        // https://github.com/defold/defold/issues/6956
        // only on Android 13
        int did_attach = 0;
        JNIAttachCurrentThreadIfNeeded(&did_attach);

        make_current(&_glfwWinAndroid);

        JNIDetachCurrentThreadIfNeeded(did_attach);
        update_width_height_info(&_glfwWin, &_glfwWinAndroid, 1);

        computeIconifiedState();
    }
}

int _glfwAndroidPlatformGetWindowRefreshRate(void)
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

int _glfwAndroidPlatformOpenWindow(int width, int height, const _GLFWwndconfig* wndconfig, const _GLFWfbconfig* fbconfig)
{
    (void)width;
    (void)height;
    (void)fbconfig;

    _glfwWin.clientAPI = wndconfig->clientAPI;

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

void _glfwAndroidPlatformCloseWindow(void)
{
    if (_glfwWin.opened && _glfwWin.clientAPI != GLFW_NO_API)
    {
        destroy_gl_surface(&_glfwWinAndroid);
        final_gl(&_glfwWinAndroid);
        _glfwWin.opened = 0;
    }
}

void _glfwAndroidPlatformSwapBuffers(void)
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
                destroy_gl_surface(&_glfwWinAndroid);
                _glfwWinAndroid.should_recreate_surface = 1;
                _glfwWin.iconified = 1;
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
        update_width_height_info(&_glfwWin, &_glfwWinAndroid, 1);
        g_PendingResize = 0;
        g_PendingResizeBecauseOfInsets = 0;
    }
}

void _glfwAndroidPlatformSwapInterval(int interval)
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

void _glfwAndroidPlatformSetPendingResizeBecauseOfInsets(void)
{
    g_PendingResizeBecauseOfInsets = 1;
}

void _glfwAndroidPlatformOnTermWindow(void)
{
    if (_glfwWin.clientAPI != GLFW_NO_API)
    {
        spinlock_lock(&_glfwWinAndroid.m_RenderLock);

        destroy_gl_surface(&_glfwWinAndroid);
        _glfwWinAndroid.surface = EGL_NO_SURFACE;

        spinlock_unlock(&_glfwWinAndroid.m_RenderLock);
    }
}

void _glfwAndroidPlatformOnInitWindow(void)
{
    // We don't get here the first time around, but from the second and onwards
    // The first time, the create_gl_surface() is called from the _glfwPlatformOpenWindow function
    if (_glfwWin.opened && _glfwWinAndroid.display != EGL_NO_DISPLAY && _glfwWinAndroid.surface == EGL_NO_SURFACE)
    {
        CreateGLSurface();
    }
}

void _glfwAndroidPlatformOnGainedFocus(void)
{
    // If we failed to create the window in APP_CMD_INIT_WINDOW, let's try again
    if (_glfwWinAndroid.surface == EGL_NO_SURFACE)
    {
        CreateGLSurface();
    }
}

void _glfwAndroidPlatformOnResize(void)
{
    g_PendingResize = 1;
}

void _glfwAndroidPlatformAfterFlushEvents(void)
{
    // Still, there seem to be room for the surface to not be ready when the rendering restarts (Issue 5358)
    if (_glfwWinAndroid.should_recreate_surface && _glfwWinAndroid.surface == EGL_NO_SURFACE)
    {
        LOGV("Recreating surface");
        CreateGLSurface();
        _glfwWinAndroid.should_recreate_surface = 0;
    }
}

void _glfwAndroidPlatformDestroyWindow(void)
{
    final_gl(&_glfwWinAndroid);
}

int _glfwAndroidPlatformQueryAuxContext(void)
{
    return _glfwWin.clientAPI == GLFW_NO_API ? 0 : query_gl_aux_context(&_glfwWinAndroid);
}

void* _glfwAndroidPlatformAcquireAuxContext(void)
{
    return _glfwWin.clientAPI == GLFW_NO_API ? 0 : acquire_gl_aux_context(&_glfwWinAndroid);
}

void _glfwAndroidPlatformUnacquireAuxContext(void* context)
{
    (void)context;
    if (_glfwWin.clientAPI != GLFW_NO_API)
    {
        unacquire_gl_aux_context(&_glfwWinAndroid);
    }
}
