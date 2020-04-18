#ifndef DM_HTTP_SERVICE
#define DM_HTTP_SERVICE

#include <stdint.h>

namespace dmHttpService
{
    typedef struct HttpService* HHttpService;

    struct Params
    {
    	Params() :
    		m_ThreadCount(4),
            m_UseHttpCache(1)
    	{}
    	uint32_t m_ThreadCount:4;
        uint32_t m_UseHttpCache:1;
    };
    HHttpService New(const Params* params);
    dmMessage::HSocket GetSocket(HHttpService http_service);
    void Delete(HHttpService http_service);

}  // namespace dmHttpService


#endif // #ifndef DM_HTTP_SERVICE
