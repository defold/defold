#ifndef DM_DNS_H
#define DM_DNS_H

namespace dmSocket
{
	struct Address;
}

namespace dmDNS
{
	typedef void* HChannel;

	enum Result
    {
        RESULT_OK             = 0,
        RESULT_INIT_ERROR     = -1,
        RESULT_HOST_NOT_FOUND = -2,
        RESULT_UNKNOWN_ERROR  = -3
    };

	Result Initialize();
	Result Finalize();
	Result NewChannel(HChannel* channel);
	void   StopChannel(HChannel channel);
	void   DeleteChannel(HChannel channel);
	Result GetHostByName(const char* name, dmSocket::Address* address, HChannel channel, bool ipv4 = true, bool ipv6 = true);
}

#endif //DM_DSTRINGS_H