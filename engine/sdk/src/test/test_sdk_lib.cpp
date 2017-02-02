
// Extension created in a separate lib in order to
// test potential problems with incorrect non-sdk includes from sdk files.
// ONLY c/c++ std includes or dmsdk includes are allowed!
// Attempting other includes will fail!

// Include generated sdk.h files including all sdk include files.
#include <dmsdk/sdk.h>

int g_TestAppInitCount = 0;
int g_TestAppEventCount = 0;

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

void OnEventTest(dmExtension::Params* params, const dmExtension::Event* event)
{
    if( event->m_Event == dmExtension::EVENT_ID_ACTIVATEAPP )
        ++g_TestAppEventCount;
    else if(event->m_Event == dmExtension::EVENT_ID_DEACTIVATEAPP)
        --g_TestAppEventCount;
}

dmExtension::Result FinalizeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(TestSdk, "test", AppInitializeTest, AppFinalizeTest, InitializeTest, UpdateTest, OnEventTest, FinalizeTest);

