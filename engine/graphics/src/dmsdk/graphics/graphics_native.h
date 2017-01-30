#ifndef DMSDK_GRAPHICS_NATIVE_H
#define DMSDK_GRAPHICS_NATIVE_H

// Setup easier defines for each platform
#if defined(__APPLE_CC__)
    #if defined(__arm__) || defined(__arm64__)
        #define DM_PLATFORM_EXPOSE_NATIVE_IOS
    #else
        #define DM_PLATFORM_EXPOSE_NATIVE_OSX
    #endif
#elif defined(ANDROID)
    #define DM_PLATFORM_EXPOSE_NATIVE_ANDROID
#elif defined(_WIN32)
    #define DM_PLATFORM_EXPOSE_NATIVE_WINDOWS
#elif defined(__EMSCRIPTEN__)
    #define DM_PLATFORM_EXPOSE_NATIVE_HTML5
#else
    #define DM_PLATFORM_EXPOSE_NATIVE_LINUX
#endif

// Include needed header for types, and typedef unknowns to void*
#if defined(DM_PLATFORM_EXPOSE_NATIVE_IOS)
    #include <objc/objc.h>
#elif defined(DM_PLATFORM_EXPOSE_NATIVE_OSX)
    #include <objc/objc.h>
#else
    typedef void* id;
#endif

#if defined(DM_PLATFORM_EXPOSE_NATIVE_ANDROID)
    #include <EGL/egl.h>
    #include <GLES/gl.h>
    #include <android/native_window.h>
    #include <android_native_app_glue.h>
#else
    typedef void* EGLContext;
    typedef void* EGLSurface;
    typedef void* JavaVM;
    typedef void* jobject;
#endif

#if defined(DM_PLATFORM_EXPOSE_NATIVE_WINDOWS)
    #include <windows.h>
#else
    typedef void* HWND;
    typedef void* HGLRC;
#endif

#if defined(DM_PLATFORM_EXPOSE_NATIVE_LINUX)
    #include <GL/glx.h>
#else
    typedef void* Window;
    typedef void* GLXContext;
#endif

#if defined(DM_PLATFORM_EXPOSE_NATIVE_HTML5)
    // HTML5 / Emscripten
#else
#endif

namespace dmGraphics
{
    // iOS
    id GetNativeiOSUVWindow(void);
    id GetNativeiOSUIView(void);
    id GetNativeiOSEAGLContext(void);

    // OSX
    id GetNativeOSXNSWindow(void);
    id GetNativeOSXNSView(void);
    id GetNativeOSXNSOpenGLContext(void);

    // Win32
    HWND GetNativeWindowsHWND(void);
    HGLRC GetNativeWindowsHGLRC(void);

    // Android
    EGLContext GetNativeAndroidEGLContext(void);
    EGLSurface GetNativeAndroidEGLSurface(void);
    JavaVM* GetNativeAndroidJavaVM(void);
    jobject GetNativeAndroidActivity(void);

    // Linux (X11)
    Window GetNativeX11Window(void);
    GLXContext GetNativeX11GLXContext(void);
}

#endif // DMSDK_GRAPHICS_NATIVE_H
