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
#include <stdio.h>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/zlib.h"

extern unsigned char FOO_GZ[];
extern uint32_t FOO_GZ_SIZE;
extern unsigned char FOO_DEFLATE[];
extern uint32_t FOO_DEFLATE_SIZE;

static bool Writer(void* context, const void* buffer, uint32_t buffer_size)
{
    std::string* s = (std::string*) context;
    s->append((char*) buffer, buffer_size);
    return true;
}

TEST(dmZlib, Inflate)
{
    std::string buf;
    dmZlib::Result r;

    buf = "";
    // Test file create with gzip
    r = dmZlib::InflateBuffer(FOO_GZ, FOO_GZ_SIZE, &buf, Writer);
    ASSERT_EQ(dmZlib::RESULT_OK, r);
    ASSERT_STREQ("foo", buf.c_str());

    buf = "";
    // Test file created with zlib.compress in python
    r = dmZlib::InflateBuffer(FOO_DEFLATE, FOO_DEFLATE_SIZE, &buf, Writer);
    ASSERT_EQ(dmZlib::RESULT_OK, r);
    ASSERT_STREQ("foo", buf.c_str());
}

TEST(dmZlib, Deflate)
{
    std::string compressed;
    std::string decompressed;

    dmZlib::Result r;
    r = dmZlib::DeflateBuffer("bar", 3, 5, &compressed, Writer);
    ASSERT_EQ(dmZlib::RESULT_OK, r);

    r = dmZlib::InflateBuffer(compressed.c_str(), compressed.size(), &decompressed, Writer);
    ASSERT_EQ(dmZlib::RESULT_OK, r);
    ASSERT_STREQ("bar", decompressed.c_str());
}

std::string RandomString(int max)
{
    std::string tmp;
    uint32_t n = rand() % max;
    for (uint32_t j = 0; j < n; ++j)
    {
        char c = 'a' + (rand() % 26);
        tmp += c;
    }
    return tmp;
}

TEST(dmZlib, Stress)
{
    for (int i = 0; i < 100; ++i) {
        std::string ref = RandomString(37342);
        std::string compressed;
        std::string decompressed;

        dmZlib::Result r;
        r = dmZlib::DeflateBuffer(ref.c_str(), ref.size(), 5, &compressed, Writer);
        ASSERT_EQ(dmZlib::RESULT_OK, r);

        r = dmZlib::InflateBuffer(compressed.c_str(), compressed.size(), &decompressed, Writer);
        ASSERT_EQ(dmZlib::RESULT_OK, r);
        ASSERT_TRUE(ref == decompressed);
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
