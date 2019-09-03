#include "dns.h"
#include "socket.h"

namespace dmDNS
{
    static Result SocketToDNSResult(const dmSocket::Result res)
    {
        Result dns_res;
        switch (res)
        {
            case dmSocket::RESULT_OK:
                dns_res = dmDNS::RESULT_OK;
                break;
            case dmSocket::RESULT_HOST_NOT_FOUND:
                dns_res = dmDNS::RESULT_HOST_NOT_FOUND;
                break;
            default:
                dns_res = dmDNS::RESULT_UNKNOWN_ERROR;
        }

        return dns_res;
    }

    Result Initialize()
    {
        return RESULT_OK;
    }
    Result Finalize()
    {
        return RESULT_OK;
    }
    Result NewChannel(HChannel* channel)
    {
        return RESULT_OK;
    }
    Result RefreshChannel(HChannel channel)
    {
        return RESULT_OK;
    }
    void StopChannel(HChannel channel) {}
    void DeleteChannel(HChannel channel) {}
    Result GetHostByName(const char* name, dmSocket::Address* address, HChannel channel, bool ipv4, bool ipv6)
    {
        return SocketToDNSResult(dmSocket::GetHostByName(name, address, ipv4, ipv6));
    }
} // namespace dmDNS
