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

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "dlib/dstrings.h"
#include "dlib/math.h"
#include "dlib/uri.h"

/*
 * TODO:
 * - Return error for string truncation?
 */

namespace dmURI
{
    // NOTE: Does not conform to rfc2396 but good enough for our purposes
    bool IsValidScheme(const char* start, const char* end)
    {
        const char*s = start;
        while (s < end)
        {
            if (!isalnum(*s))
                return false;
            ++s;
        }

        return true;
    }

    // NOTE: This parser is not even close to rfc2396 but good enough for our purposes
    Result Parse(const char* uri, Parts* parts)
    {
        parts->m_Scheme[0] = '\0';
        parts->m_Location[0] = '\0';
        parts->m_Hostname[0] = '\0';
        parts->m_Port = -1;
        parts->m_Path[0] = '\0';

        const char* scheme_end = strchr(uri, ':');
        if (!scheme_end || !IsValidScheme(uri, scheme_end))
        {
            // Assume file:/// scheme
            dmStrlCpy(parts->m_Scheme, "file", sizeof(parts->m_Scheme));
            dmStrlCpy(parts->m_Path, uri,  sizeof(parts->m_Path));
            return RESULT_OK;
        }
        else
        {
            size_t n = dmMath::Min(sizeof(parts->m_Scheme), (size_t) (scheme_end-uri) + 1);
            dmStrlCpy(parts->m_Scheme, uri, n);

            if (strcmp(parts->m_Scheme, "http") == 0 || strcmp(parts->m_Scheme, "ws") == 0)
            {
                // Default to port 80 for http
                parts->m_Port = 80;
            }
            else if (strcmp(parts->m_Scheme, "https") == 0 || strcmp(parts->m_Scheme, "wss") == 0)
            {
                // Default to port 443 for https
                parts->m_Port = 443;
            }

            const char* slash_slash = strstr(uri, "//");
            if (slash_slash)
            {
                const char* location = slash_slash + 2;
                const char* path = strchr(location, '/');
                if (path)
                {
                    dmStrlCpy(parts->m_Location, location, dmMath::Min(sizeof(parts->m_Location), (size_t) (path - location) + 1));
                    dmStrlCpy(parts->m_Path, path, sizeof(parts->m_Path));
                }
                else
                {
                    dmStrlCpy(parts->m_Location, location, sizeof(parts->m_Location));
                }
                dmStrlCpy(parts->m_Hostname, parts->m_Location, sizeof(parts->m_Hostname));
                char* port = strchr(parts->m_Hostname, ':');
                if (port)
                {
                    parts->m_Port = strtol(port + 1, 0, 10);
                    *port = '\0';
                }
            }
            else
            {
                dmStrlCpy(parts->m_Path, scheme_end+1, sizeof(parts->m_Path));
            }

            return RESULT_OK;
        }
    }

    // http://en.wikipedia.org/wiki/Percent-encoding
    static bool IsUnreserved(char c)
    {
        if (c >= 'a' && c <= 'z') {
            return true;
        } else if (c >= 'A' && c <= 'Z') {
            return true;
        } else if (c >= '0' && c <= '9') {
            return true;
        } else {
            switch (c) {
            case '-':
            case '_':
            case '.':
            case '~':
            case '/':
                return true;
            }
        }
        return false;
    }

    Result Encode(const char* src, char* dst, uint32_t dst_len, uint32_t* bytes_written)
    {
        assert(src != (const char*) dst);
        assert(dst == 0 || dst_len > 0);

        uint32_t dst_left = dst == 0 ? 0xFFFFFFFF : dst_len - 1; // Make room for null termination
        const char* s = src;
        char* d = dst;
        while (*s) {
            char c = *s;
            if (IsUnreserved(c)) {
                if (dst_left >= 1) {
                    if (dst)
                        *d = c;
                    s++;
                    d++;
                    dst_left--;
                } else {
                    *d = '\0';
                    return RESULT_TOO_SMALL_BUFFER;
                }
            } else {
                if (dst_left >= 3) {
                    if (dst)
                        dmSnPrintf(d, 4, "%%%02X", c);
                    s++;
                    d += 3;
                    dst_left -= 3;
                } else {
                    *d = '\0';
                    return RESULT_TOO_SMALL_BUFFER;
                }
            }
        }
        if (dst)
            *d = '\0';
        if (bytes_written)
            *bytes_written = d - dst + 1;
        return RESULT_OK;
    }

    void Decode(const char* src, char* dst)
    {
        size_t left = strlen(src);
        while (left > 0) {
            if (src[0] == '+') {
                *dst = ' ';
                src++;
                left--;
            }
            else if (left > 2 && src[0] == '%' && isxdigit(src[1]) && isxdigit(src[2])) {
                char tmp[3];
                tmp[0] = src[1];
                tmp[1] = src[2];
                tmp[2] = '\0';
                *dst = (char) strtoul(tmp, 0, 16);
                src += 3;
                left -= 3;
            } else {
                *dst = *src;
                src++;
                left--;
            }
            dst++;
        }
        *dst = '\0';
    }

    bool Compare(Parts* a, Parts* b)
    {
        if (a->m_Port != b->m_Port) return false;
        if (strcmp(a->m_Scheme, b->m_Scheme) != 0) return false;
        if (strcmp(a->m_Location, b->m_Location) != 0) return false;
        if (strcmp(a->m_Hostname, b->m_Hostname) != 0) return false;
        if (strcmp(a->m_Path, b->m_Path) != 0) return false;
        return true;
    }

}   // namespace dmURI
