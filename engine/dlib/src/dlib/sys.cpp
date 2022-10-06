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

#ifdef _WIN32
#include <Shlobj.h>
#include <Shellapi.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <fcntl.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISREG
#define S_ISREG(mode) (((mode)&S_IFMT) == S_IFREG)
#endif

#ifdef __MACH__
#include <CoreFoundation/CFBundle.h>
#if !defined(__arm__) && !defined(__arm64__) && !defined(IOS_SIMULATOR)
#include <Carbon/Carbon.h>
#endif
#endif

#ifdef __ANDROID__

#include <dmsdk/dlib/android.h>
#include <sys/types.h>
#include <android/asset_manager.h>
// By convention we have a global variable called g_AndroidApp
// This is currently created in glfw..
// Application life-cycle should perhaps be part of dlib instead
extern struct android_app* __attribute__((weak)) g_AndroidApp ;
#endif

#if defined(__linux__) && !defined(ANDROID)
#include <unistd.h>
#include <sys/auxv.h>
#include <libgen.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// Implemented in library_sys.js
extern "C" const char* dmSysGetUserPersistentDataRoot();
extern "C" const char* dmSysGetUserPreferredLanguage(const char* defaultlang);
extern "C" const char* dmSysGetUserAgent();
extern "C" bool dmSysOpenURL(const char* url, const char* target);
extern "C" const char* dmSysGetApplicationPath();

#endif


namespace dmSys
{
    char* GetEnv(const char* name)
    {
        return getenv(name);
    }

#if defined(__ANDROID__)

    void SetNetworkConnectivityHost(const char* host) { }

    NetworkConnectivity GetNetworkConnectivity()
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            return NETWORK_DISCONNECTED;
        }

        jclass def_activity_class = env->GetObjectClass(thread.GetActivity()->clazz);
        jmethodID get_connectivity_method = env->GetMethodID(def_activity_class, "getConnectivity", "()I");
        int reti = (int)env->CallIntMethod(thread.GetActivity()->clazz, get_connectivity_method);
        return (NetworkConnectivity)reti;
    }

#elif !defined(__MACH__) // OS X and iOS implementations in sys_cocoa.mm

    void SetNetworkConnectivityHost(const char* host) { }

    NetworkConnectivity GetNetworkConnectivity()
    {
        return NETWORK_CONNECTED;
    }

#endif


#if !defined(__EMSCRIPTEN__)
    Result RenameFile(const char* dst_filename, const char* src_filename)
    {
#if defined(_WIN32)
        bool rename_result = MoveFileExA(src_filename, dst_filename, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        bool rename_result = rename(src_filename, dst_filename) != -1;
#endif
        if (rename_result)
        {
            return RESULT_OK;
        }
        return RESULT_UNKNOWN;
    }
#else // EMSCRIPTEN
    Result RenameFile(const char* dst_filename, const char* src_filename)
    {
        FILE* src_file = fopen(src_filename, "rb");
        if (!src_file)
        {
            return RESULT_IO;
        }

        fseek(src_file, 0, SEEK_END);
        size_t buf_len = ftell(src_file);
        fseek(src_file, 0, SEEK_SET);
        char* buf = (char*)malloc(buf_len);
        if (fread(buf, 1, buf_len, src_file) != buf_len)
        {
            fclose(src_file);
            free(buf);
            return RESULT_IO;
        }

        FILE* dst_file = fopen(dst_filename, "wb");
        if (!dst_file)
        {
            fclose(src_file);
            free(buf);
            return RESULT_IO;
        }

        if(fwrite(buf, 1, buf_len, dst_file) != buf_len)
        {
            fclose(src_file);
            fclose(dst_file);
            free(buf);
            return RESULT_IO;
        }

        fclose(src_file);
        fclose(dst_file);
        free(buf);

        dmSys::Unlink(src_filename);

        return RESULT_OK;
    }

#endif

    Result ResolveMountFileName(char* buffer, size_t buffer_size, const char* path)
    {
        dmSnPrintf(buffer, buffer_size, "%s", path);
        if (dmSys::ResourceExists(buffer))
            return RESULT_OK;
        return RESULT_NOENT;
    }

#if defined(__MACH__)

// NOTE: iOS/OSX implementation of GetApplicationPath()/GetApplicationSupportPath() in sys_cocoa.mm

#elif defined(_WIN32)

    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len)
    {
        return GetApplicationSupportPath(application_name, path, path_len);
    }

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        char tmp_path[MAX_PATH];

        if(SUCCEEDED(SHGetFolderPathA(NULL,
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
        int ret = (int) ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
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

    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len)
    {
        return GetApplicationSupportPath(application_name, path, path_len);
    }

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            return RESULT_UNKNOWN;
        }

        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_files_dir_method = env->GetMethodID(activity_class, "getFilesDir", "()Ljava/io/File;");
        jobject files_dir_obj = env->CallObjectMethod(thread.GetActivity()->clazz, get_files_dir_method);
        jclass file_class = env->FindClass("java/io/File");
        jmethodID getPathMethod = env->GetMethodID(file_class, "getPath", "()Ljava/lang/String;");
        jstring path_obj = (jstring) env->CallObjectMethod(files_dir_obj, getPathMethod);

        Result res = RESULT_OK;

        if (path_obj) {
            const char* filesDir = env->GetStringUTFChars(path_obj, NULL);

            if (dmStrlCpy(path, filesDir, path_len) >= path_len) {
                res = RESULT_INVAL;
            }
            env->ReleaseStringUTFChars(path_obj, filesDir);
        } else {
            res = RESULT_UNKNOWN;
        }
        return res;
    }

    Result GetApplicationPath(char* path_out, uint32_t path_len)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            return RESULT_UNKNOWN;
        }

        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_files_dir_method = env->GetMethodID(activity_class, "getFilesDir", "()Ljava/io/File;");
        jobject files_dir_obj = env->CallObjectMethod(thread.GetActivity()->clazz, get_files_dir_method);
        jclass file_class = env->FindClass("java/io/File");
        jmethodID getPathMethod = env->GetMethodID(file_class, "getParent", "()Ljava/lang/String;");
        jstring path_obj = (jstring) env->CallObjectMethod(files_dir_obj, getPathMethod);

        Result res = RESULT_OK;

        if (path_obj) {
            const char* filesDir = env->GetStringUTFChars(path_obj, NULL);

            if (dmStrlCpy(path_out, filesDir, path_len) >= path_len) {
                path_out[0] = 0;
                res = RESULT_INVAL;
            }
            env->ReleaseStringUTFChars(path_obj, filesDir);
        } else {
            res = RESULT_UNKNOWN;
        }
        return res;
    }

    Result OpenURL(const char* url, const char* target)
    {
        if (*url == 0x0)
        {
            return RESULT_INVAL;
        }

        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            return RESULT_UNKNOWN;
        }

        jclass uri_class = env->FindClass("android/net/Uri");
        jstring str_url = env->NewStringUTF(url);
        jmethodID parse_method = env->GetStaticMethodID(uri_class, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
        jobject uri = env->CallStaticObjectMethod(uri_class, parse_method, str_url);
        env->DeleteLocalRef(str_url);
        if (uri == NULL)
        {
            return RESULT_UNKNOWN;
        }

        jclass intent_class = env->FindClass("android/content/Intent");
        jfieldID action_view_field = env->GetStaticFieldID(intent_class, "ACTION_VIEW", "Ljava/lang/String;");
        jobject str_action_view = env->GetStaticObjectField(intent_class, action_view_field);
        jmethodID intent_constructor = env->GetMethodID(intent_class, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
        jobject intent = env->NewObject(intent_class, intent_constructor, str_action_view, uri);
        if (intent == NULL)
        {
            return RESULT_UNKNOWN;
        }

        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID start_activity_method = env->GetMethodID(activity_class, "startActivity", "(Landroid/content/Intent;)V");
        env->CallVoidMethod(thread.GetActivity()->clazz, start_activity_method, intent);
        jthrowable exception = env->ExceptionOccurred();
        env->ExceptionClear();
        if (exception != NULL)
        {
            return RESULT_UNKNOWN;
        }

        return RESULT_OK;
    }


#elif defined(__EMSCRIPTEN__)

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
        } else {
            path[0] = '\0';
        }
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

    Result OpenURL(const char* url, const char* target)
    {
        if (dmSysOpenURL(url, target)) {
            return RESULT_OK;
        } else {
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

#elif defined(__linux__)

    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len)
    {
        return GetApplicationSupportPath(application_name, path, path_len);
    }

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        const char* dirs[] = {"HOME", "TMPDIR", "TMP", "TEMP"}; // Added common temp directories since server instances usually don't have a HOME set
        size_t count = sizeof(dirs)/sizeof(dirs[0]);
        const char* home = 0;
        for (size_t i = 0; i < count; ++i)
        {
            home = getenv(dirs[i]);
            if (home)
                break;
        }
        if (!home) {
            home = "."; // fall back to current directory, because the server instance might not have any of those paths set
        }

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
        char* path = (char*)malloc(path_len + 2);
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
#elif defined(_WIN32)
        char module_file_name[DMPATH_MAX_PATH];
        DWORD copied = GetModuleFileNameA(NULL, module_file_name, DMPATH_MAX_PATH);

        if (copied < DMPATH_MAX_PATH)
        {
          dmPath::Dirname(module_file_name, path, path_len);
          return RESULT_OK;
        }
        else
        {
          dmLogFatal("Unable to get module file name");
          return RESULT_UNKNOWN;
        }
#else
        dmPath::Dirname(argv[0], path, path_len);
        return RESULT_OK;
#endif
    }

#if ((defined(__arm__) || defined(__arm64__) || defined(IOS_SIMULATOR)) && defined(__MACH__))
    // NOTE: iOS implementation in sys_cocoa.mm

#elif defined(__ANDROID__)
    Result GetLogPath(char* path, uint32_t path_len)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            return RESULT_UNKNOWN;
        }

        Result res = RESULT_OK;

        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_files_dir_method = env->GetMethodID(activity_class, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
        jobject files_dir_obj = env->CallObjectMethod(thread.GetActivity()->clazz, get_files_dir_method, 0);
        if (!files_dir_obj) {
            dmLogError("Failed to get log directory. Is android.permission.WRITE_EXTERNAL_STORAGE set in AndroidManifest.xml?");
            res = RESULT_UNKNOWN;
        } else {
            jclass file_class = env->FindClass("java/io/File");
            jmethodID getPathMethod = env->GetMethodID(file_class, "getPath", "()Ljava/lang/String;");
            jstring path_obj = (jstring) env->CallObjectMethod(files_dir_obj, getPathMethod);
            if (path_obj) {
                const char* filesDir = env->GetStringUTFChars(path_obj, NULL);
                if (dmStrlCpy(path, filesDir, path_len) >= path_len) {
                    res = RESULT_INVAL;
                }
                env->ReleaseStringUTFChars(path_obj, filesDir);
            } else {
                res = RESULT_UNKNOWN;
            }
        }
        return res;
    }

#else
    // Default
    Result GetLogPath(char* path, uint32_t path_len)
    {
        if (dmStrlCpy(path, ".", path_len) >= path_len)
            return RESULT_INVAL;

        return RESULT_OK;
    }
#endif



    void FillTimeZone(struct SystemInfo* info)
    {
#if defined(_WIN32)
        // tm_gmtoff not available on windows..
        TIME_ZONE_INFORMATION t;
        GetTimeZoneInformation(&t);
        info->m_GmtOffset = -t.Bias;
#else
        time_t t;
        time(&t);
        struct tm* lt = localtime(&t);
        info->m_GmtOffset = lt->tm_gmtoff / 60;
#endif
    }

#if (defined(__linux__) && !defined(__ANDROID__)) || defined(__EMSCRIPTEN__)
    void GetSystemInfo(SystemInfo* info)
    {
        memset(info, 0, sizeof(*info));
        struct utsname uts;
        uname(&uts);

#if defined(__EMSCRIPTEN__)
        dmStrlCpy(info->m_SystemName, "HTML5", sizeof(info->m_SystemName));
#else
        dmStrlCpy(info->m_SystemName, "Linux", sizeof(info->m_SystemName));
#endif
        dmStrlCpy(info->m_SystemVersion, uts.release, sizeof(info->m_SystemVersion));
        info->m_DeviceModel[0] = '\0';

        const char* default_lang = "en_US";
#if defined(__EMSCRIPTEN__)
        info->m_UserAgent = dmSysGetUserAgent(); // transfer ownership to SystemInfo struct
        const char* const lang = dmSysGetUserPreferredLanguage(default_lang);
#else
        const char* lang = getenv("LANG");
        if (!lang) {
            dmLogWarning("Variable LANG not set");
            lang = default_lang;
        }
#endif
        FillLanguageTerritory(lang, info);
        FillTimeZone(info);

#if defined(__EMSCRIPTEN__)
        free((void*)lang);
#endif
    }

    void GetSecureInfo(SystemInfo* info)
    {
    }

#elif defined(__ANDROID__)

    void GetSystemInfo(SystemInfo* info)
    {
        memset(info, 0, sizeof(*info));
        dmStrlCpy(info->m_SystemName, "Android", sizeof(info->m_SystemName));

        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            return;
        }

        jclass locale_class = env->FindClass("java/util/Locale");
        jmethodID get_default_method = env->GetStaticMethodID(locale_class, "getDefault", "()Ljava/util/Locale;");
        jmethodID get_country_method = env->GetMethodID(locale_class, "getCountry", "()Ljava/lang/String;");
        jmethodID get_language_method = env->GetMethodID(locale_class, "getLanguage", "()Ljava/lang/String;");
        jobject locale = (jobject) env->CallStaticObjectMethod(locale_class, get_default_method);
        jstring countryObj = (jstring) env->CallObjectMethod(locale, get_country_method);
        jstring languageObj = (jstring) env->CallObjectMethod(locale, get_language_method);

        char lang[32] = {0};
        if (languageObj) {
            const char* language = env->GetStringUTFChars(languageObj, NULL);
            dmStrlCpy(lang, language, sizeof(lang));
            env->ReleaseStringUTFChars(languageObj, language);
        }
        if (countryObj) {
            dmStrlCat(lang, "_", sizeof(lang));
            const char* country = env->GetStringUTFChars(countryObj, NULL);
            dmStrlCat(lang, country, sizeof(lang));
            env->ReleaseStringUTFChars(countryObj, country);
        }
        FillLanguageTerritory(lang, info);
        FillTimeZone(info);

        jclass build_class = env->FindClass("android/os/Build");
        jstring manufacturerObj = (jstring) env->GetStaticObjectField(build_class, env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;"));
        jstring modelObj = (jstring) env->GetStaticObjectField(build_class, env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;"));

        jclass build_version_class = env->FindClass("android/os/Build$VERSION");
        jstring releaseObj = (jstring) env->GetStaticObjectField(build_version_class, env->GetStaticFieldID(build_version_class, "RELEASE", "Ljava/lang/String;"));
        jint sdkint = (jint) env->GetStaticIntField(build_version_class, env->GetStaticFieldID(build_version_class, "SDK_INT", "I")); // supported from api level 4

        dmSnPrintf(info->m_ApiVersion, sizeof(info->m_ApiVersion), "%d", sdkint);

        if (manufacturerObj) {
            const char* manufacturer = env->GetStringUTFChars(manufacturerObj, NULL);
            dmStrlCpy(info->m_Manufacturer, manufacturer, sizeof(info->m_Manufacturer));
            env->ReleaseStringUTFChars(manufacturerObj, manufacturer);
        }
        if (modelObj) {
            const char* model = env->GetStringUTFChars(modelObj, NULL);
            dmStrlCpy(info->m_DeviceModel, model, sizeof(info->m_DeviceModel));
            env->ReleaseStringUTFChars(modelObj, model);
        }
        if (releaseObj) {
            const char* release = env->GetStringUTFChars(releaseObj, NULL);
            dmStrlCpy(info->m_SystemVersion, release, sizeof(info->m_SystemVersion));
            env->ReleaseStringUTFChars(releaseObj, release);
        }
    }

    void GetSecureInfo(SystemInfo* info)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            return;
        }
        jclass activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_content_resolver_method = env->GetMethodID(activity_class, "getContentResolver", "()Landroid/content/ContentResolver;");
        jobject content_resolver = env->CallObjectMethod(thread.GetActivity()->clazz, get_content_resolver_method);

        jclass secure_class = env->FindClass("android/provider/Settings$Secure");
        if (secure_class) {
            jstring android_id_string = env->NewStringUTF("android_id");
            jmethodID get_string_method = env->GetStaticMethodID(secure_class, "getString", "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");
            jstring android_id_obj = (jstring) env->CallStaticObjectMethod(secure_class, get_string_method, content_resolver, android_id_string);
            env->DeleteLocalRef(android_id_string);
            if (android_id_obj) {
                const char* android_id = env->GetStringUTFChars(android_id_obj, NULL);
                dmStrlCpy(info->m_DeviceIdentifier, android_id, sizeof(info->m_DeviceIdentifier));
                env->ReleaseStringUTFChars(android_id_obj, android_id);
            }
        } else {
            dmLogWarning("Unable to get 'android.id'. Is permission android.permission.READ_PHONE_STATE set?")
        }
    }
#elif defined(_WIN32)
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
#endif

#if (__ANDROID__)
    bool GetApplicationInfo(const char* id, ApplicationInfo* info)
    {
        memset(info, 0, sizeof(*info));

        dmAndroid::ThreadAttacher thread;
        JNIEnv* env = thread.GetEnv();
        if (!env)
        {
            return false;
        }

        jclass def_activity_class = env->GetObjectClass(thread.GetActivity()->clazz);
        jmethodID isAppInstalled = env->GetMethodID(def_activity_class, "isAppInstalled", "(Ljava/lang/String;)Z");
        jstring str_url = env->NewStringUTF(id);
        jboolean installed = env->CallBooleanMethod(thread.GetActivity()->clazz, isAppInstalled, str_url);
        env->DeleteLocalRef(str_url);

        info->m_Installed = installed;
        return installed;
    }
#elif !defined(__MACH__) // OS X and iOS implementations in sys_cocoa.mm
    bool GetApplicationInfo(const char* id, ApplicationInfo* info)
    {
        memset(info, 0, sizeof(*info));
        return false;
    }
#endif

#ifdef __ANDROID__
    const char* FixAndroidResourcePath(const char* path)
    {
        // Fix path for android.
        // We always try to have a path-root and '.'
        // for current directory. Assets on Android
        // are always loaded with relative path from assets
        // E.g. The relative path to /assets/file is file

        if (strncmp(path, "./", 2) == 0) {
            path += 2;
        }
        while (*path == '/') {
            ++path;
        }
        return path;
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
        }
#endif
        struct stat file_stat;
        return stat(path, &file_stat) == 0;
    }

    Result ResourceSize(const char* path, uint32_t* resource_size)
    {
#ifdef __ANDROID__
        path = FixAndroidResourcePath(path);
        AAssetManager* am = g_AndroidApp->activity->assetManager;
        AAsset* asset = AAssetManager_open(am, path, AASSET_MODE_RANDOM);
        if (asset) {
            *resource_size = (uint32_t) AAsset_getLength(asset);

            AAsset_close(asset);
            return RESULT_OK;
        }
#endif
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
#ifdef __ANDROID__
        const char* asset_path = FixAndroidResourcePath(path);

        AAssetManager* am = g_AndroidApp->activity->assetManager;
        // NOTE: Is AASSET_MODE_BUFFER is much faster than AASSET_MODE_RANDOM.
        AAsset* asset = AAssetManager_open(am, asset_path, AASSET_MODE_BUFFER);
        if (asset) {
            uint32_t asset_size = (uint32_t) AAsset_getLength(asset);
            if (asset_size > buffer_size) {
                AAsset_close(asset);
                return RESULT_INVAL;
            }
            uint32_t nread = (uint32_t)AAsset_read(asset, buffer, asset_size);
            AAsset_close(asset);
            if (nread != asset_size) {
                return RESULT_IO;
            }
            *resource_size = asset_size;
            return RESULT_OK;
        }
#endif
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
            return NativeToResult(errno);
        }
    }
}
