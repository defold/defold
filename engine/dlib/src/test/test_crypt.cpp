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
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/crypt.h"


TEST(dmCrypt, XTea)
{
    uint8_t original[19];
    const char* s = "ABCDEFGH12345678XYZ";
    int n = strlen(s);
    memcpy(original, s, n);

    uint8_t buf[19];
    memcpy(buf, s, n);

    uint8_t key[16] = {0};
    memcpy(key, "12345678abcdefgh", 16);

    Encrypt(dmCrypt::ALGORITHM_XTEA, buf, n, key, 16);
    uint8_t expected[] = { 0x81, 0xb4, 0xa1, 0x04, 0x2d, 0xac, 0xe5, 0xcb, 0x77,
                           0x89, 0xec, 0x11, 0x61, 0xc3, 0xdc, 0xfa, 0xb9, 0xa3, 0x25 };
    ASSERT_EQ(sizeof(expected), (size_t)n);
    ASSERT_ARRAY_EQ(expected, buf);

    Decrypt(dmCrypt::ALGORITHM_XTEA, buf, n, key, 16);
    ASSERT_ARRAY_EQ(original, buf);
}

TEST(dmCrypt, XTeaUnaligned)
{
    const char* s = "ABCDEFGH12345678XYZ";
    int n = strlen(s);

    uint8_t storage[20];
    uint8_t* buf = storage + ((((uintptr_t) storage) & 7) == 0 ? 1 : 0);
    ASSERT_NE(0U, (uint32_t) (((uintptr_t) buf) & 7));
    memcpy(buf, s, n);

    uint8_t key[16] = {0};
    memcpy(key, "12345678abcdefgh", 16);

    Encrypt(dmCrypt::ALGORITHM_XTEA, buf, n, key, 16);
    uint8_t expected[] = { 0x81, 0xb4, 0xa1, 0x04, 0x2d, 0xac, 0xe5, 0xcb, 0x77,
                           0x89, 0xec, 0x11, 0x61, 0xc3, 0xdc, 0xfa, 0xb9, 0xa3, 0x25 };
    ASSERT_ARRAY_EQ_LEN(expected, buf, sizeof(expected));

    Decrypt(dmCrypt::ALGORITHM_XTEA, buf, n, key, 16);
    ASSERT_ARRAY_EQ_LEN(s, (const char*) buf, n);
}

TEST(dmCrypt, Random)
{
    uint8_t key[16];
    uint8_t guard[] = { 0xcc, 0xaa, 0xff, 0xee };

    for (int i = 0; i < 1025; i++) {
        uint8_t* buf = new uint8_t[2048];
        uint8_t* orig = new uint8_t[2048];

        for (int j = 0; j < i; j++) {
            orig[j] = buf[j] = rand() & 0xff;
        }
        for (int j = 0; j < 4; j++) {
            orig[i + j] = buf[i + j] = guard[j];
        }

        int keylen = rand() % 16;
        for (int k = 0; k < keylen; k++) {
            key[k] = rand() & 0xff;
        }

        Encrypt(dmCrypt::ALGORITHM_XTEA, buf, i, key, keylen);
        for (int j = 0; j < 4; j++) {
            ASSERT_EQ(orig[i + j], guard[j]);
            ASSERT_EQ(buf[i + j], guard[j]);
        }

        if (i > 0) {
            ASSERT_FALSE(memcmp(buf, orig, i) == 0);
        }

        Decrypt(dmCrypt::ALGORITHM_XTEA, buf, i, key, keylen);

        for (int j = 0; j < 4; j++) {
            ASSERT_EQ(orig[i + j], guard[j]);
            ASSERT_EQ(buf[i + j], guard[j]);
        }

        ASSERT_TRUE(memcmp(buf, orig, i) == 0);

        delete [] buf;
        delete [] orig;
    }
}


TEST(dmCrypt, MD5)
{
    uint8_t expected[] = {0x41,0xFB,0x5B,0x5A,0xE4,0xD5,0x7C,0x5E,0xE5,0x28,0xAD,0xB0,0x0E,0x5E,0x8E,0x74};
    uint8_t digest[16] = {0};
    const char* s = "This is a string";
    dmCrypt::HashMd5((const uint8_t*)s, strlen(s), digest);
    ASSERT_ARRAY_EQ(expected, digest);
}

TEST(dmCrypt, SHA1)
{
    uint8_t expected[] = {0xF7,0x20,0x17,0x48,0x5F,0xBF,0x64,0x23,0x49,0x9B,0xAF,0x9B,0x24,0x0D,0xAA,0x14,0xF5,0xF0,0x95,0xA1};
    uint8_t digest[20] = {0};
    const char* s = "This is a string";
    dmCrypt::HashSha1((const uint8_t*)s, strlen(s), digest);
    ASSERT_ARRAY_EQ(expected, digest);
}

TEST(dmCrypt, SHA256)
{
    uint8_t expected[] = {0x4E,0x95,0x18,0x57,0x54,0x22,0xC9,0x08,0x73,0x96,0x88,0x7C,0xE2,0x04,0x77,0xAB,0x5F,0x55,0x0A,0x4A,0xA3,0xD1,0x61,0xC5,0xC2,0x2A,0x99,0x6B,0x0A,0xBB,0x8B,0x35};
    uint8_t digest[32] = {0};
    const char* s = "This is a string";
    dmCrypt::HashSha256((const uint8_t*)s, strlen(s), digest);
    ASSERT_ARRAY_EQ(expected, digest);
}

// Empty input must produce the standard digest, with or without a zero-length update.
TEST(dmCrypt, SHA256StreamingEmpty)
{
    uint8_t expected[] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    for (uint32_t updates = 0; updates < 2; ++updates)
    {
        dmCrypt::HHashSha256 hash = dmCrypt::NewHashSha256();
        ASSERT_NE((dmCrypt::HHashSha256)0, hash);
        if (updates)
        {
            ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(hash, 0, 0));
        }
        uint8_t digest[32];
        ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::FinalHashSha256(hash, digest));
        dmCrypt::DeleteHashSha256(hash);
        ASSERT_ARRAY_EQ(expected, digest);
    }
}

// Splitting a standard SHA-256 vector must preserve partial blocks and final padding.
TEST(dmCrypt, SHA256StreamingChunkBoundaries)
{
    const char input[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    uint8_t expected[] = {
        0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
        0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
    };
    const uint32_t chunk_sizes[] = { 1, 7, 55, 56 };
    for (uint32_t i = 0; i < sizeof(chunk_sizes) / sizeof(chunk_sizes[0]); ++i)
    {
        dmCrypt::HHashSha256 hash = dmCrypt::NewHashSha256();
        ASSERT_NE((dmCrypt::HHashSha256)0, hash);
        uint32_t offset = 0;
        while (offset < sizeof(input) - 1)
        {
            uint32_t count = sizeof(input) - 1 - offset;
            if (count > chunk_sizes[i])
            {
                count = chunk_sizes[i];
            }
            ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(hash, (const uint8_t*)input + offset, count));
            ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(hash, 0, 0));
            offset += count;
        }
        uint8_t digest[32];
        ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::FinalHashSha256(hash, digest));
        dmCrypt::DeleteHashSha256(hash);
        ASSERT_ARRAY_EQ(expected, digest);
    }
}

// Recording-sized binary chunks and a short final chunk must hash the complete stream.
TEST(dmCrypt, SHA256StreamingRecordingChunks)
{
    uint8_t buffer[65536];
    for (uint32_t i = 0; i < sizeof(buffer); ++i)
    {
        buffer[i] = (uint8_t)i;
    }
    uint8_t expected[] = {
        0xb4, 0x93, 0x5a, 0x6c, 0xd2, 0x64, 0x74, 0x9c, 0xf4, 0xad, 0x85, 0x33, 0xda, 0xaa, 0xfb, 0x8c,
        0x8d, 0x7e, 0x4f, 0xdd, 0x23, 0xd9, 0xf4, 0x3a, 0x3e, 0xc1, 0xb9, 0x1d, 0x46, 0x2b, 0xc5, 0x54
    };
    dmCrypt::HHashSha256 hash = dmCrypt::NewHashSha256();
    ASSERT_NE((dmCrypt::HHashSha256)0, hash);
    ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(hash, buffer, sizeof(buffer)));
    ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(hash, buffer, sizeof(buffer)));
    ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(hash, buffer, 17));
    uint8_t digest[32];
    ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::FinalHashSha256(hash, digest));
    dmCrypt::DeleteHashSha256(hash);
    ASSERT_ARRAY_EQ(expected, digest);
}

// Interleaved hash operations must keep separate state, even when one is abandoned.
TEST(dmCrypt, SHA256StreamingIndependentContexts)
{
    uint8_t expected[] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    dmCrypt::HHashSha256 first = dmCrypt::NewHashSha256();
    dmCrypt::HHashSha256 second = dmCrypt::NewHashSha256();
    ASSERT_NE((dmCrypt::HHashSha256)0, first);
    ASSERT_NE((dmCrypt::HHashSha256)0, second);
    ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(first, (const uint8_t*)"a", 1));
    ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(second, (const uint8_t*)"discarded", 9));
    dmCrypt::DeleteHashSha256(second);
    ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::UpdateHashSha256(first, (const uint8_t*)"bc", 2));
    uint8_t digest[32];
    ASSERT_EQ(dmCrypt::RESULT_OK, dmCrypt::FinalHashSha256(first, digest));
    dmCrypt::DeleteHashSha256(first);
    dmCrypt::DeleteHashSha256(0);
    ASSERT_ARRAY_EQ(expected, digest);
}

TEST(dmCrypt, SHA512)
{
    uint8_t expected[] = {0xF4,0xD5,0x4D,0x32,0xE3,0x52,0x33,0x57,0xFF,0x02,0x39,0x03,0xEA,0xBA,0x27,0x21,0xE8,0xC8,0xCF,0xC7,0x70,0x26,0x63,0x78,0x2C,0xB3,0xE5,0x2F,0xAF,0x2C,0x56,0xC0,0x02,0xCC,0x30,0x96,0xB5,0xF2,0xB6,0xDF,0x87,0x0B,0xE6,0x65,0xD0,0x04,0x0E,0x99,0x63,0x59,0x0E,0xB0,0x2D,0x03,0xD1,0x66,0xE5,0x29,0x99,0xCD,0x1C,0x43,0x0D,0xB1};
    uint8_t digest[64] = {0};
    const char* s = "This is a string";
    dmCrypt::HashSha512((const uint8_t*)s, strlen(s), digest);
    ASSERT_ARRAY_EQ(expected, digest);
}


TEST(dmCrypt, Base64Encode)
{
    const char* source = "Lorem Ipsum";
    const char* expected = "TG9yZW0gSXBzdW0=";

    uint8_t dst[64];
    uint32_t dst_len = 0;
    bool result;
    result = dmCrypt::Base64Encode((const uint8_t*)source, strlen(source), dst, &dst_len);
    ASSERT_EQ(false, result);
    ASSERT_EQ(strlen(expected)+1, dst_len);

    dst_len = sizeof(dst);
    result = dmCrypt::Base64Encode((const uint8_t*)source, strlen(source), dst, &dst_len);
    ASSERT_EQ(true, result);
    ASSERT_EQ(strlen(expected), dst_len);
    ASSERT_ARRAY_EQ_LEN(expected, (const char*)dst, dst_len);
}


TEST(dmCrypt, Base64EncodeEmpty)
{
    uint8_t dst[64];
    memset(dst, 0xff, sizeof(dst));

    uint32_t dst_len = 0;
    bool result;
    result = dmCrypt::Base64Encode((const uint8_t*)"", 0, 0, &dst_len);
    ASSERT_EQ(false, result);
    ASSERT_EQ((uint32_t)1, dst_len); // room for the terminating null

    result = dmCrypt::Base64Encode((const uint8_t*)"", 0, dst, &dst_len);
    ASSERT_EQ(true, result);
    ASSERT_EQ((uint32_t)0, dst_len);
    ASSERT_EQ(0, dst[0]);
}


TEST(dmCrypt, Base64Decode)
{
    const char* source = "TG9yZW0gSXBzdW0=";
    const char* expected = "Lorem Ipsum";

    uint8_t dst[64];
    uint32_t dst_len = 0;
    bool result;
    result = dmCrypt::Base64Decode((const uint8_t*)source, strlen(source), dst, &dst_len);
    ASSERT_EQ(false, result);
    ASSERT_EQ(strlen(expected), dst_len);

    dst_len = sizeof(dst);
    result = dmCrypt::Base64Decode((const uint8_t*)source, strlen(source), dst, &dst_len);
    ASSERT_EQ(true, result);
    ASSERT_EQ(strlen(expected), dst_len);
    ASSERT_ARRAY_EQ_LEN(expected, (const char*)dst, dst_len);
}

TEST(dmCrypt, Base64DecodeEmpty)
{
    uint8_t dst[64];

    // Step 1: size query, as documented in dmsdk/dlib/crypt.h
    uint32_t dst_len = 0;
    bool result;
    result = dmCrypt::Base64Decode((const uint8_t*)"", 0, 0, &dst_len);
    ASSERT_EQ(true, result);
    ASSERT_EQ((uint32_t)0, dst_len);

    // Step 2: decode into a buffer sized from the query
    result = dmCrypt::Base64Decode((const uint8_t*)"", 0, dst, &dst_len);
    ASSERT_EQ(true, result);
    ASSERT_EQ((uint32_t)0, dst_len);

    // ... and into an oversized buffer
    dst_len = sizeof(dst);
    result = dmCrypt::Base64Decode((const uint8_t*)"", 0, dst, &dst_len);
    ASSERT_EQ(true, result);
    ASSERT_EQ((uint32_t)0, dst_len);
}

TEST(dmCrypt, Base64DecodeNoPadding)
{
    const char* source_no_pad = "TG9yZW0gSXBzdW0";
    const char* source_with_pad = "TG9yZW0gSXBzdW0=";
    const char* expected = "Lorem Ipsum";

    uint8_t dst1[64];
    uint8_t dst2[64];
    uint32_t dst_len1 = sizeof(dst1);
    uint32_t dst_len2 = sizeof(dst2);
    bool result1, result2;

    result1 = dmCrypt::Base64Decode((const uint8_t*)source_no_pad, strlen(source_no_pad), dst1, &dst_len1);
    result2 = dmCrypt::Base64Decode((const uint8_t*)source_with_pad, strlen(source_with_pad), dst2, &dst_len2);

    ASSERT_EQ(true, result1);
    ASSERT_EQ(true, result2);
    ASSERT_EQ(strlen(expected), dst_len1);
    ASSERT_EQ(strlen(expected), dst_len2);
    ASSERT_ARRAY_EQ_LEN(expected, (const char*)dst1, dst_len1);
    ASSERT_ARRAY_EQ_LEN(expected, (const char*)dst2, dst_len2);
}

TEST(dmCrypt, Base64DecodeNoPaddingJWT)
{
    const char* source = "YWJj";
    const char* expected = "abc";

    uint8_t dst[64];
    uint32_t dst_len = sizeof(dst);
    bool result = dmCrypt::Base64Decode((const uint8_t*)source, strlen(source), dst, &dst_len);

    ASSERT_EQ(true, result);
    ASSERT_EQ(strlen(expected), dst_len);
    ASSERT_ARRAY_EQ_LEN(expected, (const char*)dst, dst_len);

    const char* source2 = "YWI";
    const char* expected2 = "ab";

    dst_len = sizeof(dst);
    result = dmCrypt::Base64Decode((const uint8_t*)source2, strlen(source2), dst, &dst_len);

    ASSERT_EQ(true, result);
    ASSERT_EQ(strlen(expected2), dst_len);
    ASSERT_ARRAY_EQ_LEN(expected2, (const char*)dst, dst_len);

    const char* source3 = "YQ";
    const char* expected3 = "a";

    dst_len = sizeof(dst);
    result = dmCrypt::Base64Decode((const uint8_t*)source3, strlen(source3), dst, &dst_len);

    ASSERT_EQ(true, result);
    ASSERT_EQ(strlen(expected3), dst_len);
    ASSERT_ARRAY_EQ_LEN(expected3, (const char*)dst, dst_len);
}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
