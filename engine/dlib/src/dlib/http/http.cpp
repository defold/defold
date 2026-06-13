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

#include <dmsdk/dlib/http.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/dstrings.h>

#include "http_private.h"
#include "http_service.h"

static HttpRequestHandle InvalidRequestHandle()
{
    return HTTP_REQUEST_HANDLE_INVALID;
}

HttpRequest::HttpRequest()
{
    m_URL = 0;
    m_Method = strdup("GET");
    m_Headers = 0;
    m_HeaderCount = 0;
    m_HeaderCapacity = 0;
    m_Body = 0;
    m_BodyLength = 0;
    m_Path = 0;
    m_Proxy = 0;
    m_Callback = 0;
    m_UserData = 0;
    m_Service = 0;
    m_Owner = 0;
    m_Handle = InvalidRequestHandle();
    m_Timeout = 0;
    m_IgnoreCache = 0;
    m_ChunkedTransfer = 1;
    m_ReportProgress = 0;
    m_CancelFlag = 0;
    m_RangeStart = 0;
    m_RangeEnd = 0;
    m_DocumentSize = 0;
    m_State = HTTP_REQUEST_STATE_CREATED;
}

static bool EnsureHeaderCapacity(HttpRequest* request)
{
    if (request->m_HeaderCount == request->m_HeaderCapacity)
    {
        uint32_t capacity = request->m_HeaderCapacity == 0 ? 4 : request->m_HeaderCapacity * 2;
        char**   headers = (char**)realloc(request->m_Headers, sizeof(char*) * capacity);
        if (!headers)
        {
            return false;
        }

        request->m_Headers = headers;
        request->m_HeaderCapacity = capacity;
    }

    return true;
}

static char* DuplicateString(const char* value)
{
    return value ? strdup(value) : 0;
}

static void FreeHeaders(HttpRequest* request)
{
    for (uint32_t i = 0; i < request->m_HeaderCount; ++i)
    {
        free(request->m_Headers[i]);
    }
    free(request->m_Headers);
    request->m_Headers = 0;
    request->m_HeaderCount = 0;
    request->m_HeaderCapacity = 0;
}

void DestroyRequest(HttpRequest* request)
{
    if (request)
    {
        free(request->m_URL);
        free(request->m_Method);
        free(request->m_Body);
        free(request->m_Path);
        free(request->m_Proxy);
        FreeHeaders(request);
        delete request;
    }
}

static bool CanModifyRequest(HttpRequest* request)
{
    return request && request->m_State == HTTP_REQUEST_STATE_CREATED && request->m_Service == 0;
}

static HttpResult SetString(char** field, const char* value)
{
    if (!value)
    {
        return HTTP_RESULT_INVAL;
    }

    char* copy = DuplicateString(value);
    if (!copy)
    {
        return HTTP_RESULT_IO_ERROR;
    }

    free(*field);
    *field = copy;
    return HTTP_RESULT_OK;
}

static HttpResult SetOptionalString(char** field, const char* value)
{
    if (!value)
    {
        free(*field);
        *field = 0;
        return HTTP_RESULT_OK;
    }

    return SetString(field, value);
}

static bool IsHeaderName(const char* header, const char* name)
{
    const char* h = header;
    const char* n = name;
    while (*h && *n)
    {
        if (tolower((unsigned char)*h) != tolower((unsigned char)*n))
        {
            return false;
        }
        ++h;
        ++n;
    }
    return *n == 0 && (*h == ':' || *h == ';');
}

static void RemoveHeader(HttpRequest* request, const char* name)
{
    uint32_t i = 0;
    while (i < request->m_HeaderCount)
    {
        if (IsHeaderName(request->m_Headers[i], name))
        {
            free(request->m_Headers[i]);
            for (uint32_t j = i + 1; j < request->m_HeaderCount; ++j)
            {
                request->m_Headers[j - 1] = request->m_Headers[j];
            }
            request->m_HeaderCount--;
        }
        else
        {
            ++i;
        }
    }
}

static HttpResult AddFormattedHeader(HttpRequest* request, const char* prefix, const char* value)
{
    uint32_t header_len = (uint32_t)(strlen(prefix) + strlen(value) + 1);
    char*    header = (char*)malloc(header_len);
    if (!header)
    {
        return HTTP_RESULT_IO_ERROR;
    }

    dmSnPrintf(header, header_len, "%s%s", prefix, value);
    if (!EnsureHeaderCapacity(request))
    {
        free(header);
        return HTTP_RESULT_IO_ERROR;
    }

    request->m_Headers[request->m_HeaderCount++] = header;
    return HTTP_RESULT_OK;
}

static char* Base64Encode(const char* data, uint32_t data_size)
{
    static const char* TABLE = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    uint32_t           output_size = ((data_size + 2) / 3) * 4;
    char*              output = (char*)malloc(output_size + 1);
    if (!output)
    {
        return 0;
    }

    uint32_t in = 0;
    uint32_t out = 0;
    while (in < data_size)
    {
        uint32_t octet_a = in < data_size ? (uint8_t)data[in++] : 0;
        uint32_t octet_b = in < data_size ? (uint8_t)data[in++] : 0;
        uint32_t octet_c = in < data_size ? (uint8_t)data[in++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        output[out++] = TABLE[(triple >> 18) & 0x3f];
        output[out++] = TABLE[(triple >> 12) & 0x3f];
        output[out++] = TABLE[(triple >> 6) & 0x3f];
        output[out++] = TABLE[triple & 0x3f];
    }

    uint32_t padding = (3 - (data_size % 3)) % 3;
    for (uint32_t i = 0; i < padding; ++i)
    {
        output[output_size - 1 - i] = '=';
    }
    output[output_size] = 0;
    return output;
}

static HttpResult MapResult(dmHttpClient::Result result)
{
    switch (result)
    {
        case dmHttpClient::RESULT_NOT_200_OK:
            return HTTP_RESULT_NOT_200_OK;
        case dmHttpClient::RESULT_OK:
            return HTTP_RESULT_OK;
        case dmHttpClient::RESULT_SOCKET_ERROR:
            return HTTP_RESULT_SOCKET_ERROR;
        case dmHttpClient::RESULT_HTTP_HEADERS_ERROR:
            return HTTP_RESULT_HTTP_HEADERS_ERROR;
        case dmHttpClient::RESULT_INVALID_RESPONSE:
            return HTTP_RESULT_INVALID_RESPONSE;
        case dmHttpClient::RESULT_PARTIAL_CONTENT:
            return HTTP_RESULT_PARTIAL_CONTENT;
        case dmHttpClient::RESULT_UNSUPPORTED_TRANSFER_ENCODING:
            return HTTP_RESULT_UNSUPPORTED_TRANSFER_ENCODING;
        case dmHttpClient::RESULT_INVAL_ERROR:
            return HTTP_RESULT_INVAL_ERROR;
        case dmHttpClient::RESULT_UNEXPECTED_EOF:
            return HTTP_RESULT_UNEXPECTED_EOF;
        case dmHttpClient::RESULT_IO_ERROR:
            return HTTP_RESULT_IO_ERROR;
        case dmHttpClient::RESULT_HANDSHAKE_FAILED:
            return HTTP_RESULT_HANDSHAKE_FAILED;
        case dmHttpClient::RESULT_INVAL:
            return HTTP_RESULT_INVAL;
        case dmHttpClient::RESULT_UNKNOWN:
            return HTTP_RESULT_UNKNOWN;
    }
    return HTTP_RESULT_UNKNOWN;
}

static void HandleCallbackResult(HttpRequest* request, HttpCallbackResult result)
{
    if (result == HTTP_CALLBACK_RESULT_CANCEL && request->m_Owner && request->m_Handle != HTTP_REQUEST_HANDLE_INVALID)
    {
        HttpCancelRequest(request->m_Owner, request->m_Handle);
    }
}

static void PopulateResponseInfo(HttpRequest* request, HttpResponseInfo* response)
{
    response->m_Url = request->m_URL;
    response->m_Path = request->m_Path;
    response->m_RangeStart = request->m_RangeStart;
    response->m_RangeEnd = request->m_RangeEnd;
    response->m_DocumentSize = request->m_DocumentSize;
}

static void SendHeader(HttpRequest* request, int status, const char* header, uint32_t header_size)
{
    if (request->m_Callback)
    {
        HttpResponseInfo response;
        memset(&response, 0, sizeof(response));
        PopulateResponseInfo(request, &response);
        response.m_Event = HTTP_RESPONSE_EVENT_HEADER;
        response.m_StatusCode = status;
        response.m_Header = header;
        response.m_HeaderSize = header_size;
        HandleCallbackResult(request, request->m_Callback(request, request->m_UserData, &response));
    }
}

static void SendData(HttpRequest* request, int status, const char* data, uint32_t data_size)
{
    if (request->m_Callback && data && data_size > 0)
    {
        HttpResponseInfo response;
        memset(&response, 0, sizeof(response));
        PopulateResponseInfo(request, &response);
        response.m_Event = HTTP_RESPONSE_EVENT_DATA;
        response.m_StatusCode = status;
        response.m_Data = data;
        response.m_DataSize = data_size;
        HandleCallbackResult(request, request->m_Callback(request, request->m_UserData, &response));
    }
}

static void SendProgress(HttpRequest* request, uint32_t bytes_sent, uint32_t bytes_received, int32_t bytes_total)
{
    if (request->m_Callback)
    {
        HttpResponseInfo response;
        memset(&response, 0, sizeof(response));
        PopulateResponseInfo(request, &response);
        response.m_Event = HTTP_RESPONSE_EVENT_PROGRESS;
        response.m_BytesSent = bytes_sent;
        response.m_BytesReceived = bytes_received;
        response.m_BytesTotal = bytes_total;
        HandleCallbackResult(request, request->m_Callback(request, request->m_UserData, &response));
    }
}

static void SendComplete(HttpRequest* request, HttpResult result, int status)
{
    if (request->m_Callback)
    {
        HttpResponseInfo response;
        memset(&response, 0, sizeof(response));
        PopulateResponseInfo(request, &response);
        response.m_Event = HTTP_RESPONSE_EVENT_COMPLETE;
        response.m_Result = result;
        response.m_StatusCode = status;
        (void)request->m_Callback(request, request->m_UserData, &response);
    }
}

extern "C"
{
    HttpResponseEvent HttpResponseGetEvent(const HttpResponseInfo* response)
    {
        return response ? response->m_Event : HTTP_RESPONSE_EVENT_HEADER;
    }

    HttpResult HttpResponseGetResult(const HttpResponseInfo* response)
    {
        return response ? response->m_Result : HTTP_RESULT_INVAL;
    }

    int HttpResponseGetStatusCode(const HttpResponseInfo* response)
    {
        return response ? response->m_StatusCode : 0;
    }

    const char* HttpResponseGetHeader(const HttpResponseInfo* response)
    {
        return response ? response->m_Header : 0;
    }

    uint32_t HttpResponseGetHeaderSize(const HttpResponseInfo* response)
    {
        return response ? response->m_HeaderSize : 0;
    }

    const void* HttpResponseGetData(const HttpResponseInfo* response)
    {
        return response ? response->m_Data : 0;
    }

    uint32_t HttpResponseGetDataSize(const HttpResponseInfo* response)
    {
        return response ? response->m_DataSize : 0;
    }

    const char* HttpResponseGetURL(const HttpResponseInfo* response)
    {
        return response ? response->m_Url : 0;
    }

    const char* HttpResponseGetPath(const HttpResponseInfo* response)
    {
        return response ? response->m_Path : 0;
    }

    uint32_t HttpResponseGetRangeStart(const HttpResponseInfo* response)
    {
        return response ? response->m_RangeStart : 0;
    }

    uint32_t HttpResponseGetRangeEnd(const HttpResponseInfo* response)
    {
        return response ? response->m_RangeEnd : 0;
    }

    uint32_t HttpResponseGetDocumentSize(const HttpResponseInfo* response)
    {
        return response ? response->m_DocumentSize : 0;
    }

    uint32_t HttpResponseGetBytesSent(const HttpResponseInfo* response)
    {
        return response ? response->m_BytesSent : 0;
    }

    uint32_t HttpResponseGetBytesReceived(const HttpResponseInfo* response)
    {
        return response ? response->m_BytesReceived : 0;
    }

    int32_t HttpResponseGetBytesTotal(const HttpResponseInfo* response)
    {
        return response ? response->m_BytesTotal : -1;
    }

    HttpResult HttpNewRequest(HttpRequest** request)
    {
        if (!request)
        {
            return HTTP_RESULT_INVAL;
        }

        *request = new HttpRequest();
        if (!*request || !(*request)->m_Method)
        {
            DestroyRequest(*request);
            *request = 0;
            return HTTP_RESULT_IO_ERROR;
        }

        return HTTP_RESULT_OK;
    }

    void HttpDeleteRequest(HttpRequest* request)
    {
        if (request && request->m_State == HTTP_REQUEST_STATE_CREATED && request->m_Service == 0)
        {
            DestroyRequest(request);
        }
    }

    HttpResult HttpSetURL(HttpRequest* request, const char* url)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }
        return SetString(&request->m_URL, url);
    }

    HttpResult HttpSetMethod(HttpRequest* request, const char* method)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }
        return SetString(&request->m_Method, method);
    }

    HttpResult HttpAddHeader(HttpRequest* request, const char* header)
    {
        if (!CanModifyRequest(request) || !header)
        {
            return HTTP_RESULT_INVAL;
        }

        if (!strchr(header, ':'))
        {
            uint32_t len = (uint32_t)strlen(header);
            if (len == 0 || header[len - 1] != ';')
            {
                return HTTP_RESULT_INVAL;
            }
        }

        char* copy = DuplicateString(header);
        if (!copy)
        {
            return HTTP_RESULT_IO_ERROR;
        }

        if (!EnsureHeaderCapacity(request))
        {
            free(copy);
            return HTTP_RESULT_IO_ERROR;
        }

        request->m_Headers[request->m_HeaderCount++] = copy;
        return HTTP_RESULT_OK;
    }

    HttpResult HttpSetRequestBody(HttpRequest* request, const void* body, uint32_t body_size)
    {
        if (!CanModifyRequest(request) || (!body && body_size > 0))
        {
            return HTTP_RESULT_INVAL;
        }

        char* copy = 0;
        if (body_size > 0)
        {
            copy = (char*)malloc(body_size);
            if (!copy)
            {
                return HTTP_RESULT_IO_ERROR;
            }
            memcpy(copy, body, body_size);
        }

        free(request->m_Body);
        request->m_Body = copy;
        request->m_BodyLength = body_size;
        return HTTP_RESULT_OK;
    }

    HttpResult HttpSetResponsePath(HttpRequest* request, const char* path)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }

        return SetOptionalString(&request->m_Path, path);
    }

    HttpResult HttpSetProxy(HttpRequest* request, const char* proxy)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }

        return SetOptionalString(&request->m_Proxy, proxy);
    }

    HttpResult HttpSetIgnoreCache(HttpRequest* request, int ignore_cache)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }

        request->m_IgnoreCache = ignore_cache != 0;
        return HTTP_RESULT_OK;
    }

    HttpResult HttpSetChunkedTransfer(HttpRequest* request, int chunked_transfer)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }

        request->m_ChunkedTransfer = chunked_transfer != 0;
        return HTTP_RESULT_OK;
    }

    HttpResult HttpSetReportProgress(HttpRequest* request, int report_progress)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }

        request->m_ReportProgress = report_progress != 0;
        return HTTP_RESULT_OK;
    }

    HttpResult HttpSetBasicAuth(HttpRequest* request, const char* username, const char* password)
    {
        if (!CanModifyRequest(request) || !username || !password)
        {
            return HTTP_RESULT_INVAL;
        }

        uint32_t credentials_size = (uint32_t)(strlen(username) + 1 + strlen(password));
        char*    credentials = (char*)malloc(credentials_size + 1);
        if (!credentials)
        {
            return HTTP_RESULT_IO_ERROR;
        }

        dmSnPrintf(credentials, credentials_size + 1, "%s:%s", username, password);
        char* encoded = Base64Encode(credentials, credentials_size);
        free(credentials);

        if (!encoded)
        {
            return HTTP_RESULT_IO_ERROR;
        }

        RemoveHeader(request, "Authorization");
        HttpResult result = AddFormattedHeader(request, "Authorization: Basic ", encoded);
        free(encoded);
        return result;
    }

    HttpResult HttpSetBearerAuth(HttpRequest* request, const char* token)
    {
        if (!CanModifyRequest(request) || !token)
        {
            return HTTP_RESULT_INVAL;
        }

        RemoveHeader(request, "Authorization");
        return AddFormattedHeader(request, "Authorization: Bearer ", token);
    }

    HttpResult HttpSetTimeout(HttpRequest* request, uint32_t timeout_us)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }

        request->m_Timeout = timeout_us;
        return HTTP_RESULT_OK;
    }

    HttpResult HttpSetResponseCallback(HttpRequest* request, HttpResponseCallback callback, void* user_data)
    {
        if (!CanModifyRequest(request))
        {
            return HTTP_RESULT_INVAL;
        }

        request->m_Callback = callback;
        request->m_UserData = user_data;
        return HTTP_RESULT_OK;
    }

    HttpResult HttpPushRequest(HttpService* service, HttpRequest* request, HttpRequestHandle* request_handle)
    {
        if (request_handle)
        {
            *request_handle = InvalidRequestHandle();
        }

        if (!service || !request || !request_handle || !request->m_URL || request->m_State != HTTP_REQUEST_STATE_CREATED || request->m_Service != 0)
        {
            return HTTP_RESULT_INVAL;
        }

        return dmHttpService::PushRequest(service, request, request_handle);
    }

    HttpResult HttpCancelRequest(HttpService* service, HttpRequestHandle request_handle)
    {
        if (!service)
        {
            return HTTP_RESULT_INVAL;
        }

        return dmHttpService::CancelRequest(service, request_handle);
    }

} // extern "C"

namespace dmHttpService
{
    void DirectRequestHeader(HttpRequest* request, int status, const char* key, const char* value)
    {
        if (!request || !key || !value)
        {
            return;
        }

        uint32_t key_length = (uint32_t)strlen(key);
        uint32_t value_length = (uint32_t)strlen(value);
        uint32_t header_length = key_length + value_length + 1;
        char*    header = (char*)malloc(header_length + 1);
        if (!header)
        {
            return;
        }

        memcpy(header, key, key_length);
        header[key_length] = ':';
        memcpy(header + key_length + 1, value, value_length);
        header[header_length] = 0;

        SendHeader(request, status, header, header_length);
        free(header);
    }

    void DirectRequestContent(HttpRequest* request, int status, const void* content_data, uint32_t content_data_size, int32_t content_length, uint32_t range_start, uint32_t range_end, uint32_t document_size)
    {
        if (!request)
        {
            return;
        }

        request->m_RangeStart = range_start;
        request->m_RangeEnd = range_end;
        request->m_DocumentSize = document_size;
        (void)content_length;

        SendData(request, status, (const char*)content_data, content_data_size);
    }

    void DirectRequestProgress(HttpRequest* request, uint32_t bytes_sent, uint32_t bytes_received, int32_t bytes_total)
    {
        if (!request)
        {
            return;
        }

        SendProgress(request, bytes_sent, bytes_received, bytes_total);
    }

    void CompleteDirectRequest(HttpRequest* request, dmHttpClient::Result result, int status, const char* headers, uint32_t headers_length)
    {
        if (!request)
        {
            return;
        }

        const char* headers_end = headers ? headers + headers_length : 0;
        const char* header = headers;
        while (header && header < headers_end)
        {
            const char* header_end = (const char*)memchr(header, '\n', headers_end - header);
            if (!header_end)
            {
                header_end = headers_end;
            }

            uint32_t header_length = (uint32_t)(header_end - header);
            if (header_length > 0)
            {
                char* header_copy = (char*)malloc(header_length + 1);
                if (header_copy)
                {
                    memcpy(header_copy, header, header_length);
                    header_copy[header_length] = 0;
                    SendHeader(request, status, header_copy, header_length);
                    free(header_copy);
                }
            }

            header = header_end + (header_end < headers_end ? 1 : 0);
        }

        UnregisterRequest(request);
        SendComplete(request, request->m_CancelFlag ? HTTP_RESULT_INVAL : MapResult(result), status);
        DestroyRequest(request);
    }

} // namespace dmHttpService
