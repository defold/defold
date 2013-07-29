#include <stdlib.h>
#include <gtest/gtest.h>
#include "../extension.h"

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

// Extension in a separate library. See comment in test_extension_lib.cpp

TEST(dmExtension, Basic)
{
    ASSERT_STREQ("test", dmExtension::GetFirstExtension()->m_Name);
    ASSERT_EQ(0, dmExtension::GetFirstExtension()->m_Next);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
