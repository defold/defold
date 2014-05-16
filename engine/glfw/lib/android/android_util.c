#include "android_util.h"

#include "android_log.h"

struct {
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface surface;
    struct android_app* app;
    // pipe used to go from java thread to native (JNI)
    int m_Pipefd[2];
} g_SavedWin;

void SaveWin(_GLFWwin* win) {
    g_SavedWin.display = win->display;
    g_SavedWin.context = win->context;
    g_SavedWin.config = win->config;
    g_SavedWin.surface = win->surface;
    g_SavedWin.app = win->app;
    g_SavedWin.m_Pipefd[0] = win->m_Pipefd[0];
    g_SavedWin.m_Pipefd[1] = win->m_Pipefd[1];
}

void RestoreWin(_GLFWwin* win) {
    win->display = g_SavedWin.display;
    win->context = g_SavedWin.context;
    win->config = g_SavedWin.config;
    win->surface = g_SavedWin.surface;
    win->app = g_SavedWin.app;
    win->m_Pipefd[0] = g_SavedWin.m_Pipefd[0];
    win->m_Pipefd[1] = g_SavedWin.m_Pipefd[1];
}

int init_gl(_GLFWwin* win)
{
    LOGV("init_gl");
    const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            // NOTE: In order to run on emulator
            // EGL_CONFORMANT must not be specified
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
    EGLint format;

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    CHECK_EGL_ERROR
    eglInitialize(display, 0, 0);
    CHECK_EGL_ERROR

    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    CHECK_EGL_ERROR
    // No configs found, error out
    if (numConfigs == 0)
    {
        return 0;
    }
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    CHECK_EGL_ERROR
    ANativeWindow_setBuffersGeometry(win->app->window, 0, 0, format);

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    CHECK_EGL_ERROR

    win->display = display;
    win->context = context;
    win->config = config;

    return 1;
}

void final_gl(_GLFWwin* win)
{
    LOGV("final_gl");
    if (win->display != EGL_NO_DISPLAY)
    {
        if (win->context != EGL_NO_CONTEXT)
        {
            eglMakeCurrent(win->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
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
    LOGV("create_gl_surface");
    if (win->display != EGL_NO_DISPLAY)
    {
        EGLint w, h;
        EGLConfig config = win->config;
        EGLContext context = win->context;
        EGLDisplay display = win->display;
        EGLSurface surface = win->surface;

        if (surface == EGL_NO_SURFACE)
        {
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
}

void destroy_gl_surface(_GLFWwin* win)
{
    LOGV("destroy_gl_surface");
    if (win->display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(win->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (win->surface != EGL_NO_SURFACE)
        {
            eglDestroySurface(win->display, win->surface);
            win->surface = EGL_NO_SURFACE;
            CHECK_EGL_ERROR
        }
    }
}
