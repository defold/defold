// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_SYS_H
#define DMSDK_SYS_H

#include <stdint.h>

/*# SDK Sys API documentation
 * Sys allocation functions
 *
 * @document
 * @name Sys
 * @namespace dmSys
 * @path engine/dlib/src/dmsdk/dlib/sys.h
 */

namespace dmSys
{
    /*# result code
     *
     * Result code. Similar to standard posix result codes
     *
     * @enum
     * @name Result
     * @member dmSys::RESULT_OK      0
     * @member dmSys::RESULT_PERM   -1
     * @member dmSys::RESULT_NOENT  -2
     * @member dmSys::RESULT_SRCH   -3
     * @member dmSys::RESULT_INTR   -4
     * @member dmSys::RESULT_IO     -5
     * @member dmSys::RESULT_NXIO   -6
     * @member dmSys::RESULT_2BIG   -7
     * @member dmSys::RESULT_NOEXEC -8
     * @member dmSys::RESULT_BADF   -9
     * @member dmSys::RESULT_CHILD  -10
     * @member dmSys::RESULT_DEADLK -11
     * @member dmSys::RESULT_NOMEM  -12
     * @member dmSys::RESULT_ACCES  -13
     * @member dmSys::RESULT_FAULT  -14
     * @member dmSys::RESULT_BUSY   -15
     * @member dmSys::RESULT_EXIST  -16
     * @member dmSys::RESULT_XDEV   -17
     * @member dmSys::RESULT_NODEV  -18
     * @member dmSys::RESULT_NOTDIR -19
     * @member dmSys::RESULT_ISDIR  -20
     * @member dmSys::RESULT_INVAL  -21
     * @member dmSys::RESULT_NFILE  -22
     * @member dmSys::RESULT_MFILE  -23
     * @member dmSys::RESULT_NOTTY  -24
     * @member dmSys::RESULT_TXTBSY -25
     * @member dmSys::RESULT_FBIG   -26
     * @member dmSys::RESULT_NOSPC  -27
     * @member dmSys::RESULT_SPIPE  -28
     * @member dmSys::RESULT_ROFS   -29
     * @member dmSys::RESULT_MLINK  -30
     * @member dmSys::RESULT_PIPE   -31
     * @member dmSys::RESULT_NOTEMPTY -32
     * @member dmSys::RESULT_UNKNOWN -1000
     */
    enum Result
    {
        RESULT_OK     =  0,
        RESULT_PERM   = -1,
        RESULT_NOENT  = -2,
        RESULT_SRCH   = -3,
        RESULT_INTR   = -4,
        RESULT_IO     = -5,
        RESULT_NXIO   = -6,
        RESULT_2BIG   = -7,
        RESULT_NOEXEC = -8,
        RESULT_BADF   = -9,
        RESULT_CHILD  = -10,
        RESULT_DEADLK = -11,
        RESULT_NOMEM  = -12,
        RESULT_ACCES  = -13,
        RESULT_FAULT  = -14,
        RESULT_BUSY   = -15,
        RESULT_EXIST  = -16,
        RESULT_XDEV   = -17,
        RESULT_NODEV  = -18,
        RESULT_NOTDIR = -19,
        RESULT_ISDIR  = -20,
        RESULT_INVAL  = -21,
        RESULT_NFILE  = -22,
        RESULT_MFILE  = -23,
        RESULT_NOTTY  = -24,
        RESULT_TXTBSY = -25,
        RESULT_FBIG   = -26,
        RESULT_NOSPC  = -27,
        RESULT_SPIPE  = -28,
        RESULT_ROFS   = -29,
        RESULT_MLINK  = -30,
        RESULT_PIPE   = -31,
        RESULT_NOTEMPTY = -32,

        RESULT_UNKNOWN = -1000,//!< RESULT_UNKNOWN
    };

    /*#
     * Checks if a path exists
     * @name Exists
     * @param path [type: const char*] path to directory to create
     * @return result [type: bool] true on success
     */
    bool Exists(const char* path);

    /*#
     * Remove file
     * @name Unlink
     * @param path [type: const char*] path to file to remove
     * @return result [type: dmSys::Result] RESULT_OK on success
     */
    Result Unlink(const char* path);

    /*#
     * Move a file or directory
     * @note This operation is atomic
     * @name Rename
     * @param dst_path [type: const char*] the destination path. The file which contents is to be overwritten.
     * @param src_path [type: const char*] the source path. The contents will be written to the destination path and the file unlinked if successful.
     * @return RESULT_OK on success
    */
    Result Rename(const char* dst_path, const char* src_path);

    /*#
     * Status info for a file or directory
     * @struct
     * @name StatInfo
     * @member m_Size [type: uint32_t] the file size (if it's a file)
     * @member m_Mode [type: uint32_t] the flags of the path
     * @member m_AccessTime [type: uint32_t] the last access time
     * @member m_ModifiedTime [type: uint32_t] the last modified time
    */
    struct StatInfo
    {
        uint64_t m_Size;        // The file size
        uint32_t m_Mode;
        uint32_t m_AccessTime;
        uint32_t m_ModifiedTime;
    };

    /**
     * Remove directory tree structure and files within
     * @name Stat
     * @param path [type: const char*] path to file or directory
     * @param stat [type: StatInfo*] pointer to dmSys::StatInfo structure
     * @return result [type: dmSys::Result] RESULT_OK on success
     */
    Result Stat(const char* path, StatInfo* stat);

    /**
     * Check if the path is a directory
     * @name StatIsDir
     * @param stat [type: StatInfo*] pointer to dmSys::StatInfo structure
     * @return result [type: int] 0 if check fails, non zero otherwise
     */
    int StatIsDir(const StatInfo* stat);

    /**
     * Check if the path is a file
     * @name StatIsFile
     * @param stat stat to check
     * @param stat [type: StatInfo*] pointer to dmSys::StatInfo structure
     * @return result [type: int] 0 if check fails, non zero otherwise
     */
    int StatIsFile(const StatInfo* stat);

}

#endif // DMSDK_SYS_H
