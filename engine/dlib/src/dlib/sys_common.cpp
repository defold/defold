#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#if defined(_WIN32)
#include <direct.h>
#endif
#include <errno.h>
#include <sys/stat.h>

#include <dlib/sys.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>

namespace dmSys
{
    EngineInfo g_EngineInfo;

	#define DM_SYS_NATIVE_TO_RESULT_CASE(x) case E##x: return RESULT_##x

    Result NativeToResult(int r)
    {
        switch (r)
        {
            DM_SYS_NATIVE_TO_RESULT_CASE(PERM);
            DM_SYS_NATIVE_TO_RESULT_CASE(NOENT);
            DM_SYS_NATIVE_TO_RESULT_CASE(SRCH);
            DM_SYS_NATIVE_TO_RESULT_CASE(INTR);
            DM_SYS_NATIVE_TO_RESULT_CASE(IO);
            DM_SYS_NATIVE_TO_RESULT_CASE(NXIO);
            DM_SYS_NATIVE_TO_RESULT_CASE(2BIG);
            DM_SYS_NATIVE_TO_RESULT_CASE(NOEXEC);
            DM_SYS_NATIVE_TO_RESULT_CASE(BADF);
            DM_SYS_NATIVE_TO_RESULT_CASE(CHILD);
            DM_SYS_NATIVE_TO_RESULT_CASE(DEADLK);
            DM_SYS_NATIVE_TO_RESULT_CASE(NOMEM);
            DM_SYS_NATIVE_TO_RESULT_CASE(ACCES);
            DM_SYS_NATIVE_TO_RESULT_CASE(FAULT);
            DM_SYS_NATIVE_TO_RESULT_CASE(BUSY);
            DM_SYS_NATIVE_TO_RESULT_CASE(EXIST);
            DM_SYS_NATIVE_TO_RESULT_CASE(XDEV);
            DM_SYS_NATIVE_TO_RESULT_CASE(NODEV);
            DM_SYS_NATIVE_TO_RESULT_CASE(NOTDIR);
            DM_SYS_NATIVE_TO_RESULT_CASE(ISDIR);
            DM_SYS_NATIVE_TO_RESULT_CASE(INVAL);
            DM_SYS_NATIVE_TO_RESULT_CASE(NFILE);
            DM_SYS_NATIVE_TO_RESULT_CASE(MFILE);
            DM_SYS_NATIVE_TO_RESULT_CASE(NOTTY);
#ifndef _WIN32
            DM_SYS_NATIVE_TO_RESULT_CASE(TXTBSY);
#endif
            DM_SYS_NATIVE_TO_RESULT_CASE(FBIG);
            DM_SYS_NATIVE_TO_RESULT_CASE(NOSPC);
            DM_SYS_NATIVE_TO_RESULT_CASE(SPIPE);
            DM_SYS_NATIVE_TO_RESULT_CASE(ROFS);
            DM_SYS_NATIVE_TO_RESULT_CASE(MLINK);
            DM_SYS_NATIVE_TO_RESULT_CASE(PIPE);
        }

        dmLogError("Unknown result code %d\n", r);
        return RESULT_UNKNOWN;
    }
    #undef DM_SYS_NATIVE_TO_RESULT_CASE

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

        }
        return "RESULT_UNDEFINED";
    }
    #undef DM_SYS_RESULT_TO_STRING_CASE

    Result Rmdir(const char* path)
    {
        int ret = rmdir(path);
        if (ret == 0)
            return RESULT_OK;
        else
            return NativeToResult(errno);
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
            return NativeToResult(errno);
    }

    Result Unlink(const char* path)
    {
        int ret = unlink(path);
        if (ret == 0)
            return RESULT_OK;
        else
            return NativeToResult(errno);
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
