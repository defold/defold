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
#include <stdio.h>
#include <string.h>
#include "path.h"
#include "math.h"
#include "dstrings.h"

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

    static const wchar_t* SkipSlashesW(const wchar_t* path)
    {
        while (*path && (*path == L'/' || *path == L'\\'))
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

    void NormalizeW(const wchar_t* path, wchar_t* out, uint32_t out_size)
    {
        assert(out_size > 0);

        uint32_t i = 0;
        while (*path && i < out_size)
        {
            int c = *path;
            if (c == L'/' || c == L'\\')
            {
                out[i] = L'/';
                path = SkipSlashesW(path);
            }
            else
            {
                out[i] = c;
                ++path;
            }

            ++i;
        }

        if (i > 1 && out[i-1] == L'/')
        {
            // Remove trailing /
            out[i-1] = L'\0';
        }

        out[dmMath::Min(i, out_size-1)] = L'\0';
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
