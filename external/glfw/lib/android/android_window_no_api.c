//========================================================================
// GLFW - An OpenGL framework
// Platform:    Android no-API window backend
// API version: 2.7
//========================================================================

#include "android_window_backend.h"
#include "android_util.h"

static int g_PendingResizeBecauseOfInsets = 0;

static void UpdateNoApiWindowSize(void)
{
    ANativeWindow* window = glfwAcquireAndroidWindow();
    if (window)
    {
        int w = ANativeWindow_getWidth(window);
        int h = ANativeWindow_getHeight(window);
        glfwReleaseAndroidWindow(window);
        if ((_glfwWin.width != w || _glfwWin.height != h) && _glfwWin.windowSizeCallback)
        {
            _glfwWin.windowSizeCallback(w, h);
        }
        _glfwWin.width = w;
        _glfwWin.height = h;
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
    g_PendingResizeBecauseOfInsets = 1;
}

void _glfwAndroidPlatformOnTermWindow(void)
{
}

void _glfwAndroidPlatformOnInitWindow(void)
{
    UpdateNoApiWindowSize();
}

void _glfwAndroidPlatformOnGainedFocus(void)
{
}

void _glfwAndroidPlatformOnResize(void)
{
    UpdateNoApiWindowSize();
    g_PendingResizeBecauseOfInsets = 0;
}

void _glfwAndroidPlatformAfterFlushEvents(void)
{
    if (g_PendingResizeBecauseOfInsets)
    {
        UpdateNoApiWindowSize();
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
