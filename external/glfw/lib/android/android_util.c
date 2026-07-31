// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "android_util.h"
#include "android_jni.h"
#include "android_log.h"

#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#define EGL_RETRY_INITIAL_DELAY_US (50 * 1000)
#define EGL_RETRY_MAX_DELAY_US     (800 * 1000)


static bool is_alpha_transparency_enabled()
{
    bool result = false;
    JNIEnv* env = JNIAttachCurrentThread();
    if (env)
    {
        jobject native_activity = g_AndroidApp->activity->clazz;
        jmethodID is_alpha_transparency_enabled_method = JNIGetMethodID(env, native_activity, "isAlphaTransparencyEnabled", "()Z");
        if (is_alpha_transparency_enabled_method) {
            jboolean jresult = (*env)->CallBooleanMethod(env, native_activity, is_alpha_transparency_enabled_method);
            result = (JNI_TRUE == jresult);
        }
        JNIDetachCurrentThread();
    }
    return result;
}

typedef struct EglAttribSetting_t {
    EGLint m_Attribute;
    EGLint m_Value;
} EglAttribSetting;

static int add_egl_attrib(EglAttribSetting* buffer, int size, int offset, const EglAttribSetting* setting)
{
    int result;
    if (0 <= offset && size > offset)
    {
        buffer[offset] = *setting;
        result = offset + 1;
    }
    else
    {
        LOGV("Exhausted egl attrib buffer");
        result = -1;
    }
    return result;
}

static int add_egl_base_attrib(EglAttribSetting* buffer, int size, int offset)
{
    const EglAttribSetting surface = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT};
    return add_egl_attrib(buffer, size, offset, &surface);
}

static int add_egl_colour_attrib(EglAttribSetting* buffer, int size, int offset)
{
    const EglAttribSetting colour[] = {
        {EGL_BLUE_SIZE, 8},
        {EGL_GREEN_SIZE, 8},
        {EGL_RED_SIZE, 8}
    };
    int i;
    const int num_entries = sizeof(colour) / sizeof(EglAttribSetting);
    for (i=0; i<num_entries; ++i)
    {
        offset = add_egl_attrib(buffer, size, offset, &colour[i]);
        if (0 > offset)
            break;
    }
    return offset;
}

static int add_egl_alpha_attrib(EglAttribSetting* buffer, int size, int offset)
{
    const EglAttribSetting alpha = {EGL_ALPHA_SIZE, 8};
    return add_egl_attrib(buffer, size, offset, &alpha);
}

static int add_egl_depth_attrib(EglAttribSetting* buffer, int size, int offset)
{
    const EglAttribSetting depth = {EGL_DEPTH_SIZE, 16};
    return add_egl_attrib(buffer, size, offset, &depth);
}

static int add_egl_stencil_attrib(EglAttribSetting* buffer, int size, int offset)
{
    // TODO: Tegra support.
    const EglAttribSetting stencil = {EGL_STENCIL_SIZE, 8};
    return add_egl_attrib(buffer, size, offset, &stencil);
}

static int add_egl_concluding_attrib(EglAttribSetting* buffer, int size, int offset)
{
    const EglAttribSetting conclusion[] = {
        // NOTE: In order to run on emulator
        // EGL_CONFORMANT must not be specified
        {EGL_CONFORMANT, EGL_OPENGL_ES2_BIT},
        {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT},
        {EGL_NONE, 0}
    };
    int i = 0;
    const int num_entries = sizeof(conclusion) / sizeof(EglAttribSetting);
    for (i=0; i<num_entries; ++i) {
        offset = add_egl_attrib(buffer, size, offset, &conclusion[i]);
        if (0 > offset)
            break;
    }
    return offset;
}

static EGLint choose_egl_config(EGLDisplay display, EGLConfig* config)
{
    EGLint result = 0;
    const int max_settings = 10;
    EglAttribSetting settings[max_settings];

    bool is_alpha_enabled = is_alpha_transparency_enabled();
    int actual_settings = is_alpha_enabled ? max_settings : max_settings - 1;

    int offset = 0;
    int stencil_offset;

    offset = add_egl_base_attrib((EglAttribSetting*)&settings, actual_settings, offset);
    offset = add_egl_colour_attrib((EglAttribSetting*)&settings, actual_settings, offset);
    if (is_alpha_enabled) {
        offset = add_egl_alpha_attrib((EglAttribSetting*)&settings, actual_settings, offset);
    }
    offset = add_egl_depth_attrib((EglAttribSetting*)&settings, actual_settings, offset);
    stencil_offset = offset;
    offset = add_egl_stencil_attrib((EglAttribSetting*)&settings, actual_settings, offset);
    offset = add_egl_concluding_attrib((EglAttribSetting*)&settings, actual_settings, offset);

    eglChooseConfig(display, (const EGLint *)&settings[0], config, 1, &result);
    CHECK_EGL_ERROR
    if (0 == result)
    {
        // Something along this sort of line when adding Tegra support?
        LOGV("egl config choice failed - removing stencil");
        add_egl_concluding_attrib((EglAttribSetting*)&settings, actual_settings, stencil_offset);
        eglChooseConfig(display, (const EGLint *)&settings[0], config, 1, &result);
        CHECK_EGL_ERROR
    }

    return result;
}

static ANativeWindow* AcquireAppWindow(_GLFWwin_android* win)
{
    if (win == 0 || win->app == 0)
        return 0;

    int did_attach = 0;
    JNIAttachCurrentThreadIfNeeded(&did_attach);
    pthread_mutex_lock(&win->app->mutex);
    ANativeWindow* window = win->app->window;
    if (win->app->pendingWindow != window)
        window = NULL;
    if (window)
        ANativeWindow_acquire(window);
    pthread_mutex_unlock(&win->app->mutex);
    JNIDetachCurrentThreadIfNeeded(did_attach);

    return window;
}

static int IsAppWindowCurrent(_GLFWwin_android* win, ANativeWindow* window)
{
    if (win == 0 || win->app == 0 || window == 0 || !_glfwAndroidIsAppResumed())
        return 0;

    pthread_mutex_lock(&win->app->mutex);
    int is_current = win->app->window == window && win->app->pendingWindow == window;
    pthread_mutex_unlock(&win->app->mutex);
    return is_current;
}

static ANativeWindow* WaitForAppAndWindow(_GLFWwin_android* win)
{
    const useconds_t wait_period = 50*1000;
    int logged_wait = 0;
    while (win != 0 && win->app != 0 && !win->app->destroyRequested)
    {
        if (_glfwAndroidIsAppResumed())
        {
            ANativeWindow* window = AcquireAppWindow(win);
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

void wait_for_egl_retry(uint32_t retry_count)
{
    uint32_t delay_us = EGL_RETRY_INITIAL_DELAY_US;
    while (retry_count > 0 && delay_us < EGL_RETRY_MAX_DELAY_US)
    {
        delay_us *= 2;
        --retry_count;
    }
    if (delay_us > EGL_RETRY_MAX_DELAY_US)
        delay_us = EGL_RETRY_MAX_DELAY_US;

    LOGI("Waiting %u ms before retrying EGL initialization.", delay_us / 1000);
    usleep(delay_us);
}

static GlfwAndroidEglResult GetEglFailureResult(const char* operation, EGLint error)
{
    LOGW("%s failed, eglGetError: %X", operation, error);

    switch (error)
    {
        case EGL_BAD_ALLOC:
        case EGL_BAD_CURRENT_SURFACE:
        case EGL_BAD_NATIVE_WINDOW:
        case EGL_BAD_SURFACE:
            return GLFW_ANDROID_EGL_RESULT_DEFERRED;
        case EGL_NOT_INITIALIZED:
        case EGL_BAD_DISPLAY:
        case EGL_BAD_CONFIG:
        case EGL_BAD_CONTEXT:
        case EGL_CONTEXT_LOST:
        default:
            return GLFW_ANDROID_EGL_RESULT_FATAL;
    }
}

static void ReleaseWindow(ANativeWindow* window)
{
    if (window)
    {
        int did_attach = 0;
        JNIAttachCurrentThreadIfNeeded(&did_attach);
        ANativeWindow_release(window);
        JNIDetachCurrentThreadIfNeeded(did_attach);
    }
}

static void ReleaseNativeWindow(_GLFWwin_android* win)
{
    if (win->native_window)
    {
        ReleaseWindow(win->native_window);
        win->native_window = NULL;
    }
}

static GlfwAndroidEglResult CreateGLSurfaceForWindow(_GLFWwin_android* win, ANativeWindow* window)
{
    if (window == NULL)
        return GLFW_ANDROID_EGL_RESULT_DEFERRED;

    if (win->display == EGL_NO_DISPLAY)
    {
        ReleaseWindow(window);
        return GLFW_ANDROID_EGL_RESULT_FATAL;
    }

    if (win->surface != EGL_NO_SURFACE)
    {
        GlfwAndroidEglResult result = IsAppWindowCurrent(win, win->native_window)
            ? GLFW_ANDROID_EGL_RESULT_READY
            : GLFW_ANDROID_EGL_RESULT_DEFERRED;
        ReleaseWindow(window);
        return result;
    }

    if (!IsAppWindowCurrent(win, window))
    {
        ReleaseWindow(window);
        return GLFW_ANDROID_EGL_RESULT_DEFERRED;
    }

    EGLint format;
    if (eglGetConfigAttrib(win->display, win->config, EGL_NATIVE_VISUAL_ID, &format) != EGL_TRUE)
    {
        EGLint error = eglGetError();
        ReleaseWindow(window);
        return GetEglFailureResult("eglGetConfigAttrib", error);
    }

    if (ANativeWindow_setBuffersGeometry(window, 0, 0, format) < 0)
    {
        LOGW("ANativeWindow_setBuffersGeometry failed, deferring surface creation.");
        ReleaseWindow(window);
        return GLFW_ANDROID_EGL_RESULT_DEFERRED;
    }

    eglGetError();
    EGLSurface surface = eglCreateWindowSurface(win->display, win->config, window, NULL);
    EGLint error = eglGetError();
    if (surface == EGL_NO_SURFACE || error != EGL_SUCCESS)
    {
        GlfwAndroidEglResult result = GetEglFailureResult("eglCreateWindowSurface", error);
        if (surface != EGL_NO_SURFACE)
            eglDestroySurface(win->display, surface);
        ReleaseWindow(window);
        return result;
    }

    if (!IsAppWindowCurrent(win, window))
    {
        eglDestroySurface(win->display, surface);
        ReleaseWindow(window);
        return GLFW_ANDROID_EGL_RESULT_DEFERRED;
    }

    ReleaseNativeWindow(win);
    win->native_window = window;
    win->surface = surface;
    return GLFW_ANDROID_EGL_RESULT_READY;
}

int init_gl(_GLFWwin_android* win)
{
    LOGV("init_gl");

    ANativeWindow* window = WaitForAppAndWindow(win);
    if (!window)
    {
        LOGE("ENGINE THREAD: Window not ready. Returning from init_gl()");
        return 0;
    }

    /*
     * NOTE: The example simple_gles2 doesn't work with EGL_CONTEXT_CLIENT_VERSION
     * set to 2 in emulator. Might work on real device though
     */
    EGLint numConfigs;
    EGLConfig config;
    EGLContext context = EGL_NO_CONTEXT;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || eglInitialize(display, 0, 0) != EGL_TRUE)
    {
        EGLint error = eglGetError();
        LOGE("Failed to initialize EGL, eglGetError: %X", error);
        ReleaseWindow(window);
        return 0;
    }


    numConfigs = choose_egl_config(display, &config);
    // No configs found, error out
    if (numConfigs == 0)
    {
        eglTerminate(display);
        ReleaseWindow(window);
        return 0;
    }

    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_NONE,
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT)
    {
        eglGetError();
        contextAttribs[1] = 2;
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    }
    if (context == EGL_NO_CONTEXT)
    {
        EGLint error = eglGetError();
        LOGE("Failed to create EGL context, eglGetError: %X", error);
        eglTerminate(display);
        ReleaseWindow(window);
        return 0;
    }

    win->display = display;
    win->context = context;
    win->config = config;


    {
        // create aux context if possible
        LOGV("create_gl_aux_context..");
        win->aux_context = EGL_NO_CONTEXT;
        win->aux_surface = EGL_NO_SURFACE;
        EGLContext aux_context = eglCreateContext(display, config, context, contextAttribs);
        if(aux_context != EGL_NO_CONTEXT)
        {
            EGLint attribpbf[] =
            {
                    EGL_HEIGHT, 1,
                    EGL_WIDTH, 1,
                    EGL_NONE
            };
            EGLSurface aux_surface = eglCreatePbufferSurface(display, config, attribpbf);
            if(aux_surface == EGL_NO_SURFACE)
            {
                eglGetError();
                eglDestroyContext(display, aux_context);
                LOGV("create_gl_aux_context unsupported");
            }
            else
            {
                win->aux_context = aux_context;
                win->aux_surface = aux_surface;
                LOGV("create_gl_aux_context success");
            }
        }
        else
        {
            eglGetError();
        }
    }

    GlfwAndroidEglResult surface_result;
    uint32_t retry_count = 0;
    do
    {
        surface_result = CreateGLSurfaceForWindow(win, window);
        if (surface_result == GLFW_ANDROID_EGL_RESULT_DEFERRED)
        {
            wait_for_egl_retry(retry_count++);
            window = WaitForAppAndWindow(win);
        }
    }
    while (surface_result == GLFW_ANDROID_EGL_RESULT_DEFERRED && window != NULL);

    if (surface_result != GLFW_ANDROID_EGL_RESULT_READY)
    {
        final_gl(win);
        return 0;
    }

    return 1;
}

void final_gl(_GLFWwin_android* win)
{
    LOGV("final_gl");
    int did_attach = 0;
    JNIAttachCurrentThreadIfNeeded(&did_attach);
    if (win->display != EGL_NO_DISPLAY)
    {
        if (win->aux_context != EGL_NO_CONTEXT)
        {
            if (eglDestroySurface(win->display, win->aux_surface) != EGL_TRUE)
                GetEglFailureResult("eglDestroySurface(aux)", eglGetError());
            if (eglDestroyContext(win->display, win->aux_context) != EGL_TRUE)
                GetEglFailureResult("eglDestroyContext(aux)", eglGetError());
            win->aux_surface = EGL_NO_SURFACE;
            win->aux_context = EGL_NO_CONTEXT;
        }

        if (win->context != EGL_NO_CONTEXT)
        {
            if (eglMakeCurrent(win->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) != EGL_TRUE)
                GetEglFailureResult("eglMakeCurrent(EGL_NO_SURFACE)", eglGetError());
            if (eglDestroyContext(win->display, win->context) != EGL_TRUE)
                GetEglFailureResult("eglDestroyContext", eglGetError());
            win->context = EGL_NO_CONTEXT;
        }
        if (eglTerminate(win->display) != EGL_TRUE)
            GetEglFailureResult("eglTerminate", eglGetError());
        win->display = EGL_NO_DISPLAY;
    }
    ReleaseNativeWindow(win);
    JNIDetachCurrentThreadIfNeeded(did_attach);
}

GlfwAndroidEglResult create_gl_surface(_GLFWwin_android* win)
{
    LOGV("create_gl_surface");
    int did_attach = 0;
    JNIAttachCurrentThreadIfNeeded(&did_attach);

    GlfwAndroidEglResult result;
    if (win->display == EGL_NO_DISPLAY)
    {
        result = GLFW_ANDROID_EGL_RESULT_FATAL;
    }
    else if (win->surface != EGL_NO_SURFACE)
    {
        result = GLFW_ANDROID_EGL_RESULT_READY;
    }
    else
    {
        ANativeWindow* window = AcquireAppWindow(win);
        if (window == NULL)
            LOGV("Window not ready, deferring surface creation.");
        result = CreateGLSurfaceForWindow(win, window);
    }

    JNIDetachCurrentThreadIfNeeded(did_attach);
    return result;
}

GlfwAndroidEglResult make_current(_GLFWwin_android* win)
{
    if (win->display == EGL_NO_DISPLAY || win->context == EGL_NO_CONTEXT)
        return GLFW_ANDROID_EGL_RESULT_FATAL;
    if (win->surface == EGL_NO_SURFACE)
        return GLFW_ANDROID_EGL_RESULT_DEFERRED;
    if (!IsAppWindowCurrent(win, win->native_window))
        return GLFW_ANDROID_EGL_RESULT_DEFERRED;

    EGLBoolean res = eglMakeCurrent(win->display, win->surface, win->surface, win->context);
    if (res != EGL_TRUE)
    {
        EGLint error = eglGetError();
        return GetEglFailureResult("eglMakeCurrent", error);
    }

    return IsAppWindowCurrent(win, win->native_window)
        ? GLFW_ANDROID_EGL_RESULT_READY
        : GLFW_ANDROID_EGL_RESULT_DEFERRED;
}

GlfwAndroidEglResult update_width_height_info(_GLFWwin* win, _GLFWwin_android* win_android, int force)
{
    if (win_android->display == EGL_NO_DISPLAY)
        return GLFW_ANDROID_EGL_RESULT_FATAL;
    if (win_android->surface == EGL_NO_SURFACE)
        return GLFW_ANDROID_EGL_RESULT_DEFERRED;
    if (!IsAppWindowCurrent(win_android, win_android->native_window))
        return GLFW_ANDROID_EGL_RESULT_DEFERRED;

    EGLint w, h;
    if (eglQuerySurface(win_android->display, win_android->surface, EGL_WIDTH, &w) != EGL_TRUE)
    {
        EGLint error = eglGetError();
        return GetEglFailureResult("eglQuerySurface(EGL_WIDTH)", error);
    }
    if (eglQuerySurface(win_android->display, win_android->surface, EGL_HEIGHT, &h) != EGL_TRUE)
    {
        EGLint error = eglGetError();
        return GetEglFailureResult("eglQuerySurface(EGL_HEIGHT)", error);
    }

    if (force || (win->width != w || win->height != h))
    {
        LOGV("window size changed from %dx%d to %dx%d", _glfwWin.width, _glfwWin.height, w, h);
        if (win->windowSizeCallback)
        {
            win->windowSizeCallback(w, h);
        }
        win->width = w;
        win->height = h;
    }

    return IsAppWindowCurrent(win_android, win_android->native_window)
        ? GLFW_ANDROID_EGL_RESULT_READY
        : GLFW_ANDROID_EGL_RESULT_DEFERRED;
}

void destroy_gl_surface(_GLFWwin_android* win)
{
    LOGV("destroy_gl_surface");
    int did_attach = 0;
    JNIAttachCurrentThreadIfNeeded(&did_attach);
    if (win->display != EGL_NO_DISPLAY)
    {
        if (eglMakeCurrent(win->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) != EGL_TRUE)
        {
            EGLint error = eglGetError();
            GetEglFailureResult("eglMakeCurrent(EGL_NO_SURFACE)", error);
        }
        if (win->surface != EGL_NO_SURFACE)
        {
            if (eglDestroySurface(win->display, win->surface) != EGL_TRUE)
            {
                EGLint error = eglGetError();
                GetEglFailureResult("eglDestroySurface", error);
            }
            win->surface = EGL_NO_SURFACE;
        }
    }
    win->surface = EGL_NO_SURFACE;
    ReleaseNativeWindow(win);
    JNIDetachCurrentThreadIfNeeded(did_attach);
}

int query_gl_aux_context(_GLFWwin_android* win)
{
    return (win->aux_context == EGL_NO_CONTEXT) ? 0 : 1;
}

void* acquire_gl_aux_context(_GLFWwin_android* win)
{
    if (win->aux_context == EGL_NO_CONTEXT)
    {
        LOGV("Unable to make OpenGL aux context current, is NULL");
        return 0;
    }
    EGLBoolean res = eglMakeCurrent(win->display, win->aux_surface, win->aux_surface, win->aux_context);
    if( res != EGL_TRUE )
    {
        LOGV("Unable to make OpenGL aux context current, eglMakeCurrent failed");
        return 0;
    }
    return win->aux_context;
}


void unacquire_gl_aux_context(_GLFWwin_android* win)
{
    if (win->aux_context == EGL_NO_CONTEXT)
        return;
    eglMakeCurrent(win->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}


int32_t _glfwAndroidVerifySurfaceError(EGLint error)
{
    // Error checking inspired by Android implementation of GLSurfaceView:
    // https://android.googlesource.com/platform/frameworks/base/+/master/opengl/java/android/opengl/GLSurfaceView.java
    if (error != EGL_SUCCESS) {

        if (error == EGL_CONTEXT_LOST) {
            LOGE("egl* function failed due to EGL_CONTEXT_LOST!");
            return 0;
        } else if (error == EGL_BAD_SURFACE) {
            LOGE("egl* function failed due to EGL_BAD_SURFACE, destroy surface and wait for recreation.");
            return 0;
        } else {
            // Other errors typically mean that the current surface is bad,
            // probably because the SurfaceView surface has been destroyed,
            // but we haven't been notified yet.
            // Ignore error, but log for debugging purpose.
            LOGW("egl* function failed, eglGetError: %X", error);
            return 0;
        }
    }

    // Surface is ok
    return 1;
}
