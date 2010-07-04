#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <gtest/gtest.h>
#include "dlib/time.h"
#include "dlib/http_client.h"
#include "dlib/http_client_private.h"

class dmHttpClientTest: public ::testing::Test
{
public:
    dmHttpClient::HClient m_Client;
    std::map<std::string, std::string> m_Headers;
    std::string m_Content;
    int m_StatusCode;

    static void HttpHeader(dmHttpClient::HClient client, void* user_data, int status_code, const char* key, const char* value)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        self->m_Headers[key] = value;
    }

    static void HttpContent(dmHttpClient::HClient, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        self->m_StatusCode = status_code;
        self->m_Content.append((const char*) content_data, content_data_size);
    }

    virtual void SetUp()
    {
        dmHttpClient::NewParams params;
        params.m_Userdata = this;
        params.m_HttpContent = dmHttpClientTest::HttpContent;
        params.m_HttpHeader = dmHttpClientTest::HttpHeader;
        m_Client = dmHttpClient::New(&params, "localhost", 7000);
        ASSERT_NE((void*) 0, m_Client);
    }

    virtual void TearDown()
    {
        ASSERT_NE((void*) 0, m_Client);
        dmHttpClient::Delete(m_Client);
    }
};

class dmHttpClientParserTest: public ::testing::Test
{
public:
    std::map<std::string, std::string> m_Headers;

    int m_Major, m_Minor, m_Status;
    int m_ContentOffset;
    std::string m_StatusString;

    static void Version(void* user_data, int major, int minor, int status, const char* status_str)
    {
        dmHttpClientParserTest* self = (dmHttpClientParserTest*) user_data;
        self->m_Major = major;
        self->m_Minor = minor;
        self->m_Status = status;
        self->m_StatusString = status_str;
    }

    static void Header(void* user_data, const char* key, const char* value)
    {
        dmHttpClientParserTest* self = (dmHttpClientParserTest*) user_data;
        self->m_Headers[key] = value;
    }

    static void Content(void* user_data, int offset)
    {
        dmHttpClientParserTest* self = (dmHttpClientParserTest*) user_data;
        self->m_ContentOffset = offset;
    }

    dmHttpClientPrivate::ParseResult Parse(const char* headers)
    {
        char* h = strdup(headers);
        dmHttpClientPrivate::ParseResult r;
        r = dmHttpClientPrivate::ParseHeader(h, this,
                                             &dmHttpClientParserTest::Version,
                                             &dmHttpClientParserTest::Header,
                                             &dmHttpClientParserTest::Content);
        free(h);
        return r;
    }

    virtual void SetUp()
    {
        m_Major = m_Minor = m_Status = m_ContentOffset = -1;
        m_StatusString = "NOT SET!";
    }

    virtual void TearDown()
    {
    }
};

TEST_F(dmHttpClientParserTest, TestMoreData)
{
    const char* headers = "HTTP/1.1 200 OK\r\n";

    dmHttpClientPrivate::ParseResult r;
    r = Parse(headers);
    ASSERT_EQ(dmHttpClientPrivate::PARSE_RESULT_NEED_MORE_DATA, r);
    ASSERT_EQ(-1, m_Major);
    ASSERT_EQ(-1, m_Minor);
    ASSERT_EQ(-1, m_Status);
}

TEST_F(dmHttpClientParserTest, TestSyntaxError)
{
    const char* headers = "HTTP/x.y 200 OK\r\n\r\n";

    dmHttpClientPrivate::ParseResult r;
    r = Parse(headers);
    ASSERT_EQ(dmHttpClientPrivate::PARSE_RESULT_SYNTAX_ERROR, r);
}

TEST_F(dmHttpClientParserTest, TestMissingStatusString)
{
    const char* headers = "HTTP/1.0 200\r\n\r\n";

    dmHttpClientPrivate::ParseResult r;
    r = Parse(headers);
    ASSERT_EQ(dmHttpClientPrivate::PARSE_RESULT_SYNTAX_ERROR, r);
}

TEST_F(dmHttpClientParserTest, TestHeaders)
{
    const char* headers = "HTTP/1.1 200 OK\r\n"
"Content-Type: text/html;charset=UTF-8\r\n"
"Content-Length: 21\r\n"
"Server: Jetty(7.0.2.v20100331)\r\n"
"\r\n";

    dmHttpClientPrivate::ParseResult r;
    r = Parse(headers);
    ASSERT_EQ(dmHttpClientPrivate::PARSE_RESULT_OK, r);

    ASSERT_EQ(1, m_Major);
    ASSERT_EQ(1, m_Minor);
    ASSERT_EQ(200, m_Status);
    ASSERT_EQ("OK", m_StatusString);

    ASSERT_EQ("text/html;charset=UTF-8", m_Headers["Content-Type"]);
    ASSERT_EQ("21", m_Headers["Content-Length"]);
    ASSERT_EQ("Jetty(7.0.2.v20100331)", m_Headers["Server"]);
    ASSERT_EQ((size_t) 3, m_Headers.size());
}

TEST_F(dmHttpClientParserTest, TestContent)
{
    const char* headers = "HTTP/1.1 200 OK\r\n"
"\r\n"
"foo\r\n\r\nbar"
;

    dmHttpClientPrivate::ParseResult r;
    r = Parse(headers);
    ASSERT_EQ(dmHttpClientPrivate::PARSE_RESULT_OK, r);

    ASSERT_STREQ("foo\r\n\r\nbar", headers + m_ContentOffset);
}

TEST_F(dmHttpClientTest, Simple)
{
    char buf[128];

    for (int i = 0; i < 100; ++i)
    {
        m_Content = "";
        sprintf(buf, "/add/%d/1000", i);
        dmHttpClient::Result r;
        r = dmHttpClient::Get(m_Client, buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(1000 + i, strtol(m_Content.c_str(), 0, 10));
    }
}

TEST_F(dmHttpClientTest, Timeout1)
{
    for (int i = 0; i < 10; ++i)
    {
        dmHttpClient::Result r;
        m_Content = "";
        r = dmHttpClient::Get(m_Client, "/add/10/20");
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(30, strtol(m_Content.c_str(), 0, 10));

        // NOTE: MaxIdleTime is set to 100ms
        dmSleep(1000 * 150);

        m_Content = "";
        r = dmHttpClient::Get(m_Client, "/add/100/20");
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(120, strtol(m_Content.c_str(), 0, 10));
    }
}

TEST_F(dmHttpClientTest, Timeout2)
{
    dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_MAX_GET_RETRIES, 1);
    for (int i = 0; i < 10; ++i)
    {
        dmHttpClient::Result r;
        m_Content = "";
        r = dmHttpClient::Get(m_Client, "/add/10/20");
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(30, strtol(m_Content.c_str(), 0, 10));

        // NOTE: MaxIdleTime is set to 100ms
        dmSleep(1000 * 150);

        m_Content = "";
        r = dmHttpClient::Get(m_Client, "/add/100/20");
        ASSERT_NE(dmHttpClient::RESULT_OK, r);
        dmSocket::Result sock_r = dmHttpClient::GetLastSocketResult(m_Client);
        ASSERT_TRUE(sock_r == dmSocket::RESULT_CONNRESET || sock_r == dmSocket::RESULT_PIPE);
    }
}

TEST_F(dmHttpClientTest, ContentSizes)
{
    char buf[128];

    const uint32_t primes[] = { 0, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97 };
    for (int i = 0; i < 1000; i += 100)
    {
        for (uint32_t j = 0; j < sizeof(primes) / sizeof(primes[0]); ++j)
        {
            sprintf(buf, "/arb/%d", i + primes[j]);

            dmHttpClient::Result r;
            m_Content = "";
            r = dmHttpClient::Get(m_Client, buf);
            ASSERT_EQ(dmHttpClient::RESULT_OK, r);
            ASSERT_EQ(i + primes[j], m_Content.size());
            for (uint32_t k = 0; k < i + primes[j]; ++k)
            {
                ASSERT_EQ(k % 255, (uint32_t) (m_Content[k] & 0xff));
            }
        }
    }
}

TEST_F(dmHttpClientTest, LargeFile)
{
    char buf[128];

    int n = 1024 * 1024 + 59;
    sprintf(buf, "/arb/%d", n);
    dmHttpClient::Result r;
    m_Content = "";
    r = dmHttpClient::Get(m_Client, buf);
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_EQ((uint32_t) n, m_Content.size());

    for (uint32_t i = 0; i < (uint32_t) n; ++i)
    {
        ASSERT_EQ(i % 255, (uint32_t) (m_Content[i] & 0xff));
    }
}

TEST_F(dmHttpClientTest, TestHeaders)
{
    char buf[128];

    int n = 123;
    sprintf(buf, "/arb/%d", n);
    dmHttpClient::Result r;
    m_Content = "";
    r = dmHttpClient::Get(m_Client, buf);
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_EQ((uint32_t) n, m_Content.size());
    ASSERT_STREQ("123", m_Headers["Content-Length"].c_str());

}

TEST_F(dmHttpClientTest, Test400)
{
    for (int i = 0; i < 17; ++i)
    {
        dmHttpClient::Result r;
        m_Content = "";
        m_StatusCode = -1;
        r = dmHttpClient::Get(m_Client, "/does_not_exists");
        ASSERT_EQ(dmHttpClient::RESULT_NOT_200_OK, r);
        ASSERT_EQ(400, m_StatusCode);
    }
}

TEST(dmHttpClient, HostNotFound)
{
    dmHttpClient::NewParams params;
    dmHttpClient::HClient client = dmHttpClient::New(&params, "host_not_found", 7000);
    ASSERT_EQ((void*) 0, client);
}

TEST(dmHttpClient, ConnectionRefused)
{
    dmHttpClient::NewParams params;
    dmHttpClient::HClient client = dmHttpClient::New(&params, "localhost", 9999);
    ASSERT_NE((void*) 0, client);
    dmHttpClient::Result r = dmHttpClient::Get(client, "");
    ASSERT_EQ(dmHttpClient::RESULT_SOCKET_ERROR, r);
    ASSERT_EQ(dmSocket::RESULT_CONNREFUSED, dmHttpClient::GetLastSocketResult(client));
    dmHttpClient::Delete(client);
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}
