#ifndef DM_HTTP_SERVICE
#define DM_HTTP_SERVICE

namespace dmHttpService
{
    typedef struct HttpService* HHttpService;

    HHttpService New();
    dmMessage::HSocket GetSocket(HHttpService http_service);
    void Delete(HHttpService http_service);

}  // namespace dmHttpService


#endif // #ifndef DM_HTTP_SERVICE
