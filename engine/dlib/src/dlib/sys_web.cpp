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
#include <stdlib.h>
#include <string.h>

#include "sys.h"
#include "sys_posix.h"
#include "sys_private.h"
#include "dstrings.h"

#include <emscripten.h>

// Implemented in library_sys.js
extern "C" const char* dmSysGetUserPersistentDataRoot();
extern "C" const char* dmSysGetUserPreferredLanguage(const char* defaultlang);
extern "C" const char* dmSysGetUserAgent();
extern "C" bool        dmSysOpenURL(const char* url, const char* target);
extern "C" const char* dmSysGetApplicationPath();

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

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        const char* const DeviceMount = dmSysGetUserPersistentDataRoot();
        if (0 < strlen(DeviceMount))
        {
            if (dmStrlCpy(path, DeviceMount, path_len) >= path_len)
                return RESULT_INVAL;
            if (dmStrlCat(path, "/", path_len) >= path_len)
                return RESULT_INVAL;
        }
        else
        {
            path[0] = '\0';
        }
        if (dmStrlCat(path, ".", path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(path, application_name, path_len) >= path_len)
            return RESULT_INVAL;
        Result r = Mkdir(path, 0755);
        if (r == RESULT_EXIST)
            return RESULT_OK;
        else
            return r;
    }

    Result OpenURL(const char* url, const char* target)
    {
        if (dmSysOpenURL(url, target))
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
        const char* applicationPath = dmSysGetApplicationPath();

        if (dmStrlCpy(path_out, applicationPath, path_len) >= path_len)
        {
            path_out[0] = 0;
            return RESULT_INVAL;
        }
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

        dmStrlCpy(info->m_SystemName, "HTML5", sizeof(info->m_SystemName));

        const char* default_lang = "en_US";
        info->m_UserAgent = dmSysGetUserAgent(); // transfer ownership to SystemInfo struct
        const char* const lang = dmSysGetUserPreferredLanguage(default_lang);
        FillLanguageTerritory(lang, info);
        FillTimeZone(info);

        free((void*)lang);
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
