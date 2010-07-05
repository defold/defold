#include <string.h>
#include "dlib/dstrings.h"
#include "dlib/math.h"
#include "dlib/uri.h"

namespace dmURI
{
    Result Parse(const char* uri, Parts* parts)
    {
        parts->m_Scheme[0] = '\0';
        parts->m_Location[0] = '\0';
        parts->m_Path[0] = '\0';

        const char* scheme_end = strchr(uri, ':');
        if (!scheme_end)
        {
            // Assume file:/// scheme
            dmStrlCpy(parts->m_Scheme, "file", sizeof(parts->m_Scheme));
            dmStrlCpy(parts->m_Path, uri,  sizeof(parts->m_Path));
            return RESULT_OK;
        }
        else
        {
            size_t n = dmMin(sizeof(parts->m_Scheme), (size_t) (scheme_end-uri) + 1);
            dmStrlCpy(parts->m_Scheme, uri, n);

            const char* slash_slash = strstr(uri, "//");
            if (slash_slash)
            {
                const char* location = slash_slash + 2;
                const char* path = strchr(location, '/');
                if (path)
                {
                    dmStrlCpy(parts->m_Location, location, dmMin(sizeof(parts->m_Location), (size_t) (path - location) + 1));
                    dmStrlCpy(parts->m_Path, path, sizeof(parts->m_Path));
                }
                else
                {
                    dmStrlCpy(parts->m_Location, location, sizeof(parts->m_Location));
                }
            }
            else
            {
                dmStrlCpy(parts->m_Path, scheme_end+1, sizeof(parts->m_Path));
            }

            return RESULT_OK;
        }
    }
}  // namespace dmURI
