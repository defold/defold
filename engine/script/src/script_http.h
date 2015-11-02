#ifndef DM_SCRIPT_HTTP_H
#define DM_SCRIPT_HTTP_H

namespace dmScript
{
    void InitializeHttp(lua_State* L, dmConfigFile::HConfig config_file);
    void FinalizeHttp(lua_State* L);
    void SetHttpRequestTimeout(uint64_t timeout);
}

#endif // DM_SCRIPT_HTTP_H
