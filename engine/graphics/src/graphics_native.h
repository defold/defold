#ifndef DMGRAPHICS_GRAPHICS_NATIVE_H
#define DMGRAPHICS_GRAPHICS_NATIVE_H

/****************************************************
 * Expose native handle struct, platform dependent. *
 ****************************************************/

#if defined(__APPLE_CC__)
    #if defined(__arm__) || defined(__arm64__)
        // iOS
        #define DM_PLATFORM_EXPOSE_NATIVE_IOS
    #else
        // macOS
        #define DM_PLATFORM_EXPOSE_NATIVE_OSX
    #endif
#elif defined(ANDROID)
    // Android
    #define DM_PLATFORM_EXPOSE_NATIVE_ANDROID
#elif defined(_WIN32)
    // Windows
    #define DM_PLATFORM_EXPOSE_NATIVE_WINDOWS
#else
    // Linux
    #define DM_PLATFORM_EXPOSE_NATIVE_LINUX
#endif

// Fallback alternative, expose empty struct
#if defined(DM_PLATFORM_EXPOSE_NATIVE_EMPTY)
    namespace dmGraphics
    {
        struct NativeHandles
        {
            void* m_Dummy;
        };
    }

// iOS, exposes the UIWindow, UIView and EAGLContext
#elif defined(DM_PLATFORM_EXPOSE_NATIVE_IOS)
    #if defined(__OBJC__)
        #import <UIKit/UIKit.h>
    #else
        typedef void* id;
    #endif
    namespace dmGraphics
    {
        struct NativeHandles
        {
            id m_UIWindow;
            id m_UIView;
            id m_EAGLContext;
        };
    }

// OSX, exposes the NSWindow, NSView and NSOpenGLContext
#elif defined(DM_PLATFORM_EXPOSE_NATIVE_OSX)
    #if defined(__OBJC__)
      #include <ApplicationServices/ApplicationServices.h>
      #import <Cocoa/Cocoa.h>
    #else
      typedef void* id;
    #endif
    namespace dmGraphics
    {
        struct NativeHandles
        {
            id m_NSWindow;
            id m_NSView;
            id m_NSOpenGLContext;
        };
    }

// Android, exposes the EGLContext, EGLSurface, JNIEnv* and Activity (jobject)
#elif defined(DM_PLATFORM_EXPOSE_NATIVE_ANDROID)
    #include <EGL/egl.h>
    #include <GLES/gl.h>
    #include <android/native_window.h>
    #include <android_native_app_glue.h>
    namespace dmGraphics
    {
        struct NativeHandles
        {
            EGLContext m_EGLContext;
            EGLSurface m_EGLSurface;
            JNIEnv*    m_JNIEnv;
            jobject    m_Activity;
        };
    }

// Windows
#elif defined(DM_PLATFORM_EXPOSE_NATIVE_WINDOWS)
    #include <windows.h>
    namespace dmGraphics
    {
        struct NativeHandles
        {
            HWND  m_HWND;
            HGLRC m_HGLRC;
        };
    }
#else
    // Linux
#endif

namespace dmGraphics
{
    NativeHandles GetNativeHandles();
}

#endif
