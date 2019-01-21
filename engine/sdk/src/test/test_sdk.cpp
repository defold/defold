#include <stdlib.h>
#include <dlib/test/testutil.h>
#include <extension/extension.h>

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

// Extension in a separate library. See comment in test_sdk_lib.cpp
extern int g_TestAppInitCount;
extern int g_TestAppEventCount;

TEST(testSdk, Basic)
{
    dmExtension::AppParams appparams;
    ASSERT_EQ(0, g_TestAppInitCount);
    ASSERT_EQ(dmExtension::RESULT_OK, dmExtension::AppInitialize(&appparams));
    ASSERT_EQ(1, g_TestAppInitCount);
    ASSERT_STREQ("test", dmExtension::GetFirstExtension()->m_Name);
    ASSERT_EQ(0, dmExtension::GetFirstExtension()->m_Next);

    dmExtension::Params params;
    dmExtension::Event event;
    event.m_Event = dmExtension::EVENT_ID_ACTIVATEAPP;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(1, g_TestAppEventCount);
    event.m_Event = dmExtension::EVENT_ID_DEACTIVATEAPP;
    dmExtension::DispatchEvent(&params, &event);
    ASSERT_EQ(0, g_TestAppEventCount);

    dmExtension::AppFinalize(&appparams);
    ASSERT_EQ(0, g_TestAppInitCount);
}
