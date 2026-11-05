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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dmsdk/dlib/hash.h>

typedef struct HashTestAllocator
{
    uint8_t m_Buffer[128];
    uint32_t m_Offset;
} HashTestAllocator;

static void* HashTestAlloc(size_t size, void* user_data)
{
    HashTestAllocator* allocator = (HashTestAllocator*) user_data;
    if ((allocator->m_Offset + size) > sizeof(allocator->m_Buffer))
    {
        return 0;
    }

    void* ptr = allocator->m_Buffer + allocator->m_Offset;
    allocator->m_Offset += (uint32_t) size;
    return ptr;
}

static void HashTestFree(void* mem, void* user_data)
{
    (void) mem;
    (void) user_data;
}

TEST(Hash, Incremental)
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

TEST(Hash, CloneAndRelease)
{
    HashState32 hs32;
    HashState32 hs32_clone;
    dmHashInit32(&hs32, false);
    dmHashUpdateBuffer32(&hs32, "foo", 3);
    dmHashClone32(&hs32_clone, &hs32, false);
    dmHashUpdateBuffer32(&hs32_clone, "bar", 3);

    ASSERT_EQ(dmHashBuffer32("foo", 3), dmHashFinal32(&hs32));
    ASSERT_EQ(dmHashBuffer32("foobar", 6), dmHashFinal32(&hs32_clone));

    HashState32 hs32_release;
    dmHashInit32(&hs32_release, false);
    dmHashUpdateBuffer32(&hs32_release, "foo", 3);
    dmHashRelease32(&hs32_release);
    ASSERT_EQ(dmHashBuffer32("foo", 3), dmHashFinal32(&hs32_release));

    HashState64 hs64;
    HashState64 hs64_clone;
    dmHashInit64(&hs64, false);
    dmHashUpdateBuffer64(&hs64, "foo", 3);
    dmHashClone64(&hs64_clone, &hs64, false);
    dmHashUpdateBuffer64(&hs64_clone, "bar", 3);

    ASSERT_EQ(dmHashBuffer64("foo", 3), dmHashFinal64(&hs64));
    ASSERT_EQ(dmHashBuffer64("foobar", 6), dmHashFinal64(&hs64_clone));

    HashState64 hs64_release;
    dmHashInit64(&hs64_release, false);
    dmHashUpdateBuffer64(&hs64_release, "foo", 3);
    dmHashRelease64(&hs64_release);
    ASSERT_EQ(dmHashBuffer64("foo", 3), dmHashFinal64(&hs64_release));
}

TEST(Hash, ReverseSafeAlloc)
{
    HashTestAllocator allocator_context;
    allocator_context.m_Offset = 0;

    dmAllocator allocator;
    allocator.m_Alloc = HashTestAlloc;
    allocator.m_Free = HashTestFree;
    allocator.m_UserData = &allocator_context;

    ASSERT_STREQ("<unknown>", dmHashReverseSafe64(0x123ULL));
    ASSERT_STREQ("<unknown>", dmHashReverseSafe32(0x123U));

    const char* unknown64 = dmHashReverseSafe64Alloc(&allocator, 0x123ULL);
    const char* unknown32 = dmHashReverseSafe32Alloc(&allocator, 0x123U);

    ASSERT_EQ(0, strncmp(unknown64, "<unknown:", 9));
    ASSERT_EQ(0, strncmp(unknown32, "<unknown:", 9));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
