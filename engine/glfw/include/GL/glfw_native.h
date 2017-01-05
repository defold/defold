#ifndef _glfw_native_h_
#define _glfw_native_h_

#ifdef __cplusplus
extern "C" {
#endif

// iOS
#if defined(GLFW_EXPOSE_NATIVE_IOS)
#if defined(__OBJC__)
    #import <UIKit/UIKit.h>
#else
    typedef void* id;
#endif
struct GLFWNativeHandles
{
    id m_UIWindow;
    id m_UIView;
    id m_EAGLContext;
};

// macOS
#elif defined(GLFW_EXPOSE_NATIVE_OSX)
#include <ApplicationServices/ApplicationServices.h>
#if defined(__OBJC__)
  #import <Cocoa/Cocoa.h>
#else
  typedef void* id;
#endif
struct GLFWNativeHandles
{
    id m_NSWindow;
    id m_NSView;
    id m_NSOpenGLContext;
};

// Android
#elif defined(GLFW_EXPOSE_NATIVE_ANDROID)
#include <android_native_app_glue.h>
struct GLFWNativeHandles
{
  EGLContext m_EGLContext;
  EGLSurface m_EGLSurface;
  JNIEnv*    m_JNIEnv;
  jobject    m_Activity;
};

// Windows
#elif defined(GLFW_EXPOSE_NATIVE_WINDOWS)
struct GLFWNativeHandles
{
  void* dummy;
};

// Linux
#else
struct GLFWNativeHandles
{
  void* dummy;
};
#endif

GLFWAPI struct GLFWNativeHandles glfwGetNativeHandles(void);

#ifdef __cplusplus
}
#endif

#endif
