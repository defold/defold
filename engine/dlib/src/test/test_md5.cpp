#include <stdint.h>
#include <stdlib.h>
#define JC_TEST_IMPLEMENTATION
#include <jc/test.h>
#include "../dlib/md5.h"

static void ASSERT_MD5(uint8_t* expected, const dmMD5::Digest& d)
{
    for (int i = 0; i < sizeof(d.m_Digest); ++i) {
        ASSERT_EQ(expected[i], d.m_Digest[i]);
    }
}

TEST(dmMD5, Empty)
{
    dmMD5::State s;
    dmMD5::Digest d;
    dmMD5::Init(&s);
    dmMD5::Final(&s, &d);

    // Empty string
    uint8_t expected[] = { 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e };

    ASSERT_MD5(expected, d);
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
    ASSERT_MD5(expected, d);
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
    ASSERT_MD5(expected, d);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return JC_TEST_RUN_ALL();
}

