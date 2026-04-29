// Copyright 2020-2026 The Defold Foundation
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

#include "../script.h"
#include "test_script.h"

#include <testmain/testmain.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>


class ScriptBitopTest : public dmScriptTest::ScriptTest
{
};

TEST_F(ScriptBitopTest, TestBitop)
{
    int top = dlua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_bitop.luac"));

    dlua_getglobal(L, "functions");
    ASSERT_EQ(DLUA_TTABLE, dlua_type(L, -1));
    dlua_getfield(L, -1, "test_bitop_md5");
    ASSERT_EQ(DLUA_TFUNCTION, dlua_type(L, -1));
    int result = dmScript::PCall(L, 0, DLUA_MULTRET);
    if (result == DLUA_ERRRUN)
    {
        ASSERT_TRUE(false);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    dlua_pop(L, 1);


    ASSERT_EQ(top, dlua_gettop(L));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();

    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    return ret;
}
