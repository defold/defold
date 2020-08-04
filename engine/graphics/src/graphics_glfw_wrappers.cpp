// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
