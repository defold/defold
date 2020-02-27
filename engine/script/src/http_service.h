#ifndef DM_HTTP_SERVICE
#define DM_HTTP_SERVICE

namespace dmHttpService
{
    typedef struct HttpService* HHttpService;

    struct Params
    {
    	Params() :
    	#if defined(__NX__)
    		m_ThreadCount(2)
    	#else
    		m_ThreadCount(4)
    	#endif
    	{}
    	int m_ThreadCount;
    };
    HHttpService New(const Params* params);
    dmMessage::HSocket GetSocket(HHttpService http_service);
    void Delete(HHttpService http_service);

}  // namespace dmHttpService


#endif // #ifndef DM_HTTP_SERVICE
