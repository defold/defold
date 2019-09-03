#ifndef DM_DNS_H
#define DM_DNS_H

namespace dmSocket
{
    struct Address;
}

namespace dmDNS
{
    /**
     * A channel roughly translates to a socket on which to put the name lookup requests on.
     * Internal implementation resides in dns.cpp.
     */
    struct  Channel;
    typedef Channel* HChannel;

    enum Result
    {
        RESULT_OK             = 0,
        RESULT_INIT_ERROR     = -1,
        RESULT_HOST_NOT_FOUND = -2,
        RESULT_CANCELLED      = -3,
        RESULT_UNKNOWN_ERROR  = -4
    };

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
     * Creates a new channel that can be used for DNS queries.
     * @param channel Pointer to the created channel if successfull, will be left alone otherwise
     * @return RESULT_OK on succcess
     */
    Result NewChannel(HChannel* channel);

    /**
     * Stops the current request (if available) on a channel.
     * @param channel Handle to the channel
     */
    void StopChannel(HChannel channel);

    /**
     * Deletes the current channel and cancels all requests.
     * Note: You must always make sure to call StopChannel(channel) before calling this function.
     * @param channel Handle to the channel
     */
    void DeleteChannel(HChannel channel);

    /**
     * Refreshes the channel configuration so that the latest DNS servers are used.
     * @param channel Handle to the channel
     */
    Result RefreshChannel(HChannel channel);

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
}

#endif //DM_DSTRINGS_H
