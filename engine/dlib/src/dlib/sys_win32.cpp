// Copyright 2020-2022 The Defold Foundation
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

#if !(defined(_WIN32) || defined(_GAMING_XBOX))
    #error "Unsupported platform"
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h> // UINT_MAX

#include "safe_windows.h"
#include "sys.h"
#include "sys_private.h"
#include "log.h"
#include "dstrings.h"
#include "hash.h"
#include "path.h"
#include "math.h"
#include "time.h"

// For the IterateTree

#include "dirent.h"


#if !defined(_GAMING_XBOX)
#include <Shlobj.h>
#include <Shellapi.h>
#endif
#include <io.h>
#include <direct.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&S_IFMT) == S_IFREG)
#endif

namespace dmSys
{
    char* GetEnv(const char* name)
    {
        return getenv(name);
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
        bool rename_result = MoveFileExA(src_filename, dst_filename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (rename_result)
        {
            return RESULT_OK;
        }
        return RESULT_UNKNOWN;
    }

#if !defined(_GAMING_XBOX)

    static bool WideToACP(const wchar_t* path, char* out, uint32_t out_len)
    {
        BOOL used_default = FALSE;
        int written = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path, -1, out, out_len, NULL, &used_default);
        return written > 0 && !used_default;
    }

    static bool WidePathToHostPath(const wchar_t* path, char* out, uint32_t out_len)
    {
        if (WideToACP(path, out, out_len))
            return true;

        wchar_t short_path[MAX_PATH];
        DWORD short_path_len = GetShortPathNameW(path, short_path, MAX_PATH);
        if (short_path_len == 0 || short_path_len >= MAX_PATH)
            return false;

        return WideToACP(short_path, out, out_len);
    }

    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len)
    {
        return GetApplicationSupportPath(application_name, path, path_len);
    }

    Result GetApplicationSupportPath(const wchar_t* application_support_path, const char* application_name, char* path, uint32_t path_len)
    {
        char tmp_path[MAX_PATH];
        if (!WidePathToHostPath(application_support_path, tmp_path, sizeof(tmp_path)))
        {
            dmLogError("Failed converting wchar_t path to host path\n");
            return RESULT_UNKNOWN;
        }

        return GetApplicationSupportPath(tmp_path, application_name, path, path_len);
    }

    Result GetApplicationSupportPath(const char* application_support_path, const char* application_name, char* path, uint32_t path_len)
    {
        if (dmStrlCpy(path, application_support_path, path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(path, "/", path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(path, application_name, path_len) >= path_len)
            return RESULT_INVAL;

        Result r =  Mkdir(path, 0755);
        if (r == RESULT_EXIST)
            return RESULT_OK;
        else
            return r;
    }

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        // FOLDERID_RoamingAppData, kept local to avoid an extra uuid.lib dependency.
        static const GUID ROAMING_APP_DATA_FOLDER_ID =
        {
            0x3EB685DB, 0x65F9, 0x4CF6, { 0xA0, 0x3A, 0xE3, 0xEF, 0x65, 0x72, 0x9F, 0x3D }
        };
        PWSTR tmp_wpath = 0;
        HRESULT hr = SHGetKnownFolderPath(ROAMING_APP_DATA_FOLDER_ID, KF_FLAG_CREATE, NULL, &tmp_wpath);
        if (SUCCEEDED(hr))
        {
            Result result = GetApplicationSupportPath(tmp_wpath, application_name, path, path_len);
            CoTaskMemFree(tmp_wpath);
            return result;
        }
        else
        {
            return RESULT_UNKNOWN;
        }
    }

    Result GetApplicationPath(char* path_out, uint32_t path_len)
    {
        assert(path_len > 0);
        path_out[0] = 0;

        char module_file_name[DMPATH_MAX_PATH];
        DWORD copied = GetModuleFileNameA(NULL, module_file_name, DMPATH_MAX_PATH);
        if (copied == 0 || copied >= DMPATH_MAX_PATH)
        {
            return RESULT_INVAL;
        }

        dmPath::Dirname(module_file_name, path_out, path_len);
        return RESULT_OK;
    }

    Result OpenURL(const char* url, const char* target)
    {
        intptr_t ret = (intptr_t) ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
        if (ret > 32)
        {
            return RESULT_OK;
        }
        else
        {
            return RESULT_UNKNOWN;
        }
    }

#endif

#if defined(_GAMING_XBOX)

    // Public GDK definitions/storage mappings:
    // _GAMING_XBOX is the official GDK-on-Xbox preprocessor define:
    // https://learn.microsoft.com/en-us/gaming/gdk/docs/tools/tools-console/visualstudio/preprocessor-definitions?view=gdk-2604
    // Local storage maps installed game data to G:\ and temporary local storage to T:\:
    // https://learn.microsoft.com/en-us/gaming/gdk/docs/features/console/storage/local-storage?view=gdk-2604

    Result GetApplicationPath(char* path_out, uint32_t path_len)
    {
        int written = dmSnPrintf(path_out, path_len, "G:\\");
        if (written >= 0 && (uint32_t)written < path_len)
            return RESULT_OK;
        return RESULT_INVAL;
    }

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        (void)application_name;
        int written = dmSnPrintf(path, path_len, "T:\\");
        if (written >= 0 && (uint32_t)written < path_len)
            return RESULT_OK;
        return RESULT_INVAL;
    }

    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len)
    {
        return GetApplicationSupportPath(application_name, path, path_len);
    }

    Result OpenURL(const char* url, const char* target)
    {
        (void)url;
        (void)target;
        return RESULT_UNKNOWN;
    }

#endif

    Result GetResourcesPath(int argc, char* argv[], char* path, uint32_t path_len)
    {
        assert(path_len > 0);
        path[0] = '\0';

        char module_file_name[DMPATH_MAX_PATH];
        DWORD copied = GetModuleFileNameA(NULL, module_file_name, DMPATH_MAX_PATH);

        if (copied < DMPATH_MAX_PATH)
        {
          dmPath::Dirname(module_file_name, path, path_len);
          return RESULT_OK;
        }

        dmLogFatal("Unable to get module file name");
        return RESULT_UNKNOWN;
    }

    // Default
    Result GetLogPath(char* path, uint32_t path_len)
    {
        if (dmStrlCpy(path, ".", path_len) >= path_len)
            return RESULT_INVAL;

        return RESULT_OK;
    }



    void FillTimeZone(struct SystemInfo* info)
    {
        // tm_gmtoff not available on windows..
        TIME_ZONE_INFORMATION t;
        GetTimeZoneInformation(&t);
        info->m_GmtOffset = -t.Bias;
    }

    typedef int (WINAPI *PGETUSERDEFAULTLOCALENAME)(LPWSTR, int);
    typedef LONG (WINAPI *PRTLGETVERSION)(OSVERSIONINFOW*);

    static bool GetWindowsVersion(DWORD* major_version, DWORD* minor_version)
    {
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
        {
            return false;
        }

        PRTLGETVERSION RtlGetVersion = (PRTLGETVERSION)GetProcAddress(ntdll, "RtlGetVersion");
        if (!RtlGetVersion)
        {
            return false;
        }

        OSVERSIONINFOW version_info;
        memset(&version_info, 0, sizeof(version_info));
        version_info.dwOSVersionInfoSize = sizeof(version_info);
        if (RtlGetVersion(&version_info) != 0)
        {
            return false;
        }

        *major_version = version_info.dwMajorVersion;
        *minor_version = version_info.dwMinorVersion;
        return true;
    }

    void GetSystemInfo(SystemInfo* info)
    {
        memset(info, 0, sizeof(*info));
        PGETUSERDEFAULTLOCALENAME GetUserDefaultLocaleName = (PGETUSERDEFAULTLOCALENAME)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetUserDefaultLocaleName");
        dmStrlCpy(info->m_DeviceModel, "", sizeof(info->m_DeviceModel));
        dmStrlCpy(info->m_SystemName, "Windows", sizeof(info->m_SystemName));

        const int max_len = 256;
        char lang[max_len];
        dmStrlCpy(lang, "en-US", max_len);

        DWORD major_version = 0;
        DWORD minor_version = 0;
        if (GetWindowsVersion(&major_version, &minor_version))
        {
            dmSnPrintf(info->m_SystemVersion, sizeof(info->m_SystemVersion), "%u.%u", (uint32_t)major_version, (uint32_t)minor_version);
        }
        if (GetUserDefaultLocaleName) {
            // Only availble on >= Vista
            wchar_t tmp[max_len];
            GetUserDefaultLocaleName(tmp, max_len);
            WideCharToMultiByte(CP_UTF8, 0, tmp, -1, lang, max_len, 0, 0);
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
        struct __stat64 file_stat;
        return _stat64(path, &file_stat) == 0;
    }

    Result ResourceSize(const char* path, uint32_t* resource_size)
    {
        struct __stat64 file_stat;
        if (_stat64(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return RESULT_NOENT;
            }

            if (file_stat.st_size > UINT_MAX)
            {
                return RESULT_FBIG;
            }

            *resource_size = (uint32_t) file_stat.st_size;
            return RESULT_OK;
        } else {
            return RESULT_NOENT;
        }
    }

    Result LoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size)
    {
        *resource_size = 0;
        struct __stat64 file_stat;
        if (_stat64(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return RESULT_NOENT;
            }

            if (file_stat.st_size > UINT_MAX)
            {
                return RESULT_FBIG;
            }

            uint32_t size_as_32b = (uint32_t) file_stat.st_size;
            if (size_as_32b > buffer_size) {
                return RESULT_INVAL;
            }

            FILE* f = fopen(path, "rb");
            if (!f)
            {
                return ErrnoToResult(errno);
            }
            size_t nread = fread(buffer, 1, size_as_32b, f);
            fclose(f);

            if (nread != size_as_32b)
            {
                return RESULT_IO;
            }
            *resource_size = size_as_32b;
            return RESULT_OK;
        }
        else
        {
            return ErrnoToResult(errno);
        }
    }

    Result LoadResourcePartial(const char* path, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
    {
        if (buffer == 0 || size == 0)
            return RESULT_INVAL;

        struct __stat64 file_stat;
        if (_stat64(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return RESULT_NOENT;
            }

            FILE* f = fopen(path, "rb");
            if (!f)
            {
                return RESULT_NOENT;
            }

            int result = fseek(f, offset, SEEK_SET);
            if (result < 0)
            {
                fclose(f);
                return ErrnoToResult(errno);
            }

            size_t nmemb = fread(buffer, 1, size, f);
            if (ferror(f))
            {
                fclose(f);
                return ErrnoToResult(errno);
            }
            fclose(f);

            *nread = (uint32_t)nmemb;
        } else {
            return ErrnoToResult(errno);
        }
        return RESULT_OK;
    }

    Result Rmdir(const char* path)
    {
        int ret = _rmdir(path);
        if (ret == 0)
            return RESULT_OK;
        else
            return ErrnoToResult(errno);
    }

    Result Mkdir(const char* path, uint32_t mode)
    {
        int ret = _mkdir(path);
        if (ret == 0)
            return RESULT_OK;
        else
            return ErrnoToResult(errno);
    }

    Result IsDir(const char* path)
    {
        struct __stat64 path_stat;
        int ret = _stat64(path, &path_stat);
        if (ret != 0)
            return ErrnoToResult(errno);
        return path_stat.st_mode & S_IFDIR ? RESULT_OK : RESULT_UNKNOWN;
    }

    bool Exists(const char* path)
    {
        struct __stat64 path_stat;
        int ret = _stat64(path, &path_stat);
        return ret == 0;
    }

    Result IterateTree(const char* dirpath, bool recursive, bool call_before, void* ctx, void (*callback)(void* ctx, const char* path, bool isdir))
    {
        struct dirent *entry = NULL;
        DIR *dir = NULL;
        dir = opendir(dirpath);
        if (!dir) {
            return RESULT_NOENT;
        }

        if (call_before)
            callback(ctx, dirpath, true);

        Result res = RESULT_OK;
        while(entry = readdir(dir))
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char abs_path[DMPATH_MAX_PATH];
            int written = dmSnPrintf(abs_path, sizeof(abs_path), "%s/%s", dirpath, entry->d_name);
            if (written < 0 || written >= (int)sizeof(abs_path))
            {
                dmLogError("Failed to iterate '%s/%s': path is too long", dirpath, entry->d_name);
                continue;
            }

            struct stat path_stat;
            if (stat(abs_path, &path_stat) != 0)
            {
                if (errno == ENOENT || errno == ENOTDIR)
                {
                    continue;
                }

                res = ErrnoToResult(errno);
                goto cleanup;
            }

            bool isdir = S_ISDIR(path_stat.st_mode);


            if (call_before)
                callback(ctx, abs_path, isdir);

            if (isdir && recursive)
            {
                // Make sure the directory still exists (the callback might have removed it!)
                if (Exists(abs_path))
                {
                    res = IterateTree(abs_path, recursive, call_before, ctx, callback);
                    if (res != RESULT_OK)
                    {
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

    Result Unlink(const char* path)
    {
        int ret = _unlink(path);
        if (ret == 0)
            return RESULT_OK;
        else
            return ErrnoToResult(errno);
    }

    Result Stat(const char* path, StatInfo* stat_info)
    {
        struct __stat64 info;
        int ret = _stat64(path, &info);
        if (ret != 0)
            return RESULT_NOENT;
        stat_info->m_Size = info.st_size;
        stat_info->m_Mode = info.st_mode;
        stat_info->m_AccessTime = (uint32_t)info.st_atime;
        stat_info->m_ModifiedTime = (uint32_t)info.st_mtime;
        return RESULT_OK;
    }

    int StatIsDir(const StatInfo* stat_info)
    {
        return S_ISDIR(stat_info->m_Mode);
    }

    int StatIsFile(const StatInfo* stat_info)
    {
        return S_ISREG(stat_info->m_Mode);
    }
}
