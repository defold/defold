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
#include <string>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/hash.h"

extern "C"
{
    int dmHashCTestIncremental(void);
    int dmHashCTestCloneAndRelease(void);
    int dmHashCTestReverseSafeAlloc(void);
}

class hash : public jc_test_base_class
{
};

TEST_F(hash, Hash)
{
    uint32_t h1 = dmHashBuffer32("foo", 3);
    uint64_t h2 = dmHashBuffer64("foo", 3);

    HashState32 hs32;
    dmHashInit32(&hs32, false);
    dmHashUpdateBuffer32(&hs32, "f", 1);
    dmHashUpdateBuffer32(&hs32, "o", 1);
    dmHashUpdateBuffer32(&hs32, "o", 1);
    uint32_t h1_i = dmHashFinal32(&hs32);

    HashState64 hs64;
    dmHashInit64(&hs64, false);
    dmHashUpdateBuffer64(&hs64, "f", 1);
    dmHashUpdateBuffer64(&hs64, "o", 1);
    dmHashUpdateBuffer64(&hs64, "o", 1);
    uint64_t h2_i = dmHashFinal64(&hs64);

    ASSERT_EQ(0xd861e2f7L, h1);
    ASSERT_EQ(0xd861e2f7L, h1_i);
    ASSERT_EQ(0x97b476b3e71147f7LL, h2);
    ASSERT_EQ(0x97b476b3e71147f7LL, h2_i);
}

TEST_F(hash, HashIncremental32)
{
    for (uint32_t i = 0; i < 1000; ++i)
    {
        std::string s;
        uint32_t n = rand() % 32 + 1;
        for (uint32_t j = 0; j < n; ++j)
        {
            char tmp[] = { (char)rand(), 0 };
            s += tmp;
        }
        uint32_t h1 = dmHashString32(s.c_str());

        HashState32 hs;
        dmHashInit32(&hs, false);
        dmHashUpdateBuffer32(&hs, s.c_str(), s.size());
        uint32_t h2 = dmHashFinal32(&hs);

        dmHashInit32(&hs, false);
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

TEST_F(hash, HashIncremental64)
{
    for (uint32_t i = 0; i < 1000; ++i)
    {
        std::string s;
        uint32_t n = rand() % 32 + 1;
        for (uint32_t j = 0; j < n; ++j)
        {
            char tmp[] = { (char)rand(), 0 };
            s += tmp;
        }
        uint64_t h1 = dmHashString64(s.c_str());

        HashState64 hs;
        dmHashInit64(&hs, false);
        dmHashUpdateBuffer64(&hs, s.c_str(), s.size());
        uint64_t h2 = dmHashFinal64(&hs);

        dmHashInit64(&hs, false);
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

TEST_F(hash, HashCAPI)
{
    ASSERT_EQ(0, dmHashCTestIncremental());
    ASSERT_EQ(0, dmHashCTestCloneAndRelease());
    ASSERT_EQ(0, dmHashCTestReverseSafeAlloc());
}

TEST_F(hash, HashReverseInvalidStateSlot)
{
    dmHashEnableReverseHash(true);

    HashState32 hs32;
    dmHashInit32(&hs32, false);
    hs32.m_ReverseHashEntryIndex = 0xffffffffu;
    dmHashUpdateBuffer32(&hs32, "foo", 3);
    ASSERT_EQ(0u, hs32.m_ReverseHashEntryIndex);

    HashState64 hs64;
    dmHashInit64(&hs64, false);
    hs64.m_ReverseHashEntryIndex = 0xffffffffu;
    dmHashUpdateBuffer64(&hs64, "foo", 3);
    ASSERT_EQ(0u, hs64.m_ReverseHashEntryIndex);

    HashState32 source32;
    dmHashInit32(&source32, false);
    source32.m_ReverseHashEntryIndex = 0xffffffffu;
    HashState32 clone32;
    dmHashClone32(&clone32, &source32, true);
    ASSERT_EQ(0u, clone32.m_ReverseHashEntryIndex);

    HashState64 source64;
    dmHashInit64(&source64, false);
    source64.m_ReverseHashEntryIndex = 0xffffffffu;
    HashState64 clone64;
    dmHashClone64(&clone64, &source64, true);
    ASSERT_EQ(0u, clone64.m_ReverseHashEntryIndex);

    HashState32 final32;
    dmHashInit32(&final32, false);
    final32.m_ReverseHashEntryIndex = 0xffffffffu;
    dmHashUpdateBuffer32(&final32, "foo", 3);
    ASSERT_EQ(dmHashBuffer32("foo", 3), dmHashFinal32(&final32));
    ASSERT_EQ(0u, final32.m_ReverseHashEntryIndex);

    HashState64 final64;
    dmHashInit64(&final64, false);
    final64.m_ReverseHashEntryIndex = 0xffffffffu;
    dmHashUpdateBuffer64(&final64, "foo", 3);
    ASSERT_EQ(dmHashBuffer64("foo", 3), dmHashFinal64(&final64));
    ASSERT_EQ(0u, final64.m_ReverseHashEntryIndex);

    HashState32 release32;
    dmHashInit32(&release32, false);
    release32.m_ReverseHashEntryIndex = 0xffffffffu;
    dmHashRelease32(&release32);
    ASSERT_EQ(0u, release32.m_ReverseHashEntryIndex);

    HashState64 release64;
    dmHashInit64(&release64, false);
    release64.m_ReverseHashEntryIndex = 0xffffffffu;
    dmHashRelease64(&release64);
    ASSERT_EQ(0u, release64.m_ReverseHashEntryIndex);
}

TEST_F(hash, HashReverseStaleStateSlot)
{
    dmHashEnableReverseHash(true);

    HashState32 stale32;
    dmHashInit32(&stale32, true);
    dmHashUpdateBuffer32(&stale32, "foo", 3);
    uint32_t stale_index32 = stale32.m_ReverseHashEntryIndex;
    dmHashRelease32(&stale32);
    stale32.m_ReverseHashEntryIndex = stale_index32;
    dmHashRelease32(&stale32);
    ASSERT_EQ(0u, stale32.m_ReverseHashEntryIndex);

    HashState64 stale64;
    dmHashInit64(&stale64, true);
    dmHashUpdateBuffer64(&stale64, "foo", 3);
    uint32_t stale_index64 = stale64.m_ReverseHashEntryIndex;
    dmHashRelease64(&stale64);
    stale64.m_ReverseHashEntryIndex = stale_index64;
    dmHashRelease64(&stale64);
    ASSERT_EQ(0u, stale64.m_ReverseHashEntryIndex);

    HashState32 reset32;
    dmHashInit32(&reset32, true);
    dmHashUpdateBuffer32(&reset32, "foo", 3);
    stale_index32 = reset32.m_ReverseHashEntryIndex;
    dmHashEnableReverseHash(false);
    dmHashEnableReverseHash(true);
    reset32.m_ReverseHashEntryIndex = stale_index32;
    ASSERT_EQ(dmHashBuffer32("foo", 3), dmHashFinal32(&reset32));
    ASSERT_EQ(0u, reset32.m_ReverseHashEntryIndex);

    HashState64 reset64;
    dmHashInit64(&reset64, true);
    dmHashUpdateBuffer64(&reset64, "foo", 3);
    stale_index64 = reset64.m_ReverseHashEntryIndex;
    dmHashEnableReverseHash(false);
    dmHashEnableReverseHash(true);
    reset64.m_ReverseHashEntryIndex = stale_index64;
    ASSERT_EQ(dmHashBuffer64("foo", 3), dmHashFinal64(&reset64));
    ASSERT_EQ(0u, reset64.m_ReverseHashEntryIndex);
}

TEST_F(hash, HashReverseStateResetWhenDisabled)
{
    dmHashEnableReverseHash(true);

    HashState32 update32;
    dmHashInit32(&update32, true);
    dmHashUpdateBuffer32(&update32, "foo", 3);
    ASSERT_NE(0u, update32.m_ReverseHashEntryIndex);
    dmHashEnableReverseHash(false);
    dmHashUpdateBuffer32(&update32, "bar", 3);
    ASSERT_EQ(0u, update32.m_ReverseHashEntryIndex);

    dmHashEnableReverseHash(true);
    HashState64 final64;
    dmHashInit64(&final64, true);
    dmHashUpdateBuffer64(&final64, "foo", 3);
    ASSERT_NE(0u, final64.m_ReverseHashEntryIndex);
    dmHashEnableReverseHash(false);
    ASSERT_EQ(dmHashBuffer64("foo", 3), dmHashFinal64(&final64));
    ASSERT_EQ(0u, final64.m_ReverseHashEntryIndex);

    dmHashEnableReverseHash(true);
    HashState64 source64;
    dmHashInit64(&source64, true);
    dmHashUpdateBuffer64(&source64, "foo", 3);
    ASSERT_NE(0u, source64.m_ReverseHashEntryIndex);
    dmHashEnableReverseHash(false);

    HashState64 clone64;
    dmHashClone64(&clone64, &source64, true);
    ASSERT_EQ(0u, clone64.m_ReverseHashEntryIndex);
    dmHashRelease64(&source64);
    ASSERT_EQ(0u, source64.m_ReverseHashEntryIndex);

    dmHashEnableReverseHash(true);
}

TEST_F(hash, HashReverseManyActiveStates)
{
    dmHashEnableReverseHash(true);

    const uint32_t count = 600;
    HashState32 states32[count];
    HashState64 states64[count];

    for (uint32_t i = 0; i < count; ++i)
    {
        dmHashInit32(&states32[i], true);
        dmHashUpdateBuffer32(&states32[i], "foo", 3);
        ASSERT_NE(0u, states32[i].m_ReverseHashEntryIndex);

        dmHashInit64(&states64[i], true);
        dmHashUpdateBuffer64(&states64[i], "foo", 3);
        ASSERT_NE(0u, states64[i].m_ReverseHashEntryIndex);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_EQ(dmHashBuffer32("foo", 3), dmHashFinal32(&states32[i]));
        ASSERT_EQ(0u, states32[i].m_ReverseHashEntryIndex);

        ASSERT_EQ(dmHashBuffer64("foo", 3), dmHashFinal64(&states64[i]));
        ASSERT_EQ(0u, states64[i].m_ReverseHashEntryIndex);
    }
}

TEST_F(hash, HashReverseReenableAfterExpandedStatePool)
{
    dmHashEnableReverseHash(true);

    const uint32_t count = 600;
    HashState64 states[count];

    for (uint32_t i = 0; i < count; ++i)
    {
        dmHashInit64(&states[i], true);
        dmHashUpdateBuffer64(&states[i], "foo", 3);
        ASSERT_NE(0u, states[i].m_ReverseHashEntryIndex);
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_EQ(dmHashBuffer64("foo", 3), dmHashFinal64(&states[i]));
        ASSERT_EQ(0u, states[i].m_ReverseHashEntryIndex);
    }

    dmHashEnableReverseHash(false);
    dmHashEnableReverseHash(true);

    HashState64 state;
    dmHashInit64(&state, true);
    dmHashUpdateBuffer64(&state, "bar", 3);
    ASSERT_EQ(dmHashBuffer64("bar", 3), dmHashFinal64(&state));
    ASSERT_EQ(0u, state.m_ReverseHashEntryIndex);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
