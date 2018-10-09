#ifndef _glfw_native_h_
#define _glfw_native_h_

#ifdef __cplusplus
extern "C" {
#endif

#include "glfw.h"

//========================================================================
// Include needed header for needed native types, typedef unknowns to void*
//========================================================================
#if defined(__MACH__)
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
    #include <windows.h>
#else
    typedef void* HWND;
    typedef void* HGLRC;
#endif

#if defined(__linux__) && !defined(__EMSCRIPTEN__) && !defined(ANDROID)
    #include <GL/glx.h>
#else
    typedef void* Window;
    typedef void* GLXContext;
#endif

#if defined(__EMSCRIPTEN__)
    // HTML5 / Emscripten
#else
#endif


//========================================================================
// Declare getters for native handles on all platforms.
// They will be defined as stubs in native.c that return NULL for
// all other platforms than the target one.
//========================================================================

GLFWAPI id glfwGetiOSUIWindow(void);
GLFWAPI id glfwGetiOSUIView(void);
GLFWAPI id glfwGetiOSEAGLContext(void);

GLFWAPI id glfwGetOSXNSWindow(void);
GLFWAPI id glfwGetOSXNSView(void);
GLFWAPI id glfwGetOSXNSOpenGLContext(void);

GLFWAPI HWND glfwGetWindowsHWND(void);
GLFWAPI HGLRC glfwGetWindowsHGLRC(void);

GLFWAPI EGLContext glfwGetAndroidEGLContext(void);
GLFWAPI EGLSurface glfwGetAndroidEGLSurface(void);
GLFWAPI JavaVM* glfwGetAndroidJavaVM(void);
GLFWAPI jobject glfwGetAndroidActivity(void);
#if defined(ANDROID)
GLFWAPI struct android_app* glfwGetAndroidApp(void);
#else
GLFWAPI android_app* glfwGetAndroidApp(void);
#endif

GLFWAPI Window glfwGetX11Window(void);
GLFWAPI GLXContext glfwGetX11GLXContext(void);

#ifdef __cplusplus
}
#endif

#endif
