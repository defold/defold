#include "path.h"

#include "dstrings.h"
#include "math.h"

#include <assert.h>
#include <string.h>

namespace dmPath
{

    static const char* SkipSlashes(const char* path)
    {
        while (*path && (*path == '/' || *path == '\\'))
        {
            ++path;
        }
        return path;
    }

    void Normalize(const char* path, char* out, uint32_t out_size)
    {
        assert(out_size > 0);

        uint32_t i = 0;
        while (*path && i < out_size)
        {
            int c = *path;
            if (c == '/' || c == '\\')
            {
                out[i] = '/';
                path = SkipSlashes(path);
            }
            else
            {
                out[i] = c;
                ++path;
            }

            ++i;
        }

        if (i > 1 && out[i-1] == '/')
        {
            // Remove trailing /
            out[i-1] = '\0';
        }

        out[dmMath::Min(i, out_size-1)] = '\0';
    }

    void Dirname(const char* path, char* out, uint32_t out_size)
    {
        char buf[DMPATH_MAX_PATH];
        Normalize(path, buf, sizeof(buf));

        if (strcmp(buf, ".") == 0)
        {
            // Special case
        }
        else
        {
            char* last_slash = strrchr(buf, '/');
            if (last_slash)
            {
                if (last_slash != buf)
                {
                    // Truncate string if slash found isn't
                    // the first character (edge case for path "/")
                    *last_slash = '\0';
                }
            }
            else
            {
                buf[0] = '\0';
            }
        }
        dmStrlCpy(out, buf, out_size);
    }

    void Concat(const char* path1, const char* path2, char* out, uint32_t out_size)
    {
        char buf[DMPATH_MAX_PATH];

        if (path1[0])
        {
            dmStrlCpy(buf, path1, sizeof(buf));
            dmStrlCat(buf, "/", sizeof(buf));
        }
        else
        {
            buf[0] = '\0';
        }
        dmStrlCat(buf, path2, sizeof(buf));
        Normalize(buf, out, out_size);
    }


}
