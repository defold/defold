#include "dns.h"
#include "socket.h"

#if !defined(__EMSCRIPTEN__)

#include "time.h"
#include "mutex.h"

#define CARES_STATICLIB
#include <ares.h>
#include <assert.h>

#if defined(__linux__) || defined(__MACH__) || defined(__AVM2__)
    #include <netdb.h>
#endif

namespace dmDNS
{
    struct RequestResult
    {
        dmSocket::Address* m_Address;
        uint32_t           m_Status : 30;
        uint32_t           m_Ipv4   : 1;
        uint32_t           m_Ipv6   : 1;
    };

    struct Channel
    {
        ares_channel    m_Handle;
        dmMutex::HMutex m_RunMutex;
        bool            m_Running;
    };

    static inline Result AresStatusToDNSResult(uint32_t ares_status)
    {
        Result res;

        switch(ares_status)
        {
            case(ARES_SUCCESS):
                res = RESULT_OK;
                break;
            case(ARES_ENOTINITIALIZED):
                res = RESULT_INIT_ERROR;
                break;
            case(ARES_ENODATA):
            case(ARES_EFORMERR):
            case(ARES_ESERVFAIL):
            case(ARES_ENOTFOUND):
            case(ARES_ENOTIMP):
            case(ARES_EREFUSED):
            case(ARES_ETIMEOUT):
                res = RESULT_HOST_NOT_FOUND;
                break;
            case(ARES_ECANCELLED):
                res = RESULT_CANCELLED;
                break;
            default:
                res = RESULT_UNKNOWN_ERROR;
        }

        return res;
    }

    static void ares_gethost_callback(void *arg, int status, int timeouts, struct hostent *host)
    {
        assert(arg);
        RequestResult* req = (RequestResult*) arg;

        if (status != ARES_ECANCELLED)
        {
            req->m_Status = (uint32_t) status;
        }

        if (host == 0x0 || status != ARES_SUCCESS)
        {
            return;
        }

        assert(host->h_addr_list[0]);
        if (host->h_addrtype == AF_INET)
        {
            req->m_Address->m_family     = dmSocket::DOMAIN_IPV4;
            req->m_Address->m_address[3] = *(uint32_t*) host->h_addr_list[0];
        }
        else if (host->h_addrtype == AF_INET6)
        {
            req->m_Address->m_family = dmSocket::DOMAIN_IPV6;
            memcpy(&req->m_Address->m_address[0], host->h_addr_list[0], sizeof(struct in6_addr));
        }
    }

    static void ares_addrinfo_callback(void* arg, int status, int timeouts, struct ares_addrinfo* info)
    {
        assert(arg);
        RequestResult* req = (RequestResult*) arg;

        if (status != ARES_ECANCELLED)
        {
            req->m_Status = (uint32_t) status;
        }

        if (info == 0x0 || status != ARES_SUCCESS)
        {
            if (info)
            {
                ares_freeaddrinfo(info);
            }

            return;
        }

        // Note: This is a bit different than the 'regular' getaddrinfo call in dmSocket.
        //       Since we don't return a list of addresses, we need to pick one and since
        //       IPV6 sometimes fail when setting up the socket connection (apparently google.com),
        //       we try to select an IPV4 address first if possible.
        struct ares_addrinfo* iterator = info, *selected = 0x0;
        bool prefer_ipv4               = req->m_Ipv4 && req->m_Ipv6;
        while (iterator)
        {
            if (req->m_Ipv4 && iterator->ai_family == AF_INET)
            {
                selected = iterator;
                if (prefer_ipv4)
                {
                    break;
                }
            }
            else if (req->m_Ipv6 && iterator->ai_family == AF_INET6)
            {
                selected = iterator;
                if(!prefer_ipv4)
                {
                    break;
                }
            }

            iterator = iterator->ai_next;
        }

        if (selected)
        {
            if (selected->ai_family == AF_INET)
            {
                sockaddr_in* saddr           = (struct sockaddr_in *) selected->ai_addr;
                req->m_Address->m_family     = dmSocket::DOMAIN_IPV4;
                req->m_Address->m_address[3] = saddr->sin_addr.s_addr;
            }
            else
            {
                sockaddr_in6* saddr      = (struct sockaddr_in6 *) iterator->ai_addr;
                req->m_Address->m_family = dmSocket::DOMAIN_IPV6;
                memcpy(&req->m_Address->m_address[0], &saddr->sin6_addr, sizeof(struct in6_addr));
            }
        }
        else
        {
            req->m_Status = ARES_EBADQUERY;
        }

        ares_freeaddrinfo(info);
    }

    Result Initialize()
    {
        if (ares_library_init(ARES_LIB_INIT_ALL) == ARES_SUCCESS)
        {
            return RESULT_OK;
        }

        return  RESULT_INIT_ERROR;
    }

    Result Finalize()
    {
        ares_library_cleanup();
        return RESULT_OK;
    }

    Result NewChannel(HChannel* channel)
    {
        Channel* dns_channel = new Channel();

        if (ares_init(&dns_channel->m_Handle) != ARES_SUCCESS)
        {
            delete dns_channel;
            return RESULT_INIT_ERROR;
        }

        dns_channel->m_RunMutex = dmMutex::New();
        dns_channel->m_Running  = true;
        *channel = dns_channel;

        return RESULT_OK;
    }

    void StopChannel(HChannel channel)
    {
        Channel* dns_channel = (Channel*) channel;
        dmMutex::ScopedLock lk(dns_channel->m_RunMutex);
        dns_channel->m_Running = false;
    }

    void DeleteChannel(HChannel channel)
    {
        if (channel)
        {
            Channel* dns_channel = (Channel*) channel;
            ares_cancel(dns_channel->m_Handle);
            delete dns_channel;
        }
    }

    Result GetHostByName(const char* name, dmSocket::Address* address, HChannel channel, bool ipv4, bool ipv6)
    {
        assert(channel);
        assert(address);

        // Note: We are emulating a total-request timeout of 30 seconds here, but we can use
        //       much more sensible numbers.. This is to mock the built-in timeout in getaddrinfo.
        static const uint64_t request_timeout = 30 * 1000000;
        uint64_t              request_started = dmTime::GetTime();

        Channel* dns_channel = (Channel*) channel;

        RequestResult req;
        int32_t want_family = 0;

        if (ipv4 == ipv6)
            want_family = AF_UNSPEC;
        else if (ipv4)
            want_family = AF_INET;
        else if (ipv6)
            want_family = AF_INET6;

        req.m_Address = address;
        req.m_Status  = ARES_SUCCESS;

        struct in_addr       addr4;
        struct ares_in6_addr addr6;

        // We need to check the address if it's an actual IP address or not.
        // The regular getaddrinfo seems to return an address, so in that case
        // we need to invoke gethostbyaddr instead.
        if (ares_inet_pton(AF_INET, name, &addr4) == 1)
        {
            ares_gethostbyaddr(dns_channel->m_Handle, &addr4, sizeof(addr4), AF_INET, ares_gethost_callback, (void*)&req);
        }
        else if (ares_inet_pton(AF_INET6, name, &addr6) == 1)
        {
            ares_gethostbyaddr(dns_channel->m_Handle, &addr6, sizeof(addr6), AF_INET6, ares_gethost_callback, (void*)&req);
        }
        else
        {
            ares_addrinfo hints;
            memset(&hints, 0x0, sizeof(hints));
            hints.ai_family   = want_family;
            hints.ai_socktype = SOCK_STREAM; // Note: TCP hint not supported
            req.m_Ipv4        = ipv4;
            req.m_Ipv6        = ipv6;

            ares_getaddrinfo(dns_channel->m_Handle, name, NULL, &hints, ares_addrinfo_callback, (void*)&req);
        }

        // This is a blocking wait and presumes that a single request is handed at a time.
        // This can easily be turned into a threaded queue, but isn't for simplicity.
        while(1)
        {
            // Early-out if a different thread has decided stopped the channel.
            // This might happen when the engine is shutting down, and
            // we have lost or no connection and don't want to wait for
            // the full timeout..
            {
                dmMutex::ScopedLock lk(dns_channel->m_RunMutex);
                if (!dns_channel->m_Running)
                {
                    ares_cancel(dns_channel->m_Handle);
                    req.m_Status = ARES_ECANCELLED;
                    break;
                }
            }

            dmSocket::Selector selector;
            selector.m_Nfds = ares_fds(dns_channel->m_Handle,
                &selector.m_FdSets[dmSocket::SELECTOR_KIND_READ],
                &selector.m_FdSets[dmSocket::SELECTOR_KIND_WRITE]);

            // No more requests?
            if (selector.m_Nfds == 0)
            {
                ares_cancel(dns_channel->m_Handle);
                break;
            }
            else
            {
                // Ares adds a + 1 to the socket, as does dmSocket::Select
                // So we just compensate for this.
                selector.m_Nfds -= 1;
            }

            if ((dmTime::GetTime() - request_started) > request_timeout)
            {
                req.m_Status = ARES_ETIMEOUT;
                break;
            }

            // Select on this socket for a little bit (50ms)
            dmSocket::Select(&selector, 50000);
            ares_process(dns_channel->m_Handle,
                &selector.m_FdSets[dmSocket::SELECTOR_KIND_READ],
                &selector.m_FdSets[dmSocket::SELECTOR_KIND_WRITE]);
        }

        return AresStatusToDNSResult(req.m_Status);
    }
}
#else // no c-ares support for __EMSCRIPTEN__
namespace dmDNS
{
    static Result SocketResultToDNS(const dmSocket::Result res)
    {
        Result dns_res = dmDNS::RESULT_UNKNOWN_ERROR;
        switch(res)
        {
            case(dmSocket::RESULT_OK):
                dns_res = dmDNS::RESULT_OK;
                break;
            case(dmSocket::RESULT_HOST_NOT_FOUND):
                dns_res = dmDNS::RESULT_HOST_NOT_FOUND;
                break;
        }

        return dns_res;
    }

    Result Initialize() { return RESULT_OK; }
    Result Finalize() { return RESULT_OK; }
    Result NewChannel(HChannel* channel) { return RESULT_OK; }
    void   StopChannel(HChannel channel) {}
    void   DeleteChannel(HChannel channel) {}
    Result GetHostByName(const char* name, dmSocket::Address* address, HChannel channel, bool ipv4, bool ipv6)
    {
        return SocketResultToDNS(dmSocket::GetHostByName(name, address, ipv4, ipv6));
    }
}
#endif
