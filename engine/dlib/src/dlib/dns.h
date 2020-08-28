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

#ifndef DM_DNS_H
#define DM_DNS_H

#include <dmsdk/dlib/dns.h>

namespace dmSocket
{
    struct Address;
}

namespace dmDNS
{
    /**
     * Initialize the DNS system. Some platforms require socket initialization to be called
     * before this function. I.e: call dmSocket::Initialize() before dmDNS::Initialize() on win32.
     * @return RESULT_OK on success
     */
    Result Initialize();

    /**
     * Finalize DNS system.
     * @return RESULT_OK on success
     */
    Result Finalize();

    /**
     * Get host by name. Note that this function is not entirely thread-safe, even though it is used in a threaded environment.
     * @param name  Hostname to resolve
     * @param address Host address result
     * @param channel Channel handle to put the request on. The function does not handle invalid handles - the channel handle must exist.
     * @param ipv4 Whether or not to search for IPv4 addresses
     * @param ipv6 Whether or not to search for IPv6 addresses
     * @return RESULT_OK on success
     */
    Result GetHostByName(const char* name, dmSocket::Address* address, HChannel channel, bool ipv4 = true, bool ipv6 = true);

    // Used for unit testing
    const char* ResultToString(Result r);
}

#endif // DM_DNS_H
