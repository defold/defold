#ifndef _android_window_backend_h_
#define _android_window_backend_h_

#include "internal.h"

int   _glfwAndroidPlatformGetWindowRefreshRate(void);
int   _glfwAndroidPlatformOpenWindow(int width, int height, const _GLFWwndconfig* wndconfig, const _GLFWfbconfig* fbconfig);
void  _glfwAndroidPlatformCloseWindow(void);
void  _glfwAndroidPlatformSwapBuffers(void);
void  _glfwAndroidPlatformSwapInterval(int interval);
int32_t _glfwAndroidPlatformVerifySurface(void);
void  _glfwAndroidPlatformSetPendingResizeBecauseOfInsets(void);
void  _glfwAndroidPlatformOnTermWindow(void);
void  _glfwAndroidPlatformOnInitWindow(void);
void  _glfwAndroidPlatformOnGainedFocus(void);
void  _glfwAndroidPlatformOnResize(void);
void  _glfwAndroidPlatformAfterFlushEvents(void);
void  _glfwAndroidPlatformDestroyWindow(void);
int   _glfwAndroidPlatformQueryAuxContext(void);
void* _glfwAndroidPlatformAcquireAuxContext(void);
void  _glfwAndroidPlatformUnacquireAuxContext(void* context);

#endif
