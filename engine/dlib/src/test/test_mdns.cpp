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
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

#include <dlib/mdns.h>
#include <dlib/atomic.h>
#include <dlib/network_constants.h>
#include <dlib/socket.h>
#include <dlib/thread.h>
#include <dlib/time.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#if defined(GITHUB_CI) && defined(__MACH__)
#define DM_SKIP_MDNS_DISCOVERY_TESTS
#endif

namespace
{
    static const char* SERVICE_TYPE = "_defoldtest._tcp";

    struct EventSnapshot
    {
        dmMDNS::EventType             m_Type;
        std::string                   m_Id;
        std::string                   m_InstanceName;
        std::string                   m_Host;
        uint16_t                      m_Port;
        std::map<std::string, std::string> m_Txt;
    };

    struct EventLog
    {
        std::vector<EventSnapshot> m_Events;

        static void Callback(void* context, const dmMDNS::ServiceEvent* event)
        {
            EventLog* self = (EventLog*) context;
            EventSnapshot snapshot;
            snapshot.m_Type = event->m_Type;
            snapshot.m_Id = event->m_Id ? event->m_Id : "";
            snapshot.m_InstanceName = event->m_InstanceName ? event->m_InstanceName : "";
            snapshot.m_Host = event->m_Host ? event->m_Host : "";
            snapshot.m_Port = event->m_Port;
            for (uint32_t i = 0; i < event->m_TxtCount; ++i)
            {
                const char* key = event->m_Txt[i].m_Key ? event->m_Txt[i].m_Key : "";
                const char* value = event->m_Txt[i].m_Value ? event->m_Txt[i].m_Value : "";
                snapshot.m_Txt[key] = value;
            }
            self->m_Events.push_back(snapshot);
        }

        bool Has(dmMDNS::EventType type, const char* id) const
        {
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_Id == id)
                    return true;
            }
            return false;
        }

        const EventSnapshot* Find(dmMDNS::EventType type, const char* id) const
        {
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_Id == id)
                    return &e;
            }
            return 0;
        }

        bool HasInstance(dmMDNS::EventType type, const char* instance_name) const
        {
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_InstanceName == instance_name)
                    return true;
            }
            return false;
        }

        const EventSnapshot* FindInstance(dmMDNS::EventType type, const char* instance_name) const
        {
            for (uint32_t i = 0; i < m_Events.size(); ++i)
            {
                const EventSnapshot& e = m_Events[i];
                if (e.m_Type == type && e.m_InstanceName == instance_name)
                    return &e;
            }
            return 0;
        }
    };

    static void Pump(dmMDNS::HMDNS mdns, dmMDNS::HBrowser browser, uint32_t iterations, uint32_t sleep_us)
    {
        for (uint32_t i = 0; i < iterations; ++i)
        {
            if (mdns)
                dmMDNS::Update(mdns);
            if (browser)
                dmMDNS::UpdateBrowser(browser);
            dmTime::Sleep(sleep_us);
        }
    }

    static bool WaitForEvent(EventLog& event_log, dmMDNS::EventType event_type, const char* instance_name, dmMDNS::HMDNS mdns, dmMDNS::HBrowser browser, uint32_t timeout_ms)
    {
        const uint64_t deadline = dmTime::GetMonotonicTime() + (uint64_t) timeout_ms * 1000ULL;
        while (dmTime::GetMonotonicTime() < deadline)
        {
            if (event_log.HasInstance(event_type, instance_name))
            {
                return true;
            }
            Pump(mdns, browser, 1, 5 * 1000);
        }
        return event_log.HasInstance(event_type, instance_name);
    }

    static bool WaitForFlag(int32_atomic_t* flag, uint32_t iterations, uint32_t sleep_us)
    {
        for (uint32_t i = 0; i < iterations; ++i)
        {
            if (dmAtomicGet32(flag))
                return true;
            dmTime::Sleep(sleep_us);
        }
        return false;
    }

    static void MakeUniqueName(char* buffer, size_t buffer_size, const char* prefix, uint64_t nonce)
    {
        snprintf(buffer, buffer_size, "%s-%llu", prefix, (unsigned long long) nonce);
    }

    struct TcpServerContext
    {
        TcpServerContext()
        {
            m_Port = 0;
            m_Result = dmSocket::RESULT_OK;
            dmAtomicStore32(&m_Listening, 0);
            dmAtomicStore32(&m_Accepted, 0);
            dmAtomicStore32(&m_Completed, 0);
            dmAtomicStore32(&m_Stop, 0);
        }

        uint16_t        m_Port;
        dmSocket::Result m_Result;
        int32_atomic_t  m_Listening;
        int32_atomic_t  m_Accepted;
        int32_atomic_t  m_Completed;
        int32_atomic_t  m_Stop;
    };

    struct ScopedMdnsTestResources
    {
        ScopedMdnsTestResources()
        : m_Mdns(0)
        , m_Browser(0)
        , m_ClientSocket(dmSocket::INVALID_SOCKET_HANDLE)
        , m_ServerThread(0)
        , m_HasServerThread(false)
        , m_ServerCtx(0)
        {
        }

        ~ScopedMdnsTestResources()
        {
            if (m_ServerCtx)
                dmAtomicStore32(&m_ServerCtx->m_Stop, 1);
            if (m_HasServerThread)
                dmThread::Join(m_ServerThread);
            if (m_ClientSocket != dmSocket::INVALID_SOCKET_HANDLE)
                dmSocket::Delete(m_ClientSocket);
            if (m_Browser != 0)
                dmMDNS::DeleteBrowser(m_Browser);
            if (m_Mdns != 0)
                dmMDNS::Delete(m_Mdns);
        }

        dmMDNS::HMDNS m_Mdns;
        dmMDNS::HBrowser m_Browser;
        dmSocket::Socket m_ClientSocket;
        dmThread::Thread m_ServerThread;
        bool m_HasServerThread;
        TcpServerContext* m_ServerCtx;
    };

    static void TcpServerThread(void* ctx_ptr)
    {
        TcpServerContext* ctx = (TcpServerContext*) ctx_ptr;

        dmSocket::Socket server_socket = dmSocket::INVALID_SOCKET_HANDLE;
        dmSocket::Socket client_socket = dmSocket::INVALID_SOCKET_HANDLE;
        dmSocket::Address bind_address;
        dmSocket::Address server_name;
        dmSocket::Result r = dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &server_socket);
        if (r != dmSocket::RESULT_OK)
        {
            ctx->m_Result = r;
            dmAtomicStore32(&ctx->m_Completed, 1);
            return;
        }

        r = dmSocket::SetReuseAddress(server_socket, true);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::GetHostByName(DM_UNIVERSAL_BIND_ADDRESS_IPV4, &bind_address, true, false);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::Bind(server_socket, bind_address, 0);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::Listen(server_socket, 8);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::GetName(server_socket, &server_name, &ctx->m_Port);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        r = dmSocket::SetBlocking(server_socket, false);
        if (r != dmSocket::RESULT_OK)
            goto bail;

        dmAtomicStore32(&ctx->m_Listening, 1);

        for (uint32_t i = 0; i < 500 && !dmAtomicGet32(&ctx->m_Stop); ++i)
        {
            dmSocket::Address client_address;
            r = dmSocket::Accept(server_socket, &client_address, &client_socket);
            if (r == dmSocket::RESULT_WOULDBLOCK)
            {
                dmTime::Sleep(10 * 1000);
                continue;
            }
            if (r != dmSocket::RESULT_OK)
                goto bail;

            dmAtomicStore32(&ctx->m_Accepted, 1);

            char request[4];
            int read = 0;
            r = dmSocket::Receive(client_socket, request, sizeof(request), &read);
            if (r != dmSocket::RESULT_OK || read != (int) sizeof(request) || memcmp(request, "PING", 4) != 0)
            {
                ctx->m_Result = (r == dmSocket::RESULT_OK) ? dmSocket::RESULT_UNKNOWN : r;
                goto bail;
            }

            int written = 0;
            r = dmSocket::Send(client_socket, "PONG", 4, &written);
            if (r != dmSocket::RESULT_OK || written != 4)
            {
                ctx->m_Result = (r == dmSocket::RESULT_OK) ? dmSocket::RESULT_UNKNOWN : r;
                goto bail;
            }

            ctx->m_Result = dmSocket::RESULT_OK;
            goto bail;
        }

        ctx->m_Result = dmSocket::RESULT_TIMEDOUT;

bail:
        if (client_socket != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(client_socket);
        if (server_socket != dmSocket::INVALID_SOCKET_HANDLE)
            dmSocket::Delete(server_socket);

        dmAtomicStore32(&ctx->m_Completed, 1);
    }
}

TEST(MDNS, RegisterLifecycle)
{
    dmMDNS::HMDNS mdns = 0;

    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::New(0, 0));
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::New(0, &mdns));

    dmMDNS::Params params;
    params.m_AnnounceInterval = 1;
    params.m_Ttl = 2;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &mdns));
    ASSERT_NE((dmMDNS::HMDNS) 0, mdns);

    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::RegisterService(mdns, 0));
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::DeregisterService(mdns, 0));

    char long_instance_name[80];
    memset(long_instance_name, 'a', sizeof(long_instance_name) - 1);
    long_instance_name[sizeof(long_instance_name) - 1] = 0;

    dmMDNS::ServiceDesc invalid_service;
    memset(&invalid_service, 0, sizeof(invalid_service));
    invalid_service.m_Id = "mdns-invalid-01";
    invalid_service.m_InstanceName = long_instance_name;
    invalid_service.m_ServiceType = SERVICE_TYPE;
    invalid_service.m_Port = 18001;
    ASSERT_EQ(dmMDNS::RESULT_INVALID_ARGS, dmMDNS::RegisterService(mdns, &invalid_service));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", "mdns-lifecycle-01"},
        {"name", "lifecycle-target"},
        {"log_port", "18001"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = "mdns-lifecycle-01";
    service.m_InstanceName = "lifecycle-target";
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Port = 18001;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = 2;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(mdns, &service));
    ASSERT_EQ(dmMDNS::RESULT_ALREADY_REGISTERED, dmMDNS::RegisterService(mdns, &service));
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::DeregisterService(mdns, service.m_Id));
    ASSERT_EQ(dmMDNS::RESULT_NOT_REGISTERED, dmMDNS::DeregisterService(mdns, service.m_Id));

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::Delete(mdns));
}

TEST(MDNS, ResolveAndRemove)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    EventLog event_log;
    ScopedMdnsTestResources cleanup;

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_id[128];
    char instance_name[128];
    char advertised_id[128];
    MakeUniqueName(service_id, sizeof(service_id), "mdns-resolve", nonce);
    MakeUniqueName(instance_name, sizeof(instance_name), "resolve-target", nonce);
    MakeUniqueName(advertised_id, sizeof(advertised_id), "mdns-resolve-txt", nonce);

    dmMDNS::Params params;
    params.m_AnnounceInterval = 1;
    params.m_Ttl = 2;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &cleanup.m_Mdns));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = SERVICE_TYPE;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", advertised_id},
        {"name", instance_name},
        {"log_port", "19001"},
        {"schema", "1"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = service_id;
    service.m_InstanceName = instance_name;
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Port = 19001;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = 2;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(cleanup.m_Mdns, &service));
    ASSERT_TRUE(WaitForEvent(event_log, dmMDNS::EVENT_RESOLVED, service.m_InstanceName, cleanup.m_Mdns, cleanup.m_Browser, 15000));

    const EventSnapshot* resolved = event_log.FindInstance(dmMDNS::EVENT_RESOLVED, service.m_InstanceName);
    ASSERT_NE((const EventSnapshot*) 0, resolved);
    ASSERT_EQ(service.m_Port, resolved->m_Port);
    ASSERT_TRUE(!resolved->m_Host.empty());
    ASSERT_EQ(std::string(instance_name), resolved->m_InstanceName);
    ASSERT_EQ(std::string(advertised_id), resolved->m_Id);
    std::map<std::string, std::string>::const_iterator log_port_it = resolved->m_Txt.find("log_port");
    ASSERT_TRUE(log_port_it != resolved->m_Txt.end());
    ASSERT_EQ(std::string("19001"), log_port_it->second);
    std::map<std::string, std::string>::const_iterator schema_it = resolved->m_Txt.find("schema");
    ASSERT_TRUE(schema_it != resolved->m_Txt.end());
    ASSERT_EQ(std::string("1"), schema_it->second);

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::DeregisterService(cleanup.m_Mdns, service.m_Id));
    ASSERT_TRUE(WaitForEvent(event_log, dmMDNS::EVENT_REMOVED, service.m_InstanceName, cleanup.m_Mdns, cleanup.m_Browser, 15000));
}

TEST(MDNS, ResolveAndConnect)
{
#if defined(DM_SKIP_MDNS_DISCOVERY_TESTS)
    SKIP();
#endif

    TcpServerContext server_ctx;
    ScopedMdnsTestResources cleanup;
    cleanup.m_ServerCtx = &server_ctx;
    cleanup.m_ServerThread = dmThread::New(TcpServerThread, 0x80000, &server_ctx, "mdns-tcp-server");
    ASSERT_TRUE(cleanup.m_ServerThread != (dmThread::Thread) 0);
    cleanup.m_HasServerThread = true;
    ASSERT_TRUE(WaitForFlag(&server_ctx.m_Listening, 300, 10 * 1000));
    ASSERT_TRUE(server_ctx.m_Port != 0);

    EventLog event_log;

    const uint64_t nonce = dmTime::GetMonotonicTime();
    char service_id[128];
    char instance_name[128];
    char advertised_id[128];
    MakeUniqueName(service_id, sizeof(service_id), "mdns-connect", nonce);
    MakeUniqueName(instance_name, sizeof(instance_name), "connect-target", nonce);
    MakeUniqueName(advertised_id, sizeof(advertised_id), "mdns-connect-txt", nonce);

    dmMDNS::Params params;
    params.m_AnnounceInterval = 1;
    params.m_Ttl = 2;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::New(&params, &cleanup.m_Mdns));

    dmMDNS::BrowserParams browser_params;
    browser_params.m_ServiceType = SERVICE_TYPE;
    browser_params.m_Callback = EventLog::Callback;
    browser_params.m_Context = &event_log;
    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::NewBrowser(&browser_params, &cleanup.m_Browser));

    dmMDNS::TxtEntry txt_entries[] =
    {
        {"id", advertised_id},
        {"name", instance_name},
        {"log_port", "19999"},
    };

    dmMDNS::ServiceDesc service;
    memset(&service, 0, sizeof(service));
    service.m_Id = service_id;
    service.m_InstanceName = instance_name;
    service.m_ServiceType = SERVICE_TYPE;
    service.m_Port = server_ctx.m_Port;
    service.m_Txt = txt_entries;
    service.m_TxtCount = sizeof(txt_entries) / sizeof(txt_entries[0]);
    service.m_Ttl = 2;

    ASSERT_EQ(dmMDNS::RESULT_OK, dmMDNS::RegisterService(cleanup.m_Mdns, &service));

    for (uint32_t i = 0; i < 300 && !event_log.HasInstance(dmMDNS::EVENT_RESOLVED, service.m_InstanceName); ++i)
    {
        Pump(cleanup.m_Mdns, cleanup.m_Browser, 1, 10 * 1000);
    }

    const EventSnapshot* resolved = event_log.FindInstance(dmMDNS::EVENT_RESOLVED, service.m_InstanceName);
    ASSERT_NE((const EventSnapshot*) 0, resolved);
    ASSERT_TRUE(!resolved->m_Host.empty());
    ASSERT_EQ(std::string(instance_name), resolved->m_InstanceName);
    ASSERT_EQ(std::string(advertised_id), resolved->m_Id);

    dmSocket::Address address = dmSocket::AddressFromIPString(resolved->m_Host.c_str());
    if (address.m_family != dmSocket::DOMAIN_IPV4)
    {
        ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::GetHostByName(resolved->m_Host.c_str(), &address, true, false));
    }

    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::New(dmSocket::DOMAIN_IPV4, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &cleanup.m_ClientSocket));
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Connect(cleanup.m_ClientSocket, address, resolved->m_Port));

    int written = 0;
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Send(cleanup.m_ClientSocket, "PING", 4, &written));
    ASSERT_EQ(4, written);

    char response[4] = {0};
    int read = 0;
    ASSERT_EQ(dmSocket::RESULT_OK, dmSocket::Receive(cleanup.m_ClientSocket, response, sizeof(response), &read));
    ASSERT_EQ(4, read);
    ASSERT_TRUE(memcmp(response, "PONG", 4) == 0);

    ASSERT_TRUE(dmAtomicGet32(&server_ctx.m_Accepted) == 1);
    ASSERT_EQ(dmSocket::RESULT_OK, server_ctx.m_Result);
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSocket::Finalize();
    return ret;
}
