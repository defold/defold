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
    typedef void* android_app;
#endif

#if defined(_WIN32)
    #include <Windows.h>
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
    /*# SDK Graphics API documentation
     * [file:<dmsdk/graphics/graphics_native.h>]
     *
     * Platform specific native graphics functions.
     *
     * @document
     * @name Graphics
     * @namespace dmGraphics
     */

    /*# get iOS UIWindow
     *
     * Get iOS UIWindow native handle (id). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeiOSUIWindow
     * @return id [type:id] native handle
     */
    id GetNativeiOSUIWindow(void);

    /*# get iOS UIView
     *
     * Get iOS UIView native handle (id). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeiOSUIView
     * @return id [type:id] native handle
     */
    id GetNativeiOSUIView(void);

    /*# get iOS EAGLContext
     *
     * Get iOS EAGLContext native handle (id). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeiOSEAGLContext
     * @return id [type:id] native handle
     */
    id GetNativeiOSEAGLContext(void);

    /*# get OSX NSWindow
     *
     * Get OSX NSWindow native handle (id). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeOSXNSWindow
     * @return id [type:id] native handle
     */
    id GetNativeOSXNSWindow(void);

    /*# get OSX NSView
     *
     * Get OSX NSView native handle (id). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeOSXNSView
     * @return id [type:id] native handle
     */
    id GetNativeOSXNSView(void);

    /*# get OSX NSOpenGLContext
     *
     * Get OSX NSOpenGLContext native handle (id). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeOSXNSOpenGLContext
     * @return id [type:id] native handle
     */
    id GetNativeOSXNSOpenGLContext(void);

    /*# get Win32 HWND
     *
     * Get Win32 windows native handle (HWND). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeWindowsHWND
     * @return HWND [type:HWND] native handle
     */
    HWND GetNativeWindowsHWND(void);

    /*# get Win32 HGLRC
     *
     * Get Win32 gl rendercontext native handle (HGLRC). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeWindowsHGLRC
     * @return HGLRC [type:HGLRC] native handle
     */
    HGLRC GetNativeWindowsHGLRC(void);

    /*# get Android EGLContext
     *
     * Get Android EGLContext native handle (EGLContext). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeAndroidEGLContext
     * @return EGLContext [type:EGLContext] native handle
     */
    EGLContext GetNativeAndroidEGLContext(void);

    /*# get Android EGLSurface
     *
     * Get Android EGLSurface native handle (EGLSurface). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeAndroidEGLSurface
     * @return EGLSurface [type:EGLSurface] native handle
     */
    EGLSurface GetNativeAndroidEGLSurface(void);

    /*# get Android native JavaVM
     *
     * Get Android JavaVM ptr. Any other platform return zero.
     *
     * @name dmGraphics::GetNativeAndroidJavaVM
     * @return JavaVM* [type:JavaVM*] native handle
     */
    JavaVM* GetNativeAndroidJavaVM(void);

    /*# get Android native jobject
     *
     * Get Android native jobject. Any other platform return zero.
     *
     * @name dmGraphics::GetNativeAndroidActivity
     * @return jobject [type:jobject] native handle
     */
    jobject GetNativeAndroidActivity(void);

    /*# get Android app object
     *
     * Get Android app object. Any other platform return zero.
     *
     * @name dmGraphics::GetNativeAndroidApp
     * @return app [type:struct android_app*] native handle
     */
    android_app* GetNativeAndroidApp(void);

    /*# get Linux X11Window
     *
     * Get Linux X11Window windows native handle (Window). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeX11Window
     * @return Window [type:Window] native handle
     */
    Window GetNativeX11Window(void);

    /*# get Linux X11GLXContext
     *
     * Get Linux X11GLXContext native handle (GLXContext). Any other platform return zero.
     *
     * @name dmGraphics::GetNativeX11GLXContext
     * @return GLXContext [type:GLXContext] native handle
     */
    GLXContext GetNativeX11GLXContext(void);
}

#endif // DMSDK_GRAPHICS_NATIVE_H
