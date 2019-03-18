#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <string>
#include <map>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "dlib/stringpool.h"

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

        const char* s = dmStringPool::Add(pool, tmp.c_str());
        strings[s] = std::string(s);
    }

    std::map<const char*, std::string>::iterator iter;
    for (iter = strings.begin(); iter != strings.end(); ++iter)
    {
        const char*key = iter->first;
        const std::string& value = iter->second;

        ASSERT_STREQ(value.c_str(), key);

        // Assert that we get the same pointer back
        ASSERT_EQ((uintptr_t) key, (uintptr_t) dmStringPool::Add(pool, key));
        ASSERT_EQ((uintptr_t) key, (uintptr_t) dmStringPool::Add(pool, value.c_str()));
    }

    // Assert that at least 5 pages are allocated in string-pool
    ASSERT_GE(n_allocated, 5U * 4096U);

    dmStringPool::Delete(pool);
}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return JC_TEST_RUN_ALL();
}
