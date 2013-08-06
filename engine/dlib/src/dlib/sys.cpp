#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "sys.h"
#include "log.h"
#include "dstrings.h"
#include "path.h"

#ifdef _WIN32
#include <Shlobj.h>
#include <Shellapi.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&S_IFMT) == S_IFREG)
#endif

#ifdef __MACH__
#include <CoreFoundation/CFBundle.h>
#ifndef __arm__
#include <Carbon/Carbon.h>
#endif
#endif

#ifdef __ANDROID__
#include <jni.h>
#include <android_native_app_glue.h>
#include <sys/types.h>
#include <android/asset_manager.h>
// By convention we have a global variable called g_AndroidApp
// This is currently created in glfw..
// Application life-cycle should perhaps be part of dlib instead
extern struct android_app* __attribute__((weak)) g_AndroidApp ;
#endif


namespace dmSys
{

    #define DM_SYS_NATIVE_TO_RESULT_CASE(x) case E##x: return RESULT_##x

    static Result NativeToResult(int r)
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

#if defined(__MACH__)

#ifndef __arm__
    // NOTE: iOS implementation in sys_cocoa.mm
    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        FSRef file;
        FSFindFolder(kUserDomain, kApplicationSupportFolderType, kCreateFolder, &file);
        FSRefMakePath(&file, (UInt8*) path, path_len);
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
#endif

#elif defined(_WIN32)
    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        char tmp_path[MAX_PATH];

        if(SUCCEEDED(SHGetFolderPath(NULL,
                                     CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                                     NULL,
                                     0,
                                     tmp_path)))
        {
            if (dmStrlCpy(path, tmp_path, path_len) >= path_len)
                return RESULT_INVAL;
            if (dmStrlCat(path, "\\", path_len) >= path_len)
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

    Result OpenURL(const char* url)
    {
        int ret = (int) ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
        if (ret == 32)
        {
            return RESULT_OK;
        }
        else
        {
            return RESULT_UNKNOWN;
        }
    }

#elif defined(__ANDROID__)

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        ANativeActivity* activity = g_AndroidApp->activity;
        JNIEnv* env = 0;
        activity->vm->AttachCurrentThread( &env, 0);

        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_files_dir_method = env->GetMethodID(activity_class, "getFilesDir", "()Ljava/io/File;");
        jobject files_dir_obj = env->CallObjectMethod(activity->clazz, get_files_dir_method);
        jclass file_class = env->FindClass("java/io/File");
        jmethodID getPathMethod = env->GetMethodID(file_class, "getPath", "()Ljava/lang/String;");
        jstring path_obj = (jstring) env->CallObjectMethod(files_dir_obj, getPathMethod);
        const char* filesDir = env->GetStringUTFChars(path_obj, NULL);

        Result res = RESULT_OK;
        if (dmStrlCpy(path, filesDir, path_len) >= path_len) {
            res = RESULT_INVAL;
        }
        env->ReleaseStringUTFChars(path_obj, filesDir);
        activity->vm->DetachCurrentThread();
        return res;
    }

    Result OpenURL(const char* url)
    {
        // TODO:
        return RESULT_UNKNOWN;
    }

#elif defined(__linux__)
    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        char* home = getenv("HOME");
        if (!home)
            return RESULT_UNKNOWN;

        if (dmStrlCpy(path, home, path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(path, "/", path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(path, ".", path_len) >= path_len)
            return RESULT_INVAL;
        if (dmStrlCat(path, application_name, path_len) >= path_len)
            return RESULT_INVAL;
        Result r =  Mkdir(path, 0755);
        if (r == RESULT_EXIST)
            return RESULT_OK;
        else
            return r;
    }

    Result OpenURL(const char* url)
    {
        char buf[1024];
        DM_SNPRINTF(buf, 1024, "xdg-open %s", url);
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

#endif

    Result GetResourcesPath(int argc, char* argv[], char* path, uint32_t path_len)
    {
        assert(path_len > 0);
        path[0] = '\0';
#if defined(__MACH__)
        CFBundleRef mainBundle = CFBundleGetMainBundle();
        if (!mainBundle)
        {
            dmLogFatal("Unable to get main bundle");
            return RESULT_UNKNOWN;
        }
        CFURLRef resourceURL = CFBundleCopyResourcesDirectoryURL(mainBundle);

        if(resourceURL)
        {
            CFURLGetFileSystemRepresentation(resourceURL, true, (UInt8*) path, path_len);
            CFRelease(resourceURL);
            return RESULT_OK;
        }
        else
        {
            dmLogFatal("Unable to locate bundle resource directory");
            return RESULT_UNKNOWN;
        }
#else
        dmPath::Dirname(argv[0], path, path_len);
        return RESULT_OK;
#endif
    }

    void FillLanguageTerritory(const char* lang, struct SystemInfo* info)
    {
        const char* default_lang = "en_US";

        if (strlen(lang) < 5) {
            dmLogWarning("Unknown language format: '%s'", lang);
            lang = default_lang;
        } else if(lang[2] != '_') {
            dmLogWarning("Unknown language format: '%s'", lang);
            lang = default_lang;
        }

        info->m_Language[0] = lang[0];
        info->m_Language[1] = lang[1];
        info->m_Language[2] = '\0';
        info->m_Territory[0] = lang[3];
        info->m_Territory[1] = lang[4];
        info->m_Territory[2] = '\0';
    }

#if (defined(__MACH__) and not defined(__arm__)) or (defined(__linux__) and not defined(__ANDROID__))
    void GetSystemInfo(SystemInfo* info)
    {
        struct utsname uts;
        uname(&uts);

        dmStrlCpy(info->m_SystemName, uts.sysname, sizeof(info->m_SystemName));
        dmStrlCpy(info->m_SystemVersion, uts.release, sizeof(info->m_SystemVersion));
        info->m_DeviceModel[0] = '\0';

        const char* lang = getenv("LANG");
        const char* default_lang = "en_US";

        if (!lang) {
            dmLogWarning("Variable LANG not set");
            lang = default_lang;
        }
        FillLanguageTerritory(lang, info);
    }

#elif defined(__ANDROID__)

    void GetSystemInfo(SystemInfo* info)
    {
        dmStrlCpy(info->m_SystemName, "Android", sizeof(info->m_SystemName));

        ANativeActivity* activity = g_AndroidApp->activity;
        JNIEnv* env = 0;
        activity->vm->AttachCurrentThread( &env, 0);

        jclass locale_class = env->FindClass("java/util/Locale");
        jmethodID get_default_method = env->GetStaticMethodID(locale_class, "getDefault", "()Ljava/util/Locale;");
        jmethodID get_country_method = env->GetMethodID(locale_class, "getCountry", "()Ljava/lang/String;");
        jmethodID get_language_method = env->GetMethodID(locale_class, "getLanguage", "()Ljava/lang/String;");
        jobject locale = (jobject) env->CallStaticObjectMethod(locale_class, get_default_method);
        jstring countryObj = (jstring) env->CallObjectMethod(locale, get_country_method);
        jstring languageObj = (jstring) env->CallObjectMethod(locale, get_language_method);

        const char* country = env->GetStringUTFChars(countryObj, NULL);
        const char* language = env->GetStringUTFChars(languageObj, NULL);
        dmStrlCpy(info->m_Language, language, sizeof(info->m_Language));
        dmStrlCpy(info->m_Territory, country, sizeof(info->m_Territory));
        env->ReleaseStringUTFChars(countryObj, country);
        env->ReleaseStringUTFChars(languageObj, language);

        jclass build_class = env->FindClass("android/os/Build");
        jstring modelObj = (jstring) env->GetStaticObjectField(build_class, env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;"));

        jclass build_version_class = env->FindClass("android/os/Build$VERSION");
        jstring releaseObj = (jstring) env->GetStaticObjectField(build_version_class, env->GetStaticFieldID(build_version_class, "RELEASE", "Ljava/lang/String;"));

        const char* model = env->GetStringUTFChars(modelObj, NULL);
        const char* release = env->GetStringUTFChars(releaseObj, NULL);
        dmStrlCpy(info->m_DeviceModel, model, sizeof(info->m_DeviceModel));
        dmStrlCpy(info->m_SystemVersion, release, sizeof(info->m_SystemVersion));
        env->ReleaseStringUTFChars(modelObj, model);
        env->ReleaseStringUTFChars(releaseObj, release);

        activity->vm->DetachCurrentThread();
    }
#elif defined(_WIN32)
    typedef int (WINAPI *PGETUSERDEFAULTLOCALENAME)(LPWSTR, int);

    void GetSystemInfo(SystemInfo* info)
    {
        PGETUSERDEFAULTLOCALENAME GetUserDefaultLocaleName = (PGETUSERDEFAULTLOCALENAME)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetUserDefaultLocaleName");
        dmStrlCpy(info->m_DeviceModel, "", sizeof(info->m_DeviceModel));
        dmStrlCpy(info->m_SystemName, "Windows", sizeof(info->m_SystemName));
        OSVERSIONINFOEX version_info;
        version_info.dwOSVersionInfoSize = sizeof(version_info);
        GetVersionEx(&version_info);

        const int max_len = 256;
        char lang[max_len];
        dmStrlCpy(lang, "en-US", max_len);

        DM_SNPRINTF(info->m_SystemVersion, "%d.%d", version_info.dwMajorVersion, version_info.dwMinorVersion);
        if (GetUserDefaultLocaleName) {
            // Only availble on >= Vista
            WSTR tmp[max_len];
            GetUserDefaultLocaleName(tmp, max_len);
            WideCharToMultiByte(CP_UTF8, 0, tmp, -1, lang, max_len, 0, 0);
        }
        int index = strchr(lang, '-');
        if (index != -1) {
            lang[index] = '_';
        }
        FillLanguageTerritory(lang, info);
    }
#endif

    bool ResourceExists(const char* path)
    {
#ifdef __ANDROID__
        AAssetManager* am = g_AndroidApp->activity->assetManager;
        AAsset* asset = AAssetManager_open(am, path, AASSET_MODE_RANDOM);
        if (asset) {
            AAsset_close(asset);
            return true;
        } else {
            return false;
        }
#else
        struct stat file_stat;
        return stat(path, &file_stat) == 0;
#endif
    }

    Result LoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size)
    {
        *resource_size = 0;
#ifdef __ANDROID__
        // Fix path for android.
        // We always try to have a path-root and '.'
        // for current directory. Assets on Android
        // are always loaded with relative path from assets
        // E.g. The relative path to /assets/file is file
        if (strcmp(path, "./") == 0) {
            path += 2;
        }
        while (*path == '/') {
            ++path;
        }

        AAssetManager* am = g_AndroidApp->activity->assetManager;
        // NOTE: Is AASSET_MODE_BUFFER is much faster than AASSET_MODE_RANDOM.
        AAsset* asset = AAssetManager_open(am, path, AASSET_MODE_BUFFER);
        if (asset) {
            uint32_t asset_size = (uint32_t) AAsset_getLength(asset);
            if (asset_size > buffer_size) {
                return RESULT_INVAL;
            }
            int nread = AAsset_read(asset, buffer, asset_size);
            AAsset_close(asset);
            if (nread != asset_size) {
                return RESULT_IO;
            }
            *resource_size = asset_size;
            return RESULT_OK;
        } else {
            return RESULT_NOENT;
        }
#else
        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return RESULT_NOENT;
            }
            if (file_stat.st_size > buffer_size) {
                return RESULT_INVAL;
            }
            FILE* f = fopen(path, "rb");
            size_t nread = fread(buffer, 1, file_stat.st_size, f);
            fclose(f);
            if (nread != file_stat.st_size) {
                return RESULT_IO;
            }
            *resource_size = file_stat.st_size;
            return RESULT_OK;
        } else {
            return NativeToResult(errno);
        }
#endif
    }
}
