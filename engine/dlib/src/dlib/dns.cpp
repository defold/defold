#include "dns.h"
#include "dns_private.h"
#include "socket.h"

#include "time.h"
#include "atomic.h"

#define CARES_STATICLIB
#include <ares.h>
#include <assert.h>

#if defined(__linux__) || defined(__MACH__) || defined(__AVM2__)
    #include <netdb.h>
#endif

namespace dmDNS
{
    struct RequestInfo
    {
        dmSocket::Address m_Address;
        uint32_t          m_Status : 30;
        uint32_t          m_Ipv4   : 1;
        uint32_t          m_Ipv6   : 1;
    };

    struct Channel
    {
        ares_channel    m_Handle;
        int32_atomic_t  m_Running;
    };

    static inline Result AresStatusToDNSResult(uint32_t ares_status)
    {
        Result res;

        switch (ares_status)
        {
            case ARES_SUCCESS:
                res = RESULT_OK;
                break;
            case ARES_ENOTINITIALIZED:
                res = RESULT_INIT_ERROR;
                break;
            case ARES_ENODATA:
            case ARES_EFORMERR:
            case ARES_ESERVFAIL:
            case ARES_ENOTFOUND:
            case ARES_ENOTIMP:
            case ARES_EREFUSED:
            case ARES_ETIMEOUT:
                res = RESULT_HOST_NOT_FOUND;
                break;
            case ARES_ECANCELLED:
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
        RequestInfo* req = (RequestInfo*) arg;

        if (status == ARES_ENOTFOUND)
        {
            // If we cannot find a IP host via DNS, we set the result to SUCCESS
            // so that we can attempt to make a connection anyway.
            req->m_Status = ARES_SUCCESS;
        }
        else if (status != ARES_ECANCELLED && status != ARES_EDESTRUCTION)
        {
            // we cannot guarantee that the request data is still alive if the
            // request is either cancelled or destroyed. If so, we don't touch
            // the request pointer.
            req->m_Status = (uint32_t) status;
        }

        if (host == 0x0 || status != ARES_SUCCESS)
        {
            return;
        }

        assert(host->h_addr_list[0]);
        if (req->m_Ipv4 && host->h_addrtype == AF_INET)
        {
            req->m_Address.m_family     = dmSocket::DOMAIN_IPV4;
            req->m_Address.m_address[3] = *(uint32_t*) host->h_addr_list[0];
        }
        else if (req->m_Ipv6 && host->h_addrtype == AF_INET6)
        {
            req->m_Address.m_family = dmSocket::DOMAIN_IPV6;
            memcpy(&req->m_Address.m_address[0], host->h_addr_list[0], sizeof(struct in6_addr));
        }
        else
        {
            req->m_Status = ARES_ENOTFOUND;
        }
    }

    static void ares_addrinfo_callback(void* arg, int status, int timeouts, struct ares_addrinfo* info)
    {
        assert(arg);
        RequestInfo* req = (RequestInfo*) arg;

        // Same as ares_gethost_callback, we need to trust the req pointer.
        if (status != ARES_ECANCELLED && status != ARES_EDESTRUCTION)
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

        // Note: The address selection here is a bit different than the 'regular' getaddrinfo call in dmSocket::GetHostByName.
        //       Since we only return a single address and not a list of addresses to try to connect to, we must pick a good
        //       candidate here. IPV6 seems to fail sometimes when setting up the socket connection
        //       (apparently google.com but likely others aswell), so we try to prioritize selecting an IPV4 address first
        //       if we have both protocols available.
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
                sockaddr_in* saddr          = (struct sockaddr_in *) selected->ai_addr;
                req->m_Address.m_family     = dmSocket::DOMAIN_IPV4;
                req->m_Address.m_address[3] = saddr->sin_addr.s_addr;
            }
            else
            {
                sockaddr_in6* saddr     = (struct sockaddr_in6 *) selected->ai_addr;
                req->m_Address.m_family = dmSocket::DOMAIN_IPV6;
                memcpy(&req->m_Address.m_address[0], &saddr->sin6_addr, sizeof(struct in6_addr));
            }
        }
        else
        {
            req->m_Status = ARES_ENOTFOUND;
        }

        ares_freeaddrinfo(info);
    }

    Result Initialize()
    {
        if (ares_library_init(ARES_LIB_INIT_ALL) != ARES_SUCCESS)
        {
            return RESULT_INIT_ERROR;
        }

    #if defined(ANDROID)
        if (!InitializeAndroid())
        {
            return RESULT_INIT_ERROR;
        }
    #endif

        return RESULT_OK;
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

        dns_channel->m_Running = 1;
        *channel = dns_channel;

        return RESULT_OK;
    }

    void StopChannel(HChannel channel)
    {
        if (channel)
        {
            Channel* dns_channel = (Channel*) channel;
            dmAtomicStore32(&dns_channel->m_Running, 0);
        }
    }

    void DeleteChannel(HChannel channel)
    {
        if (channel)
        {
            Channel* dns_channel = (Channel*) channel;
            ares_destroy(dns_channel->m_Handle);
            delete dns_channel;
        }
    }

    Result RefreshChannel(HChannel channel)
    {
        if (channel && ares_init(&((Channel*)channel)->m_Handle) == ARES_SUCCESS)
        {
            return RESULT_OK;
        }

        return RESULT_INIT_ERROR;
    }

    // Note: This function should ultimately replace the dmSocket::GetHostByName, but there's a few places
    //       left in the engine where the old version is still used. This is because we need to pass along a
    //       dns channel to this function, which needs to be stored somewhere that's easily reachable by
    //       the calling functions on the main thread. We could do something like a global channel that lives
    //       in this module that can be accessed everywhere, but we need to make sure this solution works first.
    Result GetHostByName(const char* name, dmSocket::Address* address, HChannel channel, bool ipv4, bool ipv6)
    {
        assert(channel);
        assert(address);

        // Note: We are emulating a total-request timeout of 30 seconds here, but we can use
        //       much more sensible numbers.. This is to mock the built-in timeout in getaddrinfo.
        static const uint64_t request_timeout = 30 * 1000000;
        uint64_t              request_started = dmTime::GetTime();

        Channel* dns_channel = (Channel*) channel;

        RequestInfo req;
        req.m_Status  = ARES_SUCCESS;
        req.m_Ipv4    = ipv4;
        req.m_Ipv6    = ipv6;

        struct in_addr       addr4;
        struct ares_in6_addr addr6;

        // We need to check the address if it's an actual IP address or not.
        // The regular getaddrinfo returns an address in that case, so we need to
        // mimick that behaviour here aswell.
        if (ares_inet_pton(AF_INET, name, &addr4) == 1)
        {
            req.m_Address.m_family     = dmSocket::DOMAIN_IPV4;
            req.m_Address.m_address[3] = addr4.s_addr;

            ares_gethostbyaddr(dns_channel->m_Handle, &addr4, sizeof(addr4), AF_INET, ares_gethost_callback, (void*)&req);
        }
        else if (ares_inet_pton(AF_INET6, name, &addr6) == 1)
        {
            req.m_Address.m_family     = dmSocket::DOMAIN_IPV6;
            memcpy(&req.m_Address.m_address[0], &addr6, sizeof(struct in6_addr));

            ares_gethostbyaddr(dns_channel->m_Handle, &addr6, sizeof(addr6), AF_INET6, ares_gethost_callback, (void*)&req);
        }
        else
        {
            int32_t want_family = 0;

            if (ipv4 == ipv6)
                want_family = AF_UNSPEC;
            else if (ipv4)
                want_family = AF_INET;
            else if (ipv6)
                want_family = AF_INET6;

            // Note: SOCK_STREAM TCP hint doesn't do anything. To force TCP transport, set the ARES_FLAG_USEVC
            //       in the flags for ares when initializing the channel.
            ares_addrinfo hints;
            memset(&hints, 0x0, sizeof(hints));
            hints.ai_family   = want_family;
            hints.ai_socktype = SOCK_STREAM;

            ares_getaddrinfo(dns_channel->m_Handle, name, NULL, &hints, ares_addrinfo_callback, (void*)&req);
        }

        // Here, we block until the request is handled. The engine is built to wait for the result from each
        // of these requests, so we need to accept that for now. This can easily be translated into a threaded
        // queue, but is put on hold until we know this solution actually solves the ANR issue.
        while(1)
        {
            // Early-out if a different thread has decided to stop the channel.
            // This might happen when the engine is shutting down, and
            // we have lost or no connection and don't want to wait for
            // the full timeout..
            if (dns_channel->m_Running == 0)
            {
                ares_cancel(dns_channel->m_Handle);
                req.m_Status = ARES_ECANCELLED;
                break;
            }

            dmSocket::Selector selector;
            selector.m_Nfds = ares_fds(dns_channel->m_Handle,
                &selector.m_FdSets[dmSocket::SELECTOR_KIND_READ],
                &selector.m_FdSets[dmSocket::SELECTOR_KIND_WRITE]);

            if (selector.m_Nfds == 0)
            {
                ares_cancel(dns_channel->m_Handle);
                break;
            }
            else
            {
                // Ares adds a + 1 to the socket, as does our own dmSocket::Select impl,
                // so we just need to compensate for this.
                selector.m_Nfds -= 1;
            }

            // This is a manual timeout. C-ares is supposed to handle this, but it doesn't work.
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

        if (req.m_Status == ARES_SUCCESS)
        {
            memcpy(address, &req.m_Address, sizeof(req.m_Address));
        }

        return AresStatusToDNSResult(req.m_Status);
    }
}
