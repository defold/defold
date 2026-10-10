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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../font.h"
#include "../fontgen.h"

extern "C" void ResourceTypeTTF();

TEST(FontGenNull, IsUnsupportedAndCannotCreateJobs)
{
    ASSERT_FALSE(FontGenIsSupported());

    FontGenParams params = {};
    ASSERT_EQ((FontGenJobData*)0, FontGenCreateJobData(&params, 1));
    ASSERT_EQ((HJob)0, FontGenAddGlyphByIndex(0, 0, 1));
}

TEST(FontGenNull, TTFLoadingDoesNotUseTheRealParser)
{
    // Force the null archive member into the link, as the engine does through
    // its exported resource-type symbol, before FontLoadFromMemory is resolved.
    ResourceTypeTTF();

    uint8_t data[4] = {};
    ASSERT_EQ((HFont)0, FontLoadFromMemory("test.ttf", data, sizeof(data), false));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
