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
#include <stdio.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../dlib/pprint.h"

TEST(dmPPrint, Init)
{
    char buf[1024];
    dmPPrint::Printer p(buf, sizeof(buf));
    ASSERT_STREQ("", buf);
}

TEST(dmPPrint, Simple)
{
    char buf[1024];
    dmPPrint::Printer p(buf, sizeof(buf));
    p.Printf("%d", 1234);
    p.Printf("%d", 5678);
    ASSERT_STREQ("12345678", buf);
}

TEST(dmPPrint, NewLine)
{
    char buf[1024];
    dmPPrint::Printer p(buf, sizeof(buf));
    p.Printf("%d\n", 10);
    p.Printf("%d\n", 20);
    ASSERT_STREQ("10\n20\n", buf);
}

TEST(dmPPrint, Indent)
{
    char buf[1024];
    dmPPrint::Printer p(buf, sizeof(buf));
    p.SetIndent(2);
    p.Printf("%d\n", 10);
    p.Printf("%d\n", 20);
    ASSERT_STREQ("  10\n  20\n", buf);
}

TEST(dmPPrint, Truncate1)
{
    char buf[2] = { (char)0xff, (char)0xff };
    dmPPrint::Printer p(buf, sizeof(buf) - 1);
    p.Printf("%d", 1234);
    ASSERT_STREQ("", buf);
    ASSERT_EQ((char) 0xff, buf[1]);
}

TEST(dmPPrint, Truncate2)
{
    char buf[3] = { (char)0xff, (char)0xff, (char)0xff };
    dmPPrint::Printer p(buf, sizeof(buf) - 1);
    p.Printf("%d", 1234);
    ASSERT_STREQ("1", buf);
    ASSERT_EQ((char) 0xff, buf[2]);
}

TEST(dmPPrint, Truncate3)
{
    char buf[3] = { (char)0xff, (char)0xff, (char)0xff };
    dmPPrint::Printer p(buf, sizeof(buf) - 1);
    p.SetIndent(1);
    p.Printf("%d", 1234);
    ASSERT_STREQ(" ", buf);
    ASSERT_EQ((char) 0xff, buf[2]);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
