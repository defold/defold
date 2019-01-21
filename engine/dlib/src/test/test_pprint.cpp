#include <stdint.h>
#include <stdio.h>
#include "testutil.h"
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
    char buf[2] = { 0xff, 0xff };
    dmPPrint::Printer p(buf, sizeof(buf) - 1);
    p.Printf("%d", 1234);
    ASSERT_STREQ("", buf);
    ASSERT_EQ((char) 0xff, buf[1]);
}

TEST(dmPPrint, Truncate2)
{
    char buf[3] = { 0xff, 0xff, 0xff };
    dmPPrint::Printer p(buf, sizeof(buf) - 1);
    p.Printf("%d", 1234);
    ASSERT_STREQ("1", buf);
    ASSERT_EQ((char) 0xff, buf[2]);
}

TEST(dmPPrint, Truncate3)
{
    char buf[3] = { 0xff, 0xff, 0xff };
    dmPPrint::Printer p(buf, sizeof(buf) - 1);
    p.SetIndent(1);
    p.Printf("%d", 1234);
    ASSERT_STREQ(" ", buf);
    ASSERT_EQ((char) 0xff, buf[2]);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
