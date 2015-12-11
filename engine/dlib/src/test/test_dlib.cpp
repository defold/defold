#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <gtest/gtest.h>
#include "../dlib/hash.h"
#include "../dlib/log.h"

#include <dlib/sol.h>

TEST(dlib, Hash)
{
    uint32_t h1 = dmHashBuffer32("foo", 3);
    uint64_t h2 = dmHashBuffer64("foo", 3);

    HashState32 hs32;
    dmHashInit32(&hs32, true);
    dmHashUpdateBuffer32(&hs32, "f", 1);
    dmHashUpdateBuffer32(&hs32, "o", 1);
    dmHashUpdateBuffer32(&hs32, "o", 1);
    uint32_t h1_i = dmHashFinal32(&hs32);

    HashState64 hs64;
    dmHashInit64(&hs64, true);
    dmHashUpdateBuffer64(&hs64, "f", 1);
    dmHashUpdateBuffer64(&hs64, "o", 1);
    dmHashUpdateBuffer64(&hs64, "o", 1);
    uint64_t h2_i = dmHashFinal64(&hs64);

    ASSERT_EQ(0xd861e2f7L, h1);
    ASSERT_EQ(0xd861e2f7L, h1_i);
    ASSERT_EQ(0x97b476b3e71147f7LL, h2);
    ASSERT_EQ(0x97b476b3e71147f7LL, h2_i);
}

TEST(dlib, HashIncremental32)
{
    for (uint32_t i = 0; i < 1000; ++i)
    {
        std::string s;
        uint32_t n = rand() % 32 + 1;
        for (uint32_t j = 0; j < n; ++j)
        {
            char tmp[] = { rand() % 256, 0 };
            s += tmp;
        }
        uint32_t h1 = dmHashString32(s.c_str());

        HashState32 hs;
        dmHashInit32(&hs, true);
        dmHashUpdateBuffer32(&hs, s.c_str(), s.size());
        uint32_t h2 = dmHashFinal32(&hs);

        dmHashInit32(&hs, true);
        while (s.size() > 0)
        {
            int nchars = (rand() % s.size()) + 1;

            dmHashUpdateBuffer32(&hs, s.substr(0, nchars).c_str(), nchars);
            s = s.substr(nchars, s.size() - nchars);
        }
        uint32_t h3 = dmHashFinal32(&hs);

        ASSERT_EQ(h1, h2);
        ASSERT_EQ(h1, h3);
    }
}

TEST(dlib, HashIncremental64)
{
    for (uint32_t i = 0; i < 1000; ++i)
    {
        std::string s;
        uint32_t n = rand() % 32 + 1;
        for (uint32_t j = 0; j < n; ++j)
        {
            char tmp[] = { rand() % 256, 0 };
            s += tmp;
        }
        uint64_t h1 = dmHashString64(s.c_str());

        HashState64 hs;
        dmHashInit64(&hs, true);
        dmHashUpdateBuffer64(&hs, s.c_str(), s.size());
        uint64_t h2 = dmHashFinal64(&hs);

        dmHashInit64(&hs, true);
        while (s.size() > 0)
        {
            int nchars = (rand() % s.size()) + 1;

            dmHashUpdateBuffer64(&hs, s.substr(0, nchars).c_str(), nchars);
            s = s.substr(nchars, s.size() - nchars);
        }
        uint64_t h3 = dmHashFinal64(&hs);

        ASSERT_EQ(h1, h2);
        ASSERT_EQ(h1, h3);
    }
}

TEST(dlib, HashToString32)
{
    std::map<std::string, std::pair<uint32_t, uint32_t> > string_to_hash;

    for (uint32_t i = 0; i < 1000; ++i)
    {
        std::string s;
        uint32_t n = rand() % 32 + 1;
        for (uint32_t j = 0; j < n; ++j)
        {
            char tmp[] = { (rand() % ('z' - '0')) + '0', 0 };
            s += tmp;
        }

        uint32_t h = dmHashBuffer32(s.c_str(), s.size());
        string_to_hash[s] = std::pair<uint32_t, uint32_t>(h, s.size());
    }

    std::map<std::string, std::pair<uint32_t, uint32_t> >::const_iterator iter = string_to_hash.begin();
    while (iter != string_to_hash.end())
    {
        uint32_t len;
        const char* reverse = (const char*) dmHashReverse32(iter->second.first, &len);
        ASSERT_NE((void*) 0, reverse);
        ASSERT_EQ(iter->second.second, len);
        ASSERT_TRUE(memcmp(iter->first.c_str(), reverse, len) == 0);
        // Check that the buffer is null-terminated
        ASSERT_STREQ(iter->first.c_str(), reverse);
        ++iter;
    }
}

TEST(dlib, HashToString64)
{
    std::map<std::string, std::pair<uint64_t, uint32_t> > string_to_hash;

    for (uint32_t i = 0; i < 1000; ++i)
    {
        std::string s;
        uint32_t n = rand() % 32 + 1;
        for (uint32_t j = 0; j < n; ++j)
        {
            char tmp[] = { (rand() % ('z' - '0')) + '0', 0 };
            s += tmp;
        }

        uint64_t h = dmHashBuffer64(s.c_str(), s.size());
        string_to_hash[s] = std::pair<uint64_t, uint32_t>(h, s.size());
    }

    std::map<std::string, std::pair<uint64_t, uint32_t> >::const_iterator iter = string_to_hash.begin();
    while (iter != string_to_hash.end())
    {
        uint32_t len;
        const char* reverse = (const char*) dmHashReverse64(iter->second.first, &len);
        ASSERT_NE((void*) 0, reverse);
        ASSERT_EQ(iter->second.second, len);
        ASSERT_TRUE(memcmp(iter->first.c_str(), reverse, len) == 0);
        // Check that the buffer is null-terminated
        ASSERT_STREQ(iter->first.c_str(), reverse);
        ++iter;
    }
}

TEST(dlib, HashToStringIncremental32Simple)
{
    HashState32 hs;
    dmHashInit32(&hs, true);
    dmHashUpdateBuffer32(&hs, "foo", 3);
    dmHashUpdateBuffer32(&hs, "bar", 3);
    uint32_t h = dmHashFinal32(&hs);
    uint32_t len;
    const char* reverse = (const char*) dmHashReverse32(h, &len);
    ASSERT_EQ(6U, len);
    std::string reverse_string;
    reverse_string.append(reverse, len);
    ASSERT_STREQ("foobar", reverse_string.c_str());
}

TEST(dlib, HashToStringIncremental64Simple)
{
    HashState64 hs;
    dmHashInit64(&hs, true);
    dmHashUpdateBuffer64(&hs, "foo", 3);
    dmHashUpdateBuffer64(&hs, "bar", 3);
    uint64_t h = dmHashFinal64(&hs);
    uint32_t len;
    const char* reverse = (const char*) dmHashReverse64(h, &len);
    ASSERT_EQ(6U, len);
    std::string reverse_string;
    reverse_string.append(reverse, len);
    ASSERT_STREQ("foobar", reverse_string.c_str());
}

TEST(dlib, HashToStringIncremental32)
{
    std::map<std::string, std::pair<uint32_t, uint32_t> > string_to_hash;

    for (uint32_t i = 0; i < 1000; ++i)
    {
        std::string s;
        uint32_t n = rand() % 32 + 1;
        for (uint32_t j = 0; j < n; ++j)
        {
            char tmp[] = { (rand() % ('z' - '0')) + '0', 0 };
            s += tmp;
        }

        std::string s_copy = s;
        HashState32 hs;
        dmHashInit32(&hs, true);
        while (s.size() > 0)
        {
            int nchars = (rand() % s.size()) + 1;

            dmHashUpdateBuffer32(&hs, s.substr(0, nchars).c_str(), nchars);
            s = s.substr(nchars, s.size() - nchars);
        }
        uint32_t h = dmHashFinal32(&hs);
        string_to_hash[s_copy] = std::pair<uint32_t, uint32_t>(h, s_copy.size());
    }

    std::map<std::string, std::pair<uint32_t, uint32_t> >::const_iterator iter = string_to_hash.begin();
    while (iter != string_to_hash.end())
    {
        uint32_t len;
        const char* reverse = (const char*) dmHashReverse32(iter->second.first, &len);
        ASSERT_NE((void*) 0, reverse);
        uint32_t expected_len = iter->second.second;
        ASSERT_EQ(expected_len, len);
        std::string expected  = iter->first;
        ASSERT_TRUE(memcmp(expected.c_str(), reverse, len) == 0);
        // Check that the buffer is null-terminated
        ASSERT_STREQ(expected.c_str(), reverse);
        ++iter;
    }
}

TEST(dlib, HashToStringIncremental64)
{
    std::map<std::string, std::pair<uint64_t, uint32_t> > string_to_hash;

    for (uint32_t i = 0; i < 1000; ++i)
    {
        std::string s;
        uint32_t n = rand() % 32 + 1;
        for (uint32_t j = 0; j < n; ++j)
        {
            char tmp[] = { (rand() % ('z' - '0')) + '0', 0 };
            s += tmp;
        }

        std::string s_copy = s;
        HashState64 hs;
        dmHashInit64(&hs, true);
        while (s.size() > 0)
        {
            int nchars = (rand() % s.size()) + 1;

            dmHashUpdateBuffer64(&hs, s.substr(0, nchars).c_str(), nchars);
            s = s.substr(nchars, s.size() - nchars);
        }
        uint64_t h = dmHashFinal64(&hs);
        string_to_hash[s_copy] = std::pair<uint64_t, uint32_t>(h, s_copy.size());
    }

    std::map<std::string, std::pair<uint64_t, uint32_t> >::const_iterator iter = string_to_hash.begin();
    while (iter != string_to_hash.end())
    {
        uint32_t len;
        const char* reverse = (const char*) dmHashReverse64(iter->second.first, &len);
        ASSERT_NE((void*) 0, reverse);
        uint32_t expected_len = iter->second.second;
        ASSERT_EQ(expected_len, len);
        std::string expected  = iter->first;
        ASSERT_TRUE(memcmp(expected.c_str(), reverse, len) == 0);
        // Check that the buffer is null-terminated
        ASSERT_STREQ(expected.c_str(), reverse);
        ++iter;
    }
}

TEST(dlib, HashMaxReverse)
{
    char* buffer = (char*) malloc(DMHASH_MAX_REVERSE_LENGTH + 1);
    for (uint32_t i = 0; i < DMHASH_MAX_REVERSE_LENGTH + 1; ++i)
    {
        // NOTE: We hash value must be unique and differ other tests (reverse hashing test)
        // Therefore we add 10 here
        buffer[i] = (i + 10) % 255;
    }

    uint32_t h1 = dmHashBuffer32(buffer, DMHASH_MAX_REVERSE_LENGTH);
    uint64_t h2 = dmHashBuffer64(buffer, DMHASH_MAX_REVERSE_LENGTH);

    uint32_t h1_to_large = dmHashBuffer32(buffer, DMHASH_MAX_REVERSE_LENGTH + 1);
    uint64_t h2_to_large = dmHashBuffer64(buffer, DMHASH_MAX_REVERSE_LENGTH + 1);

    ASSERT_NE((const void*) 0, dmHashReverse32(h1, 0));
    ASSERT_NE((const void*) 0, dmHashReverse64(h2, 0));

    ASSERT_EQ((const void*) 0, dmHashReverse32(h1_to_large, 0));
    ASSERT_EQ((const void*) 0, dmHashReverse64(h2_to_large, 0));

    free((void*) buffer);
}

TEST(dlib, HashIncrementalReverse)
{
    char* buffer = (char*) malloc(DMHASH_MAX_REVERSE_LENGTH + 1);
    for (uint32_t i = 0; i < DMHASH_MAX_REVERSE_LENGTH + 1; ++i)
    {
        // NOTE: We hash value must be unique and differ other tests (reverse hashing test)
        // Therefore we add 11 here
        buffer[i] = (11 + i) % 255;
    }

    HashState32 state32;
    dmHashInit32(&state32, true);
    dmHashUpdateBuffer32(&state32, buffer, 16);
    dmHashUpdateBuffer32(&state32, buffer + 16, DMHASH_MAX_REVERSE_LENGTH - 16);
    uint32_t h1 = dmHashFinal32(&state32);

    HashState64 state64;
    dmHashInit64(&state64, true);
    dmHashUpdateBuffer64(&state64, buffer, 16);
    dmHashUpdateBuffer64(&state64, buffer + 16, DMHASH_MAX_REVERSE_LENGTH - 16);
    uint64_t h2 = dmHashFinal64(&state64);

    dmHashInit32(&state32, true);
    dmHashUpdateBuffer32(&state32, buffer, 16);
    dmHashUpdateBuffer32(&state32, buffer + 16, DMHASH_MAX_REVERSE_LENGTH + 1 - 16);
    uint32_t h1_to_large = dmHashFinal32(&state32);

    dmHashInit64(&state64, true);
    dmHashUpdateBuffer64(&state64, buffer, 16);
    dmHashUpdateBuffer64(&state64, buffer + 16, DMHASH_MAX_REVERSE_LENGTH + 1 - 16);
    uint64_t h2_to_large = dmHashFinal64(&state64);

    ASSERT_NE((const void*) 0, dmHashReverse32(h1, 0));
    ASSERT_NE((const void*) 0, dmHashReverse64(h2, 0));

    ASSERT_EQ((const void*) 0, dmHashReverse32(h1_to_large, 0));
    ASSERT_EQ((const void*) 0, dmHashReverse64(h2_to_large, 0));

    free((void*) buffer);
}

TEST(dlib, HashIncrementalNoReverse)
{
    char* buffer = (char*) malloc(DMHASH_MAX_REVERSE_LENGTH + 1);
    for (uint32_t i = 0; i < DMHASH_MAX_REVERSE_LENGTH + 1; ++i)
    {
        // NOTE: We hash value must be unique and differ other tests (reverse hashing test)
        // Therefore we add 12 here
        buffer[i] = (12 + i) % 255;
    }

    HashState32 state32;
    dmHashInit32(&state32, false);
    dmHashUpdateBuffer32(&state32, buffer, 16);
    dmHashUpdateBuffer32(&state32, buffer + 16, DMHASH_MAX_REVERSE_LENGTH - 16);
    uint32_t h1 = dmHashFinal32(&state32);

    HashState64 state64;
    dmHashInit64(&state64, false);
    dmHashUpdateBuffer64(&state64, buffer, 16);
    dmHashUpdateBuffer64(&state64, buffer + 16, DMHASH_MAX_REVERSE_LENGTH - 16);
    uint64_t h2 = dmHashFinal64(&state64);

    dmHashInit32(&state32, false);
    dmHashUpdateBuffer32(&state32, buffer, 16);
    dmHashUpdateBuffer32(&state32, buffer + 16, DMHASH_MAX_REVERSE_LENGTH + 1 - 16);
    uint32_t h1_to_large = dmHashFinal32(&state32);

    dmHashInit64(&state64, false);
    dmHashUpdateBuffer64(&state64, buffer, 16);
    dmHashUpdateBuffer64(&state64, buffer + 16, DMHASH_MAX_REVERSE_LENGTH + 1 - 16);
    uint64_t h2_to_large = dmHashFinal64(&state64);

    ASSERT_EQ((const void*) 0, dmHashReverse32(h1, 0));
    ASSERT_EQ((const void*) 0, dmHashReverse64(h2, 0));

    ASSERT_EQ((const void*) 0, dmHashReverse32(h1_to_large, 0));
    ASSERT_EQ((const void*) 0, dmHashReverse64(h2_to_large, 0));

    free((void*) buffer);
}

TEST(dlib, Log)
{
    dmLogWarning("Test warning message. Should have domain DLIB");
}

int main(int argc, char **argv)
{
    dmSol::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSol::FinalizeWithCheck();
    return ret;
}

