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

TEST(dmURI, TestEncodeDecode)
{
    const char* uri = "http://my_domain.com/my spaced file.html";
    char enc_uri[DMPATH_MAX_PATH];
    dmURI::Encode(uri, enc_uri, DMPATH_MAX_PATH);
    ASSERT_EQ((void*)0, strchr(enc_uri, ' '));
    ASSERT_STRNE(uri, enc_uri);
    char dec_uri[DMPATH_MAX_PATH];
    dmURI::Decode(uri, dec_uri, DMPATH_MAX_PATH);
    ASSERT_NE((void*)0, strchr(dec_uri, ' '));
    ASSERT_STRNE(dec_uri, enc_uri);
    ASSERT_STREQ(uri, dec_uri);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
