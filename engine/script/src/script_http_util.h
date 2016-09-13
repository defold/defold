namespace dmScript
{
    Result HttpResponseDecoder(lua_State* L, const dmDDF::Descriptor* desc, const char* data)
    {
        assert(desc == dmHttpDDF::HttpResponse::m_DDFDescriptor);
        dmHttpDDF::HttpResponse* resp = (dmHttpDDF::HttpResponse*) data;

        char* headers = (char*) resp->m_Headers;
        char* response = (char*) resp->m_Response;

        lua_newtable(L);

        lua_pushliteral(L, "status");
        lua_pushinteger(L, resp->m_Status);
        lua_rawset(L, -3);

        lua_pushliteral(L, "response");
        lua_pushlstring(L, response, resp->m_ResponseLength);
        lua_rawset(L, -3);

        lua_pushliteral(L, "headers");
        lua_newtable(L);
        if (resp->m_HeadersLength > 0) {
            headers[resp->m_HeadersLength-1] = '\0';

            char* s, *last;
            s = dmStrTok(headers, "\n", &last);
            while (s) {
                char* colon = strchr(s, ':');
                *colon = '\0';

                // Convert attribute to lower-case
                // for simplified handling (http header attributes are case-insensitive)
                char* tmp = s;
                while (*tmp) {
                    *tmp = tolower(*tmp);
                    tmp++;
                }

                lua_pushstring(L, s);
                *colon = ':';
                char* value = colon + 1;
                while (*value == ' ') {
                    ++value;
                }

                lua_pushstring(L, value);
                lua_rawset(L, -3);
                s = dmStrTok(0, "\n", &last);
            }
        }
        lua_rawset(L, -3);

        return RESULT_OK;
    }
}
