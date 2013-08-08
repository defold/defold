#include <stdlib.h>
#include "../extension.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

// Extension created in a separate lib in order to
// test potential problems with dead stripping of symbols

dmExtension::Result InitializeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Desc testExt = {
        "test",
        InitializeTest,
        FinalizeTest,
        0,
};

DM_REGISTER_EXTENSION(TestExt, testExt);

