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

#include "record_file.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace dmRecord
{
    FILE* FileOpen(const char* path, const char* mode)
    {
#if defined(_WIN32)
        wchar_t wide_path[1024];
        wchar_t wide_mode[16];
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide_path, sizeof(wide_path) / sizeof(wide_path[0])) ||
            !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, wide_mode, sizeof(wide_mode) / sizeof(wide_mode[0])))
        {
            return 0;
        }
        return _wfopen(wide_path, wide_mode);
#else
        return fopen(path, mode);
#endif
    }

    int FileRemove(const char* path)
    {
#if defined(_WIN32)
        wchar_t wide[1024];
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, sizeof(wide) / sizeof(wide[0])))
        {
            return -1;
        }
        return _wremove(wide);
#else
        return remove(path);
#endif
    }
}
