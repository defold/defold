//========================================================================
// GLFW - An OpenGL framework
// Platform:    Android no-API window backend
// API version: 2.7
//========================================================================


// Modified for Defold: private mobile/web backend, without the GLFW API.
#include "android_window_backend.h"
#include "android_util.h"

static int g_PendingResizeBecauseOfInsets = 0;

static void UpdateNoApiWindowSize(void)
{
    ANativeWindow* window = dmNativeAcquireAndroidWindow();
    if (window)
    {
        int w = ANativeWindow_getWidth(window);
        int h = ANativeWindow_getHeight(window);
        dmNativeReleaseAndroidWindow(window);
        if ((dmNativeWin.width != w || dmNativeWin.height != h) && dmNativeWin.windowSizeCallback)
        {
            dmNativeWin.windowSizeCallback(w, h);
        }
        dmNativeWin.width = w;
        dmNativeWin.height = h;
    }
}

int dmNativeAndroidPlatformGetWindowRefreshRate(void)
{
    return 0;
}

int dmNativeAndroidPlatformOpenWindow(int width, int height, const Nativewndconfig* wndconfig, const Nativefbconfig* fbconfig)
{
    (void)width;
    (void)height;
    (void)fbconfig;

    dmNativeWin.clientAPI = wndconfig->clientAPI;
    return dmNativeWin.clientAPI == NATIVE_NO_API ? GL_TRUE : GL_FALSE;
}

void dmNativeAndroidPlatformCloseWindow(void)
{
}

void dmNativeAndroidPlatformSwapBuffers(void)
{
}

void dmNativeAndroidPlatformSwapInterval(int interval)
{
    (void)interval;
}

int32_t dmNativeAndroidPlatformVerifySurface(void)
{
    return 1;
}

void dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets(void)
{
    g_PendingResizeBecauseOfInsets = 1;
}

void dmNativeAndroidPlatformOnTermWindow(void)
{
}

void dmNativeAndroidPlatformOnInitWindow(void)
{
    UpdateNoApiWindowSize();
}

void dmNativeAndroidPlatformOnGainedFocus(void)
{
}

void dmNativeAndroidPlatformOnResize(void)
{
    UpdateNoApiWindowSize();
    g_PendingResizeBecauseOfInsets = 0;
}

void dmNativeAndroidPlatformAfterFlushEvents(void)
{
    if (g_PendingResizeBecauseOfInsets)
    {
        UpdateNoApiWindowSize();
        g_PendingResizeBecauseOfInsets = 0;
    }
}

void dmNativeAndroidPlatformDestroyWindow(void)
{
}

int dmNativeAndroidPlatformQueryAuxContext(void)
{
    return 0;
}

void* dmNativeAndroidPlatformAcquireAuxContext(void)
{
    return 0;
}

void dmNativeAndroidPlatformUnacquireAuxContext(void* context)
{
    (void)context;
}
