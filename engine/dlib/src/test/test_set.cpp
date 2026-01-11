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

#include "dlib/set.h"

#include <stdint.h>
#include <stdlib.h> // rand
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <set>

TEST(dmSet, Constructor)
{
    dmSet<uint32_t> s;
    ASSERT_EQ(0u, s.Capacity());
    ASSERT_EQ(0u, s.Size());
    ASSERT_EQ(true, s.Full());
    ASSERT_EQ(true, s.Empty());
    ASSERT_EQ((uint32_t*)0, s.Begin());
    ASSERT_EQ((uint32_t*)0, s.End());
}

TEST(dmSet, Simple)
{
    dmSet<uint32_t> s;
    s.SetCapacity(1);
    ASSERT_EQ(1u, s.Capacity());
    s.OffsetCapacity(3);
    ASSERT_EQ(4u, s.Capacity());
    ASSERT_EQ(0u, s.Size());
    ASSERT_EQ(false, s.Full());

    for (uint32_t i = 0; i < s.Capacity(); ++i)
    {
        ASSERT_EQ(true, s.Add(10 + i));
        ASSERT_EQ(i+1, s.Size());
    }
    ASSERT_EQ(false, s.Empty());
    ASSERT_EQ(true, s.Full());


    for (uint32_t i = 0; i < s.Capacity(); ++i)
    {
        ASSERT_EQ(true, s.Contains(10 + i));
        ASSERT_EQ(false, s.Contains(i));
    }
}

TEST(dmSet, StressTest)
{
    for (uint32_t i = 0; i < 2; ++i)
    {
        uint32_t cap = i * 1000;

        std::set<uint32_t> scpp;
        dmSet<uint32_t> s;

        s.SetCapacity(cap);

        ASSERT_EQ(cap, s.Capacity());
        ASSERT_EQ(0u, s.Size());
        ASSERT_EQ(true, s.Empty());
        ASSERT_EQ(cap==0, s.Full());

        for (uint32_t k = 0; k < cap; ++k)
        {
            scpp.insert(k);
            s.Add(k);
        }

        // Compare the sets
        ASSERT_EQ(scpp.size(), s.Size());
        for (uint32_t k = 0; k < cap + 100; ++k)
        {
            std::set<uint32_t>::iterator iter = scpp.find(k);
            bool cpp_contains = iter != scpp.end();

            ASSERT_EQ(cpp_contains, s.Contains(k));
        }

        // Remove entries
        for (uint32_t k = 0; k < cap/2; ++k)
        {
            scpp.erase(k);
            s.Remove(k);
        }

        // Compare sets again
        ASSERT_EQ(scpp.size(), s.Size());
        for (uint32_t k = 0; k < cap + 100; ++k)
        {
            std::set<uint32_t>::iterator iter = scpp.find(k);
            bool cpp_contains = iter != scpp.end();

            ASSERT_EQ(cpp_contains, s.Contains(k));
        }
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
