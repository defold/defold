#include "../app/app.h"

#include <stdio.h> // debug

namespace dmApp
{
    int Run(const Params* params)
    {
        if (params->m_AppCreate)
            params->m_AppCreate(params->m_AppCtx);

        int argc = params->m_Argc;
        char** argv = params->m_Argv;
        int exit_code = 0;
        void* engine = 0;
        Result result = RESULT_OK;
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

            result = params->m_EngineUpdate(engine);

            if (RESULT_OK != result)
            {
                int run_action = 0;
                params->m_EngineGetResult(engine, &run_action, &exit_code, &argc, &argv);

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

        return exit_code;
    }
}
