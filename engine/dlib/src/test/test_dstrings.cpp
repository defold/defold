// Copyright 2020-2022 The Defold Foundation
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
#include <errno.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <string.h>
#include "dlib/dstrings.h"

TEST(dmStrings, dmSnprintfEmpty)
{
    int res;
    char buffer[1];
#if !defined(__linux__) // The newer Gcc has a more strict printf checking (zero length format string)
    res = dmSnPrintf(0x0, 0, "");
    ASSERT_EQ(-1, res);
    res = dmSnPrintf(buffer, 1, "");
    ASSERT_EQ(0, res);
#endif
    res = dmSnPrintf(buffer, 1, 0x0);
    ASSERT_EQ(-1, res);
}

TEST(dmStrings, dmSnprintf)
{
    char buffer[4];
    const char* format = "%s";
    int res = dmSnPrintf(buffer, 4, format, "abc");
    ASSERT_EQ(3, res);
    ASSERT_EQ(0, buffer[3]);
}

TEST(dmStrings, dmSnprintfOverflow)
{
    char buffer[4];
    const char* format = "%s";
    int res = dmSnPrintf(buffer, 4, format, "abcd");
    ASSERT_EQ(-1, res);
    ASSERT_EQ(0, buffer[3]);
}

TEST(dmStrings, dmStrTok1)
{
    char* string = strdup("");
    char* s, *last;
    s = dmStrTok(string, " ", &last);
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrTok2)
{
    char* string = strdup("a");
    char* s, *last;
    s = dmStrTok(string, " ", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("a", s);

    s = dmStrTok(0, " ", &last);
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrTok3)
{
    char* string = strdup("a b c");
    char* s, *last;
    s = dmStrTok(string, " ", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("a", s);

    s = dmStrTok(0, " ", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("b", s);

    s = dmStrTok(0, " ", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("c", s);

    s = dmStrTok(0, " ", &last);
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrTok4)
{
    char* string = strdup("foo\r\nbar");
    char* s, *last;
    s = dmStrTok(string, "\r\n", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("foo", s);

    s = dmStrTok(0, "\r\n", &last);
    ASSERT_NE((void*) 0, s);
    ASSERT_STREQ("bar", s);

    s = dmStrTok(0, "\r\n", &last);
    ASSERT_EQ((void*) 0, s);

    free(string);
}

TEST(dmStrings, dmStrlCpy1)
{
    const char* src = "foo";
    char dst[1];
    dst[0] = 'x';

    dmStrlCpy(dst, src, 0);
    ASSERT_EQ('x', dst[0]);
}

TEST(dmStrings, dmStrlCpy2)
{
    const char* src = "foo";
    char dst[3];

    dmStrlCpy(dst, src, sizeof(dst));
    ASSERT_STREQ("fo", dst);
}

TEST(dmStrings, dmStrlCat1)
{
    const char* src = "foo";
    char dst[1];
    dst[0] = 'x';

    dmStrlCat(dst, src, 0);
    ASSERT_EQ('x', dst[0]);
}

TEST(dmStrings, dmStrlCat2)
{
    const char* src = "foo";
    char dst[3] = { 0 };

    dmStrlCat(dst, src, sizeof(dst));
    ASSERT_STREQ("fo", dst);
}

TEST(dmStrings, dmStrlCat3)
{
    const char* src = "foo";
    char dst[6];
    dst[0] = 'x';
    dst[1] = 'y';
    dst[2] = 'z';
    dst[3] = '\0';

    dmStrlCat(dst, src, sizeof(dst));
    ASSERT_STREQ("xyzfo", dst);
}

TEST(dmStrings, dmStrCaseCmp)
{
    ASSERT_GT(0, dmStrCaseCmp("a", "b"));
    ASSERT_LT(0, dmStrCaseCmp("b", "a"));
    ASSERT_EQ(0, dmStrCaseCmp("a", "a"));
}

TEST(dmStrings, dmStrError)
{
    char buf[128];
    // Test with a ENOENT errno code
    dmStrError(buf, sizeof(buf), ENOENT);
    ASSERT_STREQ("No such file or directory", buf);

    // Pass in a small buffer
    dmStrError(buf, 4, ENOENT);
    ASSERT_STREQ("No ", buf);

    // Pass invalid errno
    dmStrError(buf, sizeof(buf), -1);
    ASSERT_STREQ("Unknown error -1", buf);

    // Nothing set in buffer
    memset(buf, 1, sizeof(buf));
    dmStrError(buf, 0, 0);
    ASSERT_EQ(1, buf[0]);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
