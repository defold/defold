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

#ifndef DM_PATH_H
#define DM_PATH_H

#include <stdint.h>
#include <stddef.h> // wchar_t

/** Maximum path length convention. This size should large enough
 * such that path truncation never occurs in practice. No functions
 * in dmPath return error codes for that reason.
 */
#define DMPATH_MAX_PATH (1024)

namespace dmPath
{
#if defined(_WIN32) || defined(_WIN64)
    static const char* const PATH_CHARACTER = "\\";
    static const char* const PATH_SEPARATOR = ";";
#else
    static const char* const PATH_CHARACTER = "/";
    static const char* const PATH_SEPARATOR = ":";
#endif

    /**
     * Path normalization. Redundant and trailing slashes are
     * removed and backslashes are translated into forward slashes
     * @param path path to normalize
     * @param out out buffer
     * @param out_size out buffer size
     */
    void Normalize(const char* path, char* out, uint32_t out_size);

    /**
     * Path normalization for wide strings. Redundant and trailing slashes are
     * removed and backslashes are translated into backward slashes
     * @param path path to normalize
     * @param out out buffer
     * @param out_size out buffer size
     */
    void NormalizeW(const wchar_t* wpath, wchar_t* out, uint32_t out_size);

    /**
     * Get directory part of path. The output path is potentially
     * normalized, but not canonicalized, according to the Normalize() function.
     * @note internal buffer of DMPATH_MAX_PATH is used when
     * normalizing
     * @param path path to get directory of
     * @param out out buffer
     * @param out_size out buffer size
     */
    void Dirname(const char* path, char* out, uint32_t out_size);

    /**
     * Path concatenation function
     * @param path1 first path to concatenate
     * @param path2 second path to concatenate
     * @param out output path
     * @param out_size output path size
     */
    void Concat(const char* path1, const char* path2, char* out, uint32_t out_size);


}

#endif // DM_PATH_H
