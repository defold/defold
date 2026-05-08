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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "sys.h"
#include "sys_posix.h"
#include "sys_private.h"
#include "dalloca.h"
#include "dstrings.h"
#include "log.h"

#include <libgen.h>

namespace dmSys
{
    char* GetEnv(const char* name)
    {
        return dmSysPosix::GetEnv(name);
    }

    void SetNetworkConnectivityHost(const char* host)
    {
    }

    NetworkConnectivity GetNetworkConnectivity()
    {
        return NETWORK_CONNECTED;
    }

    Result Rename(const char* dst_filename, const char* src_filename)
    {
        return dmSysPosix::Rename(dst_filename, src_filename);
    }

    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len)
    {
        return GetApplicationSupportPath(application_name, path, path_len);
    }

    Result GetApplicationSupportPath(const char* application_name, char* path_out, uint32_t path_len)
    {
        const char* xdg_env = dmSys::GetEnv("XDG_DATA_HOME");
        const char* xdg = xdg_env ? xdg_env : dmSys::GetEnv("HOME");
        char*       xdg_buf = (char*)dmAlloca(path_len);

        dmStrlCpy(xdg_buf, xdg, path_len);
        // The spec expects us to make $HOME/.local/share with 0700 if it doesn't exist.
        if (!xdg_env)
        {
            dmStrlCat(xdg_buf, "/.local", path_len);
            dmSys::Mkdir(xdg_buf, 0700);
            dmStrlCat(xdg_buf, "/share", path_len);
            dmSys::Mkdir(xdg_buf, 0700);
        }
        dmStrlCat(xdg_buf, "/", path_len);
        if (dmStrlCat(xdg_buf, application_name, path_len) >= path_len)
            return RESULT_INVAL;

        // No need to continue if {application_name} dir already exists
        if (realpath(xdg_buf, path_out))
            return RESULT_OK;

        const char* dirs[] = { "HOME", "TMPDIR", "TMP", "TEMP" }; // Added common temp directories since server instances usually don't have a HOME set
        size_t      count = sizeof(dirs) / sizeof(dirs[0]);
        const char* home = 0;

        for (size_t i = 0; i < count; i++)
        {
            home = dmSys::GetEnv(dirs[i]);
            if (home)
                break;
        }

        if (!home)
        {
            home = "."; // fall back to current directory, because the server instance might not have any of those paths set
        }

        char home_buf[path_len];
        if (dmStrlCpy(home_buf, home, path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(home_buf, "/", path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(home_buf, ".", path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(home_buf, application_name, path_len) >= path_len)
            return RESULT_INVAL;

        // If {home}/.{application_name exists}, return it here
        if (realpath(home_buf, path_out))
        {
            if (dmStrlCpy(path_out, home_buf, path_len) >= path_len)
                return RESULT_INVAL;
            return RESULT_OK;
        }
        // Default to $HOME/.local/store or $XDG_DATA_DIR
        if (dmStrlCpy(path_out, xdg_buf, path_len) >= path_len)
            return RESULT_INVAL;

        Result r = dmSys::Mkdir(path_out, 0755);
        if (r == RESULT_EXIST)
            return RESULT_OK;
        else
            return r;
    }

    Result OpenURL(const char* url, const char* target)
    {
        char buf[1024];
        dmSnPrintf(buf, 1024, "xdg-open %s", url);
        int ret = system(buf);
        if (ret == 0)
        {
            return RESULT_OK;
        }
        else
        {
            return RESULT_UNKNOWN;
        }
    }

    Result GetApplicationPath(char* path_out, uint32_t path_len)
    {
        // try to read the full path of the exe using readlink()
        // we need a new buffer that is slightly larger than the
        // path length so that we can detect if path_len will be
        // enough or not
        char*   path = (char*)malloc(path_len + 2);
        ssize_t ret = readlink("/proc/self/exe", path, path_len + 2);
        if (ret >= 0 && ret <= path_len + 1)
        {
            path[ret] = '\0';
            dmStrlCpy(path_out, dirname(path), path_len);
            free(path);
            return RESULT_OK;
        }
        free(path);

        // get the pathname (relative) used to execute the program
        // from the auxilary vector
        const char* relative_path = (const char*)getauxval(AT_EXECFN);
        if (!relative_path)
        {
            path_out[0] = '.';
            path_out[1] = '\0';
            return RESULT_OK;
        }

        // convert the relative pathname to an absolute pathname
        char* absolute_path = realpath(relative_path, NULL);
        if (!absolute_path)
        {
            path_out[0] = '.';
            path_out[1] = '\0';
            return RESULT_OK;
        }

        // get the directory from the pathname (ie remove exe name)
        dmStrlCpy(path_out, dirname(absolute_path), path_len);
        free(absolute_path);
        return RESULT_OK;
    }

    Result GetResourcesPath(int argc, char* argv[], char* path, uint32_t path_len)
    {
        return dmSysPosix::GetResourcesPath(argc, argv, path, path_len);
    }

    Result GetLogPath(char* path, uint32_t path_len)
    {
        return dmSysPosix::GetLogPath(path, path_len);
    }

    void FillTimeZone(SystemInfo* info)
    {
        dmSysPosix::FillTimeZone(info);
    }

    void GetSystemInfo(SystemInfo* info)
    {
        memset(info, 0, sizeof(*info));
        struct utsname uts;
        uname(&uts);

        dmStrlCpy(info->m_SystemName, "Linux", sizeof(info->m_SystemName));
        dmStrlCpy(info->m_SystemVersion, uts.release, sizeof(info->m_SystemVersion));

        const char* default_lang = "en_US";
        const char* lang = getenv("LANG");
        if (!lang)
        {
            dmLogWarning("Variable LANG not set");
            lang = default_lang;
        }
        FillLanguageTerritory(lang, info);
        FillTimeZone(info);
    }

    void GetSecureInfo(SystemInfo* info)
    {
    }

    bool GetApplicationInfo(const char* id, ApplicationInfo* info)
    {
        memset(info, 0, sizeof(*info));
        return false;
    }

    bool ResourceExists(const char* path)
    {
        return dmSysPosix::ResourceExists(path);
    }

    Result ResourceSize(const char* path, uint32_t* resource_size)
    {
        return dmSysPosix::ResourceSize(path, resource_size);
    }

    Result LoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size)
    {
        return dmSysPosix::LoadResource(path, buffer, buffer_size, resource_size);
    }

    Result LoadResourcePartial(const char* path, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
    {
        return dmSysPosix::LoadResourcePartial(path, offset, size, buffer, nread);
    }

    Result Rmdir(const char* path)
    {
        return dmSysPosix::Rmdir(path);
    }

    Result Mkdir(const char* path, uint32_t mode)
    {
        return dmSysPosix::Mkdir(path, mode);
    }

    Result IsDir(const char* path)
    {
        return dmSysPosix::IsDir(path);
    }

    bool Exists(const char* path)
    {
        return dmSysPosix::Exists(path);
    }

    Result IterateTree(const char* dirpath, bool recursive, bool call_before, void* ctx, void (*callback)(void* ctx, const char* path, bool isdir))
    {
        return dmSysPosix::IterateTree(dirpath, recursive, call_before, ctx, callback);
    }

    Result Unlink(const char* path)
    {
        return dmSysPosix::Unlink(path);
    }

    Result Stat(const char* path, StatInfo* stat_info)
    {
        return dmSysPosix::Stat(path, stat_info);
    }

    int StatIsDir(const StatInfo* stat_info)
    {
        return dmSysPosix::StatIsDir(stat_info);
    }

    int StatIsFile(const StatInfo* stat_info)
    {
        return dmSysPosix::StatIsFile(stat_info);
    }
} // namespace dmSys
