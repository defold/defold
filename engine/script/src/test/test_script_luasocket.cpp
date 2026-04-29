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

#include <jc_test/jc_test.h>

#include <stdio.h>
#include <stdint.h>

#include "script.h"
#include "script_vmath.h"
#include "test_script.h"

#include <testmain/testmain.h>
#include <dlib/log.h>

class ScriptLuasocketTest : public dmScriptTest::ScriptTest
{
};

TEST_F(ScriptLuasocketTest, TestLuasocket)
{
    int top = dlua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_luasocket.luac"));

#if defined(__EMSCRIPTEN__)
    const char *funcs[] = {
        "test_html5_minimal", 0
    };
#else
    const char *funcs[] = {
        "test_bind", "test_getaddr",
        "test_udp", "test_tcp_clientserver",
        "test_udp_clientserver", "test_bind_error", 0
    };
#endif

    for (int i=0;funcs[i];i++)
    {
        dlua_getglobal(L, "functions");
        ASSERT_EQ(DLUA_TTABLE, dlua_type(L, -1));
        dlua_getfield(L, -1, funcs[i]);
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
    }

    ASSERT_EQ(top, dlua_gettop(L));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
