#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <string>
#define JC_TEST_IMPLEMENTATION
#include <jctest/test.h>
#include "../dlib/template.h"

const char* MapReplace(void* user_data, const char* key)
{
    std::map<std::string, std::string>* map = (std::map<std::string, std::string>*) user_data;
    if (map->find(key) != map->end())
    {
        return (*map)[key].c_str();
    }
    else
    {
        return 0;
    }
}

TEST(dmTemplate, Simple)
{
    char buf[1024];
    std::map<std::string, std::string> map;
    map["FOO"] = "foo";
    dmTemplate::Result r = dmTemplate::Format(&map, buf, sizeof(buf), "${FOO}", MapReplace);
    ASSERT_EQ(dmTemplate::RESULT_OK, r);
    ASSERT_STREQ("foo", buf);
}

TEST(dmTemplate, Missing)
{
    char buf[1024];
    std::map<std::string, std::string> map;
    map["FOO"] = "foo";
    dmTemplate::Result r = dmTemplate::Format(&map, buf, sizeof(buf), "${MISSING}", MapReplace);
    ASSERT_EQ(dmTemplate::RESULT_MISSING_REPLACEMENT, r);
}

TEST(dmTemplate, SyntaxError)
{
    char buf[1024];
    std::map<std::string, std::string> map;
    map["FOO"] = "foo";
    dmTemplate::Result r = dmTemplate::Format(&map, buf, sizeof(buf), "${FOO", MapReplace);
    ASSERT_EQ(dmTemplate::RESULT_SYNTAX_ERROR, r);
}

TEST(dmTemplate, TooSmallBuffer)
{
    char buf[1024];
    std::map<std::string, std::string> map;
    map["FOO"] = "foo";
    int i;
    for (i = 0; i < 4; ++i)
    {
        dmTemplate::Result r = dmTemplate::Format(&map, buf, i, "${FOO}", MapReplace);
        ASSERT_EQ(dmTemplate::RESULT_BUFFER_TOO_SMALL, r);
    }

    dmTemplate::Result r = dmTemplate::Format(&map, buf, i, "${FOO}", MapReplace);
    ASSERT_EQ(dmTemplate::RESULT_OK, r);
    ASSERT_STREQ("foo", buf);
}

std::string RandomString()
{
    std::string tmp;
    uint32_t n = rand() % 16;
    for (uint32_t j = 0; j < n; ++j)
    {
        char c = 'a' + (rand() % 26);
        tmp += c;
    }
    return tmp;
}

TEST(dmTemplate, Stress)
{
    char buf[1024];
    for (int i = 0; i < 32; ++i)
    {
        std::map<std::string, std::string> map;
        std::vector<std::string> keys;

        for (int j = 0; j < 8; ++j)
        {
            std::string key = RandomString();
            map[key] = RandomString();
            keys.push_back(key);
        }

        std::string fmt;
        for (int j = 0; j < 11; ++j)
        {
            if (rand() % 2 == 0)
            {
                fmt += RandomString();
            }
            else
            {
                fmt += "${";
                fmt += keys[rand() % keys.size()];
                fmt += "}";
            }
        }

        dmTemplate::Result r;
        int n = 0;
        do {
            r = dmTemplate::Format(&map, buf, n, fmt.c_str(), MapReplace);
            ++n;
        } while (r != dmTemplate::RESULT_OK);
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return JC_TEST_RUN_ALL();
}
