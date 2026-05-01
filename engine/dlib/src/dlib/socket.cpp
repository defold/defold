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

#include "socket.h"
#include "math.h"
#include "dstrings.h"
#include "log.h"
#include <assert.h>
#include <dlib/file_descriptor.h>

#include <string.h>

#include "log.h"
#include "socket_private.h"
#include "thread.h"
#include "atomic.h"
#include "time.h"

// Helper and utility functions
namespace dmSocket
{
    Address::Address() {
        m_family = dmSocket::DOMAIN_MISSING;
        memset(m_address, 0x0, sizeof(m_address));
    }

    bool Empty(Address address)
    {
        return address.m_address[0] == 0x0 && address.m_address[1] == 0x0
            && address.m_address[2] == 0x0 && address.m_address[3] == 0x0;
    }

    uint32_t* IPv4(Address* address)
    {
        assert(address->m_family == DOMAIN_IPV4);
        return &address->m_address[3];
    }

    uint32_t* IPv6(Address* address)
    {
        assert(address->m_family == DOMAIN_IPV6);
        return &address->m_address[0];
    }

    uint32_t BitDifference(Address a, Address b)
    {
        // Implements the Hamming Distance algorithm.
        // https://en.wikipedia.org/wiki/Hamming_distance
        uint32_t difference = 0;
        for (uint32_t i = 0; i < (sizeof(a.m_address) / sizeof(uint32_t)); ++i)
        {
            uint32_t current = a.m_address[i] ^ b.m_address[i];
            while (current != 0)
            {
                difference += current % 2;
                current = current >> 1;
            }
        }

        return difference;
    }

    Result Initialize()
    {
        return PlatformInitialize();
    }

    Result Finalize()
    {
        return PlatformFinalize();
    }

    Address AddressFromIPString(const char* hostname)
    {
        Address address;
        GetHostByName(hostname, &address);
        return address;
    }

    struct GetHostByNameThreadContext
    {
        int32_atomic_t m_Finished;
        const char* m_Name;
        Address m_Address;
        Result m_Result;
        bool m_Ipv4;
        bool m_Ipv6;
    };

    static void GetHostByNameThreadWorker(void* arg)
    {
        GetHostByNameThreadContext* ctx = (GetHostByNameThreadContext*)arg;
        ctx->m_Result = GetHostByName(ctx->m_Name, &ctx->m_Address, ctx->m_Ipv4, ctx->m_Ipv6);

        if (dmAtomicIncrement32(&ctx->m_Finished)+1 == 2)
        {
            // The calling thread was already finished, waiting for this thread.
            free((void*)ctx->m_Name);
            delete ctx;
        }
    }

#if defined(__EMSCRIPTEN__)
    Result GetHostByNameT(const char* name, Address* address, uint64_t timeout, int* cancelflag, bool ipv4, bool ipv6)
    {
        return GetHostByName(name, address, ipv4, ipv6);
    }
#else
    Result GetHostByNameT(const char* name, Address* address, uint64_t timeout, int* cancelflag, bool ipv4, bool ipv6)
    {
        const uint32_t THREAD_STACK_SIZE = 256*1024;

        GetHostByNameThreadContext* ctx = new GetHostByNameThreadContext;
        ctx->m_Name = strdup(name);
        ctx->m_Ipv4 = ipv4;
        ctx->m_Ipv6 = ipv6;
        ctx->m_Result = RESULT_HOSTUNREACH;
        ctx->m_Finished = 0;

        dmThread::Thread t = dmThread::New(&GetHostByNameThreadWorker, THREAD_STACK_SIZE, ctx, "GetHostByName");
        uint64_t tend = timeout ? dmTime::GetMonotonicTime() + timeout : 0xFFFFFFFFFFFFFFFF;
        while (tend > dmTime::GetMonotonicTime())
        {
            if (dmAtomicAdd32(&ctx->m_Finished, 0) == 1) // the thread has finished
            {
                break;
            }

            if (cancelflag && *cancelflag)
            {
                break;
            }

            dmTime::Sleep(2000);
        }

        // if the thread hasn't finished, we let the thread clean up
        if (dmAtomicIncrement32(&ctx->m_Finished)+1 == 1)
        {
            dmThread::Detach(t);
            return RESULT_TIMEDOUT;
        }

        dmThread::Join(t);

        Result result = ctx->m_Result;
        *address = ctx->m_Address;
        free((void*)ctx->m_Name);
        delete ctx;

        return result;
    }
#endif

    #define DM_SOCKET_RESULT_TO_STRING_CASE(x) case RESULT_##x: return #x;
    const char* ResultToString(Result r)
    {
        switch (r)
        {
            DM_SOCKET_RESULT_TO_STRING_CASE(OK);

            DM_SOCKET_RESULT_TO_STRING_CASE(ACCES);
            DM_SOCKET_RESULT_TO_STRING_CASE(AFNOSUPPORT);
            DM_SOCKET_RESULT_TO_STRING_CASE(WOULDBLOCK);
            DM_SOCKET_RESULT_TO_STRING_CASE(BADF);
            DM_SOCKET_RESULT_TO_STRING_CASE(CONNRESET);
            DM_SOCKET_RESULT_TO_STRING_CASE(DESTADDRREQ);
            DM_SOCKET_RESULT_TO_STRING_CASE(FAULT);
            DM_SOCKET_RESULT_TO_STRING_CASE(HOSTUNREACH);
            DM_SOCKET_RESULT_TO_STRING_CASE(INTR);
            DM_SOCKET_RESULT_TO_STRING_CASE(INVAL);
            DM_SOCKET_RESULT_TO_STRING_CASE(ISCONN);
            DM_SOCKET_RESULT_TO_STRING_CASE(MFILE);
            DM_SOCKET_RESULT_TO_STRING_CASE(MSGSIZE);
            DM_SOCKET_RESULT_TO_STRING_CASE(NETDOWN);
            DM_SOCKET_RESULT_TO_STRING_CASE(NETUNREACH);
            //DM_SOCKET_RESULT_TO_STRING_CASE(NFILE);
            DM_SOCKET_RESULT_TO_STRING_CASE(NOBUFS);
            //DM_SOCKET_RESULT_TO_STRING_CASE(NOENT);
            //DM_SOCKET_RESULT_TO_STRING_CASE(NOMEM);
            DM_SOCKET_RESULT_TO_STRING_CASE(NOTCONN);
            //DM_SOCKET_RESULT_TO_STRING_CASE(NOTDIR);
            DM_SOCKET_RESULT_TO_STRING_CASE(NOTSOCK);
            DM_SOCKET_RESULT_TO_STRING_CASE(OPNOTSUPP);
            DM_SOCKET_RESULT_TO_STRING_CASE(PIPE);
            DM_SOCKET_RESULT_TO_STRING_CASE(PROTONOSUPPORT);
            DM_SOCKET_RESULT_TO_STRING_CASE(PROTOTYPE);
            DM_SOCKET_RESULT_TO_STRING_CASE(TIMEDOUT);

            DM_SOCKET_RESULT_TO_STRING_CASE(ADDRNOTAVAIL);
            DM_SOCKET_RESULT_TO_STRING_CASE(CONNREFUSED);
            DM_SOCKET_RESULT_TO_STRING_CASE(ADDRINUSE);
            DM_SOCKET_RESULT_TO_STRING_CASE(CONNABORTED);
            DM_SOCKET_RESULT_TO_STRING_CASE(INPROGRESS);

            DM_SOCKET_RESULT_TO_STRING_CASE(HOST_NOT_FOUND);
            DM_SOCKET_RESULT_TO_STRING_CASE(TRY_AGAIN);
            DM_SOCKET_RESULT_TO_STRING_CASE(NO_RECOVERY);
            DM_SOCKET_RESULT_TO_STRING_CASE(NO_DATA);

            DM_SOCKET_RESULT_TO_STRING_CASE(UNKNOWN);
        }
        // TODO: Add log-domain support
        dmLogError("Unable to convert result %d to string", r);

        return "RESULT_UNDEFINED";
    }
    #undef DM_SOCKET_RESULT_TO_STRING_CASE

    Selector::Selector()
    {
        SelectorZero(this);
    }

    dmFileDescriptor::PollEvent SelectorKindToPollEvent(SelectorKind selector_kind)
    {
        switch (selector_kind)
        {
            case SELECTOR_KIND_READ:
                return dmFileDescriptor::EVENT_READ;
            case SELECTOR_KIND_WRITE:
                return dmFileDescriptor::EVENT_WRITE;
            case SELECTOR_KIND_EXCEPT:
                return dmFileDescriptor::EVENT_ERROR;
            default:
                assert(false);
                return dmFileDescriptor::EVENT_ERROR;
        }
    }

    void SelectorClear(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        dmFileDescriptor::PollEvent event = SelectorKindToPollEvent(selector_kind);
        dmFileDescriptor::PollerClearEvent(&selector->m_Poller, event, socket);
    }

    void SelectorSet(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        dmFileDescriptor::PollEvent event = SelectorKindToPollEvent(selector_kind);
        dmFileDescriptor::PollerSetEvent(&selector->m_Poller, event, socket);
    }

    bool SelectorIsSet(Selector* selector, SelectorKind selector_kind, Socket socket)
    {
        dmFileDescriptor::PollEvent event = SelectorKindToPollEvent(selector_kind);
        return dmFileDescriptor::PollerHasEvent(&selector->m_Poller, event, socket);
    }

    void SelectorZero(Selector* selector)
    {
        dmFileDescriptor::PollerReset(&selector->m_Poller);
    }

    Result Select(Selector* selector, int32_t timeout)
    {
        if (timeout > 0)
        {
            timeout = timeout / 1000;
        }
        int r = dmFileDescriptor::Wait(&selector->m_Poller, (timeout < 0) ? 0 : timeout);
        if (r < 0)
        {
            return NativeToResult(__FUNCTION__, __LINE__, DM_SOCKET_ERRNO);
        }
        else if(timeout > 0 && r == 0)
        {
            return RESULT_WOULDBLOCK;
        }
        else
        {
            return RESULT_OK;
        }
    }
}
