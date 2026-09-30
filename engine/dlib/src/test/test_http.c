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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dmsdk/dlib/http.h>
#include <dlib/http/http_internal.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

typedef volatile int32_t TestAtomic32;

static void TestAtomicStore32(TestAtomic32* ptr, int32_t value)
{
#if defined(_WIN32)
    InterlockedExchange((volatile long*)ptr, (long)value);
#else
    __sync_lock_test_and_set(ptr, value);
#endif
}

static int32_t TestAtomicGet32(TestAtomic32* ptr)
{
#if defined(_WIN32)
    return (int32_t)InterlockedCompareExchange((volatile long*)ptr, 0, 0);
#else
    return __sync_fetch_and_add(ptr, 0);
#endif
}

typedef struct HttpTestServerConfig
{
    char m_ServerIP[128];
    int  m_ServerPort;
} HttpTestServerConfig;

static HttpTestServerConfig g_ServerConfig;

typedef struct HttpTestResponse
{
    char         m_Data[256];
    uint32_t     m_DataSize;
    uint32_t     m_HeaderEventCount;
    uint32_t     m_HeaderBeforeDataEventCount;
    uint32_t     m_CompleteHeaderEventCount;
    uint32_t     m_TotalDataSize;
    uint32_t     m_DataEventCount;
    uint32_t     m_CompleteDataEventCount;
    uint32_t     m_ProgressEventCount;
    uint32_t     m_BytesSent;
    uint32_t     m_BytesReceived;
    int32_t      m_BytesTotal;
    int          m_StatusCode;
    int          m_HeaderStatusCode;
    HttpResult   m_Result;
    uint32_t     m_HasContentLengthHeader;
    uint32_t     m_ContentLengthHeaderValue;
    uint32_t     m_CancelOnDataEvent;
    TestAtomic32 m_Complete;
} HttpTestResponse;

typedef struct Http
{
    HttpService*     m_Service;
    HttpRequest*     m_Request;
    // Callback data must remain alive until teardown joins the service threads.
    HttpTestResponse m_Response;
} Http;

JC_TEST_FIXTURE_SETUP(Http)
{
    Http* self = jc_test_fixture;
    self->m_Response.m_StatusCode = -1;
    self->m_Response.m_HeaderStatusCode = -1;
    self->m_Response.m_Result = HTTP_RESULT_UNKNOWN;

    ASSERT_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &self->m_Service));
    ASSERT_TRUE(self->m_Service != 0);
    ASSERT_EQ(HTTP_RESULT_OK, HttpNewRequest(&self->m_Request));
    ASSERT_TRUE(self->m_Request != 0);
}

JC_TEST_FIXTURE_TEARDOWN(Http)
{
    HttpDeleteServiceInternal(jc_test_fixture->m_Service);
    HttpDeleteRequest(jc_test_fixture->m_Request);
}

static HttpResult PushRequest(Http* self, HttpRequestHandle* request_handle)
{
    HttpResult result = HttpPushRequest(self->m_Service, self->m_Request, request_handle);
    if (result == HTTP_RESULT_OK)
    {
        // The service owns successfully submitted requests.
        self->m_Request = 0;
    }
    return result;
}

static void TestSleep(uint32_t milliseconds)
{
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

static char* Trim(char* value)
{
    char* end;
    while (isspace((unsigned char)*value))
    {
        ++value;
    }

    end = value + strlen(value);
    while (end > value && isspace((unsigned char)*(end - 1)))
    {
        --end;
    }
    *end = 0;
    return value;
}

static int ReadServerConfig(const char* path, HttpTestServerConfig* config)
{
    char  line[256];
    char  section[64] = "";
    FILE* file = fopen(path, "r");
    if (!file)
    {
        fprintf(stderr, "Failed to open server config '%s'\n", path);
        return __LINE__;
    }

    strcpy(config->m_ServerIP, "localhost");
    config->m_ServerPort = -1;

    while (fgets(line, sizeof(line), file))
    {
        char* value = Trim(line);
        char* equals;

        if (value[0] == 0 || value[0] == '#' || value[0] == ';')
        {
            continue;
        }

        if (value[0] == '[')
        {
            char* close = strchr(value, ']');
            if (close)
            {
                *close = 0;
                snprintf(section, sizeof(section), "%s", value + 1);
            }
            continue;
        }

        if (strcmp(section, "server") != 0)
        {
            continue;
        }

        equals = strchr(value, '=');
        if (!equals)
        {
            continue;
        }

        *equals = 0;
        value = Trim(value);
        equals = Trim(equals + 1);

        if (strcmp(value, "ip") == 0)
        {
            snprintf(config->m_ServerIP, sizeof(config->m_ServerIP), "%s", equals);
        }
        else if (strcmp(value, "socket") == 0)
        {
            config->m_ServerPort = atoi(equals);
        }
    }

    fclose(file);

    if (config->m_ServerPort <= 0)
    {
        fprintf(stderr, "Invalid server socket in config '%s'\n", path);
        return __LINE__;
    }

    return 0;
}

static int HeaderNameEquals(const HttpResponseInfo* response, const char* name)
{
    uint32_t    i;
    uint32_t    name_size = (uint32_t)strlen(name);
    const char* header = HttpResponseGetHeader(response);
    uint32_t    header_size = HttpResponseGetHeaderSize(response);

    if (!header || header_size <= name_size || header[name_size] != ':')
    {
        return 0;
    }

    for (i = 0; i < name_size; ++i)
    {
        if (tolower((unsigned char)header[i]) != tolower((unsigned char)name[i]))
        {
            return 0;
        }
    }

    return 1;
}

static int ParseHeaderUInt32(const HttpResponseInfo* response, const char* name, uint32_t* value)
{
    char        value_buffer[32];
    uint32_t    name_size = (uint32_t)strlen(name);
    const char* header = HttpResponseGetHeader(response);
    uint32_t    header_size = HttpResponseGetHeaderSize(response);
    const char* value_start;
    uint32_t    value_size;

    if (!HeaderNameEquals(response, name))
    {
        return 0;
    }

    value_start = header + name_size + 1;
    value_size = header_size - name_size - 1;
    while (value_size > 0 && isspace((unsigned char)*value_start))
    {
        ++value_start;
        --value_size;
    }

    if (value_size >= sizeof(value_buffer))
    {
        return 0;
    }

    memcpy(value_buffer, value_start, value_size);
    value_buffer[value_size] = 0;
    *value = (uint32_t)strtoul(value_buffer, 0, 10);
    return 1;
}

static HttpCallbackResult HttpResponse(HttpRequest* request, void* user_data, const HttpResponseInfo* response)
{
    HttpTestResponse* test_response = (HttpTestResponse*)user_data;
    (void)request;

    if (HttpResponseGetEvent(response) == HTTP_RESPONSE_EVENT_HEADER)
    {
        uint32_t content_length = 0;

        if (test_response->m_HeaderEventCount == 0)
        {
            test_response->m_HeaderStatusCode = HttpResponseGetStatusCode(response);
        }

        test_response->m_HeaderEventCount++;
        if (test_response->m_DataEventCount == 0)
        {
            test_response->m_HeaderBeforeDataEventCount++;
        }

        if (ParseHeaderUInt32(response, "Content-Length", &content_length))
        {
            test_response->m_HasContentLengthHeader = 1;
            test_response->m_ContentLengthHeaderValue = content_length;
        }
    }
    else if (HttpResponseGetEvent(response) == HTTP_RESPONSE_EVENT_DATA)
    {
        uint32_t    data_size = HttpResponseGetDataSize(response);
        const void* data = HttpResponseGetData(response);

        test_response->m_TotalDataSize += data_size;
        test_response->m_DataEventCount++;

        uint32_t remaining = (uint32_t)sizeof(test_response->m_Data) - test_response->m_DataSize - 1;
        uint32_t size = data_size < remaining ? data_size : remaining;
        memcpy(test_response->m_Data + test_response->m_DataSize, data, size);
        test_response->m_DataSize += size;
        test_response->m_Data[test_response->m_DataSize] = 0;

        if (test_response->m_CancelOnDataEvent)
        {
            return HTTP_CALLBACK_RESULT_CANCEL;
        }
    }
    else if (HttpResponseGetEvent(response) == HTTP_RESPONSE_EVENT_PROGRESS)
    {
        test_response->m_ProgressEventCount++;
        test_response->m_BytesSent = HttpResponseGetBytesSent(response);
        test_response->m_BytesReceived = HttpResponseGetBytesReceived(response);
        test_response->m_BytesTotal = HttpResponseGetBytesTotal(response);
    }
    else if (HttpResponseGetEvent(response) == HTTP_RESPONSE_EVENT_COMPLETE)
    {
        test_response->m_Result = HttpResponseGetResult(response);
        test_response->m_StatusCode = HttpResponseGetStatusCode(response);
        test_response->m_CompleteHeaderEventCount = test_response->m_HeaderEventCount;
        test_response->m_CompleteDataEventCount = test_response->m_DataEventCount;
        TestAtomicStore32(&test_response->m_Complete, 1);
    }

    return HTTP_CALLBACK_RESULT_CONTINUE;
}

static int WaitForComplete(HttpTestResponse* response, uint32_t timeout_seconds)
{
    time_t start = time(0);
    while (TestAtomicGet32(&response->m_Complete) == 0)
    {
        if ((uint32_t)(time(0) - start) >= timeout_seconds)
        {
            fprintf(stderr, "Timed out waiting for HTTP response\n");
            return __LINE__;
        }
        TestSleep(1);
    }
    return 0;
}

TEST_F(Http, RequestConfiguration)
{
    HttpRequest* request = jc_test_fixture->m_Request;

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetMethod(request, "GET"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, "https://example.com/items"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpAddHeader(request, "Accept: application/json"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetRequestBody(request, "request-body", 12));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponsePath(request, "/tmp/response.bin"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetProxy(request, "http://127.0.0.1:8080"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetIgnoreCache(request, 1));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetChunkedTransfer(request, 0));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetReportProgress(request, 1));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetBasicAuth(request, "user", "password"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetBearerAuth(request, "token"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetTimeout(request, 1000));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, 0));
}

TEST_F(Http, PostSendsData)
{
    HttpRequest*      request = jc_test_fixture->m_Request;
    HttpRequestHandle request_handle;
    HttpTestResponse* response = &jc_test_fixture->m_Response;
    char              url[256];
    const char*       body = "abc";

    memset(&request_handle, 0, sizeof(request_handle));

    snprintf(url, sizeof(url), "http://%s:%d/post", g_ServerConfig.m_ServerIP, g_ServerConfig.m_ServerPort);

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetMethod(request, "POST"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetRequestBody(request, body, (uint32_t)strlen(body)));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, response));
    ASSERT_EQ(HTTP_RESULT_OK, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_NE(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    ASSERT_EQ(0, WaitForComplete(response, 10));

    ASSERT_EQ(HTTP_RESULT_OK, response->m_Result);
    ASSERT_EQ(200, response->m_StatusCode);
    ASSERT_EQ(294, strtol(response->m_Data, 0, 10));
}

TEST_F(Http, ProgressEvents)
{
    HttpRequest*      request = jc_test_fixture->m_Request;
    HttpRequestHandle request_handle;
    HttpTestResponse* response = &jc_test_fixture->m_Response;
    char              url[256];
    const uint32_t    response_size = 1024;

    memset(&request_handle, 0, sizeof(request_handle));

    snprintf(url, sizeof(url), "http://%s:%d/arb/%u", g_ServerConfig.m_ServerIP, g_ServerConfig.m_ServerPort, response_size);

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetReportProgress(request, 1));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, response));
    ASSERT_EQ(HTTP_RESULT_OK, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_NE(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    ASSERT_EQ(0, WaitForComplete(response, 10));

    ASSERT_EQ(HTTP_RESULT_OK, response->m_Result);
    ASSERT_EQ(200, response->m_StatusCode);
    ASSERT_GT(response->m_ProgressEventCount, 0);
    ASSERT_EQ(response_size, response->m_BytesReceived);
    ASSERT_EQ(response_size, response->m_BytesTotal);
}

TEST_F(Http, GetReturnsData)
{
    HttpService*      service = jc_test_fixture->m_Service;
    HttpRequest*      request = jc_test_fixture->m_Request;
    HttpRequestHandle request_handle;
    HttpTestResponse* response = &jc_test_fixture->m_Response;
    char              url[256];

    memset(&request_handle, 0, sizeof(request_handle));

    snprintf(url, sizeof(url), "http://%s:%d/echo/Hello", g_ServerConfig.m_ServerIP, g_ServerConfig.m_ServerPort);

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    ASSERT_EQ(HTTP_RESULT_OK, HttpAddHeader(request, "Accept: text/plain"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, response));
    ASSERT_EQ(HTTP_RESULT_OK, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_NE(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    ASSERT_EQ(0, WaitForComplete(response, 10));
    ASSERT_EQ(HTTP_RESULT_INVAL, HttpCancelRequest(service, request_handle));

    ASSERT_EQ(HTTP_RESULT_OK, response->m_Result);
    ASSERT_EQ(200, response->m_StatusCode);
    ASSERT_STREQ("Hello", response->m_Data);
    ASSERT_EQ(5, response->m_TotalDataSize);
}

TEST_F(Http, AddReturnsData)
{
    HttpRequest*      request = jc_test_fixture->m_Request;
    HttpRequestHandle request_handle;
    HttpTestResponse* response = &jc_test_fixture->m_Response;
    char              url[256];

    memset(&request_handle, 0, sizeof(request_handle));

    snprintf(url, sizeof(url), "http://%s:%d/add/10/20", g_ServerConfig.m_ServerIP, g_ServerConfig.m_ServerPort);

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    ASSERT_EQ(HTTP_RESULT_OK, HttpAddHeader(request, "X-Scale: 3"));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, response));
    ASSERT_EQ(HTTP_RESULT_OK, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_NE(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    ASSERT_EQ(0, WaitForComplete(response, 10));

    ASSERT_EQ(HTTP_RESULT_OK, response->m_Result);
    ASSERT_EQ(200, response->m_StatusCode);
    ASSERT_EQ(90, strtol(response->m_Data, 0, 10));
    ASSERT_EQ(response->m_DataEventCount, response->m_CompleteDataEventCount);
}

TEST_F(Http, ResponseHeaders)
{
    HttpRequest*      request = jc_test_fixture->m_Request;
    HttpRequestHandle request_handle;
    HttpTestResponse* response = &jc_test_fixture->m_Response;
    char              url[256];
    const uint32_t    response_size = 123;

    memset(&request_handle, 0, sizeof(request_handle));

    snprintf(url, sizeof(url), "http://%s:%d/arb/%u", g_ServerConfig.m_ServerIP, g_ServerConfig.m_ServerPort, response_size);

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, response));
    ASSERT_EQ(HTTP_RESULT_OK, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_NE(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    ASSERT_EQ(0, WaitForComplete(response, 10));

    ASSERT_EQ(HTTP_RESULT_OK, response->m_Result);
    ASSERT_EQ(200, response->m_StatusCode);
    ASSERT_EQ(200, response->m_HeaderStatusCode);
    ASSERT_EQ(response_size, response->m_TotalDataSize);
    ASSERT_GT(response->m_HeaderEventCount, 0);
    ASSERT_EQ(response->m_HeaderEventCount, response->m_HeaderBeforeDataEventCount);
    ASSERT_EQ(response->m_HeaderEventCount, response->m_CompleteHeaderEventCount);
    ASSERT_EQ(1, response->m_HasContentLengthHeader);
    ASSERT_EQ(response_size, response->m_ContentLengthHeaderValue);
}

TEST_F(Http, LargeResponseStreamsChunks)
{
    HttpRequest*      request = jc_test_fixture->m_Request;
    HttpRequestHandle request_handle;
    HttpTestResponse* response = &jc_test_fixture->m_Response;
    char              url[256];
    const uint32_t    response_size = 128 * 1024;

    memset(&request_handle, 0, sizeof(request_handle));

    snprintf(url, sizeof(url), "http://%s:%d/arb/%u", g_ServerConfig.m_ServerIP, g_ServerConfig.m_ServerPort, response_size);

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, response));
    ASSERT_EQ(HTTP_RESULT_OK, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_NE(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    ASSERT_EQ(0, WaitForComplete(response, 10));

    ASSERT_EQ(HTTP_RESULT_OK, response->m_Result);
    ASSERT_EQ(200, response->m_StatusCode);
    ASSERT_EQ(response_size, response->m_TotalDataSize);
    ASSERT_GT(response->m_DataEventCount, 1);
    ASSERT_EQ(response->m_DataEventCount, response->m_CompleteDataEventCount);
}

TEST_F(Http, PushRequiresURL)
{
    HttpService*      service = jc_test_fixture->m_Service;
    HttpRequestHandle request_handle;

    memset(&request_handle, 0, sizeof(request_handle));

    ASSERT_EQ(HTTP_RESULT_INVAL, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_EQ(HTTP_REQUEST_HANDLE_INVALID, request_handle);
    ASSERT_EQ(HTTP_RESULT_INVAL, HttpCancelRequest(service, request_handle));
}

TEST_F(Http, CancelRequest)
{
    HttpService*      service = jc_test_fixture->m_Service;
    HttpRequest*      request = jc_test_fixture->m_Request;
    HttpRequestHandle request_handle;
    HttpTestResponse* response = &jc_test_fixture->m_Response;
    char              url[256];

    memset(&request_handle, 0, sizeof(request_handle));

    snprintf(url, sizeof(url), "http://%s:%d/sleep/1000", g_ServerConfig.m_ServerIP, g_ServerConfig.m_ServerPort);

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, response));
    ASSERT_EQ(HTTP_RESULT_OK, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_NE(HTTP_REQUEST_HANDLE_INVALID, request_handle);
    ASSERT_EQ(HTTP_RESULT_OK, HttpCancelRequest(service, request_handle));

    ASSERT_EQ(0, WaitForComplete(response, 10));

    ASSERT_EQ(HTTP_RESULT_INVAL, response->m_Result);
}

TEST_F(Http, CancelFromCallback)
{
    HttpRequest*      request = jc_test_fixture->m_Request;
    HttpRequestHandle request_handle;
    HttpTestResponse* response = &jc_test_fixture->m_Response;
    char              url[256];
    const uint32_t    response_size = 128 * 1024;

    memset(&request_handle, 0, sizeof(request_handle));
    response->m_CancelOnDataEvent = 1;

    snprintf(url, sizeof(url), "http://%s:%d/arb/%u", g_ServerConfig.m_ServerIP, g_ServerConfig.m_ServerPort, response_size);

    ASSERT_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    ASSERT_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, response));
    ASSERT_EQ(HTTP_RESULT_OK, PushRequest(jc_test_fixture, &request_handle));
    ASSERT_NE(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    ASSERT_EQ(0, WaitForComplete(response, 10));

    ASSERT_GT(response->m_DataEventCount, 0);
    ASSERT_EQ(HTTP_RESULT_INVAL, response->m_Result);
    ASSERT_EQ(response->m_DataEventCount, response->m_CompleteDataEventCount);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <server config>\n", argv[0]);
        return 1;
    }

    int result = ReadServerConfig(argv[1], &g_ServerConfig);
    if (result != 0)
    {
        return result;
    }

    return jc_test_run_all();
}
