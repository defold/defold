#include <graphics/glfw/glfw.h>
#include <graphics/glfw/glfw_native.h>
#include "graphics_native.h"

namespace dmGraphics
{
    id GetNativeiOSUIWindow()               { return glfwGetiOSUIWindow(); }
    id GetNativeiOSUIView()                 { return glfwGetiOSUIView(); }
    id GetNativeiOSEAGLContext()            { return glfwGetiOSEAGLContext(); }
    id GetNativeOSXNSWindow()               { return glfwGetOSXNSWindow(); }
    id GetNativeOSXNSView()                 { return glfwGetOSXNSView(); }
    id GetNativeOSXNSOpenGLContext()        { return glfwGetOSXNSOpenGLContext(); }
    HWND GetNativeWindowsHWND()             { return glfwGetWindowsHWND(); }
    HGLRC GetNativeWindowsHGLRC()           { return glfwGetWindowsHGLRC(); }
    EGLContext GetNativeAndroidEGLContext() { return glfwGetAndroidEGLContext(); }
    EGLSurface GetNativeAndroidEGLSurface() { return glfwGetAndroidEGLSurface(); }
    JavaVM* GetNativeAndroidJavaVM()        { return glfwGetAndroidJavaVM(); }
    jobject GetNativeAndroidActivity()      { return glfwGetAndroidActivity(); }
    android_app* GetNativeAndroidApp()      { return glfwGetAndroidApp(); }
    Window GetNativeX11Window()             { return glfwGetX11Window(); }
    GLXContext GetNativeX11GLXContext()     { return glfwGetX11GLXContext(); }
}
