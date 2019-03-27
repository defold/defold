#ifndef DM_SCRIPT_HTTP_H
#define DM_SCRIPT_HTTP_H

namespace dmScript
{
    typedef struct Context* HContext;

    void InitializeHttp(HContext context);

    void SetHttpRequestTimeout(uint64_t timeout);
}

#endif // DM_SCRIPT_HTTP_H
