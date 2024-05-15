// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#include <glfw/glfw.h>
#include  <glfw/glfw_native.h>
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
