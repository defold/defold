#ifndef DMGRAPHICS_GRAPHICS_NATIVE_H
#define DMGRAPHICS_GRAPHICS_NATIVE_H

/****************************************************
 * Expose native handle struct, platform dependent. *
 ****************************************************/

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
#if defined(__OBJC__)
    #if defined(DM_PLATFORM_EXPOSE_NATIVE_IOS)
        #import <UIKit/UIKit.h>
    #elif defined(DM_PLATFORM_EXPOSE_NATIVE_OSX)
        #import <Cocoa/Cocoa.h>
    #endif
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
    typedef void* JNIEnv;
    typedef void* jobject;
#endif

#if defined(DM_PLATFORM_EXPOSE_NATIVE_WINDOWS)
    #include <windows.h>
#else
    typedef void* HWND;
    typedef void* HGLRC;
#endif

#if defined(DM_PLATFORM_EXPOSE_NATIVE_LINUX)
    // Linux
#else
#endif

#if defined(DM_PLATFORM_EXPOSE_NATIVE_HTML5)
    // HTML5 / Emscripten
#else
#endif

namespace dmGraphics
{
    struct NativeHandles
    {
        // iOS
        id m_UIWindow;
        id m_UIView;
        id m_EAGLContext;

        // OSX
        id m_NSWindow;
        id m_NSView;
        id m_NSOpenGLContext;

        // Android
        EGLContext m_EGLContext;
        EGLSurface m_EGLSurface;
        JNIEnv*    m_JNIEnv;
        jobject    m_Activity;

        // Windows
        HWND  m_HWND;
        HGLRC m_HGLRC;
    };

    NativeHandles GetNativeHandles();
}

#endif
