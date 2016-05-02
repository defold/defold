//#include <extension/extension.h>
#include "test_ext.h"

/*
dmExtension::Result AppInitializeTest(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeTest(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeTest(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(Test, "Test", AppInitializeTest, AppFinalizeTest, InitializeTest, 0, 0, FinalizeTest)
*/

extern "C"
{
	void Test()
	{

	}
}
