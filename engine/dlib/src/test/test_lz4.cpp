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
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/lz4.h"
#include "../dlib/time.h"

/*
 * Test file foo.lz4 created with lz4 0.6.1 in python
 *
 * $> pip install lz4
 *
 * testfile.py:
 *
 * #!/usr/bin/env python
 * import lz4
 * s = "foo"
 * compressed_data = lz4.LZ4_compress(s)
 * cl = len(compressed_data)
 * f = open('foo.lz4', 'w')
 * # Skipping python's custom initial length header
 * f.write(compressed_data[4:len(compressed_data)])
 * f.flush()
 * f.close()
 *
 */
extern unsigned char FOO_LZ4[];
extern uint32_t FOO_LZ4_SIZE;

TEST(dmLZ4, Decompress)
{
    char buf[3];
    dmLZ4::Result r;
    int decompressed_size;
    r = dmLZ4::DecompressBuffer(FOO_LZ4, FOO_LZ4_SIZE, &buf, 3, &decompressed_size);
    ASSERT_EQ(dmLZ4::RESULT_OK, r);
    ASSERT_EQ(memcmp("foo", buf, 3), 0);
    ASSERT_EQ(3, decompressed_size);
}

TEST(dmLZ4, Compress)
{
    char compressed[50];
    char decompressed[532];

    dmLZ4::Result r;
    int compressed_size, decompressed_size;
    r = dmLZ4::CompressBuffer("bar", 3, &compressed, &compressed_size);
    ASSERT_EQ(dmLZ4::RESULT_OK, r);

    r = dmLZ4::DecompressBuffer(compressed, compressed_size, &decompressed, 3, &decompressed_size);
    ASSERT_EQ(dmLZ4::RESULT_OK, r);
    ASSERT_EQ(memcmp("bar", decompressed, 3), 0);
}

char * RandomCharArray(int max, int *real)
{
    char *tmp;
    uint32_t n = rand() % max;
    if (n == 0)
        n++;

    tmp = (char *)malloc(n + 1);

    for (uint32_t j = 0; j < n; ++j)
    {
        char c = 'a' + (rand() % 26);
        tmp[j] = c;
    }
    tmp[n] = 0;

    *real = n;

    return tmp;
}

TEST(dmLZ4, Stress)
{
    int max_size = 373425;

    srand(dmTime::GetTime());

    for (int i = 0; i < 100; ++i) {
        int ref_len;
        char *ref = RandomCharArray(max_size, &ref_len);
        ASSERT_NE(0, ref_len);
        int max_compressed_size, compressed_size;
        dmLZ4::Result r = dmLZ4::MaxCompressedSize(ref_len, &max_compressed_size);
        ASSERT_EQ(dmLZ4::RESULT_OK, r);

        char *compressed = (char *)malloc(max_compressed_size);
        ASSERT_TRUE(compressed != NULL);
        char *decompressed = (char *)malloc(ref_len);
        ASSERT_TRUE(decompressed != NULL);

        r = dmLZ4::CompressBuffer(ref, ref_len, compressed, &compressed_size);
        ASSERT_EQ(dmLZ4::RESULT_OK, r);

        int decompressed_size;
        r = dmLZ4::DecompressBuffer(compressed, compressed_size, decompressed, ref_len, &decompressed_size);
        ASSERT_EQ(dmLZ4::RESULT_OK, r);
        ASSERT_EQ(ref_len, decompressed_size);
        ASSERT_ARRAY_EQ_LEN(ref, decompressed, ref_len);

        // NOTE: If an assert fails above, we will get a mem leak report as well as
        //       the assert error. But it doesn't really matter for testing.
        free(decompressed);
        free(compressed);
        free(ref);
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
