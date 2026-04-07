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

#include <stdlib.h>
#include <string.h>

#include "../common/testmain.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static const char TEST_EXAMPLE_TXT[] = "http://httpbin.org/base64/SFRUUEJJTiBpcyBhd2Vzb21l";
static const char TEST_EXAMPLE_TXT_CONTENT[] = "HTTPBIN is awesome";

TEST(dmTestMain, PlatformInit)
{
    ASSERT_TRUE(TestMainPlatformInit());

    bool is_attached = TestMainIsDebuggerAttached();
    printf("DebuggerAttached: %d", is_attached?1:0);
    fflush(stdout);
}

TEST(dmTestMain, HttpGet)
{
    uint32_t length = 0;
    const char* content = 0;
    int result = TestHttpGet(TEST_EXAMPLE_TXT, &length, &content);
    ASSERT_EQ(200, result);
    ASSERT_EQ((uint32_t)strlen(TEST_EXAMPLE_TXT_CONTENT), length);
    ASSERT_STREQ(TEST_EXAMPLE_TXT_CONTENT, content);

    if (content != 0)
    {
        free((void*)content);
    }
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
