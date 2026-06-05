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

#ifndef DM_HTTP_SERVICE_H
#define DM_HTTP_SERVICE_H

#include <stdint.h>
#include <dlib/http/http_cache.h>
#include <dlib/http/http_client.h>
#include <dlib/message.h>
#include <dmsdk/dlib/http.h>

namespace dmHttpService
{
    typedef ::HttpService* HHttpService;

    struct Request;

    struct Response
    {
        int                  m_Status;
        const char*          m_Headers;
        uint32_t             m_HeadersLength;
        const char*          m_Response;
        uint32_t             m_ResponseLength;
        const char*          m_Url;
        const char*          m_Path;
        uint32_t             m_RangeStart;
        uint32_t             m_RangeEnd;
        uint32_t             m_DocumentSize;
        dmHttpClient::Result m_Result;
    };

    struct RequestProgress
    {
        uint32_t    m_BytesSent;
        uint32_t    m_BytesReceived;
        int32_t     m_BytesTotal;
        const char* m_Url;
    };

    typedef void (*ResponseCallback)(const Response* response, void* user_data);
    typedef void (*ReportProgressCallback)(const RequestProgress* progress, void* user_data);
    typedef void (*DestroyRequestCallback)(Request* request, void* user_data);

    struct Request
    {
        Request()
        : m_Method(0)
        , m_Url(0)
        , m_Headers(0)
        , m_HeadersLength(0)
        , m_Body(0)
        , m_BodyLength(0)
        , m_Path(0)
        , m_Proxy(0)
        , m_Timeout(0)
        , m_IgnoreCache(false)
        , m_ChunkedTransfer(true)
        , m_ReportProgress(false)
        , m_ResponseCallback(0)
        , m_DestroyCallback(0)
        , m_UserData(0)
        {
        }

        const char*            m_Method;
        const char*            m_Url;
        const char*            m_Headers;
        uint64_t               m_HeadersLength;
        const void*            m_Body;
        uint32_t               m_BodyLength;
        const char*            m_Path;
        const char*            m_Proxy;
        uint32_t               m_Timeout;
        bool                   m_IgnoreCache;
        bool                   m_ChunkedTransfer;
        bool                   m_ReportProgress;
        ResponseCallback       m_ResponseCallback;
        DestroyRequestCallback m_DestroyCallback;
        void*                  m_UserData;
    };

    struct Params
    {
        Params()
        : m_HttpCache(0)
        , m_ReportProgressCallback(0)
        , m_ThreadCount(4)
        {
        }

        dmHttpCache::HCache    m_HttpCache;
        ReportProgressCallback m_ReportProgressCallback;
        uint32_t               m_ThreadCount : 4;
    };

    HHttpService New(const Params* params);
    dmMessage::HSocket GetSocket(HHttpService http_service);
    HttpResult PushRequest(HHttpService http_service, Request* request);
    HttpResult PushRequest(HHttpService http_service, HttpRequest* request, HttpRequestHandle* request_handle);
    HttpResult CancelRequest(HHttpService http_service, HttpRequestHandle request_handle);
    void UnregisterRequest(HttpRequest* request);
    void Delete(HHttpService http_service);

}  // namespace dmHttpService


#endif // #ifndef DM_HTTP_SERVICE_H
