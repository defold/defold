#include <stdlib.h>
#include <gtest/gtest.h>
#include "../extension.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

// Extension in a separate library. See comment in test_extension_lib.cpp


extern int g_TestAppInitCount;

TEST(dmExtension, Basic)
{
    dmExtension::AppParams params;
    ASSERT_EQ(0, g_TestAppInitCount);
    dmExtension::AppInitialize(&params);
    ASSERT_EQ(1, g_TestAppInitCount);
    ASSERT_STREQ("test", dmExtension::GetFirstExtension()->m_Name);
    ASSERT_EQ(0, dmExtension::GetFirstExtension()->m_Next);
    dmExtension::AppFinalize(&params);
    ASSERT_EQ(0, g_TestAppInitCount);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
