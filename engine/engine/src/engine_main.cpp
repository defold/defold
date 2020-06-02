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

#include <app/app.h>
#include <dlib/dlib.h>
#include <dlib/socket.h>
#include <dlib/dns.h>
#include <dlib/memprofile.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/thread.h>
#include <graphics/graphics.h>
#include <crash/crash.h>

#include "engine.h"
#include "engine_version.h"

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

int engine_main(int argc, char *argv[])
{
    dmApp::Params params;
    params.m_Argc = argc;
    params.m_Argv = argv;
    params.m_AppCtx = 0;
    params.m_AppCreate = AppCreate;
    params.m_AppDestroy = AppDestroy;
    params.m_EngineCreate = (dmApp::EngineCreate)dmEngineCreate;
    params.m_EngineDestroy = (dmApp::EngineDestroy)dmEngineDestroy;
    params.m_EngineUpdate = (dmApp::EngineUpdate)dmEngineUpdate;
    params.m_EngineGetResult = (dmApp::EngineGetResult)dmEngineGetResult;
    return dmApp::Run(&params);
}
