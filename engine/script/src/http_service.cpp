// Copyright 2020-2024 The Defold Foundation
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
#include <stdio.h>
#include <string.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/message.h>
#include <dlib/http_client.h>
#include <dlib/http_cache.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/uri.h>
#include <dlib/math.h>
#include <ddf/ddf.h>
#include "http_ddf.h"
#include "http_service.h"

namespace dmHttpService
{
    #define HTTP_SOCKET_NAME "@http"

    // The stack size was increased from 0x10000 to 0x20000 due to
    // a crash happening on older Android devices (< 4.3).
    // (Reason: Our HTTP service threads call getaddrinfo() which
    //  resulted in a writes outside the stack space inside libc.)
    const uint32_t THREAD_STACK_SIZE = 0x20000;
    const uint32_t DEFAULT_RESPONSE_BUFFER_SIZE = 64 * 1024;
    const uint32_t DEFAULT_HEADER_BUFFER_SIZE = 16 * 1024;


    struct HttpService;

    struct Worker
    {
        dmThread::Thread      m_Thread;
        dmMessage::HSocket    m_Socket;
        dmHttpClient::HClient m_Client;
        dmURI::Parts          m_CurrentURL;
        dmMessage::URL        m_CurrentRequesterURL;
        dmHttpDDF::HttpRequest*   m_Request;
        const char*           m_Filepath;
        int                   m_Status;
        uintptr_t             m_ResponseUserData1;
        uintptr_t             m_ResponseUserData2;
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
        dmArray<Worker*>          m_Workers;
        dmThread::Thread          m_Balancer;
        dmMessage::HSocket        m_Socket;
        dmHttpCache::HCache       m_HttpCache;
        ReportProgressCallback    m_ReportProgressCallback;
        int                       m_LoadBalanceCount;
        volatile bool             m_Run;
    };

    void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
    {
        Worker* worker = (Worker*) user_data;
        worker->m_Status = status_code;
        dmArray<char>& h = worker->m_Headers;
        uint32_t len = strlen(key) + strlen(value) + 2;
        uint32_t left = h.Capacity() - h.Size();
        if (left < len) {
            h.OffsetCapacity((int32_t) dmMath::Max(len - left, 8U * 1024U));
        }
        h.PushArray(key, strlen(key));
        h.Push(':');
        h.PushArray(value, strlen(value));
        h.Push('\n');
    }

    void HttpContent(dmHttpClient::HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size, int32_t content_length)
    {
        Worker* worker = (Worker*) user_data;
        worker->m_Status = status_code;
        dmArray<char>& r = worker->m_Response;

        if (!content_data && !content_data_size)
        {
            r.SetSize(0);
            return;
        }

        uint32_t left = r.Capacity() - r.Size();
        if (left < content_data_size) {
            r.OffsetCapacity((int32_t) dmMath::Max(content_data_size - left, 128U * 1024U));
        }
        r.PushArray((char*) content_data, content_data_size);

        if (worker->m_ReportProgress && content_data_size > 0)
        {
            assert(worker->m_Service->m_ReportProgressCallback);

            dmHttpDDF::HttpRequestProgress progress = {};
            progress.m_BytesReceived                = r.Size();
            progress.m_BytesTotal                   = content_length;
            worker->m_Service->m_ReportProgressCallback(&progress, &worker->m_CurrentRequesterURL, worker->m_ResponseUserData2);
        }
    }

    uint32_t HttpSendContentLength(dmHttpClient::HResponse response, void* user_data)
    {
        Worker* worker = (Worker*) user_data;
        return worker->m_Request->m_RequestLength;
    }

    dmHttpClient::Result HttpWrite(dmHttpClient::HResponse response, uint32_t offset, uint32_t size, void* user_data)
    {
        Worker* worker = (Worker*) user_data;
        uint8_t* request = (uint8_t*)worker->m_Request->m_Request;
        uint32_t request_len = dmMath::Min(worker->m_Request->m_RequestLength - offset, size);

        dmHttpClient::Result res_write = dmHttpClient::Write(response, (const void*) &request[offset], request_len);

        if (res_write == dmHttpClient::RESULT_OK && worker->m_ReportProgress && size > 0)
        {
            assert(worker->m_Service->m_ReportProgressCallback);
            dmHttpDDF::HttpRequestProgress progress = {};
            progress.m_BytesSent                    = offset + size;
            progress.m_BytesTotal                   = worker->m_Request->m_RequestLength;
            worker->m_Service->m_ReportProgressCallback(&progress, &worker->m_CurrentRequesterURL, worker->m_ResponseUserData2);
        }

        return res_write;
    }

    dmHttpClient::Result HttpWriteHeaders(dmHttpClient::HResponse response, void* user_data)
    {
        Worker* worker = (Worker*) user_data;
        char* headers = 0;
        if (worker->m_Request->m_HeadersLength > 0) {
            headers = (char*) malloc(worker->m_Request->m_HeadersLength);
            // NOTE: We must copy the buffer as retry might happen
            // and dmStrTok is destructive
            // We don't know the actual size inadvance, hence the malloc()
            memcpy(headers, (char*) worker->m_Request->m_Headers, worker->m_Request->m_HeadersLength);
            headers[worker->m_Request->m_HeadersLength-1] = '\0';

            char* s, *last;
            s = dmStrTok(headers, "\n", &last);
            while (s) {
                char* colon = strchr(s, ':');
                *colon = '\0';
                dmHttpClient::Result r = dmHttpClient::WriteHeader(response, s, colon + 1);
                if (r != dmHttpClient::RESULT_OK) {
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

    static void MessageDestroyCallback(dmMessage::Message* message)
    {
        dmHttpDDF::HttpResponse* response = (dmHttpDDF::HttpResponse*)message->m_Data;
        free((void*) response->m_Headers);
        free((void*) response->m_Response);
    }

    static void SendResponse(const dmMessage::URL* requester, uintptr_t userdata1, uintptr_t userdata2, int status,
                             const char* headers, uint32_t headers_length,
                             const char* response, uint32_t response_length,
                             const char* filepath)
    {
        dmHttpDDF::HttpResponse resp;
        resp.m_Status = status;
        resp.m_HeadersLength = headers_length;
        resp.m_ResponseLength = response_length;

        resp.m_Headers = (uint64_t) malloc(headers_length);
        memcpy((void*) resp.m_Headers, headers, headers_length);
        resp.m_Response = (uint64_t) malloc(response_length);
        memcpy((void*) resp.m_Response, response, response_length);
        resp.m_Path = filepath;

        if (dmMessage::RESULT_OK != dmMessage::Post(0, requester, dmHttpDDF::HttpResponse::m_DDFHash, userdata1, userdata2, (uintptr_t) dmHttpDDF::HttpResponse::m_DDFDescriptor, &resp, sizeof(resp), MessageDestroyCallback) )
        {
            free((void*) resp.m_Headers);
            free((void*) resp.m_Response);
            dmLogWarning("Failed to return http-response. Requester deleted?");
        }
    }

    void HandleRequest(Worker* worker, const dmMessage::URL* requester, uintptr_t userdata1, uintptr_t userdata2, dmHttpDDF::HttpRequest* request)
    {
        dmURI::Parts url;
        request->m_Method = (const char*) ((uintptr_t) request + (uintptr_t) request->m_Method);
        request->m_Url = (const char*) ((uintptr_t) request + (uintptr_t) request->m_Url);
        dmURI::Result ur =  dmURI::Parse(request->m_Url, &url);
        if (ur != dmURI::RESULT_OK)
        {
            SendResponse(requester, 0, 0, 0, 0, 0, 0, 0, 0);
            return;
        }
        if (url.m_Path[0] == '\0') {
            // NOTE: Default to / for empty path
            url.m_Path[0] = '/';
            url.m_Path[1] = '\0';
        }

        if (worker->m_Client == 0 || !(strcmp(url.m_Hostname, worker->m_CurrentURL.m_Hostname) == 0 &&
                                       strcmp(url.m_Scheme, worker->m_CurrentURL.m_Scheme) == 0 &&
                                       url.m_Port == worker->m_CurrentURL.m_Port)) {
            if (worker->m_Client) {
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

            worker->m_Client = dmHttpClient::New(&params, url.m_Hostname, url.m_Port, strcmp(url.m_Scheme, "https") == 0, &worker->m_Canceled);

            memcpy(&worker->m_CurrentURL, &url, sizeof(url));
        }

        worker->m_Response.SetSize(0);
        worker->m_Response.SetCapacity(DEFAULT_RESPONSE_BUFFER_SIZE);
        worker->m_Headers.SetSize(0);
        worker->m_Headers.SetCapacity(DEFAULT_HEADER_BUFFER_SIZE);
        worker->m_Filepath = request->m_Path;

        if (request->m_ReportProgress)
        {
            worker->m_ReportProgress    = request->m_ReportProgress;
            worker->m_ResponseUserData1 = userdata1;
            worker->m_ResponseUserData2 = userdata2;
            memcpy(&worker->m_CurrentRequesterURL, requester, sizeof(dmMessage::URL));
        }

        if (worker->m_Client) {
            worker->m_Request = request;
            dmHttpClient::SetOptionInt(worker->m_Client, dmHttpClient::OPTION_REQUEST_TIMEOUT, request->m_Timeout);
            dmHttpClient::SetOptionInt(worker->m_Client, dmHttpClient::OPTION_REQUEST_IGNORE_CACHE, request->m_IgnoreCache);
            dmHttpClient::SetOptionInt(worker->m_Client, dmHttpClient::OPTION_REQUEST_CHUNKED_TRANSFER, request->m_ChunkedTransfer);

            dmHttpClient::Result r = dmHttpClient::Request(worker->m_Client, request->m_Method, url.m_Path);

            if (r == dmHttpClient::RESULT_OK || r == dmHttpClient::RESULT_NOT_200_OK) {
                SendResponse(requester, userdata1, userdata2, worker->m_Status, worker->m_Headers.Begin(), worker->m_Headers.Size(), worker->m_Response.Begin(), worker->m_Response.Size(), worker->m_Filepath);
            } else {
                // TODO: Error codes to lua?
                dmLogError("HTTP request to '%s' failed (http result: %d  socket result: %d)", request->m_Url, r, GetLastSocketResult(worker->m_Client));
                SendResponse(requester, userdata1, userdata2, 0, worker->m_Headers.Begin(), worker->m_Headers.Size(), worker->m_Response.Begin(), worker->m_Response.Size(), worker->m_Filepath);
            }
        } else {
            // TODO: Error codes to lua?
            SendResponse(requester, userdata1, userdata2, 0, worker->m_Headers.Begin(), worker->m_Headers.Size(), worker->m_Response.Begin(), worker->m_Response.Size(), worker->m_Filepath);
            dmLogError("Unable to create HTTP connection to '%s'. No route to host?", request->m_Url);
        }
    }

    void Dispatch(dmMessage::Message *message, void* user_ptr)
    {
        Worker* worker = (Worker*) user_ptr;
        if (!worker->m_Run) {
            return;
        }

        if (message->m_Descriptor)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;

            if (message->m_Descriptor == (uintptr_t) dmHttpDDF::HttpRequest::m_DDFDescriptor)
            {
                dmHttpDDF::HttpRequest* request = (dmHttpDDF::HttpRequest*) &message->m_Data[0];
                HandleRequest(worker, &message->m_Sender, 0, message->m_UserData2, request);
                free((void*) request->m_Headers);
                free((void*) request->m_Request);
            }
            else if (message->m_Descriptor == (uintptr_t) dmHttpDDF::StopHttp::m_DDFDescriptor)
            {
                worker->m_Run = false;
            }
            else
            {
                const dmMessage::URL* sender = &message->m_Sender;
                const char* socket_name = dmMessage::GetSocketName(sender->m_Socket);
                const char* path_name = dmHashReverseSafe64(sender->m_Path);
                const char* fragment_name = dmHashReverseSafe64(sender->m_Fragment);
                dmLogError("Unknown message '%s' sent to socket '%s' from %s:%s#%s.",
                           descriptor->m_Name, HTTP_SOCKET_NAME, socket_name, path_name, fragment_name);
            }
        }
        else
        {
            const dmMessage::URL* sender = &message->m_Sender;
            const char* socket_name = dmMessage::GetSocketName(sender->m_Socket);
            const char* path_name = dmHashReverseSafe64(sender->m_Path);
            const char* fragment_name = dmHashReverseSafe64(sender->m_Fragment);

            dmLogError("Only http messages can be sent to the '%s' socket. Message sent from: %s:%s#%s",
                       HTTP_SOCKET_NAME, socket_name, path_name, fragment_name);
        }
    }

    void LoadBalance(dmMessage::Message *message, void* user_ptr)
    {
        HttpService* service = (HttpService*) user_ptr;
        if (message->m_Descriptor == (uintptr_t) dmHttpDDF::StopHttp::m_DDFDescriptor) {
            service->m_Run = false;
        } else {
            dmMessage::URL r = message->m_Receiver;
            r.m_Socket = service->m_Workers[service->m_LoadBalanceCount % service->m_Workers.Size()]->m_Socket;
            dmMessage::Post(&message->m_Sender,
                            &r,
                            message->m_Id,
                            message->m_UserData1,
                            message->m_UserData2,
                            message->m_Descriptor,
                            message->m_Data,
                            message->m_DataSize, 0);
            service->m_LoadBalanceCount++;
        }
    }

    static void Loop(void* arg)
    {
        Worker* worker = (Worker*) arg;

        uint64_t flush_period = 5 * 1000000U;
        uint64_t next_flush = dmTime::GetTime() + flush_period;
        while (worker->m_Run)
        {
            dmMessage::DispatchBlocking(worker->m_Socket, &Dispatch, worker);
            if (!worker->m_Run)
                break;

            if (worker->m_CacheFlusher && dmTime::GetTime() > next_flush) {
                dmHttpCache::Flush(worker->m_Service->m_HttpCache);
                next_flush = dmTime::GetTime() + flush_period;
            }
        }
    }

    static void LoadBalancer(void* arg)
    {
        HttpService* service = (HttpService*) arg;
        while (service->m_Run) {
            dmMessage::DispatchBlocking(service->m_Socket, &LoadBalance, service);
        }
    }

    HHttpService New(const Params* params)
    {
        HttpService* service = new HttpService;

        if (params->m_UseHttpCache)
        {
            dmHttpCache::NewParams cache_params;
            char path[1024];
            dmSys::Result sys_result = dmSys::GetApplicationSupportPath("defold", path, sizeof(path));
            if (sys_result == dmSys::RESULT_OK)
            {
                // NOTE: The other cache (streaming) is called /cache
                dmStrlCat(path, "/http-cache", sizeof(path));
                cache_params.m_Path = path;
                dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &service->m_HttpCache);
                if (cache_r != dmHttpCache::RESULT_OK)
                {
                    dmLogWarning("Unable to open http cache (%d)", cache_r);
                }
            }
            else
            {
                dmLogWarning("Unable to locate application support path for \"%s\": (%d)", "defold", sys_result);
            }
        }
        else
        {
            dmLogWarning("Http cache disabled");
        }

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
            char tmp[128];
            dmSnPrintf(tmp, sizeof(tmp), "@__http_worker_%d", i);
            dmMessage::NewSocket(tmp, &worker->m_Socket);
            worker->m_Client = 0;
            memset(&worker->m_CurrentURL, 0, sizeof(worker->m_CurrentURL));
            worker->m_Request = 0;
            worker->m_Status = 0;
            worker->m_Service = service;
            worker->m_CacheFlusher = i == 0 && worker->m_Service->m_HttpCache != 0;
            worker->m_Run = true;
            worker->m_Canceled = 0;
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

    void Delete(HHttpService http_service)
    {
        dmMessage::URL url;
        url.m_Socket = http_service->m_Socket;
        dmMessage::Post(0, &url, 0, 0, (uintptr_t) dmHttpDDF::StopHttp::m_DDFDescriptor, 0, 0, 0);

        // Stop the balancer first, so we don't accept any new requests
        dmThread::Join(http_service->m_Balancer);

        // Cancel them all first, as opposed to one-by-one
        for (uint32_t i = 0; i < http_service->m_Workers.Size(); ++i)
        {
            dmHttpService::Worker* worker = http_service->m_Workers[i];

            url.m_Socket = worker->m_Socket;
            dmMessage::Post(0, &url, 0, 0, (uintptr_t) dmHttpDDF::StopHttp::m_DDFDescriptor, 0, 0, 0);

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
        if (http_service->m_HttpCache)
            dmHttpCache::Close(http_service->m_HttpCache);
        delete http_service;
    }

}
