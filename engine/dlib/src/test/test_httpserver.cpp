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
#include "../dlib/http_client.h"
#include "../dlib/hash.h"
#include "../dlib/network_constants.h"

class dmHttpServerTest: public ::testing::Test
{
public:
    dmHttpServer::HServer m_Server;
    std::map<std::string, std::string> m_Headers;

    int m_Major, m_Minor;
    int m_ContentOffset;
    std::string m_RequestMethod, m_Resource;
    std::string m_Content;
    volatile bool m_Quit;
    std::string m_ClientData;
    volatile bool m_ServerStarted;

    static void HttpHeader(void* user_data, const char* key, const char* value)
    {
        dmHttpServerTest* self = (dmHttpServerTest*) user_data;
        self->m_Headers[key] = value;
        self->m_Content.clear();
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

            std::string buf;
            for (int i = 0; i < n; ++i)
            {
                int c = (n + i*97) % ('z' - 'a');
                char s[2] = { 'a' + c, '\0' };
                buf.append(s, 1);
            }

            int sent_bytes = 0;
            while (n > 0)
            {
                int n_to_send = dmMath::Min(17, n);
                dmHttpServer::Send(request, buf.c_str() + sent_bytes, n_to_send);
                n -= n_to_send;
                sent_bytes += n_to_send;
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
        else if (strstr(self->m_Resource.c_str(), "/post"))
        {
            uint32_t total = 0;

            while (total < request->m_ContentLength)
            {
                // NOTE: Small buffer here to test buffer boundaries in dmHttpServer::Receive
                char recv_buf[17];
                uint32_t recv_bytes = 0;
                uint32_t to_recv = dmMath::Min((uint32_t) sizeof(recv_buf), (uint32_t) (request->m_ContentLength - total));
                dmHttpServer::Result r = dmHttpServer::Receive(request, recv_buf, to_recv, &recv_bytes);
                if (r != dmHttpServer::RESULT_OK)
                {
                    dmHttpServer::SetStatusCode(request, 500);
                    return;
                }
                total += recv_bytes;
                self->m_Content.append((const char*) recv_buf, recv_bytes);
            }

            char str_buf[32];
            DM_SNPRINTF(str_buf, sizeof(str_buf), "%llu", (unsigned long long)dmHashBuffer64(self->m_Content.c_str(), self->m_Content.size()));
            dmHttpServer::Send(request, str_buf, strlen(str_buf));
        }
        else
        {
            dmHttpServer::SetStatusCode(request, 404);
        }
    }

    static void ClientHttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        dmHttpServerTest* self = (dmHttpServerTest*) user_data;
        self->m_ClientData.append((const char*) content_data, content_data_size);
    }

    static void ServerThread(void* user_data)
    {
        dmHttpServerTest* self = (dmHttpServerTest*) user_data;
        self->m_ServerStarted = true;
        int iter = 0;
        while (!self->m_Quit && iter < 10000)
        {
            dmHttpServer::Update(self->m_Server);
            dmTime::Sleep(1000 * 10);
            ++iter;
        }
        ASSERT_LE(iter, 10000);
    }

    virtual void SetUp()
    {
        m_Quit = false;
        m_ServerStarted = false;
        m_ClientData = "";
        dmHttpServer::NewParams params;
        params.m_ConnectionTimeout = 30;
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

int g_PythonTestResult;
void RunPythonThread(void*)
{
#if !defined(DM_NO_SYSTEM)
    g_PythonTestResult = system("python src/test/test_httpserver.py");
#else
    g_PythonTestResult = -1;
#endif
}

TEST_F(dmHttpServerTest, TestServer)
{
    dmThread::Thread thread = dmThread::New(RunPythonThread, 0x8000, 0, "test");
    int iter = 0;
    while (!m_Quit && iter < 1000)
    {
        dmHttpServer::Update(m_Server);
        dmTime::Sleep(1000 * 10);
        ++iter;
    }
    ASSERT_LE(iter, 1000);
    dmThread::Join(thread);
    ASSERT_EQ(0, g_PythonTestResult);
}

TEST_F(dmHttpServerTest, TestServerClient)
{
    dmThread::Thread thread = dmThread::New(&ServerThread, 0x8000, this, "test");

    while (!m_ServerStarted)
    {
        dmTime::Sleep(10 * 1000);
    }

    dmHttpClient::NewParams client_params;
    client_params.m_HttpContent = &ClientHttpContent;
    client_params.m_Userdata = this;
    dmHttpClient::HClient client = dmHttpClient::New(&client_params, DM_LOOPBACK_ADDRESS_IPV4, 8500);

    dmHttpClient::Result r;
    m_ClientData = "";
    r = dmHttpClient::Get(client, "/mul/10/20");
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_STREQ("30", m_ClientData.c_str());

    for (int i = 0; i < 5000; i += 97)
    {
        char uri[64];
        DM_SNPRINTF(uri, sizeof(uri), "/respond_with_n/%d", i);
        m_ClientData = "";

        r = dmHttpClient::Get(client, uri);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);

        ASSERT_EQ(i, (int) m_ClientData.size());

        for (int j = 0; j < i; ++j)
        {
            int c = 'a' + ((i + j*97) % ('z' - 'a'));
            ASSERT_EQ((char) c, m_ClientData[j]);
        }
    }

    r = dmHttpClient::Get(client, "/quit");
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);

    dmHttpClient::Delete(client);

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
