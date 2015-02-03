#include <stdint.h>
#include <gtest/gtest.h>
#include "dlib/uri.h"
#include "dlib/path.h"

TEST(dmURI, TestEmpty)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("file", uri_parts->m_Scheme);
    ASSERT_STREQ("", uri_parts->m_Location);
    ASSERT_EQ(-1, uri_parts->m_Port);
    ASSERT_STREQ("", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, TestAssumeFile1)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("/foo/bar", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("file", uri_parts->m_Scheme);
    ASSERT_STREQ("", uri_parts->m_Location);
    ASSERT_EQ(-1, uri_parts->m_Port);
    ASSERT_STREQ("/foo/bar", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, TestAssumeFile2)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("/c:/foo/bar", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("file", uri_parts->m_Scheme);
    ASSERT_STREQ("", uri_parts->m_Location);
    ASSERT_EQ(-1, uri_parts->m_Port);
    ASSERT_STREQ("/c:/foo/bar", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, TestFile1)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("file:///foo", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("file", uri_parts->m_Scheme);
    ASSERT_STREQ("", uri_parts->m_Location);
    ASSERT_STREQ("", uri_parts->m_Hostname);
    ASSERT_EQ(-1, uri_parts->m_Port);
    ASSERT_STREQ("/foo", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, TestFile2)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("file:foo", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("file", uri_parts->m_Scheme);
    ASSERT_STREQ("", uri_parts->m_Location);
    ASSERT_STREQ("", uri_parts->m_Hostname);
    ASSERT_EQ(-1, uri_parts->m_Port);
    ASSERT_STREQ("foo", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, TestFile3)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("file://foo", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("file", uri_parts->m_Scheme);
    ASSERT_STREQ("foo", uri_parts->m_Location);
    ASSERT_STREQ("foo", uri_parts->m_Hostname);
    ASSERT_EQ(-1, uri_parts->m_Port);
    ASSERT_STREQ("", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, TestFile4)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("file:/foo", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("file", uri_parts->m_Scheme);
    ASSERT_STREQ("", uri_parts->m_Location);
    ASSERT_STREQ("", uri_parts->m_Hostname);
    ASSERT_EQ(-1, uri_parts->m_Port);
    ASSERT_STREQ("/foo", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, Test1)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("http://foo.com/x", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("http", uri_parts->m_Scheme);
    ASSERT_STREQ("foo.com", uri_parts->m_Location);
    ASSERT_STREQ("foo.com", uri_parts->m_Hostname);
    ASSERT_EQ(80, uri_parts->m_Port);
    ASSERT_STREQ("/x", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, Test2)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("http:/foo.com/x", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("http", uri_parts->m_Scheme);
    ASSERT_STREQ("", uri_parts->m_Location);
    ASSERT_STREQ("", uri_parts->m_Hostname);
    ASSERT_EQ(80, uri_parts->m_Port);
    ASSERT_STREQ("/foo.com/x", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, Test3)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("http:foo.com/x", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("http", uri_parts->m_Scheme);
    ASSERT_STREQ("", uri_parts->m_Location);
    ASSERT_STREQ("", uri_parts->m_Hostname);
    ASSERT_EQ(80, uri_parts->m_Port);
    ASSERT_STREQ("foo.com/x", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, HTTPS)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("https://foo.com/x", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_EQ(443, uri_parts->m_Port);
    delete uri_parts;
}

TEST(dmURI, TestPort)
{
    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse("http://foo.com:123/x", uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("http", uri_parts->m_Scheme);
    ASSERT_STREQ("foo.com:123", uri_parts->m_Location);
    ASSERT_STREQ("foo.com", uri_parts->m_Hostname);
    ASSERT_EQ(123, uri_parts->m_Port);
    ASSERT_STREQ("/x", uri_parts->m_Path);
    delete uri_parts;
}

TEST(dmURI, TestOverflow1)
{
    char* buf = (char*) malloc(32*128);
    buf[0] = 0;

    strcat(buf, "http://foo.com/");
    for (int i = 0; i < 32*64; ++i)
        strcat(buf, "x");

    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse(buf, uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("http", uri_parts->m_Scheme);
    ASSERT_STREQ("foo.com", uri_parts->m_Location);

    for (uint32_t i = 1; i < sizeof(uri_parts->m_Path)-1; ++i)
        ASSERT_EQ('x', uri_parts->m_Path[i]);

    free((void*) buf);
    delete uri_parts;
}

TEST(dmURI, TestOverflow2)
{
    char* buf = (char*) malloc(32*128);
    buf[0] = 0;

    strcat(buf, "http://");
    for (int i = 0; i < 32*64; ++i)
        strcat(buf, "x");

    strcat(buf, "/path");

    dmURI::Parts* uri_parts = new dmURI::Parts();
    dmURI::Result r;
    r = dmURI::Parse(buf, uri_parts);
    ASSERT_EQ(dmURI::RESULT_OK, r);
    ASSERT_STREQ("http", uri_parts->m_Scheme);

    for (uint32_t i = 0; i < sizeof(uri_parts->m_Location)-1; ++i)
        ASSERT_EQ('x', uri_parts->m_Location[i]);

    ASSERT_STREQ("/path", uri_parts->m_Path);

    free((void*) buf);
    delete uri_parts;
}

TEST(dmURI, Escape)
{
    char src[1024];
    char dst[1024];
#define CPY_TO_BUF(s) strcpy(src, s);

    CPY_TO_BUF("")
    dmURI::Encode(src, dst, sizeof(dst));
    ASSERT_STREQ("", dst);

    CPY_TO_BUF(" ")
    dmURI::Encode(src, dst, sizeof(dst));
    ASSERT_STREQ("%20", dst);

    CPY_TO_BUF("foo")
    dmURI::Encode(src, dst, sizeof(dst));
    ASSERT_STREQ("foo", dst);

    CPY_TO_BUF("foo bar")
    dmURI::Encode(src, dst, sizeof(dst));
    ASSERT_STREQ("foo%20bar", dst);

    CPY_TO_BUF("to[0]")
    dmURI::Encode(src, dst, sizeof(dst));
    ASSERT_STREQ("to%5B0%5D", dst);

#undef CPY_TO_BUF
}

TEST(dmURI, EscapeBufferSize)
{
    char src[1024];
    char dst[1024];
#define CPY_TO_BUF(s) strcpy(src, s);

    CPY_TO_BUF("")
    dmURI::Encode(src, dst, 1);
    ASSERT_STREQ("", dst);

    dst[1] = '!';
    CPY_TO_BUF(" ")
    dmURI::Encode(src, dst, 1);
    ASSERT_EQ('!', dst[1]);

    dst[2] = '!';
    CPY_TO_BUF(" ")
    dmURI::Encode(src, dst, 2);
    ASSERT_EQ('!', dst[2]);

    dst[3] = '!';
    CPY_TO_BUF(" ")
    dmURI::Encode(src, dst, 3);
    ASSERT_EQ('!', dst[3]);

    dst[4] = '!';
    CPY_TO_BUF(" ")
    dmURI::Encode(src, dst, 4);
    ASSERT_STREQ("%20", dst);
    ASSERT_EQ('!', dst[4]);

#undef CPY_TO_BUF
}

TEST(dmURI, Unescape)
{
    char buf[1024];
#define CPY_TO_BUF(s) strcpy(buf, s);

    CPY_TO_BUF("")
    dmURI::Decode(buf, buf);
    ASSERT_STREQ("", buf);

    CPY_TO_BUF("foo")
    dmURI::Decode(buf, buf);
    ASSERT_STREQ("foo", buf);

    CPY_TO_BUF("%20")
    dmURI::Decode(buf, buf);
    ASSERT_STREQ(" ", buf);

    CPY_TO_BUF("fbconnect://success?request=575262345875717&to%5B0%5D=100006469467942&to%5B1%5D=100006622931864&to%5B2%5D=100006677888489&to%5B3%5D=100006751147236&to%5B4%5D=100006793533882")
    dmURI::Decode(buf, buf);
    ASSERT_STREQ("fbconnect://success?request=575262345875717&to[0]=100006469467942&to[1]=100006622931864&to[2]=100006677888489&to[3]=100006751147236&to[4]=100006793533882", buf);

    CPY_TO_BUF("foo+bar")
    dmURI::Decode(buf, buf);
    ASSERT_STREQ("foo bar", buf);

#undef CPY_TO_BUF
}


TEST(dmURI, TestEncodeDecode)
{
    const char* uri = "http://my_domain.com/my spaced file.html";
    char enc_uri[DMPATH_MAX_PATH];
    dmURI::Encode(uri, enc_uri, DMPATH_MAX_PATH);
    ASSERT_EQ((void*)0, strchr(enc_uri, ' '));
    ASSERT_STRNE(uri, enc_uri);
    char dec_uri[DMPATH_MAX_PATH];
    dmURI::Decode(uri, dec_uri);
    ASSERT_NE((void*)0, strchr(dec_uri, ' '));
    ASSERT_STRNE(dec_uri, enc_uri);
    ASSERT_STREQ(uri, dec_uri);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
