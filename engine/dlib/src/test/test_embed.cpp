#include <stdint.h>
#include <stdlib.h>
#include <string>
#include "testutil.h"
#include "../dlib/hash.h"
#include "../dlib/log.h"

extern unsigned char TEST_EMBED[];
extern uint32_t TEST_EMBED_SIZE;
extern unsigned char GENERATED_EMBED[];

TEST(dlib, Embed)
{
    const char* test_embed = (const char*) TEST_EMBED;
    const char* generated_embed = (const char*) GENERATED_EMBED;

    std::string test_embed_str(test_embed, strlen("embedded message"));
    ASSERT_STREQ("embedded message", test_embed_str.c_str());
    ASSERT_EQ(strlen("embedded message"), TEST_EMBED_SIZE);

    std::string generated_embed_str(generated_embed, strlen("generated data"));
    ASSERT_STREQ("generated data", generated_embed_str.c_str());
}
