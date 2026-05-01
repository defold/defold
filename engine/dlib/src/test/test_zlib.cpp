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
#include <string.h>
#include <dlib/testutil.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/zlib.h"

static bool Writer(void* context, const void* buffer, uint32_t buffer_size)
{
    std::string* s = (std::string*) context;
    s->append((const char*) buffer, buffer_size);
    return true;
}

static bool ReadFile(const char* relative_path, std::string* out)
{
    char path[1024];
    dmTestUtil::MakeHostPath(path, sizeof(path), relative_path);

    FILE* file = fopen(path, "rb");
    if (file == 0)
        return false;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    out->resize(file_size > 0 ? (size_t)file_size : 0);
    size_t read = 0;
    if (file_size > 0)
        read = fread(&(*out)[0], 1, out->size(), file);

    fclose(file);
    return read == out->size();
}

enum Payload
{
    PAYLOAD_ALPHA,
    PAYLOAD_EMPTY,
    PAYLOAD_BINARY_256,
};

static std::string MakePayload(Payload payload)
{
    switch (payload)
    {
    case PAYLOAD_ALPHA:
        return "Alpha\nBeta\nGamma\n";
    case PAYLOAD_EMPTY:
        return "";
    case PAYLOAD_BINARY_256:
    {
        std::string data;
        data.resize(256);
        for (uint32_t i = 0; i < 256; ++i)
            data[i] = (char)i;
        return data;
    }
    }

    return "";
}

static void AssertBufferEqual(const std::string& expected, const std::string& actual)
{
    ASSERT_EQ(expected.size(), actual.size());
    if (!expected.empty())
        ASSERT_EQ(0, memcmp(expected.data(), actual.data(), expected.size()));
}

struct InflateFileParams
{
    const char* m_Path;
    Payload     m_Payload;
};

class InflateFileTest : public jc_test_params_class<InflateFileParams>
{
};

TEST_P(InflateFileTest, Inflate)
{
    std::string compressed;
    ASSERT_TRUE(ReadFile(GetParam().m_Path, &compressed));

    std::string decompressed;
    dmZlib::Result r = dmZlib::InflateBuffer(compressed.data(), compressed.size(),
            &decompressed, Writer);
    ASSERT_EQ(dmZlib::RESULT_OK, r);

    AssertBufferEqual(MakePayload(GetParam().m_Payload), decompressed);
}

const InflateFileParams params_inflate_files[] = {
    { "src/test/data/zip/alpha.deflate", PAYLOAD_ALPHA },
    { "src/test/data/zip/alpha.gz", PAYLOAD_ALPHA },
    { "src/test/data/zip/empty.deflate", PAYLOAD_EMPTY },
    { "src/test/data/zip/empty.gz", PAYLOAD_EMPTY },
    { "src/test/data/zip/binary_256.deflate", PAYLOAD_BINARY_256 },
    { "src/test/data/zip/binary_256.gz", PAYLOAD_BINARY_256 },
};

INSTANTIATE_TEST_CASE_P(InflateFiles, InflateFileTest,
        jc_test_values_in(params_inflate_files));

struct DeflateBufferParams
{
    Payload m_Payload;
    int     m_Level;
};

class DeflateBufferTest : public jc_test_params_class<DeflateBufferParams>
{
};

TEST_P(DeflateBufferTest, RoundTrip)
{
    std::string payload = MakePayload(GetParam().m_Payload);
    std::string compressed;
    std::string decompressed;

    dmZlib::Result r = dmZlib::DeflateBuffer(payload.data(), payload.size(),
            GetParam().m_Level, &compressed, Writer);
    ASSERT_EQ(dmZlib::RESULT_OK, r);

    r = dmZlib::InflateBuffer(compressed.data(), compressed.size(),
            &decompressed, Writer);
    ASSERT_EQ(dmZlib::RESULT_OK, r);

    AssertBufferEqual(payload, decompressed);
}

const DeflateBufferParams params_deflate_buffers[] = {
    { PAYLOAD_ALPHA, 0 },
    { PAYLOAD_ALPHA, 5 },
    { PAYLOAD_ALPHA, 9 },
    { PAYLOAD_EMPTY, 5 },
    { PAYLOAD_BINARY_256, 1 },
    { PAYLOAD_BINARY_256, 9 },
};

INSTANTIATE_TEST_CASE_P(DeflateBuffers, DeflateBufferTest,
        jc_test_values_in(params_deflate_buffers));

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
