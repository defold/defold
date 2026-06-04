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

#ifndef DM_HTTP_PRIVATE_H
#define DM_HTTP_PRIVATE_H

#include <stdint.h>

#include <dlib/http/http_client.h>
#include <dlib/mutex.h>
#include <dmsdk/dlib/http.h>

namespace dmHttpService
{
    struct HttpService;
    typedef HttpService* HHttpService;
} // namespace dmHttpService

enum HttpRequestState
{
    HTTP_REQUEST_STATE_CREATED,
    HTTP_REQUEST_STATE_QUEUED,
};

struct HttpRequestSlot
{
    HttpRequest* m_Request;
    uint16_t     m_Generation;
};

struct HttpService
{
    HttpService();
    ~HttpService();

    dmHttpService::HHttpService m_Service;
    dmMutex::HMutex             m_RequestMutex;
    HttpRequestSlot*            m_RequestSlots;
    uint32_t                    m_RequestSlotCount;
    uint32_t                    m_RequestSlotCapacity;
    uint16_t                    m_NextRequestGeneration;
};

struct HttpResponseInfo
{
    HttpResponseEvent m_Event;
    HttpResult        m_Result;
    int               m_StatusCode;
    const char*       m_Header;
    uint32_t          m_HeaderSize;
    const void*       m_Data;
    uint32_t          m_DataSize;
    const char*       m_Url;
    const char*       m_Path;
    uint32_t          m_RangeStart;
    uint32_t          m_RangeEnd;
    uint32_t          m_DocumentSize;
    uint32_t          m_BytesSent;
    uint32_t          m_BytesReceived;
    int32_t           m_BytesTotal;
};

struct HttpRequest
{
    HttpRequest();

    char*                       m_URL;
    char*                       m_Method;
    char**                      m_Headers;
    uint32_t                    m_HeaderCount;
    uint32_t                    m_HeaderCapacity;
    char*                       m_Body;
    uint32_t                    m_BodyLength;
    char*                       m_Path;
    char*                       m_Proxy;
    HttpResponseCallback        m_Callback;
    void*                       m_UserData;
    dmHttpService::HHttpService m_Service;
    HttpService*                m_Owner;
    HttpRequestHandle           m_Handle;
    uint32_t                    m_Timeout;
    int                         m_IgnoreCache;
    int                         m_ChunkedTransfer;
    int                         m_ReportProgress;
    int                         m_CancelFlag;
    uint32_t                    m_RangeStart;
    uint32_t                    m_RangeEnd;
    uint32_t                    m_DocumentSize;
    HttpRequestState            m_State;
};

namespace dmHttpService
{
    void DirectRequestHeader(HttpRequest* request, int status, const char* key, const char* value);
    void DirectRequestContent(HttpRequest* request, int status, const void* content_data, uint32_t content_data_size, int32_t content_length, uint32_t range_start, uint32_t range_end, uint32_t document_size);
    void DirectRequestProgress(HttpRequest* request, uint32_t bytes_sent, uint32_t bytes_received, int32_t bytes_total);
    void CompleteDirectRequest(HttpRequest* request, dmHttpClient::Result result, int status, const char* headers, uint32_t headers_length);
} // namespace dmHttpService

#endif // DM_HTTP_PRIVATE_H
