// Copyright 2020-2023 The Defold Foundation
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
    typedef void* Display;
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
GLFWAPI id glfwGetOSXCALayer(void);

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

#if defined(DM_PLATFORM_IOS)

    // See documentation in engine.h
    typedef void (*EngineInit)(void* ctx);
    typedef void (*EngineExit)(void* ctx);
    typedef void* (*EngineCreate)(int argc, char** argv);
    typedef void (*EngineDestroy)(void* engine);
    typedef int (*EngineUpdate)(void* engine);

    // See engine_private.h
    enum glfwAppRunAction
    {
        GLFW_APP_RUN_UPDATE = 0,
        GLFW_APP_RUN_EXIT = -1,
        GLFW_APP_RUN_REBOOT = 1,
    };
    // In case of a non zero return value from the update function, call this to get the result of the engine update
    // Gets a copy of the argument list, use free(argv[i]) for each afterwards
    typedef void (*EngineGetResult)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

    void glfwAppBootstrap(int argc, char** argv, void* init_ctx, EngineInit init_fn, EngineExit exit_fn, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn);
#endif

GLFWAPI Window glfwGetX11Window(void);
GLFWAPI GLXContext glfwGetX11GLXContext(void);
GLFWAPI Display* glfwGetX11Display(void);

#ifdef __cplusplus
}
#endif

#endif
