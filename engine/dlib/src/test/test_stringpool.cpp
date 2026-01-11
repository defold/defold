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
#include <string.h>
#include <time.h>

#include <string>
#include <map>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "dlib/stringpool.h"
#include "dlib/hash.h"

TEST(dmStringPool, Test01)
{
    dmStringPool::HPool pool = dmStringPool::New();

    std::map<const char*, std::string > strings;

    uint32_t n_allocated = 0;
    for (uint32_t i = 0; i < 20000; ++i)
    {
        std::string tmp;
        uint32_t n = rand() % 16;
        for (uint32_t j = 0; j < n; ++j)
        {
            char c = 'a' + (rand() % 26);
            tmp += c;
        }
        n_allocated += n;

        const char* s = dmStringPool::Add(pool, tmp.c_str(), tmp.length(), dmHashBufferNoReverse32(tmp.c_str(), tmp.length()));
        strings[s] = std::string(s);
    }

    std::map<const char*, std::string>::iterator iter;
    for (iter = strings.begin(); iter != strings.end(); ++iter)
    {
        const char*key = iter->first;
        const std::string& value = iter->second;

        ASSERT_STREQ(value.c_str(), key);

        // Assert that we get the same pointer back
        ASSERT_EQ((uintptr_t) key, (uintptr_t) dmStringPool::Add(pool, key, strlen(key), dmHashBufferNoReverse32(key, strlen(key))));
        ASSERT_EQ((uintptr_t) key, (uintptr_t) dmStringPool::Add(pool, value.c_str(), value.length(), dmHashBufferNoReverse32(value.c_str(), value.length())));
    }

    // Assert that at least 5 pages are allocated in string-pool
    ASSERT_GE(n_allocated, 5U * 4096U);

    dmStringPool::Delete(pool);
}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
