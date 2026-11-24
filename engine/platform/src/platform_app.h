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

#ifndef DM_PLATFORM_APP_H
#define DM_PLATFORM_APP_H

#include <stdint.h>

#if defined(ANDROID)
struct android_app;
#endif

namespace dmPlatform
{
    typedef void (*FAppInit)(void* context);
    typedef void (*FAppExit)(void* context);
    typedef void* (*FEngineCreate)(int argc, char** argv);
    typedef void (*FEngineDestroy)(void* engine);
    typedef int (*FEngineUpdate)(void* engine);
    typedef void (*FEngineGetResult)(void* engine, int* run_action, int* exit_code, int* argc, char*** argv);

    enum AppRunAction
    {
        APP_RUN_UPDATE = 0,
        APP_RUN_EXIT = -1,
        APP_RUN_REBOOT = 1,
    };

    // iOS owns the application run loop and invokes these engine callbacks.
    void AppBootstrap(int argc, char** argv, void* init_context, FAppInit init, FAppExit exit,
                      FEngineCreate create, FEngineDestroy destroy, FEngineUpdate update, FEngineGetResult get_result);
    void RegisteriOSApplicationDelegate(void* delegate);
    void UnregisteriOSApplicationDelegate(void* delegate);
    void RegisteriOSSceneDelegate(void* delegate);
    void UnregisteriOSSceneDelegate(void* delegate);

#if defined(ANDROID)
    // Android application events run on the looper thread, independently of rendering.
    void AndroidPreMain(android_app* app);
    void AndroidSetAppCallbacks(android_app* app);
    bool AndroidIsWindowOpened();
    bool AndroidInit();
    void AndroidTerminate();
    void AndroidPollEvents();
    void AndroidFlushEvents();

    typedef void (*FAndroidActivityResult)(void* env, void* activity, int32_t request_code, int32_t result_code, void* result);
    typedef void (*FAndroidActivityCreate)(void* env, void* activity);
    void AndroidRegisterActivityResultListener(FAndroidActivityResult listener);
    void AndroidUnregisterActivityResultListener(FAndroidActivityResult listener);
    void AndroidRegisterActivityCreateListener(FAndroidActivityCreate listener);
    void AndroidUnregisterActivityCreateListener(FAndroidActivityCreate listener);
#endif
}

#endif // DM_PLATFORM_APP_H
