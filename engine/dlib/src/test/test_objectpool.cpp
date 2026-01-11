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
#include <time.h>

#include <string>
#include <map>
#include <set>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "dlib/object_pool.h"

struct Object
{
    uint32_t m_Value;
};

TEST(dmObjectPool, Test)
{
    dmObjectPool<Object> pool;
    const uint32_t n = 16;
    pool.SetCapacity(n);
    std::map<uint32_t, uint32_t> mapping;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t id = pool.Alloc();
        mapping[id] = i;
        Object* o = &pool.Get(id);
        o->m_Value = i;
    }
    ASSERT_TRUE(pool.Full());

    uint32_t sum = 0;
    uint32_t sum2 = 0;
    for (uint32_t i = 0; i < n; i++) {
        sum += pool.GetRawObjects()[i].m_Value;
    }
    for (std::map<uint32_t, uint32_t>::iterator i = mapping.begin(); i != mapping.end(); ++i) {
        sum2 += pool.Get(i->first).m_Value;
    }

    ASSERT_EQ((n - 1) * n / 2, sum);
    ASSERT_EQ(sum, sum2);

    pool.Free(12, true);
    pool.Free(5, true);
    mapping.erase(12);
    mapping.erase(5);
    ASSERT_FALSE(pool.Full());

    sum = 0;
    for (uint32_t i = 0; i < n - 2; i++) {
        sum += pool.GetRawObjects()[i].m_Value;
    }

    sum2 = 0;
    for (std::map<uint32_t, uint32_t>::iterator i = mapping.begin(); i != mapping.end(); ++i) {
        sum2 += pool.Get(i->first).m_Value;
    }

    ASSERT_EQ((n - 1) * n / 2 - 12 - 5, sum);
    ASSERT_EQ(sum, sum2);

    uint32_t id1 = pool.Alloc();
    uint32_t id2 = pool.Alloc();
    ASSERT_TRUE(pool.Full());
    pool.Get(id1).m_Value = 100;
    pool.Get(id2).m_Value = 200;
    mapping[id1] = 100;
    mapping[id2] = 200;

    sum = 0;
    for (uint32_t i = 0; i < n; i++) {
        sum += pool.GetRawObjects()[i].m_Value;
    }
    sum2 = 0;
    for (std::map<uint32_t, uint32_t>::iterator i = mapping.begin(); i != mapping.end(); ++i) {
        sum2 += pool.Get(i->first).m_Value;
    }

    ASSERT_EQ((n - 1) * n / 2 - 12 - 5 + 100 + 200, sum);
    ASSERT_EQ(sum, sum2);


    pool.SetCapacity(n + 2);
    id1 = pool.Alloc();
    id2 = pool.Alloc();
    ASSERT_TRUE(pool.Full());
    pool.Get(id1).m_Value = 1000;
    pool.Get(id2).m_Value = 2000;
    mapping[id1] = 1000;
    mapping[id2] = 2000;


    sum = 0;
    for (uint32_t i = 0; i < n + 2; i++) {
        sum += pool.GetRawObjects()[i].m_Value;
    }
    sum2 = 0;
    for (std::map<uint32_t, uint32_t>::iterator i = mapping.begin(); i != mapping.end(); ++i) {
        sum2 += pool.Get(i->first).m_Value;
    }

    ASSERT_EQ((n - 1) * n / 2 - 12 - 5 + 100 + 200 + 1000 + 2000, sum);
    ASSERT_EQ(sum, sum2);
}

TEST(dmObjectPool, ClearAll)
{
    dmObjectPool<Object> pool;
    const uint32_t n = 4;
    pool.SetCapacity(n);

    for (uint32_t i = 0; i < n; i++) {
        uint32_t id = pool.Alloc();
        Object* o = &pool.Get(id);
        o->m_Value = i;
    }
    ASSERT_TRUE(pool.Full());
    pool.Free(0, true);
    pool.Free(1, true);
    pool.Free(2, true);
    pool.Free(3, true);
    ASSERT_FALSE(pool.Full());
}

uint32_t foo(dmObjectPool<Object>& pool)
{
    return pool.Alloc();
}

TEST(dmObjectPool, Stress)
{
    dmObjectPool<Object> pool;
    const uint32_t n = 64;
    pool.SetCapacity(n);
    uint32_t next_value = 0;

    std::set<uint32_t> ids;

    for (uint32_t i = 0; i < n/2; i++) {
        uint32_t id = pool.Alloc();
        ids.insert(id);
        Object* o = &pool.Get(id);
        o->m_Value = next_value++;
    }

    for (uint32_t iter = 0; iter < 1000; iter++) {
        if (rand() % 2 == 0) {
            if (!pool.Full()) {
                uint32_t id = pool.Alloc();
                ids.insert(id);
                Object* o = &pool.Get(id);
                o->m_Value = next_value++;
            }
        } else {
            if (pool.Size() > 0) {
                uint32_t id = *ids.begin();
                pool.Free(id, true);
                //ids.erase(ids.find(id), ids.end());
                ids.erase(id);
            }
        }
    }
}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
