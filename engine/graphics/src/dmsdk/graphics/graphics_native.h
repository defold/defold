#ifndef DMSDK_GRAPHICS_NATIVE_H
#define DMSDK_GRAPHICS_NATIVE_H

// Include platform specific headers, typedef unknowns to void*
#if defined(__APPLE_CC__)
    #include <objc/objc.h>
#else
    typedef void* id;
#endif

#if defined(ANDROID)
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

#if defined(_WIN32)
    #include <windows.h>
#else
    typedef void* HWND;
    typedef void* HGLRC;
#endif

#if defined(__linux__) && !defined(ANDROID)
    #include <GL/glx.h>
#else
    typedef void* Window;
    typedef void* GLXContext;
#endif

#if defined(__EMSCRIPTEN__)
    // HTML5 (Emscripten) not supported
#endif


namespace dmGraphics
{
    /**
     * Get iOS UVWindow native handle (id)
     * @return UVWindow or zero on other platforms than iOS
     */
    id GetNativeiOSUVWindow(void);
    /**
     * Get iOS UIView native handle (id)
     * @return UIView or zero on other platforms than iOS
     */
    id GetNativeiOSUIView(void);
    /**
     * Get iOS EAGLContext native handle (id)
     * @return EAGLContext ior zero on other platforms than iOS
     */
    id GetNativeiOSEAGLContext(void);

    /**
     * Get OSX NSWindow native handle (id)
     * @return NSWindow or zero on other platforms than OSX
     */
    id GetNativeOSXNSWindow(void);
    /**
     * Get OSX NSView native handle (id)
     * @return NSView or zero on other platforms than OSX
     */
    id GetNativeOSXNSView(void);
    /**
     * Get OSX NSOpenGLContext native handle (id)
     * @return NSOpenGLContext or zero on other platforms than OSX
     */
    id GetNativeOSXNSOpenGLContext(void);

    /**
     * Get Win32 native windows handle (HWND)
     * @return HWND or zero on other platforms than Win32
     */
    HWND GetNativeWindowsHWND(void);
    /**
     * Get Win32 native windows gl render context handle (HGLRC)
     * @return HGLRC or zero on other platforms than Win32
     */
    HGLRC GetNativeWindowsHGLRC(void);

    /**
     * Get Android native EGLContext
     * @return EGLContext or zero on other platforms than Android
     */
    EGLContext GetNativeAndroidEGLContext(void);
    /**
     * Get Android native EGLSurface
     * @return EGLSurface or zero on other platforms than Android
     */
    EGLSurface GetNativeAndroidEGLSurface(void);
    /**
     * Get Android native JavaVM
     * @return JavaVM ptr or zero on other platforms than Android
     */
    JavaVM* GetNativeAndroidJavaVM(void);
    /**
     * Get Android native jobject
     * @return jobject or zero on other platforms than Android
     */
    jobject GetNativeAndroidActivity(void);

    /**
     * Get Linux X11Window native handle (Window)
     * @return Window handle or zero on other platforms than Linux
     */
    Window GetNativeX11Window(void);
    /**
     * Get Linux X11GLXContext native handle (GLXContext)
     * @return GLXContext or zero on other platforms than Linux
     */
    GLXContext GetNativeX11GLXContext(void);
}

#endif // DMSDK_GRAPHICS_NATIVE_H
