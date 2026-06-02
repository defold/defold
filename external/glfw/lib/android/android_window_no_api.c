//========================================================================
// GLFW - An OpenGL framework
// Platform:    Android no-API window backend
// API version: 2.7
//========================================================================

#include "android_window_backend.h"

static int g_PendingResizeBecauseOfInsets = 0;

static void TraceNoApiWindowSize(const char* reason, int update_window_size)
{
    ANativeWindow* window = _glfwWinAndroid.app ? _glfwWinAndroid.app->window : 0;
    if (window)
    {
        int w = ANativeWindow_getWidth(window);
        int h = ANativeWindow_getHeight(window);
        int changed = _glfwWin.width != w || _glfwWin.height != h;
        if (_glfwWinAndroid.app)
        {
            LOGV("SIZE_TRACE no_api %s window=%p anative=%dx%d glfw_before=%dx%d contentRect=%d,%d,%d,%d pendingInsets=%d changed=%d update=%d",
                reason, window, w, h, _glfwWin.width, _glfwWin.height,
                _glfwWinAndroid.app->contentRect.left,
                _glfwWinAndroid.app->contentRect.top,
                _glfwWinAndroid.app->contentRect.right,
                _glfwWinAndroid.app->contentRect.bottom,
                g_PendingResizeBecauseOfInsets, changed, update_window_size);
        }
        else
        {
            LOGV("SIZE_TRACE no_api %s window=%p anative=%dx%d glfw_before=%dx%d pendingInsets=%d changed=%d update=%d",
                reason, window, w, h, _glfwWin.width, _glfwWin.height,
                g_PendingResizeBecauseOfInsets, changed, update_window_size);
        }

        if (update_window_size && changed && _glfwWin.windowSizeCallback)
        {
            _glfwWin.windowSizeCallback(w, h);
        }
        if (update_window_size)
        {
            _glfwWin.width = w;
            _glfwWin.height = h;
        }
    }
    else
    {
        LOGV("SIZE_TRACE no_api %s window=NULL glfw_before=%dx%d pendingInsets=%d update=%d",
            reason, _glfwWin.width, _glfwWin.height, g_PendingResizeBecauseOfInsets, update_window_size);
    }
}

int _glfwAndroidPlatformGetWindowRefreshRate(void)
{
    return 0;
}

int _glfwAndroidPlatformOpenWindow(int width, int height, const _GLFWwndconfig* wndconfig, const _GLFWfbconfig* fbconfig)
{
    (void)width;
    (void)height;
    (void)fbconfig;

    _glfwWin.clientAPI = wndconfig->clientAPI;
    return _glfwWin.clientAPI == GLFW_NO_API ? GL_TRUE : GL_FALSE;
}

void _glfwAndroidPlatformCloseWindow(void)
{
}

void _glfwAndroidPlatformSwapBuffers(void)
{
}

void _glfwAndroidPlatformSwapInterval(int interval)
{
    (void)interval;
}

int32_t _glfwAndroidPlatformVerifySurface(void)
{
    return 1;
}

void _glfwAndroidPlatformSetPendingResizeBecauseOfInsets(void)
{
    LOGV("SIZE_TRACE no_api set_pending_resize_because_of_insets");
    g_PendingResizeBecauseOfInsets = 1;
}

void _glfwAndroidPlatformOnTermWindow(void)
{
}

void _glfwAndroidPlatformOnInitWindow(void)
{
    TraceNoApiWindowSize("on_init_window", 1);
}

void _glfwAndroidPlatformOnGainedFocus(void)
{
    TraceNoApiWindowSize("on_gained_focus", 0);
}

void _glfwAndroidPlatformOnResize(void)
{
    TraceNoApiWindowSize("on_resize", 1);
    g_PendingResizeBecauseOfInsets = 0;
}

void _glfwAndroidPlatformAfterFlushEvents(void)
{
    if (g_PendingResizeBecauseOfInsets)
    {
        TraceNoApiWindowSize("after_flush_events_pending_insets", 1);
        g_PendingResizeBecauseOfInsets = 0;
    }
}

void _glfwAndroidPlatformDestroyWindow(void)
{
}

int _glfwAndroidPlatformQueryAuxContext(void)
{
    return 0;
}

void* _glfwAndroidPlatformAcquireAuxContext(void)
{
    return 0;
}

void _glfwAndroidPlatformUnacquireAuxContext(void* context)
{
    (void)context;
}
