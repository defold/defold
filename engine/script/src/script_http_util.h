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
