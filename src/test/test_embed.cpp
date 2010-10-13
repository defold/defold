#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <gtest/gtest.h>
#include "../dlib/hash.h"
#include "../dlib/log.h"

extern const char TEST_EMBED[];

TEST(dlib, Embed)
{
    const char* test_embed = (const char*) TEST_EMBED;

    std::string test_embed_str(test_embed, strlen("embedded message"));
    ASSERT_STREQ("embedded message", test_embed_str.c_str());
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
