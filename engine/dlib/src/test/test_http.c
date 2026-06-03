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

#include <dmsdk/dlib/http.h>
#include <dlib/http/http_internal.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

#define TEST_COLOR_RESET "\033[0m"
#define TEST_COLOR_RED "\033[31m"
#define TEST_COLOR_GREEN "\033[32m"
#define TEST_COLOR_CYAN "\033[36m"

typedef volatile int32_t TestAtomic32;

static int               g_TestColorOutput = 0;

static void              TestAtomicStore32(TestAtomic32* ptr, int32_t value)
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

#if defined(_WIN32)
static int TestEnableVirtualTerminalProcessing(void)
{
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;

    if (stdout_handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    if (!GetConsoleMode(stdout_handle, &mode))
    {
        return 0;
    }

    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    {
        return 1;
    }

    return SetConsoleMode(stdout_handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}
#endif

static int TestStdoutSupportsColor(void)
{
    const char* no_color = getenv("NO_COLOR");
    if (no_color && no_color[0] != 0)
    {
        return 0;
    }

#if defined(_WIN32)
    if (_isatty(_fileno(stdout)) == 0)
    {
        return 0;
    }

    return TestEnableVirtualTerminalProcessing();
#else
    const char* term = getenv("TERM");
    return isatty(STDOUT_FILENO) && term && strcmp(term, "dumb") != 0;
#endif
}

static void PrintTestStatus(const char* color, const char* tag, const char* name)
{
    if (g_TestColorOutput)
    {
        printf("%s%s%s %s\n", color, tag, TEST_COLOR_RESET, name);
    }
    else
    {
        printf("%s %s\n", tag, name);
    }
    fflush(stdout);
}

static void PrintTestFailed(const char* name, int result)
{
    if (g_TestColorOutput)
    {
        fprintf(stderr, "%s[  FAILED  ]%s %s: %d\n", TEST_COLOR_RED, TEST_COLOR_RESET, name, result);
    }
    else
    {
        fprintf(stderr, "[  FAILED  ] %s: %d\n", name, result);
    }
}

#define TEST_CHECK(_EXPR) \
    do \
    { \
        if (!(_EXPR)) \
        { \
            fprintf(stderr, "TEST_CHECK failed at line %s:%d: %s\n", __FILE__, __LINE__, #_EXPR); \
            return __LINE__; \
        } \
    } while (0)
#define TEST_CHECK_EQ(_EXPECTED, _ACTUAL) \
    do \
    { \
        int expected = (int)(_EXPECTED); \
        int actual = (int)(_ACTUAL); \
        if (actual != expected) \
        { \
            fprintf(stderr, "TEST_CHECK_EQ failed at line %s:%d: expected %s == %d, got %s == %d\n", __FILE__, __LINE__, #_EXPECTED, expected, #_ACTUAL, actual); \
            return __LINE__; \
        } \
    } while (0)
#define TEST_CHECK_EQ_U32(_EXPECTED, _ACTUAL) \
    do \
    { \
        uint32_t expected = (uint32_t)(_EXPECTED); \
        uint32_t actual = (uint32_t)(_ACTUAL); \
        if (actual != expected) \
        { \
            fprintf(stderr, "TEST_CHECK_EQ_U32 failed at line %s:%d: expected %s == %u, got %s == %u\n", __FILE__, __LINE__, #_EXPECTED, expected, #_ACTUAL, actual); \
            return __LINE__; \
        } \
    } while (0)
#define TEST_CHECK_GT_U32(_ACTUAL, _MINIMUM) \
    do \
    { \
        uint32_t actual = (uint32_t)(_ACTUAL); \
        uint32_t minimum = (uint32_t)(_MINIMUM); \
        if (actual <= minimum) \
        { \
            fprintf(stderr, "TEST_CHECK_GT_U32 failed at line %s:%d: expected %s == %u > %s == %u\n", __FILE__, __LINE__, #_ACTUAL, actual, #_MINIMUM, minimum); \
            return __LINE__; \
        } \
    } while (0)
#define TEST_CHECK_NE_U32(_UNEXPECTED, _ACTUAL) \
    do \
    { \
        uint32_t unexpected = (uint32_t)(_UNEXPECTED); \
        uint32_t actual = (uint32_t)(_ACTUAL); \
        if (actual == unexpected) \
        { \
            fprintf(stderr, "TEST_CHECK_NE_U32 failed at line %s:%d: expected %s != %u, got %s == %u\n", __FILE__, __LINE__, #_UNEXPECTED, unexpected, #_ACTUAL, actual); \
            return __LINE__; \
        } \
    } while (0)
#define TEST_CHECK_STREQ(_EXPECTED, _ACTUAL) \
    do \
    { \
        const char* expected = (_EXPECTED); \
        const char* actual = (_ACTUAL); \
        if (strcmp(expected, actual) != 0) \
        { \
            fprintf(stderr, "TEST_CHECK_STREQ failed at line %s:%d: expected %s == '%s', got %s == '%s'\n", __FILE__, __LINE__, #_EXPECTED, expected, #_ACTUAL, actual); \
            return __LINE__; \
        } \
    } while (0)
#define RUN_TEST(_NAME, _CALL) \
    do \
    { \
        PrintTestStatus(TEST_COLOR_CYAN, "[ RUN      ]", (_NAME)); \
        result = (_CALL); \
        if (result != 0) \
        { \
            PrintTestFailed((_NAME), result); \
            return result; \
        } \
        PrintTestStatus(TEST_COLOR_GREEN, "[       OK ]", (_NAME)); \
    } while (0)

typedef struct HttpTestServerConfig
{
    char m_ServerIP[128];
    int  m_ServerPort;
} HttpTestServerConfig;

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
    uint32_t i;
    uint32_t name_size = (uint32_t)strlen(name);

    if (!response->m_Header || response->m_HeaderSize <= name_size || response->m_Header[name_size] != ':')
    {
        return 0;
    }

    for (i = 0; i < name_size; ++i)
    {
        if (tolower((unsigned char)response->m_Header[i]) != tolower((unsigned char)name[i]))
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
    const char* value_start;
    uint32_t    value_size;

    if (!HeaderNameEquals(response, name))
    {
        return 0;
    }

    value_start = response->m_Header + name_size + 1;
    value_size = response->m_HeaderSize - name_size - 1;
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

    if (response->m_Event == HTTP_RESPONSE_EVENT_HEADER)
    {
        uint32_t content_length = 0;

        if (test_response->m_HeaderEventCount == 0)
        {
            test_response->m_HeaderStatusCode = response->m_StatusCode;
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
    else if (response->m_Event == HTTP_RESPONSE_EVENT_DATA)
    {
        test_response->m_TotalDataSize += response->m_DataSize;
        test_response->m_DataEventCount++;

        uint32_t remaining = (uint32_t)sizeof(test_response->m_Data) - test_response->m_DataSize - 1;
        uint32_t size = response->m_DataSize < remaining ? response->m_DataSize : remaining;
        memcpy(test_response->m_Data + test_response->m_DataSize, response->m_Data, size);
        test_response->m_DataSize += size;
        test_response->m_Data[test_response->m_DataSize] = 0;

        if (test_response->m_CancelOnDataEvent)
        {
            return HTTP_CALLBACK_RESULT_CANCEL;
        }
    }
    else if (response->m_Event == HTTP_RESPONSE_EVENT_PROGRESS)
    {
        test_response->m_ProgressEventCount++;
        test_response->m_BytesSent = response->m_BytesSent;
        test_response->m_BytesReceived = response->m_BytesReceived;
        test_response->m_BytesTotal = response->m_BytesTotal;
    }
    else if (response->m_Event == HTTP_RESPONSE_EVENT_COMPLETE)
    {
        test_response->m_Result = response->m_Result;
        test_response->m_StatusCode = response->m_StatusCode;
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

static int TestRequestConfiguration(void)
{
    HttpRequest* request = 0;
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetMethod(request, "GET"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, "https://example.com/items"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpAddHeader(request, "Accept: application/json"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetRequestBody(request, "request-body", 12));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponsePath(request, "/tmp/response.bin"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetProxy(request, "http://127.0.0.1:8080"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetIgnoreCache(request, 1));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetChunkedTransfer(request, 0));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetReportProgress(request, 1));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetBasicAuth(request, "user", "password"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetBearerAuth(request, "token"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetTimeout(request, 1000));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, 0));

    HttpDeleteRequest(request);
    return 0;
}

static int TestPostSendsData(const HttpTestServerConfig* server_config)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;
    HttpTestResponse  response;
    char              url[256];
    int               result;
    const char*       body = "abc";

    memset(&request_handle, 0, sizeof(request_handle));
    memset(&response, 0, sizeof(response));
    response.m_StatusCode = -1;
    response.m_Result = HTTP_RESULT_UNKNOWN;

    snprintf(url, sizeof(url), "http://%s:%d/post", server_config->m_ServerIP, server_config->m_ServerPort);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetMethod(request, "POST"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetRequestBody(request, body, (uint32_t)strlen(body)));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, &response));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_NE_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    result = WaitForComplete(&response, 10);
    if (result != 0)
    {
        HttpDeleteServiceInternal(service);
        return result;
    }

    HttpDeleteServiceInternal(service);

    TEST_CHECK_EQ(HTTP_RESULT_OK, response.m_Result);
    TEST_CHECK_EQ(200, response.m_StatusCode);
    TEST_CHECK_EQ(294, strtol(response.m_Data, 0, 10));
    return 0;
}

static int TestProgressEvents(const HttpTestServerConfig* server_config)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;
    HttpTestResponse  response;
    char              url[256];
    int               result;
    const uint32_t    response_size = 1024;

    memset(&request_handle, 0, sizeof(request_handle));
    memset(&response, 0, sizeof(response));
    response.m_StatusCode = -1;
    response.m_Result = HTTP_RESULT_UNKNOWN;

    snprintf(url, sizeof(url), "http://%s:%d/arb/%u", server_config->m_ServerIP, server_config->m_ServerPort, response_size);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetReportProgress(request, 1));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, &response));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_NE_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    result = WaitForComplete(&response, 10);
    if (result != 0)
    {
        HttpDeleteServiceInternal(service);
        return result;
    }

    HttpDeleteServiceInternal(service);

    TEST_CHECK_EQ(HTTP_RESULT_OK, response.m_Result);
    TEST_CHECK_EQ(200, response.m_StatusCode);
    TEST_CHECK_GT_U32(response.m_ProgressEventCount, 0);
    TEST_CHECK_EQ_U32(response_size, response.m_BytesReceived);
    TEST_CHECK_EQ(response_size, response.m_BytesTotal);
    return 0;
}

static int TestGetReturnsData(const HttpTestServerConfig* server_config)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;
    HttpTestResponse  response;
    char              url[256];
    int               result;

    memset(&request_handle, 0, sizeof(request_handle));
    memset(&response, 0, sizeof(response));
    response.m_StatusCode = -1;
    response.m_Result = HTTP_RESULT_UNKNOWN;

    snprintf(url, sizeof(url), "http://%s:%d/echo/Hello", server_config->m_ServerIP, server_config->m_ServerPort);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpAddHeader(request, "Accept: text/plain"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, &response));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_NE_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    result = WaitForComplete(&response, 10);
    if (result != 0)
    {
        HttpDeleteServiceInternal(service);
        return result;
    }

    TEST_CHECK_EQ(HTTP_RESULT_INVAL, HttpCancelRequest(service, request_handle));

    HttpDeleteServiceInternal(service);

    TEST_CHECK_EQ(HTTP_RESULT_OK, response.m_Result);
    TEST_CHECK_EQ(200, response.m_StatusCode);
    TEST_CHECK_STREQ("Hello", response.m_Data);
    TEST_CHECK_EQ_U32(5, response.m_TotalDataSize);
    return 0;
}

static int TestAddReturnsData(const HttpTestServerConfig* server_config)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;
    HttpTestResponse  response;
    char              url[256];
    int               result;

    memset(&request_handle, 0, sizeof(request_handle));
    memset(&response, 0, sizeof(response));
    response.m_StatusCode = -1;
    response.m_Result = HTTP_RESULT_UNKNOWN;

    snprintf(url, sizeof(url), "http://%s:%d/add/10/20", server_config->m_ServerIP, server_config->m_ServerPort);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpAddHeader(request, "X-Scale: 3"));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, &response));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_NE_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    result = WaitForComplete(&response, 10);
    if (result != 0)
    {
        HttpDeleteServiceInternal(service);
        return result;
    }

    HttpDeleteServiceInternal(service);

    TEST_CHECK_EQ(HTTP_RESULT_OK, response.m_Result);
    TEST_CHECK_EQ(200, response.m_StatusCode);
    TEST_CHECK_EQ(90, strtol(response.m_Data, 0, 10));
    TEST_CHECK_EQ_U32(response.m_DataEventCount, response.m_CompleteDataEventCount);
    return 0;
}

static int TestResponseHeaders(const HttpTestServerConfig* server_config)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;
    HttpTestResponse  response;
    char              url[256];
    int               result;
    const uint32_t    response_size = 123;

    memset(&request_handle, 0, sizeof(request_handle));
    memset(&response, 0, sizeof(response));
    response.m_StatusCode = -1;
    response.m_HeaderStatusCode = -1;
    response.m_Result = HTTP_RESULT_UNKNOWN;

    snprintf(url, sizeof(url), "http://%s:%d/arb/%u", server_config->m_ServerIP, server_config->m_ServerPort, response_size);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, &response));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_NE_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    result = WaitForComplete(&response, 10);
    if (result != 0)
    {
        HttpDeleteServiceInternal(service);
        return result;
    }

    HttpDeleteServiceInternal(service);

    TEST_CHECK_EQ(HTTP_RESULT_OK, response.m_Result);
    TEST_CHECK_EQ(200, response.m_StatusCode);
    TEST_CHECK_EQ(200, response.m_HeaderStatusCode);
    TEST_CHECK_EQ_U32(response_size, response.m_TotalDataSize);
    TEST_CHECK_GT_U32(response.m_HeaderEventCount, 0);
    TEST_CHECK_EQ_U32(response.m_HeaderEventCount, response.m_HeaderBeforeDataEventCount);
    TEST_CHECK_EQ_U32(response.m_HeaderEventCount, response.m_CompleteHeaderEventCount);
    TEST_CHECK_EQ_U32(1, response.m_HasContentLengthHeader);
    TEST_CHECK_EQ_U32(response_size, response.m_ContentLengthHeaderValue);
    return 0;
}

static int TestLargeResponseStreamsChunks(const HttpTestServerConfig* server_config)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;
    HttpTestResponse  response;
    char              url[256];
    int               result;
    const uint32_t    response_size = 128 * 1024;

    memset(&request_handle, 0, sizeof(request_handle));
    memset(&response, 0, sizeof(response));
    response.m_StatusCode = -1;
    response.m_Result = HTTP_RESULT_UNKNOWN;

    snprintf(url, sizeof(url), "http://%s:%d/arb/%u", server_config->m_ServerIP, server_config->m_ServerPort, response_size);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, &response));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_NE_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    result = WaitForComplete(&response, 10);
    if (result != 0)
    {
        HttpDeleteServiceInternal(service);
        return result;
    }

    HttpDeleteServiceInternal(service);

    TEST_CHECK_EQ(HTTP_RESULT_OK, response.m_Result);
    TEST_CHECK_EQ(200, response.m_StatusCode);
    TEST_CHECK_EQ_U32(response_size, response.m_TotalDataSize);
    TEST_CHECK_GT_U32(response.m_DataEventCount, 1);
    TEST_CHECK_EQ_U32(response.m_DataEventCount, response.m_CompleteDataEventCount);
    return 0;
}

static int TestPushRequiresURL(void)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;

    memset(&request_handle, 0, sizeof(request_handle));

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_INVAL, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_EQ_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);
    TEST_CHECK_EQ(HTTP_RESULT_INVAL, HttpCancelRequest(service, request_handle));

    HttpDeleteRequest(request);
    HttpDeleteServiceInternal(service);
    return 0;
}

static int TestCancelRequest(const HttpTestServerConfig* server_config)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;
    HttpTestResponse  response;
    char              url[256];
    int               result;

    memset(&request_handle, 0, sizeof(request_handle));
    memset(&response, 0, sizeof(response));
    response.m_StatusCode = -1;
    response.m_Result = HTTP_RESULT_UNKNOWN;

    snprintf(url, sizeof(url), "http://%s:%d/sleep/1000", server_config->m_ServerIP, server_config->m_ServerPort);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, &response));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_NE_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpCancelRequest(service, request_handle));

    result = WaitForComplete(&response, 10);
    if (result != 0)
    {
        HttpDeleteServiceInternal(service);
        return result;
    }

    HttpDeleteServiceInternal(service);

    TEST_CHECK_EQ(HTTP_RESULT_INVAL, response.m_Result);
    return 0;
}

static int TestCancelFromCallback(const HttpTestServerConfig* server_config)
{
    HttpService*      service = 0;
    HttpRequest*      request = 0;
    HttpRequestHandle request_handle;
    HttpTestResponse  response;
    char              url[256];
    int               result;
    const uint32_t    response_size = 128 * 1024;

    memset(&request_handle, 0, sizeof(request_handle));
    memset(&response, 0, sizeof(response));
    response.m_StatusCode = -1;
    response.m_Result = HTTP_RESULT_UNKNOWN;
    response.m_CancelOnDataEvent = 1;

    snprintf(url, sizeof(url), "http://%s:%d/arb/%u", server_config->m_ServerIP, server_config->m_ServerPort, response_size);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewServiceInternal(1, &service));
    TEST_CHECK(service != 0);

    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpNewRequest(&request));
    TEST_CHECK(request != 0);
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetURL(request, url));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpSetResponseCallback(request, HttpResponse, &response));
    TEST_CHECK_EQ(HTTP_RESULT_OK, HttpPushRequest(service, request, &request_handle));
    TEST_CHECK_NE_U32(HTTP_REQUEST_HANDLE_INVALID, request_handle);

    result = WaitForComplete(&response, 10);
    if (result != 0)
    {
        HttpDeleteServiceInternal(service);
        return result;
    }

    HttpDeleteServiceInternal(service);

    TEST_CHECK_GT_U32(response.m_DataEventCount, 0);
    TEST_CHECK_EQ(HTTP_RESULT_INVAL, response.m_Result);
    TEST_CHECK_EQ_U32(response.m_DataEventCount, response.m_CompleteDataEventCount);
    return 0;
}

int main(int argc, char** argv)
{
    HttpTestServerConfig server_config;
    char                 server_status[192];
    int                  result;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <server config>\n", argv[0]);
        return 1;
    }

    result = ReadServerConfig(argv[1], &server_config);
    if (result != 0)
    {
        return result;
    }

    g_TestColorOutput = TestStdoutSupportsColor();
    snprintf(server_status, sizeof(server_status), "test server %s:%d", server_config.m_ServerIP, server_config.m_ServerPort);
    PrintTestStatus(TEST_COLOR_CYAN, "[ HTTP     ]", server_status);

    RUN_TEST("TestRequestConfiguration", TestRequestConfiguration());
    RUN_TEST("TestGetReturnsData", TestGetReturnsData(&server_config));
    RUN_TEST("TestPostSendsData", TestPostSendsData(&server_config));
    RUN_TEST("TestAddReturnsData", TestAddReturnsData(&server_config));
    RUN_TEST("TestResponseHeaders", TestResponseHeaders(&server_config));
    RUN_TEST("TestProgressEvents", TestProgressEvents(&server_config));
    RUN_TEST("TestPushRequiresURL", TestPushRequiresURL());
    RUN_TEST("TestLargeResponseStreamsChunks", TestLargeResponseStreamsChunks(&server_config));
    RUN_TEST("TestCancelRequest", TestCancelRequest(&server_config));
    RUN_TEST("TestCancelFromCallback", TestCancelFromCallback(&server_config));

    PrintTestStatus(TEST_COLOR_GREEN, "[  PASSED  ]", "test_http");

    return 0;
}
