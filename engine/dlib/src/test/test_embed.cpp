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

#include <stdint.h>
#include <stdlib.h>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/hash.h"
#include "../dlib/log.h"

extern unsigned char TEST_EMBED[];
extern uint32_t TEST_EMBED_SIZE;
extern unsigned char GENERATED_EMBED[];

TEST(dlib, Embed)
{
    const char* test_embed = (const char*) TEST_EMBED;
    const char* generated_embed = (const char*) GENERATED_EMBED;

    std::string test_embed_str(test_embed, strlen("embedded message"));
    ASSERT_STREQ("embedded message", test_embed_str.c_str());
    ASSERT_EQ(strlen("embedded message"), TEST_EMBED_SIZE);

    std::string generated_embed_str(generated_embed, strlen("generated data"));
    ASSERT_STREQ("generated data", generated_embed_str.c_str());
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
