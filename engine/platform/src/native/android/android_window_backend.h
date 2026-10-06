#ifndef _android_window_backend_h_
#define _android_window_backend_h_

#include "internal.h"

int   dmNativeAndroidPlatformGetWindowRefreshRate(void);
int   dmNativeAndroidPlatformOpenWindow(int width, int height, const Nativewndconfig* wndconfig, const Nativefbconfig* fbconfig);
void  dmNativeAndroidPlatformCloseWindow(void);
void  dmNativeAndroidPlatformSwapBuffers(void);
void  dmNativeAndroidPlatformSwapInterval(int interval);
int32_t dmNativeAndroidPlatformVerifySurface(void);
void  dmNativeAndroidPlatformSetPendingResizeBecauseOfInsets(void);
void  dmNativeAndroidPlatformOnTermWindow(void);
void  dmNativeAndroidPlatformOnInitWindow(void);
void  dmNativeAndroidPlatformOnGainedFocus(void);
void  dmNativeAndroidPlatformOnResize(void);
void  dmNativeAndroidPlatformAfterFlushEvents(void);
void  dmNativeAndroidPlatformDestroyWindow(void);
int   dmNativeAndroidPlatformQueryAuxContext(void);
void* dmNativeAndroidPlatformAcquireAuxContext(void);
void  dmNativeAndroidPlatformUnacquireAuxContext(void* context);

#endif
