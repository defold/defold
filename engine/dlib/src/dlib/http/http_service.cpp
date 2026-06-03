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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/message.h>
#include <dlib/http/http_cache.h>
#include <dlib/http/http_client.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/uri.h>
#include <dlib/math.h>
#include "http_private.h"
#include "http_service.h"

namespace dmHttpService
{
#define HTTP_SOCKET_NAME "@http"

    // The stack size was increased from 0x10000 to 0x20000 due to
    // a crash happening on older Android devices (< 4.3).
    // (Reason: Our HTTP service threads call getaddrinfo() which
    //  resulted in a writes outside the stack space inside libc.)
    const uint32_t   THREAD_STACK_SIZE = 0x20000;
    const uint32_t   DEFAULT_RESPONSE_BUFFER_SIZE = 64 * 1024;
    const uint32_t   DEFAULT_HEADER_BUFFER_SIZE = 16 * 1024;

    static uintptr_t GetRequestMessageDescriptor()
    {
        static int request_message_descriptor;
        return (uintptr_t)&request_message_descriptor;
    }

    static uintptr_t GetStopMessageDescriptor()
    {
        static int stop_message_descriptor;
        return (uintptr_t)&stop_message_descriptor;
    }

    enum RequestMessageType
    {
        REQUEST_MESSAGE_TYPE_HTTP_REQUEST,
        REQUEST_MESSAGE_TYPE_SERVICE_REQUEST,
    };

    struct RequestMessage
    {
        RequestMessageType m_Type;
        void*              m_Request;
    };

    struct RequestContext
    {
        HttpRequest* m_HttpRequest;
        Request*     m_ServiceRequest;
        const char*  m_Method;
        const char*  m_Url;
        const char*  m_Headers;
        uint64_t     m_HeadersLength;
        char**       m_DirectHeaders;
        uint32_t     m_DirectHeaderCount;
        const void*  m_Body;
        uint32_t     m_BodyLength;
        const char*  m_Path;
        const char*  m_Proxy;
        uint32_t     m_Timeout;
        bool         m_IgnoreCache;
        bool         m_ChunkedTransfer;
        bool         m_ReportProgress;
        void*        m_UserData;
    };

    struct Worker
    {
        dmThread::Thread      m_Thread;
        dmMessage::HSocket    m_Socket;
        dmHttpClient::HClient m_Client;
        dmURI::Parts          m_CurrentURL;
        dmURI::Parts          m_CurrentProxyURL;
        RequestContext*       m_Request;
        const char*           m_Filepath;
        int                   m_Status;
        uint32_t              m_RangeStart;
        uint32_t              m_RangeEnd;
        uint32_t              m_DocumentSize;
        uint32_t              m_BytesReceived;
        dmArray<char>         m_Response;
        dmArray<char>         m_Headers;
        const HttpService*    m_Service;
        bool                  m_CacheFlusher;
        volatile bool         m_Run;
        int                   m_Canceled;
        bool                  m_ReportProgress;
    };

    struct HttpService
    {
        HttpService()
        {
            m_Balancer = 0;
            m_Socket = 0;
            m_HttpCache = 0;
            m_LoadBalanceCount = 0;
            m_Run = false;
        }
        dmArray<Worker*>       m_Workers;
        dmThread::Thread       m_Balancer;
        dmMessage::HSocket     m_Socket;
        dmHttpCache::HCache    m_HttpCache;
        ReportProgressCallback m_ReportProgressCallback;
        int                    m_LoadBalanceCount;
        volatile bool          m_Run;
    };

    void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
    {
        Worker* worker = (Worker*)user_data;
        worker->m_Status = status_code;

        if (worker->m_Request && worker->m_Request->m_HttpRequest)
        {
            DirectRequestHeader(worker->m_Request->m_HttpRequest, status_code, key, value);
            return;
        }

        dmArray<char>& h = worker->m_Headers;
        uint32_t       len = strlen(key) + strlen(value) + 2;
        uint32_t       left = h.Capacity() - h.Size();
        if (left < len)
        {
            h.OffsetCapacity((int32_t)dmMath::Max(len - left, 8U * 1024U));
        }
        h.PushArray(key, strlen(key));
        h.Push(':');
        h.PushArray(value, strlen(value));
        h.Push('\n');
    }

    // Called from the http thread(s)
    void HttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length, uint32_t range_start, uint32_t range_end, uint32_t document_size, const char* method)
    {
        Worker* worker = (Worker*)user_data;
        worker->m_Status = status_code;
        worker->m_RangeStart = range_start;
        worker->m_RangeEnd = range_end;
        worker->m_DocumentSize = document_size;
        bool method_is_head = method && strcmp(method, "HEAD") == 0;

        if (worker->m_Request && worker->m_Request->m_HttpRequest)
        {
            if (!method_is_head && !content_data && !content_data_size)
            {
                worker->m_BytesReceived = 0;
            }

            DirectRequestContent(worker->m_Request->m_HttpRequest, status_code, content_data, content_data_size, content_length, range_start, range_end, document_size);

            if (!method_is_head && content_data_size > 0)
            {
                worker->m_BytesReceived += content_data_size;
            }

            if (worker->m_ReportProgress && !worker->m_Canceled && (method_is_head || content_data_size > 0))
            {
                DirectRequestProgress(worker->m_Request->m_HttpRequest, 0, worker->m_BytesReceived, content_length);
            }
            return;
        }

        if (!method_is_head && !content_data && !content_data_size)
        {
            dmArray<char>& r = worker->m_Response;
            r.SetSize(0);
            worker->m_BytesReceived = 0;
            return;
        }

        uint32_t bytes_received = 0;
        if (!method_is_head)
        {
            dmArray<char>& r = worker->m_Response;
            // do we have enough room to fit the content? if not, grow the array
            uint32_t left = r.Capacity() - r.Size();
            if (content_data_size > left)
            {
                r.OffsetCapacity(content_data_size - left);
            }
            r.PushArray((char*)content_data, content_data_size);
            bytes_received = r.Size();
            worker->m_BytesReceived = bytes_received;
        }

        if (worker->m_ReportProgress && (method_is_head || content_data_size > 0))
        {
            assert(worker->m_Service->m_ReportProgressCallback);

            RequestProgress progress = {};
            progress.m_BytesReceived = bytes_received;
            progress.m_BytesTotal = content_length;
            progress.m_Url = worker->m_Request->m_Url;
            worker->m_Service->m_ReportProgressCallback(&progress, worker->m_Request->m_UserData);
        }
    }

    uint32_t HttpSendContentLength(dmHttpClient::HResponse response, void* user_data)
    {
        Worker* worker = (Worker*)user_data;
        return worker->m_Request->m_BodyLength;
    }

    dmHttpClient::Result HttpWrite(dmHttpClient::HResponse response, uint32_t offset, uint32_t size, void* user_data)
    {
        Worker*        worker = (Worker*)user_data;
        const uint8_t* request_data = (const uint8_t*)worker->m_Request->m_Body;
        uint32_t       request_len = offset < worker->m_Request->m_BodyLength ? dmMath::Min(worker->m_Request->m_BodyLength - offset, size) : 0;
        if (request_len == 0)
        {
            return dmHttpClient::RESULT_OK;
        }
        if (!request_data)
        {
            return dmHttpClient::RESULT_INVAL;
        }

        dmHttpClient::Result res_write = dmHttpClient::Write(response, (const void*)&request_data[offset], request_len);

        if (res_write == dmHttpClient::RESULT_OK && worker->m_ReportProgress && size > 0)
        {
            if (worker->m_Request->m_HttpRequest)
            {
                DirectRequestProgress(worker->m_Request->m_HttpRequest, offset + request_len, 0, worker->m_Request->m_BodyLength);
            }
            else
            {
                assert(worker->m_Service->m_ReportProgressCallback);
                RequestProgress progress = {};
                progress.m_BytesSent = offset + request_len;
                progress.m_BytesTotal = worker->m_Request->m_BodyLength;
                progress.m_Url = worker->m_Request->m_Url;
                worker->m_Service->m_ReportProgressCallback(&progress, worker->m_Request->m_UserData);
            }
        }

        return res_write;
    }

    static dmHttpClient::Result WriteHeaderLine(dmHttpClient::HResponse response, const char* line)
    {
        char* header = strdup(line);
        if (!header)
        {
            return dmHttpClient::RESULT_IO_ERROR;
        }

        dmHttpClient::Result result = dmHttpClient::RESULT_INVAL;
        char*                colon = strchr(header, ':');
        if (colon)
        {
            *colon = '\0';
            result = dmHttpClient::WriteHeader(response, header, colon + 1);
            *colon = ':';
        }
        else
        {
            uint32_t len = (uint32_t)strlen(header);
            if (len > 0 && header[len - 1] == ';')
            {
                header[len - 1] = 0;
                result = dmHttpClient::WriteHeader(response, header, "");
            }
        }

        free(header);
        return result;
    }

    dmHttpClient::Result HttpWriteHeaders(dmHttpClient::HResponse response, void* user_data)
    {
        Worker*         worker = (Worker*)user_data;
        RequestContext* request = worker->m_Request;

        if (request->m_HttpRequest)
        {
            for (uint32_t i = 0; i < request->m_DirectHeaderCount; ++i)
            {
                dmHttpClient::Result r = WriteHeaderLine(response, request->m_DirectHeaders[i]);
                if (r != dmHttpClient::RESULT_OK)
                {
                    return r;
                }
            }
            return dmHttpClient::RESULT_OK;
        }

        char* headers = 0;
        if (request->m_Headers && request->m_HeadersLength > 0)
        {
            headers = (char*)malloc(request->m_HeadersLength);
            if (!headers)
            {
                return dmHttpClient::RESULT_IO_ERROR;
            }
            // NOTE: We must copy the buffer as retry might happen
            // and dmStrTok is destructive
            // We don't know the actual size inadvance, hence the malloc()
            memcpy(headers, (char*)request->m_Headers, request->m_HeadersLength);
            headers[request->m_HeadersLength - 1] = '\0';

            char *s, *last;
            s = dmStrTok(headers, "\n", &last);
            while (s)
            {
                char* colon = strchr(s, ':');
                *colon = '\0';
                dmHttpClient::Result r = dmHttpClient::WriteHeader(response, s, colon + 1);
                if (r != dmHttpClient::RESULT_OK)
                {
                    free(headers);
                    return r;
                }
                *colon = ':';
                s = dmStrTok(0, "\n", &last);
            }
        }

        free(headers);
        return dmHttpClient::RESULT_OK;
    }

    static void SendResponse(RequestContext* request, int status, const char* headers, uint32_t headers_length, const char* response, uint32_t response_length, const char* url, const char* filepath, uint32_t range_start, uint32_t range_end, uint32_t document_size, dmHttpClient::Result result)
    {
        if (request->m_HttpRequest)
        {
            (void)response;
            (void)response_length;
            CompleteDirectRequest(request->m_HttpRequest, result, status, headers, headers_length);
            return;
        }

        if (request->m_ServiceRequest && request->m_ServiceRequest->m_ResponseCallback)
        {
            Response service_response;
            service_response.m_Status = status;
            service_response.m_Headers = headers;
            service_response.m_HeadersLength = headers_length;
            service_response.m_Response = response;
            service_response.m_ResponseLength = response_length;
            service_response.m_Url = url;
            service_response.m_Path = filepath;
            service_response.m_RangeStart = range_start;
            service_response.m_RangeEnd = range_end;
            service_response.m_DocumentSize = document_size;
            service_response.m_Result = result;
            request->m_ServiceRequest->m_ResponseCallback(&service_response, request->m_UserData);
        }
    }

    static const char* FindHeader(Worker* worker, const char* header, char* buffer, uint32_t buffer_length)
    {
        // Headers are either 0, of a list of strings "header1: value\nheader2: value\n"
        RequestContext* request = worker->m_Request;
        uint32_t        header_length = strlen(header);

        if (request->m_HttpRequest)
        {
            for (uint32_t i = 0; i < request->m_DirectHeaderCount; ++i)
            {
                const char* current = request->m_DirectHeaders[i];
                uint32_t    length = strlen(current);
                if (length >= header_length && memcmp(current, header, header_length) == 0)
                {
                    if (length < buffer_length)
                    {
                        memcpy(buffer, current, length);
                        buffer[length] = 0;
                        return buffer;
                    }
                }
            }
            return 0;
        }

        if (!request->m_Headers || request->m_HeadersLength == 0)
        {
            return 0;
        }

        const char* current = (const char*)request->m_Headers;
        const char* headers_end = current + request->m_HeadersLength;
        while (current < headers_end)
        {
            const char* end = (const char*)memchr(current, '\n', headers_end - current);
            if (!end)
            {
                end = headers_end;
            }
            uint32_t length = end - current;
            if (length >= header_length && memcmp(current, header, header_length) == 0)
            {
                if (length < buffer_length)
                {
                    memcpy(buffer, current, length);
                    buffer[length] = 0;
                    return buffer;
                }
            }
            current = end + (end < headers_end ? 1 : 0);
        }
        return 0;
    }

    static void HandleRequest(Worker* worker, RequestContext* request)
    {
        dmURI::Parts  url;

        dmURI::Result ur = dmURI::Parse(request->m_Url, &url);
        if (ur != dmURI::RESULT_OK)
        {
            SendResponse(request, 0, 0, 0, 0, 0, request->m_Url, 0, 0, 0, 0, dmHttpClient::RESULT_INVAL);
            return;
        }
        if (url.m_Path[0] == '\0')
        {
            // NOTE: Default to / for empty path
            url.m_Path[0] = '/';
            url.m_Path[1] = '\0';
        }

        dmURI::Parts proxy_url;
        if (request->m_Proxy)
        {
            dmURI::Result pr = dmURI::Parse(request->m_Proxy, &proxy_url);
            if (pr != dmURI::RESULT_OK)
            {
                SendResponse(request, 0, 0, 0, 0, 0, request->m_Url, 0, 0, 0, 0, dmHttpClient::RESULT_INVAL);
                return;
            }
        }
        else
        {
            memset(&proxy_url, 0, sizeof(proxy_url));
        }

        worker->m_Request = request;
        worker->m_Canceled = request->m_HttpRequest ? request->m_HttpRequest->m_CancelFlag : 0;

        if (worker->m_Client == 0 ||
            !dmURI::Compare(&url, &worker->m_CurrentURL) ||
            !dmURI::Compare(&proxy_url, &worker->m_CurrentProxyURL))
        {
            if (worker->m_Client)
            {
                dmHttpClient::Delete(worker->m_Client);
            }
            // New connection
            dmHttpClient::NewParams params;
            params.m_HttpContent = &HttpContent;
            params.m_HttpHeader = &HttpHeader;
            params.m_HttpSendContentLength = &HttpSendContentLength;
            params.m_HttpWrite = &HttpWrite;
            params.m_HttpWriteHeaders = &HttpWriteHeaders;
            params.m_Userdata = worker;
            params.m_HttpCache = worker->m_Service->m_HttpCache;
            params.m_RequestTimeout = request->m_Timeout;

            worker->m_Client = dmHttpClient::New(&params, &url, &worker->m_Canceled, &proxy_url);

            memcpy(&worker->m_CurrentURL, &url, sizeof(url));
            memcpy(&worker->m_CurrentProxyURL, &proxy_url, sizeof(proxy_url));
        }

        if (request->m_HttpRequest)
        {
            worker->m_Response.SetSize(0);
            worker->m_Headers.SetSize(0);
        }
        else
        {
            worker->m_Response.SetSize(0);
            worker->m_Response.SetCapacity(DEFAULT_RESPONSE_BUFFER_SIZE);
            worker->m_Headers.SetSize(0);
            worker->m_Headers.SetCapacity(DEFAULT_HEADER_BUFFER_SIZE);
        }
        worker->m_Filepath = request->m_Path;
        worker->m_RangeStart = 0;
        worker->m_RangeEnd = 0;
        worker->m_DocumentSize = 0;
        worker->m_BytesReceived = 0;
        worker->m_ReportProgress = false;

        if (request->m_ReportProgress)
        {
            worker->m_ReportProgress = request->m_ReportProgress;
        }

        if (worker->m_Client)
        {
            dmHttpClient::SetOptionInt(worker->m_Client, dmHttpClient::OPTION_REQUEST_TIMEOUT, request->m_Timeout);
            dmHttpClient::SetOptionInt(worker->m_Client, dmHttpClient::OPTION_REQUEST_IGNORE_CACHE, request->m_IgnoreCache);
            dmHttpClient::SetOptionInt(worker->m_Client, dmHttpClient::OPTION_REQUEST_CHUNKED_TRANSFER, request->m_ChunkedTransfer);

            char cache_key[dmURI::MAX_URI_LEN];
            dmHttpClient::GetURI(worker->m_Client, url.m_Path, cache_key, sizeof(cache_key));

            char        header_buffer[256];
            const char* range_header = FindHeader(worker, "Range:", header_buffer, sizeof(header_buffer));
            if (range_header)
            {
                // If we find a range header, let's use it to append to the cache key
                range_header += strlen("Range:");
                while (*range_header == ' ')
                    ++range_header;
                dmStrlCat(cache_key, "=", sizeof(cache_key));
                dmStrlCat(cache_key, range_header, sizeof(cache_key)); // "=bytes=%d-%d"
            }
            dmHttpClient::SetCacheKey(worker->m_Client, cache_key);

            dmHttpClient::Result r = dmHttpClient::Request(worker->m_Client, request->m_Method, url.m_Path);

            if (r == dmHttpClient::RESULT_OK || r == dmHttpClient::RESULT_NOT_200_OK)
            {
                SendResponse(request, worker->m_Status, worker->m_Headers.Begin(), worker->m_Headers.Size(), worker->m_Response.Begin(), worker->m_Response.Size(), request->m_Url, worker->m_Filepath, worker->m_RangeStart, worker->m_RangeEnd, worker->m_DocumentSize, r);
            }
            else
            {
                // TODO: Error codes to lua?
                if (!worker->m_Canceled)
                {
                    dmLogError("HTTP request to '%s' failed (http result: %d  socket result: %d)", request->m_Url, r, GetLastSocketResult(worker->m_Client));
                }
                SendResponse(request, 0, worker->m_Headers.Begin(), worker->m_Headers.Size(), worker->m_Response.Begin(), worker->m_Response.Size(), request->m_Url, worker->m_Filepath, worker->m_RangeStart, worker->m_RangeEnd, worker->m_DocumentSize, r);
            }
        }
        else
        {
            // TODO: Error codes to lua?
            if (!worker->m_Canceled)
            {
                dmLogError("Unable to create HTTP connection to '%s'. No route to host?", request->m_Url);
            }
            SendResponse(request, 0, worker->m_Headers.Begin(), worker->m_Headers.Size(), worker->m_Response.Begin(), worker->m_Response.Size(), request->m_Url, worker->m_Filepath, worker->m_RangeStart, worker->m_RangeEnd, worker->m_DocumentSize, dmHttpClient::RESULT_SOCKET_ERROR);
        }

        worker->m_Request = 0;
    }

    static void InitRequestContext(RequestContext* context, Request* request)
    {
        memset(context, 0, sizeof(*context));

        context->m_ServiceRequest = request;
        context->m_Method = request->m_Method ? request->m_Method : "GET";
        context->m_Url = request->m_Url;
        context->m_Headers = request->m_Headers;
        context->m_HeadersLength = request->m_HeadersLength;
        context->m_Body = request->m_Body;
        context->m_BodyLength = request->m_BodyLength;
        context->m_Path = request->m_Path;
        context->m_Proxy = request->m_Proxy;
        context->m_Timeout = request->m_Timeout;
        context->m_IgnoreCache = request->m_IgnoreCache;
        context->m_ChunkedTransfer = request->m_ChunkedTransfer;
        context->m_ReportProgress = request->m_ReportProgress;
        context->m_UserData = request->m_UserData;
    }

    static void InitRequestContext(RequestContext* context, HttpRequest* request)
    {
        memset(context, 0, sizeof(*context));

        context->m_HttpRequest = request;
        context->m_Method = request->m_Method ? request->m_Method : "GET";
        context->m_Url = request->m_URL;
        context->m_DirectHeaders = request->m_Headers;
        context->m_DirectHeaderCount = request->m_HeaderCount;
        context->m_Body = request->m_Body;
        context->m_BodyLength = request->m_BodyLength;
        context->m_Path = request->m_Path;
        context->m_Proxy = request->m_Proxy;
        context->m_Timeout = request->m_Timeout;
        context->m_IgnoreCache = request->m_IgnoreCache != 0;
        context->m_ChunkedTransfer = request->m_ChunkedTransfer != 0;
        context->m_ReportProgress = request->m_ReportProgress != 0;
        context->m_UserData = request->m_UserData;
    }

    void Dispatch(dmMessage::Message* message, void* user_ptr)
    {
        Worker* worker = (Worker*)user_ptr;
        if (!worker->m_Run)
        {
            return;
        }

        if (message->m_Descriptor == GetRequestMessageDescriptor())
        {
            RequestMessage request_message;
            memcpy(&request_message, &message->m_Data[0], sizeof(request_message));

            if (request_message.m_Type == REQUEST_MESSAGE_TYPE_HTTP_REQUEST)
            {
                HttpRequest*   request = (HttpRequest*)request_message.m_Request;
                RequestContext context;
                InitRequestContext(&context, request);
                HandleRequest(worker, &context);
            }
            else if (request_message.m_Type == REQUEST_MESSAGE_TYPE_SERVICE_REQUEST)
            {
                Request*       request = (Request*)request_message.m_Request;
                RequestContext context;
                InitRequestContext(&context, request);
                HandleRequest(worker, &context);
                if (request->m_DestroyCallback)
                {
                    request->m_DestroyCallback(request, request->m_UserData);
                }
            }
        }
        else if (message->m_Descriptor == GetStopMessageDescriptor())
        {
            worker->m_Run = false;
        }
        else
        {
            const dmMessage::URL* sender = &message->m_Sender;
            const char*           socket_name = dmMessage::GetSocketName(sender->m_Socket);
            const char*           path_name = dmHashReverseSafe64(sender->m_Path);
            const char*           fragment_name = dmHashReverseSafe64(sender->m_Fragment);

            dmLogError("Unknown HTTP service message sent to socket '%s' from %s:%s#%s.",
                       HTTP_SOCKET_NAME,
                       socket_name,
                       path_name,
                       fragment_name);
        }
    }

    void LoadBalance(dmMessage::Message* message, void* user_ptr)
    {
        HttpService* service = (HttpService*)user_ptr;
        if (message->m_Descriptor == GetStopMessageDescriptor())
        {
            service->m_Run = false;
        }
        else
        {
            dmMessage::URL r = message->m_Receiver;
            r.m_Socket = service->m_Workers[service->m_LoadBalanceCount % service->m_Workers.Size()]->m_Socket;
            dmMessage::Post(&message->m_Sender,
                            &r,
                            message->m_Id,
                            message->m_UserData1,
                            message->m_UserData2,
                            message->m_Descriptor,
                            message->m_Data,
                            message->m_DataSize,
                            0);
            service->m_LoadBalanceCount++;
        }
    }

    static void Loop(void* arg)
    {
        Worker*  worker = (Worker*)arg;

        uint64_t flush_period = 5 * 1000000U;
        uint64_t next_flush = dmTime::GetMonotonicTime() + flush_period;
        while (worker->m_Run)
        {
            dmMessage::DispatchBlocking(worker->m_Socket, &Dispatch, worker);
            if (!worker->m_Run)
                break;

            if (worker->m_CacheFlusher && dmTime::GetMonotonicTime() > next_flush)
            {
                dmHttpCache::Flush(worker->m_Service->m_HttpCache);
                next_flush = dmTime::GetMonotonicTime() + flush_period;
            }
        }
    }

    static void LoadBalancer(void* arg)
    {
        HttpService* service = (HttpService*)arg;
        while (service->m_Run)
        {
            dmMessage::DispatchBlocking(service->m_Socket, &LoadBalance, service);
        }
    }

    HHttpService New(const Params* params)
    {
        HttpService* service = new HttpService;

        service->m_HttpCache = params->m_HttpCache;

        int threadcount = params->m_ThreadCount;
#if defined(__NX__)
        if (threadcount > 2)
            threadcount = 2;
#endif

        service->m_Run = true;
        dmMessage::NewSocket(HTTP_SOCKET_NAME, &service->m_Socket);
        service->m_Workers.SetCapacity(threadcount);
        for (uint32_t i = 0; i < threadcount; ++i)
        {
            Worker* worker = new Worker();
            char    tmp[128];
            dmSnPrintf(tmp, sizeof(tmp), "@__http_worker_%d", i);
            dmMessage::NewSocket(tmp, &worker->m_Socket);
            worker->m_Client = 0;
            memset(&worker->m_CurrentURL, 0, sizeof(worker->m_CurrentURL));
            memset(&worker->m_CurrentProxyURL, 0, sizeof(worker->m_CurrentProxyURL));
            worker->m_Request = 0;
            worker->m_Status = 0;
            worker->m_Service = service;
            worker->m_CacheFlusher = i == 0 && worker->m_Service->m_HttpCache != 0;
            worker->m_Run = true;
            worker->m_Canceled = 0;
            worker->m_ReportProgress = false;
            worker->m_BytesReceived = 0;
            service->m_Workers.Push(worker);

            dmThread::Thread t = dmThread::New(&Loop, THREAD_STACK_SIZE, worker, "http");
            worker->m_Thread = t;
        }

        dmThread::Thread t = dmThread::New(&LoadBalancer, THREAD_STACK_SIZE, service, "http_balance");
        service->m_Balancer = t;

        service->m_ReportProgressCallback = params->m_ReportProgressCallback;

        return service;
    }

    dmMessage::HSocket GetSocket(HHttpService http_service)
    {
        return http_service->m_Socket;
    }

    static HttpResult PostRequest(HHttpService http_service, const RequestMessage* request_message)
    {
        dmMessage::URL receiver;
        dmMessage::ResetURL(&receiver);
        receiver.m_Socket = http_service->m_Socket;

        dmMessage::Result post_result = dmMessage::Post(0, &receiver, (dmhash_t)GetRequestMessageDescriptor(), 0, 0, GetRequestMessageDescriptor(), request_message, sizeof(*request_message), 0);
        if (post_result != dmMessage::RESULT_OK)
        {
            return HTTP_RESULT_IO_ERROR;
        }

        return HTTP_RESULT_OK;
    }

    HttpResult PushRequest(HHttpService http_service, Request* request)
    {
        if (!http_service || !request || !request->m_Url)
        {
            return HTTP_RESULT_INVAL;
        }

        RequestMessage request_message;
        request_message.m_Type = REQUEST_MESSAGE_TYPE_SERVICE_REQUEST;
        request_message.m_Request = request;
        return PostRequest(http_service, &request_message);
    }

    HttpResult PushRequest(HHttpService http_service, HttpRequest* request)
    {
        if (!http_service || !request || !request->m_URL || request->m_State != HTTP_REQUEST_STATE_CREATED || request->m_Service != 0)
        {
            return HTTP_RESULT_INVAL;
        }

        request->m_Service = http_service;
        request->m_State = HTTP_REQUEST_STATE_QUEUED;

        RequestMessage request_message;
        request_message.m_Type = REQUEST_MESSAGE_TYPE_HTTP_REQUEST;
        request_message.m_Request = request;
        HttpResult post_result = PostRequest(http_service, &request_message);
        if (post_result != HTTP_RESULT_OK)
        {
            request->m_Service = 0;
            request->m_State = HTTP_REQUEST_STATE_CREATED;
            return post_result;
        }

        return HTTP_RESULT_OK;
    }

    HttpResult CancelRequest(HHttpService http_service, HttpRequest* request)
    {
        if (!http_service || !request || request->m_Service != http_service)
        {
            return HTTP_RESULT_INVAL;
        }

        request->m_CancelFlag = 1;
        for (uint32_t i = 0; i < http_service->m_Workers.Size(); ++i)
        {
            dmHttpService::Worker* worker = http_service->m_Workers[i];
            if (worker->m_Request && worker->m_Request->m_HttpRequest == request)
            {
                worker->m_Canceled = 1;
            }
        }

        return HTTP_RESULT_OK;
    }

    void Delete(HHttpService http_service)
    {
        dmMessage::URL url;
        dmMessage::ResetURL(&url);
        url.m_Socket = http_service->m_Socket;
        dmMessage::Post(0, &url, 0, 0, 0, GetStopMessageDescriptor(), 0, 0, 0);

        // Stop the balancer first, so we don't accept any new requests
        dmThread::Join(http_service->m_Balancer);

        // Cancel them all first, as opposed to one-by-one
        for (uint32_t i = 0; i < http_service->m_Workers.Size(); ++i)
        {
            dmHttpService::Worker* worker = http_service->m_Workers[i];

            url.m_Socket = worker->m_Socket;
            dmMessage::Post(0, &url, 0, 0, 0, GetStopMessageDescriptor(), 0, 0, 0);

            worker->m_Canceled = 1;
        }

        for (uint32_t i = 0; i < http_service->m_Workers.Size(); ++i)
        {
            dmHttpService::Worker* worker = http_service->m_Workers[i];

            // DNS lookups using dmSocket::GetHostByName are using getaddrinfo which may block for an undefined
            // amount of time. We do not wish to wait for the thread during shutdown
            // We now use detach() on the thread on creation so that we don't have to wait for the thread
            if (worker->m_Thread)
            {
                dmThread::Join(worker->m_Thread);
            }

            dmMessage::DeleteSocket(worker->m_Socket);
            if (worker->m_Client)
            {
                dmHttpClient::Delete(worker->m_Client);
            }
            delete worker;
        }

        dmMessage::DeleteSocket(http_service->m_Socket);
        delete http_service;
    }

} // namespace dmHttpService
