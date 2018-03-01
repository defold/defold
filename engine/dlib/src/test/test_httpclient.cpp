#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <gtest/gtest.h>
#include "dlib/configfile.h"
#include "dlib/dstrings.h"
#include "dlib/time.h"
#include "dlib/log.h"
#include "dlib/thread.h"
#include "dlib/uri.h"
#include "dlib/http_client.h"
#include "dlib/http_client_private.h"
#include "dlib/http_cache_verify.h"
#include "testutil.h"

int g_HttpPort = -1;
int g_HttpPortSSL = -1;
int g_HttpPortSSLTest = -1;

#define NAME_SOCKET "{server_socket}"
#define NAME_SOCKET_SSL "{server_socket_ssl}"
#define NAME_SOCKET_SSL_TEST "{server_socket_ssl_test}"

class dmHttpClientTest: public ::testing::TestWithParam<const char*>
{
public:
    dmHttpClient::HClient m_Client;
    std::map<std::string, std::string> m_Headers;
    std::string m_Content;
    std::string m_ToPost;
    int m_StatusCode;
    int m_XScale;
    dmURI::Parts m_URI;

    static void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        self->m_Headers[key] = value;
    }

    static void HttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        self->m_StatusCode = status_code;
        self->m_Content.append((const char*) content_data, content_data_size);
    }

    static uint32_t HttpSendContentLength(dmHttpClient::HResponse response, void* user_data)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        const char* s = self->m_ToPost.c_str();
        return strlen(s);
    }

    static dmHttpClient::Result HttpWrite(dmHttpClient::HResponse response, void* user_data)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        const char* s = self->m_ToPost.c_str();
        return dmHttpClient::Write(response, s, strlen(s));
    }

    static dmHttpClient::Result HttpWriteHeaders(dmHttpClient::HResponse response, void* user_data)
    {
        dmHttpClientTest* self = (dmHttpClientTest*) user_data;
        char scale[128];
        DM_SNPRINTF(scale, sizeof(scale), "%d", self->m_XScale);
        return dmHttpClient::WriteHeader(response, "X-Scale", scale);
    }


    virtual void SetUp()
    {
        m_Client = 0;

        char url[1024];
        strcpy(url, GetParam());

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
        ASSERT_NE(-1, port);

        sprintf(portstr, "%d", port);

        dmURI::Result parse_r = dmURI::Parse(url, &m_URI);
        ASSERT_EQ(dmURI::RESULT_OK, parse_r);

#if !defined(DM_NO_SYSTEM)
        int ret = system("python src/test/test_httpclient.py");
#else
        int ret = -1;
#endif
        ASSERT_EQ(0, ret);

        dmHttpClient::NewParams params;
        params.m_Userdata = this;
        params.m_HttpContent = dmHttpClientTest::HttpContent;
        params.m_HttpHeader = dmHttpClientTest::HttpHeader;
        params.m_HttpSendContentLength = dmHttpClientTest::HttpSendContentLength;
        params.m_HttpWrite = dmHttpClientTest::HttpWrite;
        params.m_HttpWriteHeaders = dmHttpClientTest::HttpWriteHeaders;
        bool secure = strcmp(m_URI.m_Scheme, "https") == 0;
        m_Client = dmHttpClient::New(&params, m_URI.m_Hostname, m_URI.m_Port, secure);
        ASSERT_NE((void*) 0, m_Client);

        m_XScale = 1;
        m_StatusCode = -1;
    }

    virtual void TearDown()
    {
        if (m_Client)
            dmHttpClient::Delete(m_Client);
    }
};

class dmHttpClientTestSSL : public dmHttpClientTest
{
    // for gtest
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

TEST_F(dmHttpClientParserTest, TestWhitespaceHeaders)
{
    const char* headers = "HTTP/1.1 200 OK\r\n"
"Content-Type:  text/html;charset=UTF-8\r\n"
"Content-Length:  21\r\n"
"Server:  Jetty(7.0.2.v20100331)\r\n"
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

#ifndef _WIN32

// NOTE: Tests disabled. Currently we need bash to start and shutdown http server.

TEST_P(dmHttpClientTest, Simple)
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

struct HttpStressHelper
{
    int m_StatusCode;
    std::string m_Content;
    dmHttpClient::HClient m_Client;

    HttpStressHelper(const dmURI::Parts& uri)
    {
        bool secure = strcmp(uri.m_Scheme, "https") == 0;
        m_StatusCode = 0;
        dmHttpClient::NewParams params;
        params.m_Userdata = this;
        params.m_HttpContent = HttpStressHelper::HttpContent;
        m_Client = dmHttpClient::New(&params, uri.m_Hostname, uri.m_Port, secure);
    }

    ~HttpStressHelper()
    {
        dmHttpClient::Delete(m_Client);
    }

    static void HttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        HttpStressHelper* self = (HttpStressHelper*) user_data;
        self->m_StatusCode = status_code;
        self->m_Content.append((const char*) content_data, content_data_size);
    }
};

static void HttpStressThread(void* param)
{
    char buf[128];
    int c = rand() % 3359;
    HttpStressHelper* h = (HttpStressHelper*) param;
    for (int i = 0; i < 100; ++i) {
        h->m_Content = "";
        sprintf(buf, "/add/%d/1000", i * c);
        dmHttpClient::Result r;
        r = dmHttpClient::Get(h->m_Client, buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(1000 + i * c, strtol(h->m_Content.c_str(), 0, 10));
    }
}

TEST_P(dmHttpClientTest, ThreadStress)
{
    const int thread_count = 16;
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
    char buf[128];

    for (int i = 0; i < 100; ++i)
    {
        m_Content = "";
        sprintf(buf, "/no-keep-alive");
        dmHttpClient::Result r;
        r = dmHttpClient::Get(m_Client, buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_STREQ("will close connection now.", m_Content.c_str());
    }
}

TEST_P(dmHttpClientTest, CustomRequestHeaders)
{
    char buf[128];

    m_XScale = 123;
    for (int i = 0; i < 100; ++i)
    {
        m_Content = "";
        sprintf(buf, "/add/%d/1000", i);
        dmHttpClient::Result r;
        r = dmHttpClient::Get(m_Client, buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ((1000 + i) * m_XScale, strtol(m_Content.c_str(), 0, 10));
    }
}

TEST_P(dmHttpClientTest, ServerTimeout)
{
    for (int i = 0; i < 10; ++i)
    {
        dmHttpClient::Result r;
        m_Content = "";
        r = dmHttpClient::Get(m_Client, "/add/10/20");
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(30, strtol(m_Content.c_str(), 0, 10));

        // NOTE: MaxIdleTime is set to 300ms
        dmTime::Sleep(1000 * 350);

        m_Content = "";
        r = dmHttpClient::Get(m_Client, "/add/100/20");
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(120, strtol(m_Content.c_str(), 0, 10));
    }
}

TEST_P(dmHttpClientTest, ClientTimeout)
{
    // The TCP + SSL connection handshake take up a considerable amount of time on linux (peaks of 60+ ms), and
    // since we don't want to enable the TCP_NODELAY at this time, we increase the timeout values for these tests.
    // We also want to keep the unit tests below a certain amount of seconds, so we also decrease the number of iterations in this loop.
    dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_REQUEST_TIMEOUT, 130 * 1000); // microseconds

    char buf[128];
    for (int i = 0; i < 7; ++i)
    {
        dmHttpClient::Result r;
        m_StatusCode = -1;
        m_Content = "";
        r = dmHttpClient::Get(m_Client, "/sleep/5000"); // milliseconds
        ASSERT_NE(dmHttpClient::RESULT_OK, r);
        ASSERT_NE(dmHttpClient::RESULT_NOT_200_OK, r);
        ASSERT_EQ(-1, m_StatusCode);
        ASSERT_EQ(dmSocket::RESULT_WOULDBLOCK, dmHttpClient::GetLastSocketResult(m_Client));

        m_Content = "";
        sprintf(buf, "/add/%d/1000", i);
        r = dmHttpClient::Get(m_Client, buf);
        ASSERT_EQ(dmHttpClient::RESULT_OK, r);
        ASSERT_EQ(1000 + i, strtol(m_Content.c_str(), 0, 10));
        ASSERT_EQ(200, m_StatusCode);
    }
    dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_REQUEST_TIMEOUT, 0);
}

TEST_P(dmHttpClientTestSSL, FailedSSLHandshake)
{
    for( int i = 0; i < 5; ++i )
    {
        uint64_t timeout = 130 * 1000;
        dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_REQUEST_TIMEOUT, timeout); // microseconds

        uint64_t timestart = dmTime::GetTime();
        dmHttpClient::Result r = dmHttpClient::Get(m_Client, "/sleep/5000"); // milliseconds
        uint64_t timeend = dmTime::GetTime();

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



TEST_P(dmHttpClientTest, ServerClose)
{
    dmHttpClient::SetOptionInt(m_Client, dmHttpClient::OPTION_MAX_GET_RETRIES, 1);
    for (int i = 0; i < 10; ++i)
    {
        dmHttpClient::Result r;
        m_Content = "";
        r = dmHttpClient::Get(m_Client, "/close");
        ASSERT_NE(dmHttpClient::RESULT_OK, r);
        dmSocket::Result sock_r = dmHttpClient::GetLastSocketResult(m_Client);
        ASSERT_TRUE(r == dmHttpClient::RESULT_UNEXPECTED_EOF || sock_r == dmSocket::RESULT_CONNRESET || sock_r == dmSocket::RESULT_PIPE);
    }
}

void ShutdownThread(void *args)
{
    bool* gotit = (bool*) args;
    while (true)
    {
        // Now we give the test time to connect and be in-flight
        dmTime::Sleep(1000 * 500);
        if (dmHttpClient::ShutdownConnectionPool() > 0) {
            // it was in flight and now it should be cancelled and fail.
            *gotit = true;
        } else {
            break; // done.
        }
    }
}

TEST_P(dmHttpClientTest, ClientThreadedShutdown)
{
    bool gotit = false;
    for (int i=0;i<10;i++) {
        // Create a request that proceeds for a long time and cancel it in-flight with the
        // shutdown thread. If it managed to get the conneciton it will set gotit to true.
        dmThread::Thread thr = dmThread::New(&ShutdownThread, 65536, &gotit, "cst");
        dmHttpClient::Result r = dmHttpClient::Get(m_Client, "/sleep/10000");
        ASSERT_NE(dmHttpClient::RESULT_OK, r);

        // Wait until no are open
        dmThread::Join(thr);

        // Pool shut down; must fail.
        r = dmHttpClient::Get(m_Client, "/sleep/10000");
        ASSERT_NE(dmHttpClient::RESULT_OK, r);

        dmHttpClient::ReopenConnectionPool();

        if (gotit)
            break;
    }

    ASSERT_TRUE(gotit);

    // Reopened so should succeed.
    dmHttpClient::Result r = dmHttpClient::Get(m_Client, "/sleep/10");
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

TEST_P(dmHttpClientTest, LargeFile)
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

TEST_P(dmHttpClientTest, TestHeaders)
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

TEST_P(dmHttpClientTest, Test404)
{
    for (int i = 0; i < 17; ++i)
    {
        dmHttpClient::Result r;
        m_Content = "";
        m_StatusCode = -1;
        r = dmHttpClient::Get(m_Client, "/does_not_exists");
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
            char buf[2] = { (char)((rand() % 255) - 128), 0 };
            m_ToPost.append(buf);
            sum += buf[0];
        }

        dmHttpClient::Result r;
        m_Content = "";
        m_StatusCode = -1;
        r = dmHttpClient::Post(m_Client, "/post");
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
    cache_params.m_Path = "tmp/cache";
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, m_URI.m_Hostname, m_URI.m_Port, strcmp(m_URI.m_Scheme, "https") == 0);
    ASSERT_NE((void*) 0, m_Client);

    for (int i = 0; i < 100; ++i)
    {
        m_Content = "";
        dmHttpClient::Result r;
        r = dmHttpClient::Get(m_Client, "/cached/0");
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
    ASSERT_EQ(100U, stats.m_Responses - stats.m_Reconnections);
    ASSERT_EQ(99U, stats.m_CachedResponses);
    cache_r = dmHttpCache::Close(params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
}

TEST_P(dmHttpClientTest, MaxAgeCache)
{
    dmHttpClient::Delete(m_Client);

    // Reinit client with http-cache
    dmHttpClient::NewParams params;
    params.m_Userdata = this;
    params.m_HttpContent = dmHttpClientTest::HttpContent;
    params.m_HttpHeader = dmHttpClientTest::HttpHeader;
    dmHttpCache::NewParams cache_params;
    cache_params.m_Path = "tmp/cache";
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, m_URI.m_Hostname, m_URI.m_Port, strcmp(m_URI.m_Scheme, "https") == 0);
    ASSERT_NE((void*) 0, m_Client);

    dmHttpClient::Result r;
    r = dmHttpClient::Get(m_Client, "/max-age-cached");
    ASSERT_TRUE((r == dmHttpClient::RESULT_OK && m_StatusCode == 200) || (r == dmHttpClient::RESULT_NOT_200_OK && m_StatusCode == 304));
    std::string val = m_Content;

    // The web-server is returning System.currentTimeMillis()
    // Ensure next ms
    dmTime::Sleep(1U);

    m_Content = "";
    r = dmHttpClient::Get(m_Client, "/max-age-cached");
    ASSERT_TRUE((r == dmHttpClient::RESULT_OK && m_StatusCode == 200) || (r == dmHttpClient::RESULT_NOT_200_OK && m_StatusCode == 304));
    ASSERT_STREQ(m_Content.c_str(), val.c_str());

    // max-age is 1 second
    dmTime::Sleep(1000000U);

    m_Content = "";
    r = dmHttpClient::Get(m_Client, "/max-age-cached");
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
    dmURI::Encode(buf, uri, sizeof(uri));

    m_Content = "";
    dmHttpClient::Result r = dmHttpClient::Get(m_Client, uri);
    ASSERT_EQ(dmHttpClient::RESULT_OK, r);
    ASSERT_STREQ(message, m_Content.c_str());
}


INSTANTIATE_TEST_CASE_P(dmHttpClientTest,
                        dmHttpClientTest,
                        ::testing::Values("http://localhost:" NAME_SOCKET, "https://localhost:" NAME_SOCKET_SSL));

INSTANTIATE_TEST_CASE_P(dmHttpClientTestSSL,
                        dmHttpClientTestSSL,
                        ::testing::Values("https://localhost:" NAME_SOCKET_SSL_TEST));


class dmHttpClientTestCache : public dmHttpClientTest
{
public:
    void GetFiles(bool check_content)
    {
        char buf[512];
        for (int i = 0; i < 100; ++i)
        {
            m_Content = "";
            dmHttpClient::Result r;
            snprintf(buf, sizeof(buf), "/tmp/http_files/%c.txt", 'a' + (i % 4));
            r = dmHttpClient::Get(m_Client, buf);
            if (r == dmHttpClient::RESULT_OK)
            {
                ASSERT_EQ(200, m_StatusCode);
            }
            else
            {
                ASSERT_EQ(dmHttpClient::RESULT_NOT_200_OK, r);
                ASSERT_EQ(304, m_StatusCode);
            }

            if (check_content)
            {
                if (i % 4 == 0)
                    ASSERT_EQ(std::string("You will find this data in a.txt and d.txt"), m_Content);
                else if (i % 4 == 1)
                    ASSERT_EQ(std::string("Some data in file b"), m_Content);
                else if (i % 4 == 2)
                    ASSERT_EQ(std::string("Some data in file c"), m_Content);
                else if (i % 4 == 3)
                    ASSERT_EQ(std::string("You will find this data in a.txt and d.txt"), m_Content);
                else
                    ASSERT_TRUE(false);
            }
        }
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
    dmHttpCache::NewParams cache_params;
    cache_params.m_Path = "tmp/cache";
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, m_URI.m_Hostname, m_URI.m_Port);
    ASSERT_NE((void*) 0, m_Client);

    GetFiles(true);

    dmHttpClient::Statistics stats;
    dmHttpClient::GetStatistics(m_Client, &stats);
    // NOTE: m_Responses is increased for every request. Therefore we must compensate for potential re-connections
    ASSERT_EQ(100U, stats.m_Responses - stats.m_Reconnections);
    ASSERT_EQ(96U, stats.m_CachedResponses);
    ASSERT_EQ(0U, stats.m_DirectFromCache);

    // Change consistency police to "trust-cache". All files are retrieved and should therefore be already verified
    dmHttpCache::SetConsistencyPolicy(params.m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);
    GetFiles(true);
    dmHttpClient::GetStatistics(m_Client, &stats);
    // Should be equivalent to above
    ASSERT_EQ(100U, stats.m_Responses - stats.m_Reconnections);
    // Should be equivalent to above
    ASSERT_EQ(96U, stats.m_CachedResponses);
    // All files directly from cache
    ASSERT_EQ(100U, stats.m_DirectFromCache);

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
    cache_params.m_Path = "tmp/cache";
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, m_URI.m_Hostname, m_URI.m_Port);
    ASSERT_NE((void*) 0, m_Client);

    // Change consistency police to "trust-cache". After the first four files are files should be directly retrieved from the cache.
    dmHttpCache::SetConsistencyPolicy(params.m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);

    GetFiles(true);

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
    cache_params.m_Path = "tmp/cache";
    dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, m_URI.m_Hostname, m_URI.m_Port);
    ASSERT_NE((void*) 0, m_Client);

    // Warmup cache
    GetFiles(true);

    // Reopen
    dmHttpClient::Delete(m_Client);
    cache_r = dmHttpCache::Close(params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    cache_r = dmHttpCache::Open(&cache_params, &params.m_HttpCache);
    ASSERT_EQ(dmHttpCache::RESULT_OK, cache_r);
    m_Client = dmHttpClient::New(&params, m_URI.m_Hostname, m_URI.m_Port);
    ASSERT_NE((void*) 0, m_Client);


    dmHttpCacheVerify::Result verify_r = dmHttpCacheVerify::VerifyCache(params.m_HttpCache, &m_URI, 60 * 60 * 24 * 5);
    ASSERT_EQ(dmHttpCacheVerify::RESULT_OK, verify_r);

    // Change consistency police to "trust-cache". After the first four files are files should be directly retrieved from the cache.
    dmHttpCache::SetConsistencyPolicy(params.m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);

    GetFiles(true);
    dmHttpClient::Statistics stats;
    dmHttpClient::GetStatistics(m_Client, &stats);
    // Zero responses, all direct from cache
    ASSERT_EQ(0U, stats.m_Responses);
    // Zero cached responses, all direct from cache
    ASSERT_EQ(0U, stats.m_CachedResponses);
    // All are loaded directly from the cache
    ASSERT_EQ(100U, stats.m_DirectFromCache);

    // Change a.txt file.
    FILE* a_file = fopen("tmp/http_files/a.txt", "wb");
    ASSERT_NE((FILE*) 0, a_file);
    fwrite("foo", 1, 3, a_file);
    fclose(a_file);

    GetFiles(true);
    dmHttpClient::GetStatistics(m_Client, &stats);
    // Zero responses, all direct from cache
    ASSERT_EQ(0U, stats.m_Responses);
    // Zero cached responses, all direct from cache
    ASSERT_EQ(0U, stats.m_CachedResponses);
    // All are loaded directly from the cache
    ASSERT_EQ(200U, stats.m_DirectFromCache);

    // Change a.txt file again
    a_file = fopen("tmp/http_files/a.txt", "wb");
    ASSERT_NE((FILE*) 0, a_file);
    fwrite("bar", 1, 3, a_file);
    fclose(a_file);

    // Change policy
    dmHttpCache::SetConsistencyPolicy(params.m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_VERIFY);
    GetFiles(false);
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

INSTANTIATE_TEST_CASE_P(dmHttpClientTestCache,
                        dmHttpClientTestCache,
                        ::testing::Values("http://localhost:" NAME_SOCKET));

#endif // #ifndef _WIN32

TEST(dmHttpClient, HostNotFound)
{
    dmHttpClient::NewParams params;
    dmHttpClient::HClient client = dmHttpClient::New(&params, "host_not_found", g_HttpPort);
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

static void Usage()
{
    dmLogError("Usage: <exe> <config>");
    dmLogError("Be sure to start the http server before starting this test.");
    dmLogError("You can use the config file created by the server");
}

int main(int argc, char **argv)
{
    if(argc > 1)
    {
        dmConfigFile::HConfig config;
        if( dmConfigFile::Load(argv[1], argc, (const char**)argv, &config) != dmConfigFile::RESULT_OK )
        {
            dmLogError("Could not read config file '%s'", argv[1]);
            Usage();
            return 1;
        }
        dmTestUtil::GetSocketsFromConfig(config, &g_HttpPort, &g_HttpPortSSL, &g_HttpPortSSLTest);
        dmConfigFile::Delete(config);
    }
    else
    {
        Usage();
        return 1;
    }

    dmLogSetlevel(DM_LOG_SEVERITY_INFO);
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}
