#include "uri.h"

#include "dstrings.h"
#include "math.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

            if (strcmp(parts->m_Scheme, "http") == 0)
            {
                // Default to port 80 for http
                parts->m_Port = 80;
            }
            else if (strcmp(parts->m_Scheme, "https") == 0)
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

    void Encode(const char* src, char* dst, uint32_t dst_len)
    {
        assert(src != (const char*) dst);
        assert(dst_len > 0);

        uint32_t dst_left = dst_len - 1; // Make room for null termination
        const char* s = src;
        char* d = dst;
        while (*s) {
            char c = *s;
            if (IsUnreserved(c)) {
                if (dst_left >= 1) {
                    *d = c;
                    s++;
                    d++;
                    dst_left--;
                } else {
                    break;
                }
            } else {
                if (dst_left >= 3) {
                    DM_SNPRINTF(d, 4, "%%%02X", c);
                    s++;
                    d += 3;
                    dst_left -= 3;
                } else {
                    break;
                }
            }
        }

        *d = '\0';
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

}   // namespace dmURI
