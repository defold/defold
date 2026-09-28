// Copyright 2020-2026 The Defold Foundation
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

#include "graphics_native.h"

#if defined(DM_PLATFORM_IOS)
    #include <platform/platform_window_ios.h>
#elif defined(ANDROID)
    #include <platform/platform_window_android.h>
#elif defined(__MACH__)
    #include <platform/platform_window_osx.h>
#elif defined(_MSC_VER)
    #include <platform/platform_window_win32.h>
#elif defined(__linux__) && !defined(ANDROID)
    #include <platform/platform_window_linux.h>
#endif

#include "graphics_private.h"

namespace dmGraphics
{
#if defined(DM_PLATFORM_IOS)
    id GetNativeiOSUIWindow()               { return dmPlatform::GetiOSUIWindow(); }
    id GetNativeiOSUIView()                 { return dmPlatform::GetiOSUIView(); }
    id GetNativeiOSEAGLContext()            { return dmPlatform::GetiOSEAGLContext(); }
#else
    id GetNativeiOSUIWindow()               { return 0; }
    id GetNativeiOSUIView()                 { return 0; }
    id GetNativeiOSEAGLContext()            { return 0; }
#endif

#if defined(__MACH__) && !defined(DM_PLATFORM_IOS)
    id GetNativeOSXNSWindow()               { return dmPlatform::GetOSXNSWindow(GetWindow(GetInstalledContext())); }
    id GetNativeOSXNSView()                 { return dmPlatform::GetOSXNSView(GetWindow(GetInstalledContext())); }
    id GetNativeOSXNSOpenGLContext()        { return dmPlatform::GetOSXNSOpenGLContext(GetWindow(GetInstalledContext())); }
#else
    id GetNativeOSXNSWindow()               { return 0; }
    id GetNativeOSXNSView()                 { return 0; }
    id GetNativeOSXNSOpenGLContext()        { return 0; }
#endif

#if defined(_MSC_VER)
    HWND GetNativeWindowsHWND()             { return dmPlatform::GetWindowsHWND(GetWindow(GetInstalledContext())); }
    HGLRC GetNativeWindowsHGLRC()           { return dmPlatform::GetWindowsHGLRC(GetWindow(GetInstalledContext())); }
#else
    HWND GetNativeWindowsHWND()             { return 0; }
    HGLRC GetNativeWindowsHGLRC()           { return 0; }
#endif

#if defined(ANDROID)
    EGLContext GetNativeAndroidEGLContext() { return dmPlatform::GetAndroidEGLContext(); }
    EGLSurface GetNativeAndroidEGLSurface() { return dmPlatform::GetAndroidEGLSurface(); }
    JavaVM* GetNativeAndroidJavaVM()        { return dmPlatform::GetAndroidJavaVM(); }
    jobject GetNativeAndroidActivity()      { return dmPlatform::GetAndroidActivity(); }
    android_app* GetNativeAndroidApp()      { return dmPlatform::GetAndroidApp(); }
#else
    EGLContext GetNativeAndroidEGLContext() { return 0; }
    EGLSurface GetNativeAndroidEGLSurface() { return 0; }
    JavaVM* GetNativeAndroidJavaVM()        { return 0; }
    jobject GetNativeAndroidActivity()      { return 0; }
    android_app* GetNativeAndroidApp()      { return 0; }
#endif

#if defined(__linux__) && !defined(ANDROID)
    Window GetNativeX11Window()             { return dmPlatform::GetX11Window(GetWindow(GetInstalledContext())); }
    GLXContext GetNativeX11GLXContext()     { return dmPlatform::GetX11GLXContext(GetWindow(GetInstalledContext())); }
#else
    Window GetNativeX11Window()             { return 0; }
    GLXContext GetNativeX11GLXContext()     { return 0; }
#endif
}
