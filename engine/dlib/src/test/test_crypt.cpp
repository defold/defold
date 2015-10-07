#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <gtest/gtest.h>
#include "../dlib/crypt.h"

TEST(dmCrypt, SameAsLibMCrypt)
{
    uint8_t buf[256];
    const char* s = "ABCDEFGH12345678XYZ";
    int n = strlen(s);
    memcpy(buf, s, n);

    uint8_t key[16] = {0};
    memcpy(key, "12345678abcdefgh", 16);

    Encrypt(dmCrypt::ALGORITHM_XTEA, buf, n, key, 16);
    uint8_t expected[] = { 0x81, 0xb4, 0xa1, 0x4, 0x2d, 0xac, 0xe5, 0xcb, 0x77,
                           0x89, 0xec, 0x11, 0x61, 0xc3, 0xdc, 0xfa, 0xb9, 0xa3, 0x25 };

    ASSERT_EQ(sizeof(expected), n);

    for (uint32_t i = 0; i < sizeof(expected); i++) {
        ASSERT_EQ(expected[i], buf[i]);
    }
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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
