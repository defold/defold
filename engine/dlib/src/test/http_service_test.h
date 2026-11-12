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

#ifndef DM_HTTP_SERVICE_TEST_H
#define DM_HTTP_SERVICE_TEST_H

#include <jc_test/jc_test.h>
#include <dlib/array.h>
#include <dlib/atomic.h>
#include <dlib/dstrings.h>
#include <dlib/http/http_client.h>
#include <dlib/mutex.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/uri.h>
#include <string.h>

// Deadlock guards only; tests wait for responses or explicit state changes.
static const uint64_t SERVICE_TEST_TIMEOUT = 60000000;

class HttpServiceTest : public jc_test_params_class<bool>
{
    public:
    dmArray<char>             m_Body;
    char                      m_ContentType[64];
    char                      m_Allow[32];
    char                      m_Connection[16];
    char                      m_CacheControl[32];
    char                      m_Location[192];
    const char*               m_RequestInstance;
    const char*               m_RequestContentType;
    int                       m_Status;
    int32_atomic_t            m_Done;
    const char*               m_Method;
    const char*               m_Path;
    const char*               m_RequestBody;
    uint32_t                  m_RequestBodySize;
    bool                      m_SendRequestBody;
    int32_t                   m_RequestPrefixSize;
    int                       m_ProbeStatus;
    dmHttpClient::Result      m_ProbeResult;
    dmHttpClient::Result      m_Result;
    uint64_t                  m_PollTime;
    uint64_t                  m_MaxPollTime;
    uint32_t                  m_PollCount;
    dmArray<dmSocket::Socket> m_RawClients;
    dmMutex::HMutex           m_ClientMutex;
    dmThread::Thread          m_Watchdog;
    int32_atomic_t            m_StopWatchdog;
    int32_atomic_t            m_WatchdogFired;
    uint64_t                  m_ClientDeadline;

    void SetUp()
    {
        m_RequestPrefixSize = -1;
        m_RequestInstance = 0;
        m_RequestContentType = 0;
        m_PollTime = 0;
        m_MaxPollTime = 0;
        m_PollCount = 0;
        m_ClientMutex = dmMutex::New();
        m_Watchdog = 0;
        dmAtomicStore32(&m_StopWatchdog, 0);
        dmAtomicStore32(&m_WatchdogFired, 0);
        dmSocket::Initialize();
    }

    void TearDown()
    {
        dmAtomicStore32(&m_StopWatchdog, 1);
        if (m_Watchdog)
        {
            dmThread::Join(m_Watchdog);
        }
        for (uint32_t i = 0; i < m_RawClients.Size(); ++i)
        {
            dmSocket::Delete(m_RawClients[i]);
        }
        dmMutex::Delete(m_ClientMutex);
        dmSocket::Finalize();
        ASSERT_EQ(0, dmAtomicGet32(&m_WatchdogFired));
    }

    static void Header(dmHttpClient::HResponse, void* context, int status, const char* key, const char* value)
    {
        HttpServiceTest* self = (HttpServiceTest*)context;
        self->m_Status = status;
        if (dmStrCaseCmp(key, "Content-Type") == 0)
        {
            dmStrlCpy(self->m_ContentType, value, sizeof(self->m_ContentType));
        }
        if (dmStrCaseCmp(key, "Allow") == 0)
        {
            dmStrlCpy(self->m_Allow, value, sizeof(self->m_Allow));
        }
        if (dmStrCaseCmp(key, "Connection") == 0)
        {
            dmStrlCpy(self->m_Connection, value, sizeof(self->m_Connection));
        }
        if (dmStrCaseCmp(key, "Location") == 0)
        {
            dmStrlCpy(self->m_Location, value, sizeof(self->m_Location));
        }
        if (dmStrCaseCmp(key, "Cache-Control") == 0)
        {
            dmStrlCpy(self->m_CacheControl, value, sizeof(self->m_CacheControl));
        }
    }

    static void Content(dmHttpClient::HResponse, void* context, int status, const void* data, uint32_t size,
                        int32_t, uint32_t, uint32_t, uint32_t, const char*)
    {
        HttpServiceTest* self = (HttpServiceTest*)context;
        self->m_Status = status;
        if (size)
        {
            uint32_t offset = self->m_Body.Size();
            self->m_Body.SetCapacity(offset + size + 1);
            self->m_Body.SetSize(offset + size);
            memcpy(self->m_Body.Begin() + offset, data, size);
        }
    }

    static void Request(void* context)
    {
        HttpServiceTest* self = (HttpServiceTest*)context;
        char url[64];
        dmSnPrintf(url, sizeof(url), "http://127.0.0.1:%u", self->GetPort());
        dmURI::Parts uri;
        dmURI::Parse(url, &uri);
        dmHttpClient::NewParams params;
        params.m_Userdata = self;
        params.m_HttpHeader = Header;
        params.m_HttpContent = Content;
        params.m_RequestTimeout = SERVICE_TEST_TIMEOUT;
        params.m_MaxGetRetries = 1;
        params.m_HttpSendContentLength = ContentLength;
        params.m_HttpWriteHeaders = WriteHeaders;
        params.m_HttpWrite = self->m_RequestPrefixSize < 0 ? Write : WritePartialBody;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
        self->m_Result = dmHttpClient::Request(client, self->m_Method, self->m_Path);
        dmHttpClient::Delete(client);
        dmAtomicStore32(&self->m_Done, 1);
    }

    static dmHttpClient::Result WriteHeaders(dmHttpClient::HResponse response, void* context)
    {
        HttpServiceTest* self = (HttpServiceTest*)context;
        dmHttpClient::Result result = dmHttpClient::RESULT_OK;
        if (self->m_RequestInstance)
        {
            result = dmHttpClient::WriteHeader(response, "X-Engine-Instance", self->m_RequestInstance);
        }
        if (result == dmHttpClient::RESULT_OK && self->m_RequestContentType)
        {
            result = dmHttpClient::WriteHeader(response, "Content-Type", self->m_RequestContentType);
        }
        return result;
    }

    static uint32_t ContentLength(dmHttpClient::HResponse, void* context)
    {
        return ((HttpServiceTest*)context)->m_RequestBodySize;
    }

    static dmHttpClient::Result Write(dmHttpClient::HResponse response, uint32_t offset, uint32_t length, void* context)
    {
        HttpServiceTest* self = (HttpServiceTest*)context;
        if (!self->m_SendRequestBody)
        {
            return dmHttpClient::RESULT_OK;
        }
        return dmHttpClient::Write(response, self->m_RequestBody + offset, length);
    }

    static void ProbeContent(dmHttpClient::HResponse, void* context, int status, const void*, uint32_t, int32_t, uint32_t, uint32_t, uint32_t, const char*)
    {
        *(int*)context = status;
    }

    static dmHttpClient::Result WritePartialBody(dmHttpClient::HResponse response, uint32_t offset, uint32_t length, void* context)
    {
        HttpServiceTest* self = (HttpServiceTest*)context;
        uint32_t prefix_size = self->m_RequestPrefixSize;
        dmHttpClient::Result written = dmHttpClient::Write(response, self->m_RequestBody + offset, prefix_size);
        if (written != dmHttpClient::RESULT_OK)
        {
            return written;
        }

        // Require another request to complete while this upload is incomplete.
        char url[64];
        dmSnPrintf(url, sizeof(url), "http://127.0.0.1:%u", self->GetPort());
        dmURI::Parts uri;
        dmURI::Parse(url, &uri);
        dmHttpClient::NewParams params;
        params.m_Userdata = &self->m_ProbeStatus;
        params.m_HttpContent = ProbeContent;
        params.m_RequestTimeout = SERVICE_TEST_TIMEOUT;
        params.m_MaxGetRetries = 1;
        dmHttpClient::HClient client = dmHttpClient::New(&params, &uri);
        self->m_ProbeResult = dmHttpClient::Request(client, "GET", "/ping");
        dmHttpClient::Delete(client);

        // Release a blocking handler even if the probe timed out, so the regression fails without hanging the suite.
        return dmHttpClient::Write(response, self->m_RequestBody + offset + prefix_size, length - prefix_size);
    }

    void Fetch(const char* method, const char* path, const char* body = "", bool send_body = true, uint32_t body_size = UINT32_MAX)
    {
        m_Method = method;
        m_Path = path;
        m_RequestBody = body;
        m_RequestBodySize = body_size == UINT32_MAX ? strlen(body) : body_size;
        m_SendRequestBody = send_body;
        m_ProbeStatus = 0;
        m_ProbeResult = dmHttpClient::RESULT_UNKNOWN;
        m_Status = 0;
        m_ContentType[0] = 0;
        m_Allow[0] = 0;
        m_Connection[0] = 0;
        m_CacheControl[0] = 0;
        m_Location[0] = 0;
        m_Body.SetSize(0);
        m_PollTime = 0;
        m_MaxPollTime = 0;
        m_PollCount = 0;
        dmAtomicStore32(&m_Done, 0);
        dmThread::Thread client = dmThread::New(Request, 0x80000, this, "openapi-client");
        while (!dmAtomicGet32(&m_Done))
        {
            Poll();
        }
        dmThread::Join(client);
        if (m_Body.Full())
        {
            m_Body.OffsetCapacity(1);
        }
        m_Body.Push(0);
    }

    virtual void     Poll() = 0;
    virtual uint16_t GetPort() = 0;

    dmSocket::Socket OpenRequest(const char* request)
    {
        dmSocket::Address address;
        dmSocket::GetHostByName("127.0.0.1", &address);
        dmSocket::Socket socket;
        if (dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket) != dmSocket::RESULT_OK)
        {
            return dmSocket::INVALID_SOCKET_HANDLE;
        }
        if (dmSocket::Connect(socket, address, GetPort()) != dmSocket::RESULT_OK)
        {
            dmSocket::Delete(socket);
            return dmSocket::INVALID_SOCKET_HANDLE;
        }
        int sent;
        dmSocket::Send(socket, request, strlen(request), &sent);
        dmSocket::SetBlocking(socket, false);
        {
            DM_MUTEX_SCOPED_LOCK(m_ClientMutex);
            if (m_RawClients.Full())
            {
                m_RawClients.OffsetCapacity(16);
            }
            m_RawClients.Push(socket);
            m_ClientDeadline = dmTime::GetMonotonicTime() + SERVICE_TEST_TIMEOUT;
        }
        if (!m_Watchdog)
        {
            m_Watchdog = dmThread::New(Watchdog, 0x80000, this, "service-watchdog");
        }
        return socket;
    }

    static void Watchdog(void* context)
    {
        HttpServiceTest* self = (HttpServiceTest*)context;
        while (!dmAtomicGet32(&self->m_StopWatchdog))
        {
            {
                DM_MUTEX_SCOPED_LOCK(self->m_ClientMutex);
                if (!self->m_RawClients.Empty() && dmTime::GetMonotonicTime() >= self->m_ClientDeadline)
                {
                    // Only a regression watchdog: releasing withheld uploads lets
                    // a blocking handler return so the ordinary assertions fail.
                    dmAtomicStore32(&self->m_WatchdogFired, 1);
                    for (uint32_t i = 0; i < self->m_RawClients.Size(); ++i)
                    {
                        dmSocket::Shutdown(self->m_RawClients[i], dmSocket::SHUTDOWNTYPE_READWRITE);
                    }
                    return;
                }
            }
            dmTime::Sleep(1000);
        }
    }

    void CloseRequest(dmSocket::Socket socket)
    {
        DM_MUTEX_SCOPED_LOCK(m_ClientMutex);
        for (uint32_t i = 0; i < m_RawClients.Size(); ++i)
        {
            if (m_RawClients[i] == socket)
            {
                m_RawClients.EraseSwap(i);
                break;
            }
        }
        dmSocket::Shutdown(socket, dmSocket::SHUTDOWNTYPE_READWRITE);
        dmSocket::Delete(socket);
    }

    void ReadResponse(dmSocket::Socket socket)
    {
        m_Body.SetSize(0);
        m_Body.SetCapacity(65536);
        m_Body.Push(0);
        uint64_t deadline = dmTime::GetMonotonicTime() + SERVICE_TEST_TIMEOUT;
        while (!strstr(m_Body.Begin(), "\r\n0\r\n\r\n") && dmTime::GetMonotonicTime() < deadline)
        {
            Poll();
            char buffer[4096];
            int received = 0;
            dmSocket::Result result = dmSocket::Receive(socket, buffer, sizeof(buffer), &received);
            if (result == dmSocket::RESULT_TRY_AGAIN || result == dmSocket::RESULT_WOULDBLOCK)
            {
                continue;
            }
            if (result != dmSocket::RESULT_OK || !received)
            {
                break;
            }
            uint32_t offset = m_Body.Size() - 1;
            if (offset + received + 1 > m_Body.Capacity())
            {
                m_Body.OffsetCapacity(65536);
            }
            m_Body.SetSize(offset + received + 1);
            memcpy(m_Body.Begin() + offset, buffer, received);
            m_Body[m_Body.Size() - 1] = 0;
        }
        CloseRequest(socket);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\r\n0\r\n\r\n"));
    }

    void WaitForResponse(dmSocket::Socket socket)
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + SERVICE_TEST_TIMEOUT;
        bool ready = false;
        while (!ready && dmTime::GetMonotonicTime() < deadline)
        {
            Poll();
            dmSocket::Selector selector;
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, socket);
            ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Select(&selector, 0));
            ready = dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, socket);
        }
        ASSERT_TRUE(ready);
    }

};

#endif
