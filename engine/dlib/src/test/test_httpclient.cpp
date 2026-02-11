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
#include "dlib/atomic.h"
#include "dlib/configfile.h"
#include "dlib/dstrings.h"
#include "dlib/time.h"
#include "dlib/log.h"
#include "dlib/math.h"
#include "dlib/thread.h"
#include "dlib/uri.h"
#include "dlib/sys.h"
#include "dlib/socket.h"
#include "dlib/sslsocket.h"
#include "dlib/http_client.h"
#include "dlib/http_cache_verify.h"
#include "dlib/testutil.h"

#define JC_TEST_IMPLEMENTATION
#if !defined(JC_TEST_NO_DEATH_TEST)
    #define JC_TEST_NO_DEATH_TEST
#endif
#include <jc_test/jc_test.h>

#if !defined(DM_NO_SIGNAL_FUNCTION)
    #include <signal.h>
#endif

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, dmHttpClient::Result r) {
    return buffer + dmSnPrintf(buffer, buffer_len, "%s", dmHttpClient::ResultToString(r));
}

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, dmSocket::Result r) {
    return buffer + dmSnPrintf(buffer, buffer_len, "%s", dmSocket::ResultToString(r));
}


#if defined(_WIN32) || (defined(GITHUB_CI) && defined(__MACH__))
#ifndef DM_DISABLE_HTTPCLIENT_TESTS
    #define DM_DISABLE_HTTPCLIENT_TESTS
#endif
#endif


int g_HttpPort = -1;
int g_HttpPortSSL = -1;
int g_HttpPortSSLTest = -1;
char SERVER_IP[64] = "localhost";

#define NAME_SERVER_IP "localhost"
#define NAME_SOCKET "{server_socket}"
#define NAME_SOCKET_SSL "{server_socket_ssl}"
#define NAME_SOCKET_SSL_TEST "{server_socket_ssl_test}"

class dmHttpClientTest: public jc_test_params_class<const char*>
{
public:
    dmHttpClient::HClient m_Client;
    std::map<std::string, std::string> m_Headers;
    std::string m_Content;
    std::string m_ToPost;
    int m_StatusCode;
    int m_XScale;
    uint32_t m_RangeStart;
    uint32_t m_RangeEnd;
    uint32_t m_DocumentSize;
    dmURI::Parts m_URI;
    char m_CacheDir[256];

    static void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        self->m_Headers[key] = value;
    }

    static void HttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length,
                            uint32_t range_start, uint32_t range_end, uint32_t document_size,
                            const char* method)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        self->m_StatusCode = status_code;
        self->m_RangeStart = range_start;
        self->m_RangeEnd = range_end;
        self->m_DocumentSize = document_size;
        self->m_Content.append((const char*) content_data, content_data_size);
    }

    static uint32_t HttpSendContentLength(dmHttpClient::HResponse response, void* user_data)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        const char* s = self->m_ToPost.c_str();
        return strlen(s);
    }

    static dmHttpClient::Result HttpWrite(dmHttpClient::HResponse response, uint32_t offset, uint32_t length, void* user_data)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        const char* s = self->m_ToPost.c_str();
        uint32_t len = dmMath::Min((uint32_t)self->m_ToPost.size() - offset, length);
        return dmHttpClient::Write(response, &s[offset], len);
    }

    static dmHttpClient::Result HttpWriteHeaders(dmHttpClient::HResponse response, void* user_data)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        char scale[128];
        dmSnPrintf(scale, sizeof(scale), "%d", self->m_XScale);
        self->m_Headers["X-Scale"] = std::string(scale);

        // Let's use the m_Headers for writing as well.
        std::map<std::string, std::string>::iterator it;
        for (it = self->m_Headers.begin(); it != self->m_Headers.end(); it++)
        {
            dmHttpClient::Result result = dmHttpClient::WriteHeader(response, it->first.c_str(), it->second.c_str());
            if (result != dmHttpClient::RESULT_OK)
            {
                dmLogError("Failed to write header '%s=%s'", it->first.c_str(), it->second.c_str());
                return result;
            }
        }

        self->m_Headers.clear(); // Prepare for receiving
        return dmHttpClient::RESULT_OK;
    }

    dmHttpClient::Result HttpGet(const char* url)
    {
        m_StatusCode = -1;
        m_RangeStart = 0xFFFFFFFF;
        m_RangeEnd = 0xFFFFFFFF;
        m_DocumentSize = 0xFFFFFFFF;
        m_Content.clear();
        m_Headers.clear();
        dmHttpClient::SetCacheKey(m_Client, url);
        return dmHttpClient::Get(m_Client, url);
    }

    dmHttpClient::Result HttpGetPartial(const char* url, int start, int end)
    {
        m_StatusCode = -1;
        m_RangeStart = 0xFFFFFFFF;
        m_RangeEnd = 0xFFFFFFFF;
        m_DocumentSize = 0xFFFFFFFF;
        m_Content.clear();
        m_Headers.clear();

        if (start >= 0 && end >= start)
        {
            char range[512] = "";
            dmSnPrintf(range, sizeof(range), "bytes=%d-%d", start, end);
            m_Headers["Range"] = range;

            char cache_key[1024] = "";
            dmSnPrintf(cache_key, sizeof(cache_key), "%s=%s", url, range);
            dmHttpClient::SetCacheKey(m_Client, cache_key);
        }

        return dmHttpClient::Get(m_Client, url);
    }

    dmHttpClient::Result HttpPost(const char* url)
    {
        m_StatusCode = -1;
        m_RangeStart = 0xFFFFFFFF;
        m_RangeEnd = 0xFFFFFFFF;
        m_DocumentSize = 0xFFFFFFFF;
        m_Content.clear();
        m_Headers.clear();
        return dmHttpClient::Post(m_Client, url);
    }

    void CreateFile(const char* path, const char* content)
    {
        char apppath[256];
        dmTestUtil::MakeHostPath(apppath, sizeof(apppath), path);

        FILE* f = fopen(apppath, "wb");
        ASSERT_NE((FILE*)0, f);
        fwrite(content, 1, strlen(content), f);
        fclose(f);
    }

    void SetupTestData()
    {
        char tmp_dir[256];
        char http_files[256];
        dmTestUtil::MakeHostPath(tmp_dir, sizeof(tmp_dir), "tmp");
        dmTestUtil::MakeHostPath(m_CacheDir, sizeof(m_CacheDir), "tmp/cache");
        dmTestUtil::MakeHostPath(http_files, sizeof(http_files), "tmp/http_files");

        dmSys::RmTree(tmp_dir);

        dmSys::Mkdir(tmp_dir, 0755);
        dmSys::Mkdir(m_CacheDir, 0755);
        dmSys::Mkdir(http_files, 0755);

        CreateFile("tmp/http_files/a.txt", "You will find this data in a.txt and d.txt");
        CreateFile("tmp/http_files/b.txt", "Some data in file b");
        CreateFile("tmp/http_files/c.txt", "Some data in file c");
        CreateFile("tmp/http_files/d.txt", "You will find this data in a.txt and d.txt");
    }

    void SetUp() override
    {
        m_Client = 0;

        char url[1024];
        strcpy(url, GetParam());

        bool is_https = strstr(url, "https://") != 0;

        char* host = strstr(url, NAME_SERVER_IP);
        if (host != 0) {
            char urlcopy[1024];
            // poor mans replace
            const char* rest = host + strlen(NAME_SERVER_IP);
            dmSnPrintf(urlcopy, sizeof(urlcopy), "%s://%s%s", is_https?"https":"http", SERVER_IP, rest);
            memcpy(url, urlcopy, sizeof(url));
        }

        char* portstr = strrchr(url, ':');
        ++portstr;

        int port = -1;
        if( strcmp(NAME_SOCKET, portstr) == 0 )
        {
            port = g_HttpPort;
        }
        else if( strcmp(NAME_SOCKET_SSL, portstr) == 0 )
        {
            port = g_HttpPortSSL;
        }
        else if( strcmp(NAME_SOCKET_SSL_TEST, portstr) == 0 )
        {
            port = g_HttpPortSSLTest;
        }
        else {
            // no port
        }

        if (port != -1)
        {
            dmSnPrintf(portstr, sizeof(url) - (portstr - url), "%d", port);
        }


        dmURI::Result parse_r = dmURI::Parse(url, &m_URI);
        ASSERT_EQ(dmURI::RESULT_OK, parse_r);

        SetupTestData();

        dmHttpClient::NewParams params;
        params.m_Userdata = this;
        params.m_HttpContent = dmHttpClientTest::HttpContent;
        params.m_HttpHeader = dmHttpClientTest::HttpHeader;
        params.m_HttpSendContentLength = dmHttpClientTest::HttpSendContentLength;
        params.m_HttpWrite = dmHttpClientTest::HttpWrite;
        params.m_HttpWriteHeaders = dmHttpClientTest::HttpWriteHeaders;
        m_Client = dmHttpClient::New(&params, &m_URI, 0);
        ASSERT_NE((void*) 0, m_Client);

        m_XScale = 1;
        m_StatusCode = -1;
    }

    void TearDown() override
    {
        if (m_Client)
            dmHttpClient::Delete(m_Client);
    }
};

class dmHttpClientTestSSL : public dmHttpClientTest
{
    // for jctest
};

class dmHttpClientTestExternal : public dmHttpClientTest
{
    // for jctest
};

class dmHttpClientParserTest: public jc_test_base_class
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

    dmHttpClient::ParseResult Parse(const char* headers, bool end_of_receive)
    {
        char* h = strdup(headers);
        dmHttpClient::ParseResult r;
        r = dmHttpClient::ParseHeader(h, this, end_of_receive,
                                     &dmHttpClientParserTest::Version,
                                     &dmHttpClientParserTest::Header,
                                     &dmHttpClientParserTest::Content);
        free(h);
        return r;
    }

    void SetUp() override
    {
        m_Major = m_Minor = m_Status = m_ContentOffset = -1;
        m_StatusString = "NOT SET!";
    }

    void TearDown() override
    {
    }
};

TEST_F(dmHttpClientParserTest, TestMoreData)
{
    const char* headers = "HTTP/1.1 200 OK\r\n";

    dmHttpClient::ParseResult r;
    r = Parse(headers, false);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_NEED_MORE_DATA, r);
    ASSERT_EQ(-1, m_Status);
}

TEST_F(dmHttpClientParserTest, TestSyntaxError)
{
    const char* headers = "HTTP/x.y 200 OK\r\n\r\n";

    dmHttpClient::ParseResult r;
    r = Parse(headers, false);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_SYNTAX_ERROR, r);
}

TEST_F(dmHttpClientParserTest, TestMissingStatusString)
{
    const char* headers = "HTTP/1.0 200\r\n\r\n";

    dmHttpClient::ParseResult r;
    r = Parse(headers, false);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_SYNTAX_ERROR, r);
}

TEST_F(dmHttpClientParserTest, TestHeaders)
{
    const char* headers = "HTTP/1.1 200 OK\r\n"
"Content-Type: text/html;charset=UTF-8\r\n"
"Content-Length: 21\r\n"
"Server: Jetty(7.0.2.v20100331)\r\n"
"\r\n";

    dmHttpClient::ParseResult r;
    r = Parse(headers, false);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_OK, r);

    ASSERT_EQ(1, m_Major);
    ASSERT_EQ(1, m_Minor);
    ASSERT_EQ(200, m_Status);
    ASSERT_EQ("OK", m_StatusString);

    ASSERT_EQ("text/html;charset=UTF-8", m_Headers["Content-Type"]);
    ASSERT_EQ("21", m_Headers["Content-Length"]);
    ASSERT_EQ("Jetty(7.0.2.v20100331)", m_Headers["Server"]);
    ASSERT_EQ((size_t) 3, m_Headers.size());
}

TEST_F(dmHttpClientParserTest, TestWhitespaceHeaders)
{
    const char* headers = "HTTP/1.1 200 OK\r\n"
"Content-Type:  text/html;charset=UTF-8\r\n"
"Content-Length:  21\r\n"
"Server:  Jetty(7.0.2.v20100331)\r\n"
"\r\n";

    dmHttpClient::ParseResult r;
    r = Parse(headers, false);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_OK, r);

    ASSERT_EQ(1, m_Major);
    ASSERT_EQ(1, m_Minor);
    ASSERT_EQ(200, m_Status);
    ASSERT_EQ("OK", m_StatusString);

    ASSERT_EQ("text/html;charset=UTF-8", m_Headers["Content-Type"]);
    ASSERT_EQ("21", m_Headers["Content-Length"]);
    ASSERT_EQ("Jetty(7.0.2.v20100331)", m_Headers["Server"]);
    ASSERT_EQ((size_t) 3, m_Headers.size());
}

TEST_F(dmHttpClientParserTest, TestNoWhitespaceHeaders)
{
    const char* headers = "HTTP/1.1 200 OK\r\n"
"Content-Type:text/html;charset=UTF-8\r\n"
"Content-Length:21\r\n"
"Server:Jetty(7.0.2.v20100331)\r\n"
"\r\n";

    dmHttpClient::ParseResult r;
    r = Parse(headers, false);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_OK, r);

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

    dmHttpClient::ParseResult r;
    r = Parse(headers, false);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_OK, r);

    ASSERT_STREQ("foo\r\n\r\nbar", headers + m_ContentOffset);
}

TEST_F(dmHttpClientParserTest, TestContentAndHeaders)
{
    const char* headers = "HTTP/1.1 200 OK\r\n"
"Content-Type: text/html;charset=UTF-8\r\n"
"Content-Length: 2\r\n"
"Server: Jetty(7.0.2.v20100331)\r\n"
"\r\n"
"30"
;

    dmHttpClient::ParseResult r;
    r = Parse(headers, false);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_OK, r);
    ASSERT_EQ("text/html;charset=UTF-8", m_Headers["Content-Type"]);
    ASSERT_EQ("2", m_Headers["Content-Length"]);
    ASSERT_EQ("Jetty(7.0.2.v20100331)", m_Headers["Server"]);

    ASSERT_STREQ("30", headers + m_ContentOffset);
}

TEST_F(dmHttpClientParserTest, TestNoContent)
{
    // Most servers seem to not add the empty line at the end of a 204 response even though the spec says we should. Test this specific case.
    const char* headers = "HTTP/1.1 204 No Content\r\n"
"Server: Jetty(7.0.2.v20100331)\r\n"
;
    dmHttpClient::ParseResult r;
    r = Parse(headers, true);
    ASSERT_EQ(dmHttpClient::PARSE_RESULT_OK, r);
    ASSERT_EQ(1, m_Major);
    ASSERT_EQ(1, m_Minor);
    ASSERT_EQ(204, m_Status);
    ASSERT_EQ("No Content", m_StatusString);
    ASSERT_EQ((size_t) 1, m_Headers.size());
    ASSERT_EQ("Jetty(7.0.2.v20100331)", m_Headers["Server"]);
}

#ifndef DM_DISABLE_HTTPCLIENT_TESTS

#if defined(DM_PLATFORM_VENDOR)
    #define NUM_ITERATIONS 25
#else
    #define NUM_ITERATIONS 100
#endif

TEST_P(dmHttpClientTest, Simple)
{
    char buf[128];

    for (int i = 0; i < NUM_ITERATIONS; ++i)
    {
        dmSnPrintf(buf, sizeof(buf), "/add/%d/1000", i);
        dmHttpClient::Result r = HttpGet(buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(1000 + i, strtol(m_Content.c_str(), 0, 10));
    }
}

struct HttpStressHelper
{
    int m_StatusCode;
    uint32_t m_RangeStart;
    uint32_t m_RangeEnd;
    uint32_t m_DocumentSize;
    std::string m_Content;
    dmHttpClient::HClient m_Client;

    HttpStressHelper(const dmURI::Parts& uri)
    {
        bool secure = strcmp(uri.m_Scheme, "https") == 0;
        m_StatusCode = 0;
        m_RangeStart = 0xFFFFFFFF;
        m_RangeEnd = 0xFFFFFFFF;
        m_DocumentSize = 0xFFFFFFFF;
        dmHttpClient::NewParams params;
        params.m_Userdata = this;
        params.m_HttpContent = HttpStressHelper::HttpContent;
        m_Client = dmHttpClient::New(&params, (dmURI::Parts*)&uri);
    }

    ~HttpStressHelper()
    {
        dmHttpClient::Delete(m_Client);
    }

    static void HttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length,
                                uint32_t range_start, uint32_t range_end, uint32_t document_size,
                                const char* method)
    {
        HttpStressHelper* self = (HttpStressHelper*) user_data;
        self->m_StatusCode = status_code;
        self->m_RangeStart = range_start;
        self->m_RangeEnd = range_end;
        self->m_DocumentSize = document_size;
        self->m_Content.append((const char*) content_data, content_data_size);
    }
};

#define T_ASSERT_EQ(_A, _B) \
    if ( (_A) != (_B) ) { \
        printf("%s:%d: ASSERT: %s != %s: %d != %d", __FILE__, __LINE__, #_A, #_B, (int)(_A), (int)(_B)); \
    } \
    assert( (_A) == (_B) );

static void HttpStressThread(void* param)
{
    char buf[128];
    int c = rand() % 3359;
    HttpStressHelper* h = (HttpStressHelper*) param;
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        h->m_Content = "";
        dmSnPrintf(buf, sizeof(buf), "/add/%d/1000", i * c);
        dmHttpClient::Result r;
        r = dmHttpClient::Get(h->m_Client, buf);
        T_ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        T_ASSERT_EQ(1000 + i * c, strtol(h->m_Content.c_str(), 0, 10));
    }
}

TEST_P(dmHttpClientTest, ThreadStress)
{
    // Shut down and reopen the connection pool so we can use it to it's full extent
    // on each pass
    ASSERT_EQ(0u, dmHttpClient::ShutdownConnectionPool());
    dmHttpClient::ReopenConnectionPool();

#if defined(GITHUB_CI) || defined(DM_PLATFORM_VENDOR)
    const int thread_count = 2;    // 32 is the maximum number of items in the connection pool, stay below that
#else
    const int thread_count = 16;   // 32 is the maximum number of items in the connection pool, stay below that
#endif
    dmThread::Thread threads[thread_count];
    HttpStressHelper* helpers[thread_count];

    for (int i = 0; i < thread_count; ++i) {
        helpers[i] = new HttpStressHelper(m_URI);
        dmThread::Thread t = dmThread::New(HttpStressThread, 0x80000, helpers[i], "test");
        threads[i] = t;
    }

    for (int i = 0; i < thread_count; ++i) {
        dmThread::Join(threads[i]);
        delete helpers[i];
    }
}

TEST_P(dmHttpClientTest, NoKeepAlive)
{
#if defined(GITHUB_CI) && defined(DM_SANITIZE_ADDRESS)
    if (strstr(GetParam(), "https://") != 0) {
        dmLogWarning("Test '%s' skipped due to the added time it takes to perform", __FUNCTION__);
        SKIP();
    }
#endif
    char buf[128];

    for (int i = 0; i < NUM_ITERATIONS; ++i)
    {
        dmSnPrintf(buf, sizeof(buf), "/no-keep-alive");
        dmHttpClient::Result r = HttpGet(buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_STREQ("will close connection now.", m_Content.c_str());
    }
}

TEST_P(dmHttpClientTest, CustomRequestHeaders)
{
    char buf[128];

    m_XScale = 123;
    for (int i = 0; i < NUM_ITERATIONS; ++i)
    {
        dmSnPrintf(buf, sizeof(buf), "/add/%d/1000", i);
        dmHttpClient::Result r = HttpGet(buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ((1000 + i) * m_XScale, strtol(m_Content.c_str(), 0, 10));
    }
}

TEST_P(dmHttpClientTest, ServerTimeout)
{
    for (int i = 0; i < 3; ++i)
    {
        dmHttpClient::Result r = HttpGet("/add/10/20");
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(30, strtol(m_Content.c_str(), 0, 10));

        // NOTE: MaxIdleTime is set to 500ms, see TestHttpServer.cpp line 300
        dmTime::Sleep(1000 * 550);

        r = HttpGet("/add/100/20");
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(120, strtol(m_Content.c_str(), 0, 10));
    }
}

TEST_P(dmHttpClientTest, ClientTimeout)
{
    // The TCP + SSL connection handshake take up a considerable amount of time on linux (peaks of 60+ ms), and
    // since we don't want to enable the TCP_NODELAY at this time, we increase the timeout values for these tests.
    // We also want to keep the unit tests below a certain amount of seconds, so we also decrease the number of iterations in this loop.

    int sleep_time_ms = 5 * 1000;
    #if defined(DM_PLATFORM_VENDOR) || defined (DM_SANITIZE_THREAD)
        const int timeout_us = 5000 * 1000;
        sleep_time_ms = 50 * 1000;
    #elif defined(__SCE__)
        const int timeout_us = 1000 * 1000;
    #else
        const int timeout_us = 500 * 1000;
    #endif

    dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_REQUEST_TIMEOUT, timeout_us); // microseconds

    char buf[128];
    for (int i = 0; i < 3; ++i)
    {
        dmHttpClient::Result r;
        m_StatusCode = -1;
        dmSnPrintf(buf, sizeof(buf), "/sleep/%d", sleep_time_ms); // milliseconds
        r = HttpGet(buf);
        ASSERT_NE(dmHttpClient::RESULT_OK, r);
        ASSERT_NE(dmHttpClient::RESULT_NOT_200_OK, r);
        ASSERT_EQ(-1, m_StatusCode);
        ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, dmHttpClient::GetLastSocketResult(m_Client));

        dmSnPrintf(buf, sizeof(buf), "/add/%d/1000", i);
        r = HttpGet(buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(1000 + i, strtol(m_Content.c_str(), 0, 10));
        ASSERT_EQ(200, m_StatusCode);
    }
    dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_REQUEST_TIMEOUT, 0);
}

TEST_P(dmHttpClientTest, ServerClose)
{
    dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_MAX_GET_RETRIES, 1);
    for (int i = 0; i < 10; ++i)
    {
        dmHttpClient::Result r = HttpGet("/close");
        ASSERT_NE(dmHttpClient::RESULT_OK, r);
        dmSocket::Result sock_r = dmHttpClient::GetLastSocketResult(m_Client);
        ASSERT_TRUE(r == dmHttpClient::RESULT_UNEXPECTED_EOF || sock_r == dmSocket::RESULT_CONNRESET || sock_r == dmSocket::RESULT_PIPE);
    }
}

struct ShutdownThreadContext
{
    int32_atomic_t m_GotIt;
};

static void ShutdownThread(void *args)
{
    ShutdownThreadContext* ctx = (ShutdownThreadContext*)args;
    while (!dmAtomicGet32(&ctx->m_GotIt))
    {
        // Now we give the test time to connect and be in-flight

        if (dmHttpClient::GetNumPoolConnections() == 0)
        {
            dmTime::Sleep(1);
            continue;
        }

        // There is a small gap between it is in use and it is connected
        dmTime::Sleep(100);

        if (dmHttpClient::ShutdownConnectionPool() > 0) {
            dmAtomicStore32(&ctx->m_GotIt, 1);
        } else {
            break; // done.
        }
    }
}

TEST_P(dmHttpClientTest, ClientThreadedShutdown)
{
    dmHttpClient::ShutdownConnectionPool();
    dmHttpClient::ReopenConnectionPool();

    char buf[128];
    ShutdownThreadContext ctx;
    ctx.m_GotIt = 0;

    int sleep_time_ms = 5 * 1000;
    #if defined (DM_SANITIZE_THREAD)
        sleep_time_ms = 5 * 1000;
    #endif

    for (int i=0;i<5;i++) {
        // Create a request that proceeds for a long time and cancel it in-flight with the
        // shutdown thread. If it managed to get the connection it will set m_GotIt to true.
        dmThread::Thread thr = dmThread::New(&ShutdownThread, 65536, &ctx, "cts");
        dmSnPrintf(buf, sizeof(buf), "/sleep/%d", sleep_time_ms);
        dmHttpClient::Result r = HttpGet(buf);
        ASSERT_NE(dmHttpClient::RESULT_OK, r);

        // Wait until no connections are open
        dmThread::Join(thr);

        // Pool shut down; must fail.
        r = HttpGet("/sleep/10000");
        ASSERT_NE(dmHttpClient::RESULT_OK, r);

        dmHttpClient::ReopenConnectionPool();

        if (dmAtomicGet32(&ctx.m_GotIt))
            break;
    }

    ASSERT_EQ(1, dmAtomicGet32(&ctx.m_GotIt));

    // Reopened so should succeed.
    dmHttpClient::Result r = r = HttpGet("/sleep/10");
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
}


TEST_P(dmHttpClientTest, ContentSizes)
{
    char buf[128];

    const uint32_t primes[] = { 0, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97 };
    for (int i = 0; i < 1000; i += 100)
    {
        for (uint32_t j = 0; j < sizeof(primes) / sizeof(primes[0]); ++j)
        {
            dmSnPrintf(buf, sizeof(buf), "/arb/%d", i + primes[j]);

            dmHttpClient::Result r = HttpGet(buf);
            ASSERT_EQ(dmHttpClient::RESULT_OK, r);
            ASSERT_EQ(i + primes[j], m_Content.size());
            for (uint32_t k = 0; k < i + primes[j]; ++k)
            {
                ASSERT_EQ(k % 255, (uint32_t) (m_Content[k] & 0xff));
            }
        }
    }
}

TEST_P(dmHttpClientTest, LargeFile)
{
    char buf[128];

    int n = 1024 * 1024 + 59;
    dmSnPrintf(buf, sizeof(buf), "/arb/%d", n);
    dmHttpClient::Result r = HttpGet(buf);
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_EQ((uint32_t) n, m_Content.size());

    for (uint32_t i = 0; i < (uint32_t) n; ++i)
    {
        ASSERT_EQ(i % 255, (uint32_t) (m_Content[i] & 0xff));
    }
}

TEST_P(dmHttpClientTest, TestHeaders)
{
    char buf[128];

    int n = 123;
    dmSnPrintf(buf, sizeof(buf), "/arb/%d", n);
    dmHttpClient::Result r = HttpGet(buf);
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_EQ((uint32_t) n, m_Content.size());
    ASSERT_STREQ("123", m_Headers["Content-Length"].c_str());
}

TEST_P(dmHttpClientTest, Test404)
{
    for (int i = 0; i < 17; ++i)
    {
        dmHttpClient::Result r = HttpGet("/does_not_exists");
        ASSERT_EQ(dmHttpClient::RESULT_NOT_200_OK, r);
        ASSERT_EQ(404, m_StatusCode);
    }
}

TEST_P(dmHttpClientTest, Post)
{
    for (int i = 0; i < 27; ++i)
    {
        int n = (rand() % 123) + 171;
        int sum = 0;
        m_ToPost = "";

        for (int j = 0; j < n; ++j) {
            int8_t buf[2] = { (int8_t)((rand() % 255) - 128), 0 };
            sum += buf[0];
            m_ToPost.append((char*)buf);
        }

        dmHttpClient::Result r = HttpPost("/post");
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(200, m_StatusCode);
        ASSERT_EQ(sum, atoi(m_Content.c_str()));
    }
}

TEST_P(dmHttpClientTest, PostLarge)
{
    for (int i = 0; i < 10; ++i)
    {
        int n = 13777 * i;
        int sum = 0;
        m_ToPost = "";

        for (int j = 0; j < n; ++j) {
            int8_t buf[2] = { (int8_t)((rand() % 255) - 128), 0 };
            m_ToPost.append((char*)buf);
            sum += buf[0];
        }

        dmHttpClient::Result r = HttpPost("/post");
        printf("POST'ing to %s://%s%s\n", m_URI.m_Scheme, m_URI.m_Location, m_URI.m_Path);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(200, m_StatusCode);
        ASSERT_EQ(sum, atoi(m_Content.c_str()));
    }
}

TEST_P(dmHttpClientTest, Cache)
{
    dmHttpClient::Delete(m_Client);

    // Reinit client with http-cache
    dmHttpClient::NewParams params;
    params.m_Userdata = this;
    params.m_HttpContent = dmHttpClientTest::HttpContent;
    params.m_HttpHeader = dmHttpClientTest::HttpHeader;
    dmHttpCache::NewParams cache_params;
    cache_params.m_Path = m_CacheDir;
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, &m_URI, 0);
    ASSERT_NE((void*) 0, m_Client);

    for (int i = 0; i < NUM_ITERATIONS; ++i)
    {
        dmHttpClient::Result r = HttpGet("/cached/0");
        if (r == dmHttpClient::RESULT_OK)
        {
            ASSERT_EQ(200, m_StatusCode);
        }
        else
        {
            ASSERT_EQ(dmHttpClient::RESULT_NOT_200_OK, r);
            ASSERT_EQ(304, m_StatusCode);
        }
        ASSERT_EQ(std::string("cached_content"), m_Content);
    }

    dmHttpClient::Statistics stats;
    dmHttpClient::GetStatistics(m_Client, &stats);
    // NOTE: m_Responses is increased for every request. Therefore we must compensate for potential re-connections
    ASSERT_EQ(uint32_t(NUM_ITERATIONS), stats.m_Responses - stats.m_Reconnections);
    ASSERT_EQ(uint32_t(NUM_ITERATIONS - 1), stats.m_CachedResponses);
    cache_r = dmHttpCache::Close(params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
}

TEST_P(dmHttpClientTest, MaxAgeCache)
{
#if !defined(DM_NO_SIGNAL_FUNCTION)
    signal(SIGPIPE, SIG_IGN);
#endif

    dmHttpClient::Delete(m_Client);

    // Reinit client with http-cache
    dmHttpClient::NewParams params;
    params.m_Userdata = this;
    params.m_HttpContent = dmHttpClientTest::HttpContent;
    params.m_HttpHeader = dmHttpClientTest::HttpHeader;
    dmHttpCache::NewParams cache_params;
    cache_params.m_Path = m_CacheDir;
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, &m_URI, 0);
    ASSERT_NE((void*) 0, m_Client);

    dmHttpClient::Result r = HttpGet("/max-age-cached");
    ASSERT_TRUE((r == dmHttpClient::RESULT_OK && m_StatusCode == 200) || (r == dmHttpClient::RESULT_NOT_200_OK && m_StatusCode == 304));
    std::string val = m_Content;

    // The web-server is returning System.currentTimeMillis()
    // Ensure next ms
    dmTime::Sleep(1U);

    r = HttpGet("/max-age-cached");
    ASSERT_TRUE((r == dmHttpClient::RESULT_OK && m_StatusCode == 200) || (r == dmHttpClient::RESULT_NOT_200_OK && m_StatusCode == 304));
    ASSERT_STREQ(m_Content.c_str(), val.c_str());

    // max-age is 1 second
    dmTime::Sleep(1000000U);

    r = HttpGet("/max-age-cached");
    ASSERT_TRUE((r == dmHttpClient::RESULT_OK && m_StatusCode == 200) || (r == dmHttpClient::RESULT_NOT_200_OK && m_StatusCode == 304));
    ASSERT_STRNE(m_Content.c_str(), val.c_str());

    cache_r = dmHttpCache::Close(params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
}

TEST_P(dmHttpClientTest, PathWithSpaces)
{
    char buf[128], uri[128];

    // should fail since it is not properly encoded.
    // NOTE: Really really should be only encoding the 'message' and not the whole path.
    //       But Encode for now is kind to not encode '/'
    const char* message = "testing 1 2";
    snprintf(buf, 128, "/echo/%s", message);
    dmURI::Encode(buf, uri, sizeof(uri), 0);

    dmHttpClient::Result r = HttpGet(uri);
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_STREQ(message, m_Content.c_str());
}

TEST_P(dmHttpClientTestExternal, PostExternal)
{
    int n = 17000;

    m_ToPost = "";

    for (int j = 0; j < n; ++j) {
        char buf[2] = { 'a', 0 };
        m_ToPost.append(buf);
    }

    dmHttpClient::Result r;
    m_Content = "";
    m_StatusCode = -1;

    printf("POST'ing to %s://%s%s\n", m_URI.m_Scheme, m_URI.m_Location, m_URI.m_Path);
    r = dmHttpClient::Post(m_Client, m_URI.m_Path);

    printf("STATUS: %d\n", m_StatusCode);
    if (503 == m_StatusCode)
    {
        // It's an external page, which may be down from time to time
        return;
    }

    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_EQ(200, m_StatusCode);

    printf("CONTENT:\n%s\n", m_Content.c_str());
}

// Until we've figured out how to access the local server on windows from the PS4
#if !(defined(DM_TEST_DLIB_HTTPCLIENT_NO_HOST_SERVER))

const char* params_http_client_test[] = {"http://localhost:" NAME_SOCKET, "https://localhost:" NAME_SOCKET_SSL};
INSTANTIATE_TEST_CASE_P(dmHttpClientTest, dmHttpClientTest, jc_test_values_in(params_http_client_test));

#endif

#if !(defined(GITHUB_CI) || defined(DM_TEST_DLIB_HTTPCLIENT_NO_HOST_SERVER))
// For easier debugging, we can use external sites to monitor their responses
// NOTE: These buckets might expire. If so, we'll have to disable that server test
const char* params_http_client_external_test[] = {  // They expire after a few days, but I keep it here in case you wish to test with it
													// during development "https://hookb.in/je1lZ3X0ngTobX0plMME",
                                                    //                    "https://webhook.site/e22f2d03-abf4-4f23-a8dc-7e24126cefab",
                                                    "https://httpbin.org/post"
                                                };
INSTANTIATE_TEST_CASE_P(dmHttpClientTestExternal, dmHttpClientTestExternal, jc_test_values_in(params_http_client_external_test));
#endif

TEST_P(dmHttpClientTestSSL, FailedSSLHandshake)
{
    for( int i = 0; i < 3; ++i )
    {
        uint64_t timeout = 130 * 1000;
        dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_REQUEST_TIMEOUT, timeout); // microseconds

        uint64_t timestart = dmTime::GetMonotonicTime();
        dmHttpClient::Result r = dmHttpClient::Get(m_Client, "/sleep/5000"); // milliseconds
        uint64_t timeend = dmTime::GetMonotonicTime();

        ASSERT_NE(dmHttpClient::RESULT_OK, r);
        ASSERT_NE(dmHttpClient::RESULT_NOT_200_OK, r);
        ASSERT_EQ(-1, m_StatusCode);
        ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, dmHttpClient::GetLastSocketResult(m_Client));

        uint64_t elapsed = timeend - timestart;

        if(elapsed < timeout)
        {
            dmLogError("The test was too short! It cannot possibly have timed out!\n");
            ASSERT_TRUE(0);
        }

        if( elapsed / 1000000 > 10)
        {
            dmLogError("The test timed out\n");
            ASSERT_TRUE(0);
        }
    }
}

// Until we've figured out how to access the local server on windows from the device
#if !(defined(DM_TEST_DLIB_HTTPCLIENT_NO_HOST_SERVER))

const char* params_http_client_test_ssl[] = {"https://localhost:" NAME_SOCKET_SSL_TEST};
INSTANTIATE_TEST_CASE_P(dmHttpClientTestSSL, dmHttpClientTestSSL, jc_test_values_in(params_http_client_test_ssl));

#endif

class dmHttpClientTestCache : public dmHttpClientTest
{
public:
    void GetFiles(bool check_content, bool partial, uint32_t count)
    {
        std::string expected_data[4] = {
            std::string("You will find this data in a.txt and d.txt"),
            std::string("Some data in file b"),
            std::string("Some data in file c"),
            std::string("You will find this data in a.txt and d.txt")
        };

        const char* names[4] = {
            "/tmp/http_files/a.txt",
            "/tmp/http_files/b.txt",
            "/tmp/http_files/c.txt",
            "/tmp/http_files/d.txt",
        };

        // we want to make the same requests each time
        uint32_t offsets[4];
        uint32_t sizes[4];

        if (partial)
        {
            for (int i = 0; i < 4; ++i)
            {
                int idx = i % 4;
                const char* expected = expected_data[idx].c_str();
                // avoid rand() as we want deterministic ranges (which will affect the cache keys!)
                // between runs
                uint32_t size = strlen(expected);
                uint32_t offset = size / 3;
                size = (size - offset) / 2;
                if (size == 0)
                    size = 1;
                offsets[idx] = offset;
                sizes[idx] = size;
            }
        }

        for (int i = 0; i < (int)count; ++i)
        {
            int idx = i % 4;
            const char* name = names[idx];
            const char* expected = expected_data[idx].c_str();

            dmHttpClient::Result r;
            if (partial)
            {
                uint32_t end = offsets[idx] + sizes[idx] - 1;
                r = HttpGetPartial(name, offsets[idx], end);

                if (r == dmHttpClient::RESULT_PARTIAL_CONTENT)
                {
                    ASSERT_EQ(206, m_StatusCode);
                }
            }
            else
            {
                r = HttpGet(name);

                if (r == dmHttpClient::RESULT_OK)
                {
                    ASSERT_EQ(200, m_StatusCode);
                }
                else
                {
                    ASSERT_EQ(dmHttpClient::RESULT_NOT_200_OK, r);
                    ASSERT_EQ(304, m_StatusCode);
                }

            }

            if (check_content)
            {
                uint32_t document_size = (uint32_t)strlen(expected);
                if (partial)
                {
                    uint32_t sz = sizes[idx];
                    const char* expected_partial = &expected[offsets[idx]];
                    ASSERT_ARRAY_EQ_LEN(expected_partial, m_Content.c_str(), sz);
                    ASSERT_EQ(sz, (uint32_t)m_Content.size());

                    uint32_t end = offsets[idx] + sizes[idx] - 1;
                    ASSERT_EQ(offsets[idx], m_RangeStart);
                    ASSERT_EQ(end, m_RangeEnd);
                    ASSERT_EQ(document_size, m_DocumentSize);
                }
                else
                {
                    ASSERT_STREQ(expected, m_Content.c_str());
                    ASSERT_EQ(0U, m_RangeStart);
                    ASSERT_EQ(document_size-1, m_RangeEnd);
                    ASSERT_EQ(document_size, m_DocumentSize);
                }
            }
        }
    }

    void GetFiles(bool check_content, bool partial)
    {
        GetFiles(check_content, partial, 100);
    }
};

TEST_P(dmHttpClientTestCache, DirectFromCache)
{
    dmHttpClient::Delete(m_Client);

    // Reinit client with http-cache
    dmHttpClient::NewParams params;
    params.m_Userdata = this;
    params.m_HttpContent = dmHttpClientTest::HttpContent;
    params.m_HttpHeader = dmHttpClientTest::HttpHeader;

    params.m_HttpSendContentLength = dmHttpClientTest::HttpSendContentLength;
    params.m_HttpWrite = dmHttpClientTest::HttpWrite;
    params.m_HttpWriteHeaders = dmHttpClientTest::HttpWriteHeaders;

    dmHttpCache::NewParams cache_params;
    cache_params.m_Path = m_CacheDir;
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, &m_URI);
    ASSERT_NE((void*) 0, m_Client);

    uint32_t count = 50;
    uint32_t iter = 2;
    for (uint32_t i = 0; i < iter; ++i)
    {
        bool partial = i > 0;
        GetFiles(true, partial, count);
    }

    dmHttpClient::Statistics stats;
    dmHttpClient::GetStatistics(m_Client, &stats);
    // NOTE: m_Responses is increased for every request. Therefore we must compensate for potential re-connections
    ASSERT_EQ(count*iter, stats.m_Responses - stats.m_Reconnections);
    ASSERT_EQ((count-4U)*iter, stats.m_CachedResponses);
    ASSERT_EQ(0U, stats.m_DirectFromCache);

    // Change consistency police to "trust-cache". All files are retrieved and should therefore be already verified
    dmHttpCache::SetConsistencyPolicy(params.m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);

    for (uint32_t i = 0; i < iter; ++i)
    {
        bool partial = i > 0;
        GetFiles(true, partial, count);
    }

    dmHttpClient::GetStatistics(m_Client, &stats);

    ASSERT_EQ(count*iter, stats.m_Responses - stats.m_Reconnections);
    // Should be equivalent to above
    ASSERT_EQ((count-4U)*iter, stats.m_CachedResponses);
    // All files directly from cache
    ASSERT_EQ(count*iter, stats.m_DirectFromCache);

    cache_r = dmHttpCache::Close(params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
}

TEST_P(dmHttpClientTestCache, TrustCacheNoValidate)
{
    dmHttpClient::Delete(m_Client);

    // Reinit client with http-cache
    dmHttpClient::NewParams params;
    params.m_Userdata = this;
    params.m_HttpContent = dmHttpClientTest::HttpContent;
    params.m_HttpHeader = dmHttpClientTest::HttpHeader;
    dmHttpCache::NewParams cache_params;
    cache_params.m_Path = m_CacheDir;
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, &m_URI);
    ASSERT_NE((void*) 0, m_Client);

    // Change consistency police to "trust-cache". After the first four files are files should be directly retrieved from the cache.
    dmHttpCache::SetConsistencyPolicy(params.m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);

    GetFiles(true, false);

    dmHttpClient::Statistics stats;
    dmHttpClient::GetStatistics(m_Client, &stats);

    // Four non-cached and four cached
    // NOTE: m_Responses is increased for every request. Therefore we must compensate for potential re-connections
    ASSERT_EQ(8U, stats.m_Responses - stats.m_Reconnections);
    // Four cached responses
    ASSERT_EQ(4U, stats.m_CachedResponses);
    // Rest are loaded directly from the cache
    ASSERT_EQ(92U, stats.m_DirectFromCache);

    cache_r = dmHttpCache::Close(params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
}

TEST_P(dmHttpClientTestCache, BatchValidateCache)
{
    dmHttpClient::Delete(m_Client);

    // Reinit client with http-cache
    dmHttpClient::NewParams params;
    params.m_Userdata = this;
    params.m_HttpContent = dmHttpClientTest::HttpContent;
    params.m_HttpHeader = dmHttpClientTest::HttpHeader;
    dmHttpCache::NewParams cache_params;
    cache_params.m_Path = m_CacheDir;
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, &m_URI);
    ASSERT_NE((void*) 0, m_Client);

    // Warmup cache
    GetFiles(true, false);

    // Reopen
    dmHttpClient::Delete(m_Client);
    cache_r = dmHttpCache::Close(params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, &m_URI);
    ASSERT_NE((void*) 0, m_Client);


    dmHttpCacheVerify::Result verify_r = dmHttpCacheVerify::VerifyCache(params.m_HttpCache, &m_URI, 60 * 60 * 24 * 5);
    ASSERT_EQ(dmHttpCacheVerify::RESULT_OK, verify_r);

    // Change consistency police to "trust-cache". After the first four files are files should be directly retrieved from the cache.
    dmHttpCache::SetConsistencyPolicy(params.m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);

    GetFiles(true, false);
    dmHttpClient::Statistics stats;
    dmHttpClient::GetStatistics(m_Client, &stats);
    // Zero responses, all direct from cache
    ASSERT_EQ(0U, stats.m_Responses);
    // Zero cached responses, all direct from cache
    ASSERT_EQ(0U, stats.m_CachedResponses);
    // All are loaded directly from the cache
    ASSERT_EQ(100U, stats.m_DirectFromCache);

    // Change a.txt file.
    char path[512];
    dmTestUtil::MakeHostPath(path, sizeof(path), "tmp/http_files/a.txt");

    FILE* a_file = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, a_file);
    fwrite("foo", 1, 3, a_file);
    fclose(a_file);

    GetFiles(true, false);
    dmHttpClient::GetStatistics(m_Client, &stats);
    // Zero responses, all direct from cache
    ASSERT_EQ(0U, stats.m_Responses);
    // Zero cached responses, all direct from cache
    ASSERT_EQ(0U, stats.m_CachedResponses);
    // All are loaded directly from the cache
    ASSERT_EQ(200U, stats.m_DirectFromCache);

    // Change a.txt file again
    a_file = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, a_file);
    fwrite("bar", 1, 3, a_file);
    fclose(a_file);

    // Change policy
    dmHttpCache::SetConsistencyPolicy(params.m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_VERIFY);
    GetFiles(false, false);
    dmHttpClient::GetStatistics(m_Client, &stats);
    // Zero responses, all direct from cache
    // NOTE: m_Responses is increased for every request. Therefore we must compensate for potential re-connections
    ASSERT_EQ(100U, stats.m_Responses - stats.m_Reconnections);
    // Zero cached responses, all direct from cache
    ASSERT_EQ(99U, stats.m_CachedResponses);
    // All are loaded directly from the cache
    ASSERT_EQ(200U, stats.m_DirectFromCache);

    cache_r = dmHttpCache::Close(params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
}

// Until we've figured out how to access the local server on windows from the device
#if !(defined(DM_TEST_DLIB_HTTPCLIENT_NO_HOST_SERVER))

const char* params_http_client_cache[] = {"http://localhost:" NAME_SOCKET};
INSTANTIATE_TEST_CASE_P(dmHttpClientTestCache, dmHttpClientTestCache, jc_test_values_in(params_http_client_cache));

#endif

#else

#endif // #ifndef DM_DISABLE_HTTPCLIENT_TESTS

TEST(dmHttpClient, HostNotFound)
{
    dmURI::Parts uri;
    dmURI::Parse("http://host_not_found", &uri);

    dmHttpClient::NewParams params;
    dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
    ASSERT_EQ((void*) 0, client);
}

TEST(dmHttpClient, ConnectionRefused)
{
    dmURI::Parts uri;
    dmURI::Parse("http://0.0.0.0:9999", &uri);

    dmHttpClient::NewParams params;
    dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
    ASSERT_NE((void*) 0, client);
    dmHttpClient::Result r = dmHttpClient::Get(client, "");
    ASSERT_EQ(dmHttpClient::RESULT_SOCKET_ERROR, r);
    #if defined(__linux__) || defined(DM_PLATFORM_VENDOR)
    ASSERT_EQ(dmSocket::RESULT_HOST_NOT_FOUND, dmHttpClient::GetLastSocketResult(client));
    #elif defined(WIN32)
    ASSERT_EQ(dmSocket::RESULT_ADDRNOTAVAIL, dmHttpClient::GetLastSocketResult(client));
    #else
    ASSERT_EQ(dmSocket::RESULT_CONNREFUSED, dmHttpClient::GetLastSocketResult(client));
    #endif
    dmHttpClient::Delete(client);
}

TEST(dmHttpClient, Proxy)
{
    char proxy_server_url[1024];
    dmSnPrintf(proxy_server_url, sizeof(proxy_server_url), "http://localhost:%d", g_HttpPort);
    dmURI::Parts proxy;
    dmURI::Parse(proxy_server_url, &proxy);

    dmURI::Parts uri;
    dmURI::Parse("http://www.google.com", &uri);

    // create a client which is establishing a proxy tunnel from
    // localhost to www.google.com/proxy/to/www.google.com
    // in this test we're not actually doing a request to google
    // but we check that the httpclient first does a CONNECT and then
    // a GET
    dmHttpClient::NewParams params;
    dmHttpClient::HClient client = dmHttpClient::New(&params, &uri, 0, &proxy);
    ASSERT_NE((void*) 0, client);
    dmHttpClient::Result r = dmHttpClient::Request(client, "GET", "/proxy/to/www.google.com");
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    dmHttpClient::Delete(client);

    // create another client and check that we properly handle
    // connection issues
    client = dmHttpClient::New(&params, &uri, 0, &proxy);
    ASSERT_NE((void*) 0, client);
    r = dmHttpClient::Request(client, "GET", "/proxy/broken");
    ASSERT_NE(dmHttpClient::RESULT_OK, r);
    dmHttpClient::Delete(client);
}

static void Usage()
{
    dmLogError("Usage: <exe> <config>");
    dmLogError("Be sure to start the http server before starting this test.");
    dmLogError("You can use the config file created by the server");
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);

    if(argc > 1)
    {
        char path[512];
        dmTestUtil::MakeHostPath(path, sizeof(path), argv[1]);

        dmConfigFile::HConfig config;
        if( dmConfigFile::Load(path, argc, (const char**)argv, &config) != dmConfigFile::RESULT_OK )
        {
            dmLogError("Could not read config file '%s'", argv[1]);
            Usage();
            return 1;
        }

        const char* ip = dmConfigFile::GetString(config, "server.ip", "localhost");
        dmStrlCpy(SERVER_IP, ip, sizeof(SERVER_IP));

        dmTestUtil::GetSocketsFromConfig(config, &g_HttpPort, &g_HttpPortSSL, &g_HttpPortSSLTest);
        dmConfigFile::Delete(config);
    }
    else
    {
        Usage();
        return 1;
    }

    dmLogSetLevel(LOG_SEVERITY_INFO);
    dmSocket::Initialize();
    dmSSLSocket::Initialize();

    int ret = jc_test_run_all();
    dmSSLSocket::Finalize();
    dmSocket::Finalize();
    return ret;
}
