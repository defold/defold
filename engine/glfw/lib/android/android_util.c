#include "android_util.h"

#include "android_log.h"

void init_gl(EGLDisplay* out_display, EGLContext* out_context, EGLConfig* out_config)
{
    if (*out_display != EGL_NO_DISPLAY)
        return;
    LOGV("init_gl");
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
    };

    /*
     * NOTE: The example simple_gles2 doesn't work with EGL_CONTEXT_CLIENT_VERSION
     * set to 2 in emulator. Might work on real device though
     */
    const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2, // GLES 2.x support
            EGL_NONE, // terminator
    };

    EGLint numConfigs;
    EGLConfig config;
    EGLContext context;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    CHECK_EGL_ERROR
    eglInitialize(display, 0, 0);
    CHECK_EGL_ERROR
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    CHECK_EGL_ERROR
    context = eglCreateContext(display, config, NULL, contextAttribs);
    CHECK_EGL_ERROR

    (void)numConfigs;

    *out_display = display;
    *out_context = context;
    *out_config = config;
}

void final_gl(_GLFWwin* win)
{
    LOGV("final_gl");
    if (win->display != EGL_NO_DISPLAY)
    {
        if (win->context != EGL_NO_CONTEXT)
        {
            eglDestroyContext(win->display, win->context);
            CHECK_EGL_ERROR
            win->context = EGL_NO_CONTEXT;
        }
        eglTerminate(win->display);
        CHECK_EGL_ERROR
        win->display = EGL_NO_DISPLAY;
    }
}

void create_gl_surface(_GLFWwin* win)
{
    EGLint w, h, format;
    EGLConfig config = win->config;
    EGLContext context = win->context;
    EGLDisplay display = win->display;
    EGLSurface surface = EGL_NO_SURFACE;

    LOGV("create_gl_surface");
    if (win->surface == EGL_NO_SURFACE)
    {
        eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
        CHECK_EGL_ERROR
        ANativeWindow_setBuffersGeometry(win->app->window, 0, 0, format);
        surface = eglCreateWindowSurface(display, config, win->app->window, NULL);
        CHECK_EGL_ERROR
    }
    EGLBoolean res = eglMakeCurrent(display, surface, surface, context);
    assert(res == EGL_TRUE);
    CHECK_EGL_ERROR

    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    CHECK_EGL_ERROR
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    CHECK_EGL_ERROR

    if (win->windowSizeCallback)
    {
        win->windowSizeCallback(w, h);
    }

    win->surface = surface;

    win->width = w;
    win->height = h;
}

void destroy_gl_surface(_GLFWwin* win)
{
    LOGV("destroy_gl_surface");
    eglMakeCurrent(win->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (win->surface != EGL_NO_SURFACE)
    {
        eglDestroySurface(win->display, win->surface);
        win->surface = EGL_NO_SURFACE;
        CHECK_EGL_ERROR
    }
}
