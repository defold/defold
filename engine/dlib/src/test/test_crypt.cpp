#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/crypt.h"

int jc_test_cmp_array(const uint8_t* a, const uint8_t* b, size_t len, const char* format, size_t offsetstride, const char* exprA, const char* exprB) {
    if (memcmp(a, b, sizeof(uint8_t)*len) == 0) return 1;
    const int max_num_to_display = 32; // max number of differing characters
    char stra[max_num_to_display*2+1] = {0};
    char strb[max_num_to_display*2+1] = {0};
    char diff[max_num_to_display*2+1] = {0};
    int erroroffset = 0;
    for (int i = 0; i < len; ++i) { // find the first difference
        if (a[i] != b[i]) {
            erroroffset = i;
            break;
        }
    }
    int start = (erroroffset - max_num_to_display/2); // to make it look good
    start -= start & 1;
    if (start<0)
        start = 0;
    int end = start + max_num_to_display;
    if (end > len)
        end = len;

    for(int i = start; i < end; ++i) {
        int offset = (i - start) * offsetstride;
        JC_TEST_SNPRINTF(stra+offset, sizeof(stra) - offset, format, (uint32_t)a[i]);
        JC_TEST_SNPRINTF(strb+offset, sizeof(strb) - offset, format, (uint32_t)b[i]);
        diff[offset+0] = (a[i] == b[i]) ? ' ' : '^';
        diff[offset+1] = diff[offset+0];
    }

    const char* emptyprefix = "   ";
    const char* ellipsis = "...";
    const char* prefix = start != 0 ? ellipsis : emptyprefix;
    const char* suffix = end < len ? ellipsis : emptyprefix;
    JC_TEST_LOGF(jc_test_get_fixture(), jc_test_get_test(), 0, JC_TEST_EVENT_ASSERT_FAILED, "\nValue of %s == %s\nOffset:      %d\nExpected: %s%s%s\nActual:   %s%s%s\nDiff:     %s%s\n", exprA, exprB, start, prefix, stra, suffix, prefix, strb, suffix, emptyprefix, diff);
    return 0;
}

template<size_t N>
int jc_test_cmp_ARRAY_EQ(const char (&a)[N], const char (&b)[N], const char* exprA, const char* exprB) {
    return jc_test_cmp_array((const uint8_t*)a, (const uint8_t*)b, N, "%c", 1, exprA, exprB);
}

template<size_t N, typename T>
int jc_test_cmp_ARRAY_EQ(T (&a)[N], T (&b)[N], const char* exprA, const char* exprB) {
    return jc_test_cmp_array(a, b, N, "%02x", 2, exprA, exprB);
}

#define ASSERT_ARRAY_EQ( A, B )  JC_ASSERT_TEST_OP( ARRAY_EQ, A, B, JC_TEST_FATAL_FAILURE )


TEST(dmCrypt, SameAsLibMCrypt)
{
    uint8_t buf[19];
    const char* s = "ABCDEFGH12345678XYZ";
    int n = strlen(s);
    memcpy(buf, s, n);

    uint8_t key[16] = {0};
    memcpy(key, "12345678abcdefgh", 16);

    Encrypt(dmCrypt::ALGORITHM_XTEA, buf, n, key, 16);
    uint8_t expected[] = { 0x81, 0xb4, 0xa1, 0x04, 0x2d, 0xac, 0xe5, 0xcb, 0x77,
                           0x89, 0xec, 0x11, 0x61, 0xc3, 0xdc, 0xfa, 0xb9, 0xa3, 0x25 };

    ASSERT_EQ(sizeof(expected), (size_t)n);
    ASSERT_ARRAY_EQ(expected, buf);
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
    uint8_t digest[16];
    const char* s = "This is a string";
    dmCrypt::HashMd5((const uint8_t*)s, strlen(s), digest);
    ASSERT_ARRAY_EQ(expected, digest);
}

TEST(dmCrypt, SHA1)
{
    uint8_t expected[] = {0xF7,0x20,0x17,0x48,0x5F,0xBF,0x64,0x23,0x49,0x9B,0xAF,0x9B,0x24,0x0D,0xAA,0x14,0xF5,0xF0,0x95,0xA1};
    uint8_t digest[20];
    const char* s = "This is a string";
    dmCrypt::HashSha1((const uint8_t*)s, strlen(s), digest);
    ASSERT_ARRAY_EQ(expected, digest);
}

TEST(dmCrypt, SHA256)
{
    uint8_t expected[] = {0x4E,0x95,0x18,0x57,0x54,0x22,0xC9,0x08,0x73,0x96,0x88,0x7C,0xE2,0x04,0x77,0xAB,0x5F,0x55,0x0A,0x4A,0xA3,0xD1,0x61,0xC5,0xC2,0x2A,0x99,0x6B,0x0A,0xBB,0x8B,0x35};
    uint8_t digest[32];
    const char* s = "This is a string";
    dmCrypt::HashSha256((const uint8_t*)s, strlen(s), digest);
    ASSERT_ARRAY_EQ(expected, digest);
}

TEST(dmCrypt, SHA512)
{
    uint8_t expected[] = {0xF4,0xD5,0x4D,0x32,0xE3,0x52,0x33,0x57,0xFF,0x02,0x39,0x03,0xEA,0xBA,0x27,0x21,0xE8,0xC8,0xCF,0xC7,0x70,0x26,0x63,0x78,0x2C,0xB3,0xE5,0x2F,0xAF,0x2C,0x56,0xC0,0x02,0xCC,0x30,0x96,0xB5,0xF2,0xB6,0xDF,0x87,0x0B,0xE6,0x65,0xD0,0x04,0x0E,0x99,0x63,0x59,0x0E,0xB0,0x2D,0x03,0xD1,0x66,0xE5,0x29,0x99,0xCD,0x1C,0x43,0x0D,0xB1};
    uint8_t digest[64];
    const char* s = "This is a string";
    dmCrypt::HashSha512((const uint8_t*)s, strlen(s), digest);
    ASSERT_ARRAY_EQ(expected, digest);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
