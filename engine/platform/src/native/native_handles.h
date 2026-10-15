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

#ifndef DM_PLATFORM_NATIVE_HANDLES_H
#define DM_PLATFORM_NATIVE_HANDLES_H

#include "native.h"
#if defined(DM_PLATFORM_IOS)
#include <objc/objc.h>
#elif defined(ANDROID)
#include <EGL/egl.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ANDROID)
EGLContext dmNativeGetAndroidEGLContext(void);
EGLSurface dmNativeGetAndroidEGLSurface(void);
JavaVM* dmNativeGetAndroidJavaVM(void);
jobject dmNativeGetAndroidActivity(void);
struct android_app* dmNativeGetAndroidApp(void);
ANativeWindow* dmNativeAcquireAndroidWindow(void);
ANativeWindow* dmNativeWaitForAndroidWindow(void);
int dmNativeAndroidIsWindowCurrent(ANativeWindow* window);
void dmNativeReleaseAndroidWindow(ANativeWindow* window);
#elif defined(DM_PLATFORM_IOS)
id dmNativeGetiOSUIWindow(void);
id dmNativeGetiOSUIView(void);
id dmNativeGetiOSEAGLContext(void);
    // See documentation in engine.h
    typedef void (*EngineInit)(void* ctx);
    typedef void (*EngineExit)(void* ctx);
    typedef void* (*EngineCreate)(int argc, char** argv);
    typedef void (*EngineDestroy)(void* engine);
    typedef int (*EngineUpdate)(void* engine);

    // See engine_private.h
    enum dmNativeAppRunAction
    {
        NATIVE_APP_RUN_UPDATE = 0,
        NATIVE_APP_RUN_EXIT = -1,
        NATIVE_APP_RUN_REBOOT = 1,
    };
    // In case of a non zero return value from the update function, call this to get the result of the engine update
    // Gets a copy of the argument list, use free(argv[i]) for each afterwards
    typedef void (*EngineGetResult)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

    void dmNativeAppBootstrap(int argc, char** argv, void* init_ctx, EngineInit init_fn, EngineExit exit_fn, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn);
#endif


#ifdef __cplusplus
}
#endif
#endif
