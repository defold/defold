#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <gtest/gtest.h>
#include "../dlib/hash.h"
#include "../dlib/log.h"

TEST(dlib, Hash)
{
    uint32_t h1 = dmHashBuffer32("foo", 3);
    uint64_t h2 = dmHashBuffer64("foo", 3);

    HashState32 hs32;
    dmHashInit32(&hs32);
    dmHashUpdateBuffer32(&hs32, "f", 1);
    dmHashUpdateBuffer32(&hs32, "o", 1);
    dmHashUpdateBuffer32(&hs32, "o", 1);
    uint32_t h1_i = dmHashFinal32(&hs32);

    HashState64 hs64;
    dmHashInit64(&hs64);
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
        dmHashInit32(&hs);
        dmHashUpdateBuffer32(&hs, s.c_str(), s.size());
        uint32_t h2 = dmHashFinal32(&hs);

        dmHashInit32(&hs);
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
        dmHashInit64(&hs);
        dmHashUpdateBuffer64(&hs, s.c_str(), s.size());
        uint64_t h2 = dmHashFinal64(&hs);

        dmHashInit64(&hs);
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

TEST(dlib, Log)
{
    dmLogWarning("Test warning message. Should have domain DLIB");
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

