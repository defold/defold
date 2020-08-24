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

#include <dlib/dlib.h>
#include <dlib/socket.h>
#include <dlib/dns.h>
#include <dlib/memprofile.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/thread.h>
#include <graphics/graphics.h>
#include <crash/crash.h>

#if defined(ANDROID)
#include <android_native_app_glue.h>

#include <dlib/time.h>
#include <graphics/glfw/glfw.h>
#endif

#include "engine.h"
#include "engine_version.h"
#include "engine_private.h"

static void AppCreate(void* _ctx)
{
    (void)_ctx;
    dmThread::SetThreadName(dmThread::GetCurrentThread(), "engine_main");

#if DM_RELEASE
    dLib::SetDebugMode(false);
#endif
    dmHashEnableReverseHash(dLib::IsDebugMode());

    dmCrash::Init(dmEngineVersion::VERSION, dmEngineVersion::VERSION_SHA1);
    dmDDF::RegisterAllTypes();
    dmSocket::Initialize();
    dmDNS::Initialize();
    dmMemProfile::Initialize();
    dmProfile::Initialize(256, 1024 * 16, 128);
    dmLogParams params;
    dmLogInitialize(&params);
}

static void AppDestroy(void* _ctx)
{
    (void)_ctx;
    dmGraphics::Finalize();
    dmLogFinalize();
    dmProfile::Finalize();
    dmMemProfile::Finalize();
    dmDNS::Finalize();
    dmSocket::Finalize();
}

static int EngineMain(int argc, char *argv[])
{
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
    while (glfwAndroidWindowOpened() == 0)
    {
        int ident;
        int events;
        struct android_poll_source* source;

        if ((ident=ALooper_pollAll(300, NULL, &events, (void**)&source)) >= 0)
        {
            // Process this event.
            if (source != NULL) {
                source->process(g_AndroidApp, source);
            }
        }

        glfwAndroidFlushEvents();
        dmTime::Sleep(300);
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
