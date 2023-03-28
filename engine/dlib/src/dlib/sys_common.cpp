// Copyright 2020-2023 The Defold Foundation
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
#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <sys/stat.h>

#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>


// TODO: Move to platform specific headers
// For the IterateTree
#if defined(_WIN32)
#include "dirent.h"
#elif defined(__SCE__)
    // use platform specific code
#else
#include <dirent.h>
#endif

#ifdef __EMSCRIPTEN__
// Implemented in library_sys.js
extern "C" void dmSysPumpMessageQueue();
#endif

namespace dmSys
{
    EngineInfo g_EngineInfo;

    #define DM_SYS_ERRNO_TO_RESULT_CASE(x) case E##x: return RESULT_##x

    Result ErrnoToResult(int r)
    {
        switch (r)
        {
            DM_SYS_ERRNO_TO_RESULT_CASE(PERM);
            DM_SYS_ERRNO_TO_RESULT_CASE(NOENT);
            DM_SYS_ERRNO_TO_RESULT_CASE(SRCH);
            DM_SYS_ERRNO_TO_RESULT_CASE(INTR);
            DM_SYS_ERRNO_TO_RESULT_CASE(IO);
            DM_SYS_ERRNO_TO_RESULT_CASE(NXIO);
            DM_SYS_ERRNO_TO_RESULT_CASE(2BIG);
            DM_SYS_ERRNO_TO_RESULT_CASE(NOEXEC);
            DM_SYS_ERRNO_TO_RESULT_CASE(BADF);
            DM_SYS_ERRNO_TO_RESULT_CASE(CHILD);
            DM_SYS_ERRNO_TO_RESULT_CASE(DEADLK);
            DM_SYS_ERRNO_TO_RESULT_CASE(NOMEM);
            DM_SYS_ERRNO_TO_RESULT_CASE(ACCES);
            DM_SYS_ERRNO_TO_RESULT_CASE(FAULT);
            DM_SYS_ERRNO_TO_RESULT_CASE(BUSY);
            DM_SYS_ERRNO_TO_RESULT_CASE(EXIST);
            DM_SYS_ERRNO_TO_RESULT_CASE(XDEV);
            DM_SYS_ERRNO_TO_RESULT_CASE(NODEV);
            DM_SYS_ERRNO_TO_RESULT_CASE(NOTDIR);
            DM_SYS_ERRNO_TO_RESULT_CASE(ISDIR);
            DM_SYS_ERRNO_TO_RESULT_CASE(INVAL);
            DM_SYS_ERRNO_TO_RESULT_CASE(NFILE);
            DM_SYS_ERRNO_TO_RESULT_CASE(MFILE);
            DM_SYS_ERRNO_TO_RESULT_CASE(NOTTY);
#ifndef _WIN32
            DM_SYS_ERRNO_TO_RESULT_CASE(TXTBSY);
#endif
            DM_SYS_ERRNO_TO_RESULT_CASE(FBIG);
            DM_SYS_ERRNO_TO_RESULT_CASE(NOSPC);
            DM_SYS_ERRNO_TO_RESULT_CASE(SPIPE);
            DM_SYS_ERRNO_TO_RESULT_CASE(ROFS);
            DM_SYS_ERRNO_TO_RESULT_CASE(MLINK);
            DM_SYS_ERRNO_TO_RESULT_CASE(PIPE);
            DM_SYS_ERRNO_TO_RESULT_CASE(NOTEMPTY);
        }

        dmLogError("Unknown result code %d\n", r);
        return RESULT_UNKNOWN;
    }
    #undef DM_SYS_ERRNO_TO_RESULT_CASE

    // Currently only used in tests
    #define DM_SYS_RESULT_TO_STRING_CASE(x) case RESULT_##x: return #x;
    const char* ResultToString(Result r)
    {
        switch (r)
        {
            DM_SYS_RESULT_TO_STRING_CASE(OK);
            DM_SYS_RESULT_TO_STRING_CASE(PERM);
            DM_SYS_RESULT_TO_STRING_CASE(NOENT);
            DM_SYS_RESULT_TO_STRING_CASE(SRCH);
            DM_SYS_RESULT_TO_STRING_CASE(INTR);
            DM_SYS_RESULT_TO_STRING_CASE(IO);
            DM_SYS_RESULT_TO_STRING_CASE(NXIO);
            DM_SYS_RESULT_TO_STRING_CASE(2BIG);
            DM_SYS_RESULT_TO_STRING_CASE(NOEXEC);
            DM_SYS_RESULT_TO_STRING_CASE(BADF);
            DM_SYS_RESULT_TO_STRING_CASE(CHILD);
            DM_SYS_RESULT_TO_STRING_CASE(DEADLK);
            DM_SYS_RESULT_TO_STRING_CASE(NOMEM);
            DM_SYS_RESULT_TO_STRING_CASE(ACCES);
            DM_SYS_RESULT_TO_STRING_CASE(FAULT);
            DM_SYS_RESULT_TO_STRING_CASE(BUSY);
            DM_SYS_RESULT_TO_STRING_CASE(EXIST);
            DM_SYS_RESULT_TO_STRING_CASE(XDEV);
            DM_SYS_RESULT_TO_STRING_CASE(NODEV);
            DM_SYS_RESULT_TO_STRING_CASE(NOTDIR);
            DM_SYS_RESULT_TO_STRING_CASE(ISDIR);
            DM_SYS_RESULT_TO_STRING_CASE(INVAL);
            DM_SYS_RESULT_TO_STRING_CASE(NFILE);
            DM_SYS_RESULT_TO_STRING_CASE(MFILE);
            DM_SYS_RESULT_TO_STRING_CASE(NOTTY);
            DM_SYS_RESULT_TO_STRING_CASE(TXTBSY);
            DM_SYS_RESULT_TO_STRING_CASE(FBIG);
            DM_SYS_RESULT_TO_STRING_CASE(NOSPC);
            DM_SYS_RESULT_TO_STRING_CASE(SPIPE);
            DM_SYS_RESULT_TO_STRING_CASE(ROFS);
            DM_SYS_RESULT_TO_STRING_CASE(MLINK);
            DM_SYS_RESULT_TO_STRING_CASE(PIPE);
            DM_SYS_RESULT_TO_STRING_CASE(UNKNOWN);
            DM_SYS_RESULT_TO_STRING_CASE(NOTEMPTY);

        }
        return "RESULT_UNDEFINED";
    }
    #undef DM_SYS_RESULT_TO_STRING_CASE

#if !defined(__SCE__)

    Result Rmdir(const char* path)
    {
        int ret = rmdir(path);
        if (ret == 0)
            return RESULT_OK;
        else
            return ErrnoToResult(errno);
    }

    Result Mkdir(const char* path, uint32_t mode)
    {
#ifdef _WIN32
        int ret = mkdir(path);
#else
        int ret = mkdir(path, (mode_t) mode);
#endif
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
        #if defined(__linux__) || defined(_WIN32) || (defined(__MACH__) && !(defined(__arm__) || defined(__arm64__)))
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

        #else
        dmLogOnceError("RmTree is not supported on this platform");
        return RESULT_IO;
        #endif
    }

#endif

    static void Iterate_RemoveFile(void*, const char* path, bool isdir)
    {
        if (!isdir) {
            dmSys::Unlink(path);
        }
    }

    static void Iterate_RemoveDir(void*, const char* path, bool isdir)
    {
        if (isdir) {
            dmSys::Rmdir(path);
        }
    }

    Result RmTree(const char* path)
    {
    #if defined(__linux__) || defined(_WIN32) || (defined(__MACH__) && !(defined(__arm__) || defined(__arm64__))) || defined(__SCE__)
        bool call_before = false;
        Result result = IterateTree(path, true, call_before, 0, Iterate_RemoveFile);
        if (result != RESULT_OK) {
            dmLogError("Failed to remove file tree '%s': %s", path, ResultToString(result));
            return result;
        }
        result = IterateTree(path, true, call_before, 0, Iterate_RemoveDir);
        if (result != RESULT_OK) {
            dmLogError("Failed to remove directory tree '%s': %s", path, ResultToString(result));
            return result;
        }
        return RESULT_OK;

    #else
        dmLogOnceError("RmTree is not supported on this platform");
        return RESULT_NOENT;
    #endif
    }

    Result Unlink(const char* path)
    {
        int ret = unlink(path);
        if (ret == 0)
            return RESULT_OK;
        else
            return ErrnoToResult(errno);
    }

    void GetEngineInfo(EngineInfo* info)
    {
        *info = g_EngineInfo;
    }

    void SetEngineInfo(EngineInfoParam& info)
    {
        size_t copied = dmStrlCpy(g_EngineInfo.m_Version, info.m_Version, sizeof(g_EngineInfo.m_Version));
        assert(copied < sizeof(g_EngineInfo.m_Version));
        copied = dmStrlCpy(g_EngineInfo.m_VersionSHA1, info.m_VersionSHA1, sizeof(g_EngineInfo.m_VersionSHA1));
        assert(copied < sizeof(g_EngineInfo.m_VersionSHA1));
        copied = dmStrlCpy(g_EngineInfo.m_Platform, info.m_Platform, sizeof(g_EngineInfo.m_Platform));
        assert(copied < sizeof(g_EngineInfo.m_Platform));
        g_EngineInfo.m_IsDebug = info.m_IsDebug;
    }

    void FillLanguageTerritory(const char* lang, struct SystemInfo* info)
    {
        // find first separator ("-" or "_")
        size_t lang_len = lang ? strlen(lang) : 0;
        if(lang_len == 0)
        {
            lang = "en_US";
            lang_len = strlen(lang);
            dmLogWarning("Invalid language parameter (empty field), using default: \"%s\"", lang);
        }
        const char* sep_first = lang;
        while((*sep_first) && (*sep_first != '-') && (*sep_first != '_'))
            ++sep_first;
        const char* sep_last = lang + lang_len;
        while((sep_last != sep_first) && (*sep_last != '-') && (*sep_last != '_'))
            --sep_last;

        dmStrlCpy(info->m_Language, lang, dmMath::Min((size_t)(sep_first+1 - lang), sizeof(info->m_Language)));

        if(sep_first != sep_last)
        {
            // Language script. If there is more than one separator, this is what is up to the last separator (<language>-<script>-<territory> format)
            dmStrlCpy(info->m_DeviceLanguage, lang, dmMath::Min((size_t)(sep_last+1 - lang), sizeof(info->m_DeviceLanguage)));
            info->m_DeviceLanguage[sep_first - lang] = '-';
        }
        else
        {
            // No language script, default to language
            dmStrlCpy(info->m_DeviceLanguage, info->m_Language, dmMath::Min(sizeof(info->m_DeviceLanguage), sizeof(info->m_Language)));
        }

        if(sep_last != lang + lang_len)
        {
            dmStrlCpy(info->m_Territory, sep_last + 1, dmMath::Min((size_t)((lang + lang_len) - sep_last), sizeof(info->m_Territory)));
        }
        else
        {
            info->m_Territory[0] = '\0';
            dmLogWarning("No territory detected in language string: \"%s\"", lang);
        }

    }

    void PumpMessageQueue() {
#if defined(__EMSCRIPTEN__)
        dmSysPumpMessageQueue();
#endif
    }
} // namespace
