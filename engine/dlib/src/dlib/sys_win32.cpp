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

#include "sys.h"
#include "log.h"
#include "dstrings.h"
#include "hash.h"
#include "path.h"
#include "math.h"
#include "time.h"

// For the IterateTree

#include "dirent.h"


#include <Shlobj.h>
#include <Shellapi.h>
#include <io.h>
#include <direct.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&S_IFMT) == S_IFREG)
#warning "S_ISREG WAS ADDED AS A DEFINE! /MAWE"
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

    Result GetHostFileName(char* buffer, size_t buffer_size, const char* path)
    {
        dmSnPrintf(buffer, buffer_size, "%s", path);
        return RESULT_OK;
    }

    Result ResolveMountFileName(char* buffer, size_t buffer_size, const char* path)
    {
        dmSnPrintf(buffer, buffer_size, "%s", path);
        if (dmSys::ResourceExists(buffer))
            return RESULT_OK;
        return RESULT_NOENT;
    }


#if !defined(DM_PLATFORM_VENDOR)

    static const wchar_t* SkipSlashesW(const wchar_t* path)
    {
        while (*path && (*path == L'/' || *path == L'\\'))
        {
            ++path;
        }
        return path;
    }

    // Is a path contains unicode characters, we need to make it 8.3 in order to properly use char* functions
    static bool MakePath_8_3(const wchar_t* wpath, wchar_t* out)
    {
        wchar_t tmp[MAX_PATH] = { 0 };
        dmPath::NormalizeW(wpath, tmp, MAX_PATH);
        out[0] = L'\0';

        int has_drive = 0;
        wchar_t* cursor = tmp;
        while (*cursor != L'\0')
        {
            if (!has_drive)
            {
                cursor = wcschr(cursor, L':');
                if (!cursor)
                {
                    dmLogError("Failed to find drive in path: '%ls'\n", wpath);
                    return false;
                }
                has_drive = 1;
                cursor++; // Skip past the ':'"

                // Copy the drive "C:"
                wchar_t c = *cursor;
                *cursor = L'\0';
                wcscpy(out, tmp);

                *cursor = c;
                cursor = (wchar_t*)SkipSlashesW(cursor+1);
                continue;
            }

            wchar_t* sep = wcschr(cursor, L'\\');
            if (!sep)
                sep = wcschr(cursor, L'/');

            // Temporarily null terminate the string
            if (sep)
                *sep = L'\0';

            // Create a version of the previously parsed path, and the latest (untransformed) directory name
            wchar_t testpath[MAX_PATH] = { 0 };
            wcscpy(testpath, out); // The previously path with 8.3 names
            wcscat(testpath, L"/");
            wcscat(testpath, cursor); // The last folder to test

            WIN32_FIND_DATAW fd{0};
            HANDLE h = FindFirstFileW( tmp, &fd );
            if (h == INVALID_HANDLE_VALUE)
            {
                dmLogError("FindFirstFileW failed\n");
                return false;
            }

            wcscat(out, L"/");
            if (fd.cAlternateFileName[0] == L'\0')
                wcscat(out, fd.cFileName);
            else
                wcscat(out, fd.cAlternateFileName);

            if (sep)
                *sep = L'/';
            else
                break;

            cursor = sep + 1;
        }

        return true;
    }

    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len)
    {
        return GetApplicationSupportPath(application_name, path, path_len);
    }

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        wchar_t tmp_wpath[MAX_PATH];

        if(SUCCEEDED(SHGetFolderPathW(NULL,
                                     CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                                     NULL,
                                     0,
                                     tmp_wpath)))
        {
            // Make any unicode directories into 8.3 format if necessary
            wchar_t short_path[MAX_PATH];
            MakePath_8_3(tmp_wpath, short_path);

            int wlength = (int)wcslen(short_path);
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, short_path, wlength, NULL, 0, NULL, NULL);
            if (size_needed == 0)
            {
                dmLogError("Failed converting wchar_t -> char\n");
                return RESULT_UNKNOWN;
            }

            char* tmp_path = (char*)_alloca(size_needed + 1);
            WideCharToMultiByte(CP_UTF8, 0, short_path, wlength, tmp_path, size_needed, NULL, NULL);
            tmp_path[size_needed] = 0;

            if (dmStrlCpy(path, tmp_path, path_len) >= path_len)
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
        else
        {
            return RESULT_UNKNOWN;
        }
    }

    Result GetApplicationPath(char* path_out, uint32_t path_len)
    {
        assert(path_len > 0);
        assert(path_len >= MAX_PATH);
        size_t ret = GetModuleFileNameA(GetModuleHandle(NULL), path_out, path_len);
        if (ret > 0 && ret < path_len) {
            // path_out contains path+filename
            // search for last path separator and end the string there,
            // effectively removing the filename and keeping the path
            size_t i = strlen(path_out);
            do
            {
                i -= 1;
                if (path_out[i] == '\\')
                {
                    path_out[i] = 0;
                    break;
                }
            }
            while (i >= 0);
        }
        else
        {
            path_out[0] = 0;
            return RESULT_INVAL;
        }
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

    void GetSystemInfo(SystemInfo* info)
    {
        memset(info, 0, sizeof(*info));
        PGETUSERDEFAULTLOCALENAME GetUserDefaultLocaleName = (PGETUSERDEFAULTLOCALENAME)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetUserDefaultLocaleName");
        dmStrlCpy(info->m_DeviceModel, "", sizeof(info->m_DeviceModel));
        dmStrlCpy(info->m_SystemName, "Windows", sizeof(info->m_SystemName));
        OSVERSIONINFOA version_info;
        version_info.dwOSVersionInfoSize = sizeof(version_info);
        GetVersionExA(&version_info);

        const int max_len = 256;
        char lang[max_len];
        dmStrlCpy(lang, "en-US", max_len);

        dmSnPrintf(info->m_SystemVersion, sizeof(info->m_SystemVersion), "%d.%d", version_info.dwMajorVersion, version_info.dwMinorVersion);
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
        struct stat file_stat;
        return stat(path, &file_stat) == 0;
    }

    Result ResourceSize(const char* path, uint32_t* resource_size)
    {
        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return RESULT_NOENT;
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
        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return RESULT_NOENT;
            }
            if ((uint32_t) file_stat.st_size > buffer_size) {
                return RESULT_INVAL;
            }
            FILE* f = fopen(path, "rb");
            size_t nread = fread(buffer, 1, file_stat.st_size, f);
            fclose(f);
            if (nread != (size_t) file_stat.st_size) {
                return RESULT_IO;
            }
            *resource_size = file_stat.st_size;
            return RESULT_OK;
        } else {
            return ErrnoToResult(errno);
        }
    }

    Result LoadResourcePartial(const char* path, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
    {
        if (buffer == 0 || size == 0)
            return RESULT_INVAL;

        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {
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
        struct stat path_stat;
        int ret = stat(path, &path_stat);
        if (ret != 0)
            return ErrnoToResult(errno);
        return path_stat.st_mode & S_IFDIR ? RESULT_OK : RESULT_UNKNOWN;
    }

    bool Exists(const char* path)
    {
        struct stat path_stat;
        int ret = stat(path, &path_stat);
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
            DIR* sub_dir = NULL;
            FILE* file = NULL;

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char abs_path[1024];
            dmSnPrintf(abs_path, sizeof(abs_path), "%s/%s", dirpath, entry->d_name);

            struct stat path_stat;
            stat(abs_path, &path_stat);

            bool isdir = S_ISDIR(path_stat.st_mode);


            if (call_before)
                callback(ctx, abs_path, isdir);

            if (isdir && recursive) {

                // Make sure the directory still exists (the callback might have removed it!)
                if (Exists(abs_path))
                {
                    res = IterateTree(abs_path, recursive, call_before, ctx, callback);
                    if (res != RESULT_OK) {
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
        struct stat info;
        int ret = stat(path, &info);
        if (ret != 0)
            return RESULT_NOENT;
        stat_info->m_Size = (uint64_t)info.st_size;
        stat_info->m_Mode = info.st_mode;
        stat_info->m_AccessTime = info.st_atime;
        stat_info->m_ModifiedTime = info.st_mtime;
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
