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

#ifndef DMSDK_DNS_H
#define DMSDK_DNS_H


/*# SDK DNS API documentation
 * [file:<dmsdk/dlib/dns.h>]
 *
 * Dns functions.
 *
 * @document
 * @name Dns
 * @namespace dmDNS
 */
namespace dmDNS
{
    /*#
     * A channel roughly translates to a socket on which to put the name lookup requests on.
     * Internal implementation resides in dns.cpp.
     * @typedef
     * @name dmDNS::HChannel
     */
    typedef struct Channel* HChannel;

    /*# result type
     * Result type
     * @enum
     * @name dmDNS::Result
     * @member dmDNS::RESULT_OK 0
     * @member dmDNS::RESULT_INIT_ERROR -1
     * @member dmDNS::RESULT_HOST_NOT_FOUND -2
     * @member dmDNS::RESULT_CANCELLED -3
     * @member dmDNS::RESULT_UNKNOWN_ERROR -4
     */
    enum Result
    {
        RESULT_OK             = 0,
        RESULT_INIT_ERROR     = -1,
        RESULT_HOST_NOT_FOUND = -2,
        RESULT_CANCELLED      = -3,
        RESULT_UNKNOWN_ERROR  = -4,
    };

    /*# create a new channel
     * Creates a new channel that can be used for DNS queries.
     * @param channel [type:dmDNS::HChannel*] Pointer to the created channel if successful, will be left alone otherwise
     * @name dmDNS::NewChannel
     * @return RESULT_OK on succcess
     */
    Result NewChannel(HChannel* channel);

    /*# stop current request
     * Stops the current request (if available) on a channel.
     * @name dmDNS::StopChannel
     * @param channel [type:dmDNS::HChannel] Handle to the channel
     */
    void StopChannel(HChannel channel);

    /*# delete channel
     * Deletes the current channel and cancels all requests.
     * Note: You must always make sure to call StopChannel(channel) before calling this function.
     * @name dmDNS::DeleteChannel
     * @param channel [type:dmDNS::HChannel] Handle to the channel
     */
    void DeleteChannel(HChannel channel);

    /*# refresh to the latest dns servers
     * Refreshes the channel configuration so that the latest DNS servers are used.
     * @name dmDNS::RefreshChannel
     * @param channel [type:dmDNS::HChannel] Handle to the channel
     * @return RESULT_OK on succcess
     */
    Result RefreshChannel(HChannel channel);
}

#endif // DMSDK_DNS_H
