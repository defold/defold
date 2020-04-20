#include <graphics/glfw/glfw.h>
#include <graphics/glfw/glfw_native.h>
#include "graphics_native.h"

namespace dmGraphics
{
	#define WRAP_GLFW_NATIVE_HANDLE_CALL(return_type, func_name) return_type GetNative##func_name() { return glfwGet##func_name(); }

    WRAP_GLFW_NATIVE_HANDLE_CALL(id, iOSUIWindow);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, iOSUIView);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, iOSEAGLContext);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, OSXNSWindow);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, OSXNSView);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, OSXNSOpenGLContext);
    WRAP_GLFW_NATIVE_HANDLE_CALL(HWND, WindowsHWND);
    WRAP_GLFW_NATIVE_HANDLE_CALL(HGLRC, WindowsHGLRC);
    WRAP_GLFW_NATIVE_HANDLE_CALL(EGLContext, AndroidEGLContext);
    WRAP_GLFW_NATIVE_HANDLE_CALL(EGLSurface, AndroidEGLSurface);
    WRAP_GLFW_NATIVE_HANDLE_CALL(JavaVM*, AndroidJavaVM);
    WRAP_GLFW_NATIVE_HANDLE_CALL(jobject, AndroidActivity);
    WRAP_GLFW_NATIVE_HANDLE_CALL(android_app*, AndroidApp);
    WRAP_GLFW_NATIVE_HANDLE_CALL(Window, X11Window);
    WRAP_GLFW_NATIVE_HANDLE_CALL(GLXContext, X11GLXContext);

    #undef WRAP_GLFW_NATIVE_HANDLE_CALL
}
