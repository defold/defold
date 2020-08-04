// Copyright 2020 The Defold Foundation
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
