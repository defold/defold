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

#import <Foundation/Foundation.h>

#include <assert.h>
#include <stdint.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include "log.h"
#include "socket.h"
#include "network_constants.h"

namespace dmSocket
{
    Result GetLocalAddress(Address* address)
    {
        /*
         * NOTE: The IP address lookup code is currently tailored
         * for iOS as we assume en0 as adapter. This code doens't work for OSX in general
         */
        NSString *address_string = 0;
        struct ifaddrs *interfaces = NULL;
        struct ifaddrs *temp_addr = NULL;
        int success = 0;

        success = getifaddrs(&interfaces);
        if (success == 0)
        {
            temp_addr = interfaces;
            while(temp_addr != NULL)
            {
                if(temp_addr->ifa_addr->sa_family == AF_INET)
                {
                    if([[NSString stringWithUTF8String:temp_addr->ifa_name] isEqualToString:@"en0"])
                    {
                        address_string = [NSString stringWithUTF8String:inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr)];
                        break;
                    }
                }
                temp_addr = temp_addr->ifa_next;
            }
        }

        freeifaddrs(interfaces);

        if (address_string)
        {
            char ip[255];
            strcpy(ip, [address_string cStringUsingEncoding: NSISOLatin1StringEncoding]);
            *address = AddressFromIPString(ip);
            return RESULT_OK;
        }
        else
        {
            // We fallback to loopback address
            *address = AddressFromIPString(DM_LOOPBACK_ADDRESS_IPV4);
            return RESULT_OK;
        }
    }
}
