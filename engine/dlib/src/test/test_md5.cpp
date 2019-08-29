#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/md5.h"

int jc_test_cmp_MD5_EQ(const uint8_t a[DM_MD5_SIZE], const dmMD5::Digest& b, const char* exprA, const char* exprB) {
    if (memcmp(a, b.m_Digest, DM_MD5_SIZE) == 0) return 1;
    char stra[41] = {0};
    char strb[41] = {0};
    char diff[41] = {0};
    for(int i = 0; i < DM_MD5_SIZE; ++i) {
        int offset = i * 2;
        JC_TEST_SNPRINTF(stra+offset, sizeof(stra) - offset, "%02x", (uint32_t)a[i]);
        JC_TEST_SNPRINTF(strb+offset, sizeof(strb) - offset, "%02x", (uint32_t)b.m_Digest[i]);
        diff[offset+0] = (a[i] == b.m_Digest[i]) ? ' ' : '^';
        diff[offset+1] = diff[offset+0];
    }
    JC_TEST_LOGF(jc_test_get_fixture(), jc_test_get_test(), 0, JC_TEST_EVENT_ASSERT_FAILED, "\nValue of %s == %s\nExpected: %s\nActual:   %s\nDiff:     %s\n", exprA, exprB, stra, strb, diff);

    return 0;
}

#define ASSERT_MD5_EQ( A, B )  JC_ASSERT_TEST_OP( MD5_EQ, A, B, JC_TEST_FATAL_FAILURE )


TEST(dmMD5, Empty)
{
    dmMD5::State s;
    dmMD5::Digest d;
    dmMD5::Init(&s);
    dmMD5::Final(&s, &d);

    // Empty string
    uint8_t expected[] = { 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e };

    ASSERT_MD5_EQ(expected, d);
}

TEST(dmMD5, Simple)
{
    dmMD5::State s;
    dmMD5::Digest d;
    dmMD5::Init(&s);
    dmMD5::Update(&s, (const void*) "foo", 3);
    dmMD5::Final(&s, &d);

    // foo
    uint8_t expected[] = { 0xac, 0xbd, 0x18, 0xdb, 0x4c, 0xc2, 0xf8, 0x5c, 0xed, 0xef, 0x65, 0x4f, 0xcc, 0xc4, 0xa4, 0xd8 };
    ASSERT_MD5_EQ(expected, d);
}

TEST(dmMD5, Incremental)
{
    dmMD5::State s;
    dmMD5::Digest d;
    dmMD5::Init(&s);
    dmMD5::Update(&s, (const void*) "f", 1);
    dmMD5::Update(&s, (const void*) "o", 1);
    dmMD5::Update(&s, (const void*) "o", 1);
    dmMD5::Final(&s, &d);

    // foo
    uint8_t expected[] = { 0xac, 0xbd, 0x18, 0xdb, 0x4c, 0xc2, 0xf8, 0x5c, 0xed, 0xef, 0x65, 0x4f, 0xcc, 0xc4, 0xa4, 0xd8 };
    ASSERT_MD5_EQ(expected, d);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

