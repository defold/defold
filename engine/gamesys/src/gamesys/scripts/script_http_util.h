// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#ifndef DM_GAMESYS_SCRIPT_HTTP_UTIL_H
#define DM_GAMESYS_SCRIPT_HTTP_UTIL_H

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/sys.h>

namespace dmGameSystem
{
    static bool WriteResponseToFile(const char* path, const void* data, uint32_t data_len)
    {
        char tmpname[DMPATH_MAX_PATH];
        dmStrlCpy(tmpname, path, sizeof(tmpname));
        dmStrlCat(tmpname, "._httptmp", sizeof(tmpname));
        FILE* f = fopen(tmpname, "wb");
        if (!f) {
            return false;
        }
        size_t nwritten = fwrite(data, 1, data_len, f);
        fclose(f);
        if (nwritten != data_len)
        {
            dmLogError("Failed to write '%u' bytes to '%s'", data_len, path);
            return false;
        }

        dmSys::Result result = dmSys::Rename(path, tmpname);
        if (dmSys::RESULT_OK != result)
        {
            dmLogError("Failed to rename '%s' to '%s'", tmpname, path);
            return false;
        }
        return true;
    }

    dmScript::Result HttpResponseDecoder(lua_State* L, const dmDDF::Descriptor* desc, const char* data)
    {
        assert(desc == dmHttpDDF::HttpResponse::m_DDFDescriptor);
        dmHttpDDF::HttpResponse* resp = (dmHttpDDF::HttpResponse*) data;

        char* headers = (char*) resp->m_Headers;
        char* response = (char*) resp->m_Response;

        lua_newtable(L);

        lua_pushinteger(L, resp->m_Status);
        lua_setfield(L, -2, "status");

        if (resp->m_Path)
        {
            if (resp->m_Status == 200) {
                if (!WriteResponseToFile(resp->m_Path, response, resp->m_ResponseLength))
                {
                    lua_pushliteral(L, "Failed to write to temp file");
                    lua_setfield(L, -2, "error");
                }
            }

            lua_pushstring(L, resp->m_Path);
            lua_setfield(L, -2, "path");
        } else {
            lua_pushlstring(L, response, resp->m_ResponseLength);
            lua_setfield(L, -2, "response");
        }

        if (resp->m_Url)
        {
            lua_pushstring(L, resp->m_Url);
            lua_setfield(L, -2, "url");
        }

        lua_pushinteger(L, resp->m_RangeStart);
        lua_setfield(L, -2, "range_start");
        lua_pushinteger(L, resp->m_RangeEnd);
        lua_setfield(L, -2, "range_end");
        lua_pushinteger(L, resp->m_DocumentSize);
        lua_setfield(L, -2, "document_size");

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

        return dmScript::RESULT_OK;
    }

    dmScript::Result HttpRequestProgressDecoder(lua_State* L, const dmDDF::Descriptor* desc, const char* data)
    {
        dmScript::PushDDFNoDecoder(L, desc, (const char*)data, false);
        return dmScript::RESULT_OK;
    }
}

#endif // DM_GAMESYS_SCRIPT_HTTP_UTIL_H
