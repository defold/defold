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
#include <stdlib.h>
#include <map>
#include <string>
#include "../dlib/atomic.h"
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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

class dmHttpServerTest: public jc_test_base_class
{
public:
    dmHttpServer::HServer m_Server;
    std::map<std::string, std::string> m_Headers;

    int m_Major, m_Minor;
    int m_ContentOffset;
    std::string m_RequestMethod, m_Resource;
    std::string m_Content;
    std::string m_ClientData;
    int32_atomic_t m_Quit;
    int32_atomic_t m_ServerStarted;

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
                char s[2] = { (char)('a' + (char)c), '\0' };
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
            dmSnPrintf(buf, sizeof(buf), "%d", c);
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
            dmAtomicStore32(&self->m_Quit, 1);
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
            dmSnPrintf(str_buf, sizeof(str_buf), "%llu", (unsigned long long)dmHashBuffer64(self->m_Content.c_str(), self->m_Content.size()));
            dmHttpServer::Send(request, str_buf, strlen(str_buf));
        }
        else
        {
            dmHttpServer::SetStatusCode(request, 404);
        }
    }

    static void ClientHttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length,
                                    uint32_t range_start, uint32_t range_end, uint32_t document_size,
                                    const char* method)
    {
        dmHttpServerTest* self = (dmHttpServerTest*) user_data;
        self->m_ClientData.append((const char*) content_data, content_data_size);
    }

#define T_ASSERT_LE(_A, _B) \
    if ( !((_A) < (_B)) ) { \
        printf("%s:%d: ASSERT: %s < %s: %d < %d", __FILE__, __LINE__, #_A, #_B, (int)(_A), (int)(_B)); \
    } \
    assert( (_A) < (_B) );

    static void ServerThread(void* user_data)
    {
        dmHttpServerTest* self = (dmHttpServerTest*) user_data;
        dmAtomicStore32(&self->m_ServerStarted, 1);
        int iter = 0;
        while (!dmAtomicGet32(&self->m_Quit) && iter < 10000)
        {
            dmHttpServer::Update(self->m_Server);
            dmTime::Sleep(1000 * 10);
            ++iter;
        }
        T_ASSERT_LE(iter, 10000);
    }

    void SetUp() override
    {
        m_Quit = 0;
        m_ServerStarted = 0;
        m_ClientData = "";
        dmHttpServer::NewParams params;
        params.m_ConnectionTimeout = 30;
        params.m_Userdata = this;
        params.m_HttpHeader = dmHttpServerTest::HttpHeader;
        params.m_HttpResponse = dmHttpServerTest::HttpResponse;
        dmHttpServer::Result result_server = dmHttpServer::New(&params, 8500, &m_Server);
        ASSERT_EQ(dmHttpServer::RESULT_OK, result_server);
    }

    void TearDown() override
    {
        if (m_Server)
            dmHttpServer::Delete(m_Server);
    }
};

class dmHttpServerParserTest: public jc_test_base_class
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

    void SetUp() override
    {
        m_Major = m_Minor = m_ContentOffset = -1;
    }

    void TearDown() override
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


#define HAS_SYSTEM_FUNCTION
#if defined(DM_NO_SYSTEM_FUNCTION)
    #undef HAS_SYSTEM_FUNCTION
#endif

#if defined(HAS_SYSTEM_FUNCTION)

void RunPythonThread(void* ctx)
{
    int* result = (int*)ctx;
    *result = system("python src/test/test_httpserver.py");
}

#if !defined(DM_SANITIZE_ADDRESS) // until we can load the dylibs properly

TEST_F(dmHttpServerTest, TestServer)
{
    int python_result = -1;
    dmThread::Thread thread = dmThread::New(RunPythonThread, 0x8000, &python_result, "test");
    int iter = 0;
    while (!dmAtomicGet32(&m_Quit) && iter < 1000)
    {
        dmHttpServer::Update(m_Server);
        dmTime::Sleep(1000 * 10);
        ++iter;
    }
    ASSERT_LE(iter, 1000);
    dmThread::Join(thread);
    ASSERT_EQ(0, python_result);
}

#endif
#endif // HAS_SYSTEM_FUNCTION

TEST_F(dmHttpServerTest, TestServerClient)
{
    dmThread::Thread thread = dmThread::New(&ServerThread, 0x8000, this, "test");

    while (!dmAtomicGet32(&m_ServerStarted))
    {
        dmTime::Sleep(10 * 1000);
    }

    dmURI::Parts uri;
    dmURI::Parse("http://127.0.0.1:8500", &uri);

    dmHttpClient::NewParams client_params;
    client_params.m_HttpContent = &ClientHttpContent;
    client_params.m_Userdata = this;
    dmHttpClient::HClient client = dmHttpClient::New(&client_params, &uri);

    dmHttpClient::Result r;
    m_ClientData = "";
    r = dmHttpClient::Get(client, "/mul/10/20");
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_STREQ("30", m_ClientData.c_str());

    for (int i = 0; i < 5000; i += 97)
    {
        char uri[64];
        dmSnPrintf(uri, sizeof(uri), "/respond_with_n/%d", i);
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
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSocket::Finalize();
    return ret;
}
