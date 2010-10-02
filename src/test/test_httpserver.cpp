#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <gtest/gtest.h>
#include "../dlib/time.h"
#include "../dlib/socket.h"
#include "../dlib/math.h"
#include "../dlib/thread.h"
#include "../dlib/dstrings.h"
#include "../dlib/http_server.h"
#include "../dlib/http_server_private.h"


class dmHttpServerTest: public ::testing::Test
{
public:
    dmHttpServer::HServer m_Server;
    std::map<std::string, std::string> m_Headers;

    int m_Major, m_Minor;
    int m_ContentOffset;
    std::string m_RequestMethod, m_Resource;
    bool m_Quit;

    static void HttpHeader(void* user_data, const char* key, const char* value)
    {
        dmHttpServerTest* self = (dmHttpServerTest*) user_data;
        self->m_Headers[key] = value;
    }

    static void HttpResponse(void* user_data, const dmHttpServer::Request* request)
    {
        dmHttpServerTest* self = (dmHttpServerTest*) user_data;
        self->m_Major = request->m_Major;
        self->m_Minor = request->m_Minor;
        self->m_RequestMethod = request->m_Method;
        self->m_Resource = request->m_Resource;

        if (strstr(self->m_Resource.c_str(), "/respond_with_n/"))
        {
            int n;
            sscanf(self->m_Resource.c_str(), "/respond_with_n/%d", &n);
            char buf[17];
            for (uint32_t i = 0; i < sizeof(buf); ++i)
            {
                buf[i] = 'x';
            }

            while (n > 0)
            {
                int n_to_send = dmMath::Min((int) sizeof(buf), n);
                dmHttpServer::Send(request, buf, n_to_send);
                n -= n_to_send;
            }
        }
        else if (strstr(self->m_Resource.c_str(), "/mul/"))
        {
            int a,b;
            sscanf(self->m_Resource.c_str(), "/mul/%d/%d", &a,&b);
            int c = a + b;
            char buf[16];
            DM_SNPRINTF(buf, sizeof(buf), "%d", c);
            dmHttpServer::Send(request, buf, strlen(buf));
        }
        else if (strstr(self->m_Resource.c_str(), "/test_html"))
        {
            const char* html = "<html></html>";
            dmHttpServer::SendAttribute(request, "Content-Type", "text/html");
            dmHttpServer::Send(request, html, strlen(html));
        }
        else if (strstr(self->m_Resource.c_str(), "/quit"))
        {
            self->m_Quit = true;
        }
        else
        {
            dmHttpServer::SetStatusCode(request, 404);
        }
    }

    virtual void SetUp()
    {
        m_Quit = false;
        dmHttpServer::NewParams params;
        params.m_ConnectionTimeout = 20;
        params.m_Userdata = this;
        params.m_HttpHeader = dmHttpServerTest::HttpHeader;
        params.m_HttpResponse = dmHttpServerTest::HttpResponse;
        dmHttpServer::Result r = dmHttpServer::New(&params, 8500, &m_Server);
        ASSERT_EQ(dmHttpServer::RESULT_OK, r);
    }

    virtual void TearDown()
    {
        if (m_Server)
            dmHttpServer::Delete(m_Server);
    }
};

class dmHttpServerParserTest: public ::testing::Test
{
public:
    std::map<std::string, std::string> m_Headers;

    int m_Major, m_Minor;
    int m_ContentOffset;
    std::string m_RequestMethod, m_Resource;

    static void Request(void* user_data, const char* request_method, const char* resource, int major, int minor)
    {
        dmHttpServerParserTest* self = (dmHttpServerParserTest*) user_data;
        self->m_Major = major;
        self->m_Minor = minor;
        self->m_RequestMethod = request_method;
        self->m_Resource = resource;
    }

    static void Header(void* user_data, const char* key, const char* value)
    {
        dmHttpServerParserTest* self = (dmHttpServerParserTest*) user_data;
        self->m_Headers[key] = value;
    }

    static void Content(void* user_data, int offset)
    {
        dmHttpServerParserTest* self = (dmHttpServerParserTest*) user_data;
        self->m_ContentOffset = offset;
    }

    dmHttpServerPrivate::ParseResult Parse(const char* headers)
    {
        char* h = strdup(headers);
        dmHttpServerPrivate::ParseResult r;
        r = dmHttpServerPrivate::ParseHeader(h, this,
                                             &dmHttpServerParserTest::Request,
                                             &dmHttpServerParserTest::Header,
                                             &dmHttpServerParserTest::Content);
        free(h);
        return r;
    }

    virtual void SetUp()
    {
        m_Major = m_Minor = m_ContentOffset = -1;
    }

    virtual void TearDown()
    {
    }
};

TEST_F(dmHttpServerParserTest, TestMoreData)
{
    const char* headers = "GET / HTTP/1.0\r\n";

    dmHttpServerPrivate::ParseResult r;
    r = Parse(headers);
    ASSERT_EQ(dmHttpServerPrivate::PARSE_RESULT_NEED_MORE_DATA, r);
    ASSERT_EQ(-1, m_Major);
    ASSERT_EQ(-1, m_Minor);
}

TEST_F(dmHttpServerParserTest, TestSyntaxError)
{
    const char* headers = "GET / HTTP/x.y\r\n\r\n";

    dmHttpServerPrivate::ParseResult r;
    r = Parse(headers);
    ASSERT_EQ(dmHttpServerPrivate::PARSE_RESULT_SYNTAX_ERROR, r);
    ASSERT_EQ(-1, m_Major);
    ASSERT_EQ(-1, m_Minor);
}

TEST_F(dmHttpServerParserTest, TestHeaders)
{
    const char* headers = "GET / HTTP/1.1\r\n"
            "Connection: close\r\n"
            "Content-Length: 21\r\n"
            "Host: foobar\r\n"
            "\r\n";

    dmHttpServerPrivate::ParseResult r;
    r = Parse(headers);
    ASSERT_EQ(dmHttpServerPrivate::PARSE_RESULT_OK, r);
    ASSERT_EQ(1, m_Major);
    ASSERT_EQ(1, m_Minor);

    ASSERT_EQ("close", m_Headers["Connection"]);
    ASSERT_EQ("21", m_Headers["Content-Length"]);
    ASSERT_EQ("foobar", m_Headers["Host"]);
    ASSERT_EQ((size_t) 3, m_Headers.size());
}

void RunPythonThread(void*)
{
    system("python src/test/test_httpserver.py");
}

TEST_F(dmHttpServerTest, TestServer)
{
    dmThread::Thread thread = dmThread::New(RunPythonThread, 0x8000, 0);
    //ASSERT_EQ(0, ret);
    int iter = 0;
    while (!m_Quit && iter < 1000)
    {
        dmHttpServer::Update(m_Server);
        dmTime::Sleep(1000 * 10);
        ++iter;
    }
    ASSERT_LE(iter, 1000);
    dmThread::Join(thread);
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}
