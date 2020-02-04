#include <extension/extension.h>

namespace dmCrash
{
    static dmExtension::Result InitializeCrash(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result FinalizeCrash(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(CrashExt, "Crash", 0, 0, InitializeCrash, 0, 0, FinalizeCrash)
}

