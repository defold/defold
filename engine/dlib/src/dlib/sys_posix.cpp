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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "sys.h"
#include "sys_posix.h"
#include "sys_private.h"
#include "dstrings.h"
#include "path.h"

// For the IterateTree
#include <dirent.h>

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&S_IFMT) == S_IFREG)
#endif

namespace dmSysPosix
{
    char* GetEnv(const char* name)
    {
        return getenv(name);
    }

    dmSys::Result Rename(const char* dst_filename, const char* src_filename)
    {
        bool rename_result = rename(src_filename, dst_filename) != -1;
        if (rename_result)
        {
            return dmSys::RESULT_OK;
        }
        return dmSys::RESULT_UNKNOWN;
    }

    dmSys::Result GetResourcesPath(int argc, char* argv[], char* path, uint32_t path_len)
    {
        assert(path_len > 0);
        path[0] = '\0';
        dmPath::Dirname(argv[0], path, path_len);
        return dmSys::RESULT_OK;
    }

    dmSys::Result GetLogPath(char* path, uint32_t path_len)
    {
        if (dmStrlCpy(path, ".", path_len) >= path_len)
            return dmSys::RESULT_INVAL;

        return dmSys::RESULT_OK;
    }

    void FillTimeZone(dmSys::SystemInfo* info)
    {
        time_t t;
        time(&t);
        struct tm* lt = localtime(&t);
        info->m_GmtOffset = lt->tm_gmtoff / 60;
    }

    bool ResourceExists(const char* path)
    {
        struct stat file_stat;
        return stat(path, &file_stat) == 0;
    }

    dmSys::Result ResourceSize(const char* path, uint32_t* resource_size)
    {
        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return dmSys::RESULT_NOENT;
            }
            *resource_size = (uint32_t) file_stat.st_size;
            return dmSys::RESULT_OK;
        } else {
            return dmSys::RESULT_NOENT;
        }
    }

    dmSys::Result LoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size)
    {
        *resource_size = 0;

        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return dmSys::RESULT_NOENT;
            }
            if ((uint32_t) file_stat.st_size > buffer_size) {
                return dmSys::RESULT_INVAL;
            }
            FILE* f = fopen(path, "rb");
            size_t nread = fread(buffer, 1, file_stat.st_size, f);
            fclose(f);
            if (nread != (size_t) file_stat.st_size) {
                return dmSys::RESULT_IO;
            }
            *resource_size = file_stat.st_size;
            return dmSys::RESULT_OK;
        } else {
            return dmSys::ErrnoToResult(errno);
        }
    }

    dmSys::Result LoadResourcePartial(const char* path, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
    {
        if (buffer == 0 || size == 0)
            return dmSys::RESULT_INVAL;

        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return dmSys::RESULT_NOENT;
            }

            FILE* f = fopen(path, "rb");
            if (!f)
            {
                return dmSys::RESULT_NOENT;
            }

            int result = fseek(f, offset, SEEK_SET);
            if (result < 0)
            {
                fclose(f);
                return dmSys::ErrnoToResult(errno);
            }

            size_t nmemb = fread(buffer, 1, size, f);
            if (ferror(f))
            {
                fclose(f);
                return dmSys::ErrnoToResult(errno);
            }
            fclose(f);

            *nread = (uint32_t)nmemb;
        } else {
            return dmSys::ErrnoToResult(errno);
        }
        return dmSys::RESULT_OK;
    }

    dmSys::Result Rmdir(const char* path)
    {
        int ret = rmdir(path);
        if (ret == 0)
            return dmSys::RESULT_OK;
        else
            return dmSys::ErrnoToResult(errno);
    }

    dmSys::Result Mkdir(const char* path, uint32_t mode)
    {
        int ret = mkdir(path, (mode_t) mode);
        if (ret == 0)
            return dmSys::RESULT_OK;
        else
            return dmSys::ErrnoToResult(errno);
    }

    dmSys::Result IsDir(const char* path)
    {
        struct stat path_stat;
        int ret = stat(path, &path_stat);
        if (ret != 0)
            return dmSys::ErrnoToResult(errno);
        return path_stat.st_mode & S_IFDIR ? dmSys::RESULT_OK : dmSys::RESULT_UNKNOWN;
    }

    bool Exists(const char* path)
    {
        struct stat path_stat;
        int ret = stat(path, &path_stat);
        return ret == 0;
    }

    dmSys::Result IterateTree(const char* dirpath, bool recursive, bool call_before, void* ctx, void (*callback)(void* ctx, const char* path, bool isdir))
    {
        struct dirent *entry = NULL;
        DIR *dir = NULL;
        dir = opendir(dirpath);
        if (!dir) {
            return dmSys::RESULT_NOENT;
        }

        if (call_before)
            callback(ctx, dirpath, true);

        dmSys::Result res = dmSys::RESULT_OK;
        while((entry = readdir(dir)))
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char abs_path[1024];
            dmSnPrintf(abs_path, sizeof(abs_path), "%s/%s", dirpath, entry->d_name);

            struct stat path_stat = {};
            int stat_result = stat(abs_path, &path_stat);

            if (stat_result != 0)
            {
                if (errno == ENOENT)
                {
                    continue;
                }
                res = dmSys::ErrnoToResult(errno);
                goto cleanup;
            }

            bool isdir = S_ISDIR(path_stat.st_mode);

            if (call_before)
                callback(ctx, abs_path, isdir);

            if (isdir && recursive) {

                // Make sure the directory still exists (the callback might have removed it!)
                if (Exists(abs_path))
                {
                    res = IterateTree(abs_path, recursive, call_before, ctx, callback);
                    if (res != dmSys::RESULT_OK) {
                        goto cleanup;
                    }
                }
            }

            if (!call_before)
                callback(ctx, abs_path, isdir);
        }

        if (!call_before)
            callback(ctx, dirpath, true);

    cleanup:

        closedir(dir);
        return res;
    }

    dmSys::Result Unlink(const char* path)
    {
        int ret = unlink(path);
        if (ret == 0)
            return dmSys::RESULT_OK;
        else
            return dmSys::ErrnoToResult(errno);
    }

    dmSys::Result Stat(const char* path, dmSys::StatInfo* stat_info)
    {
        struct stat info;
        int ret = stat(path, &info);
        if (ret != 0)
            return dmSys::RESULT_NOENT;
        stat_info->m_Size = (uint64_t)info.st_size;
        stat_info->m_Mode = info.st_mode;
        stat_info->m_AccessTime = info.st_atime;
        stat_info->m_ModifiedTime = info.st_mtime;
        return dmSys::RESULT_OK;
    }

    int StatIsDir(const dmSys::StatInfo* stat_info)
    {
        return S_ISDIR(stat_info->m_Mode);
    }

    int StatIsFile(const dmSys::StatInfo* stat_info)
    {
        return S_ISREG(stat_info->m_Mode);
    }

} // namespace dmSysPosix
