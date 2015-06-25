#include <stdlib.h>
#include "../extension.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

// Extension created in a separate lib in order to
// test potential problems with dead stripping of symbols

int g_TestAppInitCount = 0;

dmExtension::Result AppInitializeTest(dmExtension::AppParams* params)
{
    g_TestAppInitCount++;
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeTest(dmExtension::AppParams* params)
{
    g_TestAppInitCount--;
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result UpdateTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(TestExt, "test", AppInitializeTest, AppFinalizeTest, InitializeTest, UpdateTest, FinalizeTest)
