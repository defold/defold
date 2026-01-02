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

#include "engine_private.h"
#include "engine.h"
#include <crash/crash.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
    #include <emscripten/emscripten.h>
#endif

namespace dmEngine
{

#if defined(DM_PLATFORM_IOS)

    int RunLoop(const RunLoopParams* params)
    {
        int argc = params->m_Argc;
        char** argv = params->m_Argv;
        // Calls UIApplicationMain, which we won't return from
        dmGraphics::AppBootstrap(argc, argv, params->m_AppCtx,  (dmGraphics::EngineInit)params->m_AppCreate,
                                                                (dmGraphics::EngineExit)params->m_AppDestroy,
                                                                (dmGraphics::EngineCreate)params->m_EngineCreate,
                                                                (dmGraphics::EngineDestroy)params->m_EngineDestroy,
                                                                (dmGraphics::EngineUpdate)params->m_EngineUpdate,
                                                                (dmGraphics::EngineGetResult)params->m_EngineGetResult);
        return 0;
    }

#else // not iOS

#ifdef __EMSCRIPTEN__
    struct StepContext
    {
        const RunLoopParams*    m_Params;
        void*                   m_Engine;
    };

    static void PerformStep(void* context);
#endif

    int RunLoop(const RunLoopParams* params)
    {
        if (params->m_AppCreate)
            params->m_AppCreate(params->m_AppCtx);

        int argc = params->m_Argc;
        char** argv = params->m_Argv;
        int allocated_argc = 0;
        char** allocated_argv = 0;
        int exit_code = 0;
        dmEngine::HEngine engine = 0;
        dmEngine::UpdateResult result = RESULT_OK;
        while (RESULT_OK == result)
        {
            if (engine == 0)
            {
                engine = params->m_EngineCreate(argc, argv);
                if (!engine)
                {
                    exit_code = 1;
                    break;
                }
            }

#ifdef __EMSCRIPTEN__
            StepContext ctx;
            ctx.m_Params = params;
            ctx.m_Engine = engine;
            // Won't return from this call
            emscripten_set_main_loop_arg(PerformStep, &ctx, 0, 1);
#else
            result = params->m_EngineUpdate(engine);
#endif

            if (RESULT_OK != result)
            {
                int run_action = 0;
                for (int i = 0; i < allocated_argc; ++i)
                {
                    free(allocated_argv[i]);
                }
                free(allocated_argv);
                allocated_argv = 0;
                allocated_argc = 0;

                params->m_EngineGetResult(engine, &run_action, &exit_code, &argc, &argv);
                if (argv != params->m_Argv)
                {
                    allocated_argv = argv;
                    allocated_argc = argc;
                }

                params->m_EngineDestroy(engine);
                engine = 0;

                if (RESULT_REBOOT == result)
                {
                    // allows us to reboot
                    result = RESULT_OK;
                }
            }
        }

        if (params->m_AppDestroy)
            params->m_AppDestroy(params->m_AppCtx);

        for (int i = 0; i < allocated_argc; ++i)
        {
            free(allocated_argv[i]);
        }
        free(allocated_argv);

        return exit_code;
    }

#endif // iOS

#if defined(__EMSCRIPTEN__)
    static void PreStepEmscripten(void* _ctx)
    {
        StepContext* ctx = (StepContext*)_ctx;
        const RunLoopParams* params = ctx->m_Params;
        HEngine engine = (HEngine)ctx->m_Engine;

        int argc = params->m_Argc;
        char** argv = params->m_Argv;
        int exit_code = 0;
        int result = 0;
        params->m_EngineGetResult(engine, &result, &exit_code, &argc, &argv);

        if (RESULT_OK != result)
        {
            dmCrash::SetEnabled(false); // because emscripten_cancel_main_loop throws an 'unwind' exception (emscripten issue #11682)
            emscripten_pause_main_loop(); // stop further callbacks
            emscripten_cancel_main_loop(); // Causes an exception (emscripten issue #11682)

            params->m_EngineDestroy(engine);
            engine = 0;

            if (RESULT_REBOOT == result)
            {
                ctx->m_Engine = params->m_EngineCreate(argc, argv);
                if (ctx->m_Engine)
                {
                    // Won't return from this
                    emscripten_set_main_loop_arg(PerformStep, ctx, 0, 1);
                }
                else {
                    dmLogError("Engine failed to reboot");
                    exit_code = 1;
                }
            }

            dmLogInfo("Engine exited with code %d", exit_code);
            exit(exit_code);
        }

        if (!dmCrash::IsEnabled()) {
            dmCrash::SetEnabled(true);
        }
    }

    static void PerformStep(void* _ctx)
    {
        StepContext* ctx = (StepContext*)_ctx;
        HEngine engine = (HEngine)ctx->m_Engine;

        // check if we want to reboot
        PreStepEmscripten(_ctx);

        ctx->m_Params->m_EngineUpdate(engine);
    }
#endif // emscripten

} // namespace
