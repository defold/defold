// Copyright 2020-2025 The Defold Foundation
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

#if defined(ANDROID)
#include <android_native_app_glue.h>

#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/thread.h>
#include <glfw/glfw.h>
#endif

#include "engine.h"
#include "engine_version.h"
#include "engine_private.h"

extern "C" void dmExportedSymbols(); // Found in "__exported_symbols.cpp"

static void AppCreate(void* _ctx)
{
    (void)_ctx;
    dmEngineInitialize();
}

static void AppDestroy(void* _ctx)
{
    (void)_ctx;
    dmEngineFinalize();
}

static int EngineMain(int argc, char *argv[])
{
    dmExportedSymbols(); // Instead of our previous global constructor

    dmEngine::RunLoopParams params;
    params.m_Argc = argc;
    params.m_Argv = argv;
    params.m_AppCtx = 0;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = (dmEngine::EngineCreate)dmEngineCreate;
    params.m_EngineDestroy = (dmEngine::EngineDestroy)dmEngineDestroy;
    params.m_EngineUpdate = (dmEngine::EngineUpdate)dmEngineUpdate;
    params.m_EngineGetResult = (dmEngine::EngineGetResult)dmEngineGetResult;
    return dmEngine::RunLoop(&params);
}

#if defined(ANDROID)

extern struct android_app* __attribute__((weak)) g_AndroidApp;

struct EngineMainThreadArgs
{
    char** m_Argv;
    int m_Argc;
    int m_ExitCode;
    int m_Finished;
};

static void EngineMainThread(void* ctx)
{
    dmThread::SetThreadName(dmThread::GetCurrentThread(), "engine_main");
    EngineMainThreadArgs* args = (EngineMainThreadArgs*)ctx;
    args->m_ExitCode = EngineMain(args->m_Argc, args->m_Argv);
    args->m_Finished = 1;
}

static int WaitForWindow()
{
    while (glfwAndroidWindowOpened() == 0)
    {
        void* data = NULL;
        int ident = ALooper_pollOnce(300, NULL, NULL, &data);

        if (ident >= 0 && data != NULL) {
            struct android_poll_source* source = (struct android_poll_source*)data;
            source->process(g_AndroidApp, source);
        }
        if (ident == ALOOPER_POLL_ERROR) {
            dmLogFatal("ALooper_pollOnce returned an error");
            return 0;
        }

        glfwAndroidFlushEvents();
        if (g_AndroidApp->destroyRequested) {
            return 0;
        }
        dmTime::Sleep(300);
    }
    return 1;
}

int engine_main(int argc, char *argv[])
{
    dmThread::SetThreadName(dmThread::GetCurrentThread(), "looper_main");

    pthread_attr_t attr;
    size_t stacksize;

    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstacksize(&attr, &stacksize);

    g_AndroidApp->onAppCmd = glfwAndroidHandleCommand;
    g_AndroidApp->onInputEvent = glfwAndroidHandleInput;

    // Wait for window to become ready (APP_CMD_INIT_WINDOW in handleCommand)
    if (!WaitForWindow())
    {
        // When phone lock/unlock app may receive APP_CMD_DESTROY without APP_CMD_INIT_WINDOW
        // in this case app should exit immediately
        return 0;
    }

    glfwInit();

    EngineMainThreadArgs args;
    args.m_Argc = argc;
    args.m_Argv = argv;
    args.m_Finished = 0;
    dmThread::Thread t = dmThread::New(EngineMainThread, stacksize, &args, "engine_main");
    while (!args.m_Finished)
    {
        glfwAndroidPollEvents();
        dmTime::Sleep(0);
        if (g_AndroidApp->destroyRequested) {
            // App requested exit. It doesn't wait when thread work finished because app is in background already.
            // App will never end up here from within the app itself, only using OS functions.
            return 0;
        }
    }
    dmThread::Join(t);

    glfwTerminate();
    return args.m_ExitCode;
}

#else

int engine_main(int argc, char *argv[])
{
    return EngineMain(argc, argv);
}

#endif
