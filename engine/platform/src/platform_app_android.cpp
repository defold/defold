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

#include "platform_app.h"

#include <android_native_app_glue.h>
#include "native/native.h"

extern "C" void dmNativePreMain(android_app* app);

namespace dmPlatform
{
    void AndroidPreMain(android_app* app) { dmNativePreMain(app); }

    void AndroidSetAppCallbacks(android_app* app)
    {
        app->onAppCmd = dmNativeAndroidHandleCommand;
        app->onInputEvent = dmNativeAndroidHandleInput;
    }

    bool AndroidIsWindowOpened() { return dmNativeAndroidWindowOpened() != 0; }
    bool AndroidInit()           { return dmNativeInit() != 0; }
    void AndroidTerminate()     { dmNativeTerminate(); }
    void AndroidPollEvents()    { dmNativeAndroidPollEvents(); }
    void AndroidFlushEvents()   { dmNativeAndroidFlushEvents(); }

    void AndroidRegisterActivityResultListener(FAndroidActivityResult listener)   { dmNativeAndroidRegisterOnActivityResultListener(listener); }
    void AndroidUnregisterActivityResultListener(FAndroidActivityResult listener) { dmNativeAndroidUnregisterOnActivityResultListener(listener); }
    void AndroidRegisterActivityCreateListener(FAndroidActivityCreate listener)   { dmNativeAndroidRegisterOnCreateListener(listener); }
    void AndroidUnregisterActivityCreateListener(FAndroidActivityCreate listener) { dmNativeAndroidUnregisterOnCreateListener(listener); }
}
