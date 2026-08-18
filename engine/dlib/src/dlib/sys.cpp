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
#include <errno.h>
#include <string.h>

#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/path.h>

#ifdef __EMSCRIPTEN__
// Implemented in library_sys.js
extern "C" void dmSysPumpMessageQueue();
#endif

#if defined(__APPLE__)
    #include <TargetConditionals.h>
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
    #if defined(__linux__) || defined(_WIN32) || (defined(TARGET_OS_OSX) && TARGET_OS_OSX) || defined(DM_PLATFORM_VENDOR)
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

#if !defined(DM_SYS_CUSTOM_HOST_PATHS)
    Result ResolveMountFileName(char* buffer, size_t buffer_size, const char* path)
    {
        dmSnPrintf(buffer, buffer_size, "%s", path);
        if (dmSys::ResourceExists(buffer))
            return RESULT_OK;

        char host_path[DMPATH_MAX_PATH];
        dmSys::GetHostFileName(host_path, sizeof(host_path), path);
        if (dmSys::ResourceExists(host_path))
        {
            dmStrlCpy(buffer, host_path, buffer_size);
            return RESULT_OK;
        }

        return RESULT_NOENT;
    }
#endif

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
        // Platform locale names start with language[-script][-region]. Apple
        // and POSIX may use underscores; POSIX may append codeset or modifier data.
        const char* separators = "-_.@";
        size_t language_len = lang ? strcspn(lang, separators) : 0;
        if (language_len == 0)
        {
            lang = "en_US";
            language_len = 2;
            dmLogWarning("Invalid language parameter (empty field), using default: \"%s\"", lang);
        }
        dmStrlCpy(info->m_Language, lang, dmMath::Min(sizeof(info->m_Language), language_len + 1));

        const char* script = 0;
        const char* region = 0;
        size_t region_len = 0;

        // The subtag after the language is either a four-letter script or the
        // region itself.
        const char* subtag = lang + language_len;
        if (*subtag == '-' || *subtag == '_')
            ++subtag;
        size_t subtag_len = strcspn(subtag, separators);

        // RFC 5646 section 2.2.3 defines script as an optional subtag that is
        // separate from region. Preserve it only when the platform supplied it.
        // https://www.rfc-editor.org/rfc/rfc5646.html#section-2.2.3
        const char* ascii_letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        if (subtag_len == 4 && strspn(subtag, ascii_letters) == subtag_len)
        {
            script = subtag;

            // If there is a script, the following subtag may be the region.
            subtag += subtag_len;
            if (*subtag == '-' || *subtag == '_')
                ++subtag;
            subtag_len = strcspn(subtag, separators);
        }

        // Regions are either two ASCII letters or three digits.
        bool is_region =
            (subtag_len == 2 && strspn(subtag, ascii_letters) == subtag_len) ||
            (subtag_len == 3 && strspn(subtag, "0123456789") == subtag_len);
        if (is_region)
        {
            region = subtag;
            region_len = subtag_len;
        }

        // Expose only explicitly supplied language-script and region. POSIX
        // metadata, variants, extensions, and Windows sort-order subtags are ignored.
        dmStrlCpy(info->m_DeviceLanguage, info->m_Language, sizeof(info->m_DeviceLanguage));
        if (script)
        {
            char script_code[5];
            dmStrlCpy(script_code, script, sizeof(script_code));
            dmStrlCat(info->m_DeviceLanguage, "-", sizeof(info->m_DeviceLanguage));
            dmStrlCat(info->m_DeviceLanguage, script_code, sizeof(info->m_DeviceLanguage));
        }

        info->m_Territory[0] = '\0';
        if (region)
            dmStrlCpy(info->m_Territory, region, region_len + 1);
    }

    void PumpMessageQueue() {
#if defined(__EMSCRIPTEN__)
        dmSysPumpMessageQueue();
#endif
    }
} // namespace
