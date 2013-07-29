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

    void Encode(const char *str, char* out, uint32_t out_size)
    {
        dmStrlCpy(out, str, out_size);
        char* p = out;
        while (*p != 0)
        {
            if (*p == ' ')
                *p = '+';
            ++p;
        }
    }

    void Decode(const char *str, char* out, uint32_t out_size)
    {
        dmStrlCpy(out, str, out_size);
        char* p = out;
        while (*p != 0)
        {
            if (*p == '+')
                *p = ' ';
            ++p;
        }
    }

}   // namespace dmURI
