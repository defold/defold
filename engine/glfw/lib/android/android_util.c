#include "android_util.h"

#include "android_log.h"

struct {
    EGLDisplay display;
    EGLContext context;
    EGLContext aux_context;
    EGLConfig config;
    EGLSurface surface;
    EGLSurface aux_surface;
    struct android_app* app;
    // pipe used to go from java thread to native (JNI)
    int m_Pipefd[2];
} g_SavedWin;

typedef struct EglAttribSetting_t {
    EGLint m_Attribute;
    EGLint m_Value;
} EglAttribSetting;

void SaveWin(_GLFWwin* win) {
    g_SavedWin.display = win->display;
    g_SavedWin.context = win->context;
    g_SavedWin.aux_context = win->aux_context;
    g_SavedWin.config = win->config;
    g_SavedWin.aux_surface = win->aux_surface;
    g_SavedWin.app = win->app;
    g_SavedWin.m_Pipefd[0] = win->m_Pipefd[0];
    g_SavedWin.m_Pipefd[1] = win->m_Pipefd[1];
}

void RestoreWin(_GLFWwin* win) {
    win->display = g_SavedWin.display;
    win->context = g_SavedWin.context;
    win->aux_context = g_SavedWin.aux_context;
    win->config = g_SavedWin.config;
    win->surface = g_SavedWin.surface;
    win->aux_surface = g_SavedWin.aux_surface;
    win->app = g_SavedWin.app;
    win->m_Pipefd[0] = g_SavedWin.m_Pipefd[0];
    win->m_Pipefd[1] = g_SavedWin.m_Pipefd[1];
}

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
    const int max_settings = 9;
    EglAttribSetting settings[max_settings];

    int offset = 0;
    int stencil_offset;

    offset = add_egl_base_attrib(&settings, max_settings, offset);
    offset = add_egl_colour_attrib(&settings, max_settings, offset);
    offset = add_egl_depth_attrib(&settings, max_settings, offset);
    stencil_offset = offset;
    offset = add_egl_stencil_attrib(&settings, max_settings, offset);
    offset = add_egl_concluding_attrib(&settings, max_settings, offset);

    eglChooseConfig(display, (const EGLint *)&settings[0], config, 1, &result);
    CHECK_EGL_ERROR
    if (0 == result)
    {
        // Something along this sort of line when adding Tegra support?
        LOGV("egl config choice failed - removing stencil");
        add_egl_concluding_attrib(&settings, max_settings, stencil_offset);
        eglChooseConfig(display, (const EGLint *)&settings[0], config, 1, &result);
        CHECK_EGL_ERROR
    }

    return result;
}

int init_gl(_GLFWwin* win)
{
    LOGV("init_gl");

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


    numConfigs = choose_egl_config(display, &config);
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
    }


    return 1;
}

void final_gl(_GLFWwin* win)
{
    LOGV("final_gl");
    if (win->display != EGL_NO_DISPLAY)
    {
        if (win->aux_context != EGL_NO_CONTEXT)
        {
            eglDestroySurface(win->display, win->aux_surface);
            eglDestroyContext(win->display, win->aux_context);
        }

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
        win->hasSurface = 1;

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

int query_gl_aux_context(_GLFWwin* win)
{
    return (win->aux_context == EGL_NO_CONTEXT) ? 0 : 1;
}

void* acquire_gl_aux_context(_GLFWwin* win)
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


void unacquire_gl_aux_context(_GLFWwin* win)
{
    if (win->aux_context == EGL_NO_CONTEXT)
        return;
    eglMakeCurrent(win->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}


