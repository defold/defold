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
#include "log.h"

#include <dmsdk/dlib/android.h>

#include <android/asset_manager.h>
#include <sys/types.h>

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
        dmAndroid::ThreadAttacher thread;
        JNIEnv*                   env = thread.GetEnv();
        if (!env)
        {
            return NETWORK_DISCONNECTED;
        }

        jclass    def_activity_class = env->GetObjectClass(thread.GetActivity()->clazz);
        jmethodID get_connectivity_method = env->GetMethodID(def_activity_class, "getConnectivity", "()I");
        int       reti = (int)env->CallIntMethod(thread.GetActivity()->clazz, get_connectivity_method);
        return (NetworkConnectivity)reti;
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
        struct android_app* app = dmAndroid::GetAndroidApp();
        if (!app)
        {
            // For unit tests
            dmSnPrintf(path, path_len, "/data/local/tmp");
            return RESULT_OK;
        }

        dmAndroid::ThreadAttacher thread;
        JNIEnv*                   env = thread.GetEnv();
        if (!env)
        {
            return RESULT_UNKNOWN;
        }

        jclass    activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_files_dir_method = env->GetMethodID(activity_class, "getFilesDir", "()Ljava/io/File;");
        jobject   files_dir_obj = env->CallObjectMethod(thread.GetActivity()->clazz, get_files_dir_method);
        jclass    file_class = env->FindClass("java/io/File");
        jmethodID getPathMethod = env->GetMethodID(file_class, "getPath", "()Ljava/lang/String;");
        jstring   path_obj = (jstring)env->CallObjectMethod(files_dir_obj, getPathMethod);

        Result    res = RESULT_OK;

        if (path_obj)
        {
            const char* filesDir = env->GetStringUTFChars(path_obj, NULL);

            if (dmStrlCpy(path, filesDir, path_len) >= path_len)
            {
                res = RESULT_INVAL;
            }
            env->ReleaseStringUTFChars(path_obj, filesDir);
        }
        else
        {
            res = RESULT_UNKNOWN;
        }
        return res;
    }

    Result GetApplicationPath(char* path_out, uint32_t path_len)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv*                   env = thread.GetEnv();
        if (!env)
        {
            return RESULT_UNKNOWN;
        }

        jclass    activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_files_dir_method = env->GetMethodID(activity_class, "getFilesDir", "()Ljava/io/File;");
        jobject   files_dir_obj = env->CallObjectMethod(thread.GetActivity()->clazz, get_files_dir_method);
        jclass    file_class = env->FindClass("java/io/File");
        jmethodID getPathMethod = env->GetMethodID(file_class, "getParent", "()Ljava/lang/String;");
        jstring   path_obj = (jstring)env->CallObjectMethod(files_dir_obj, getPathMethod);

        Result    res = RESULT_OK;

        if (path_obj)
        {
            const char* filesDir = env->GetStringUTFChars(path_obj, NULL);

            if (dmStrlCpy(path_out, filesDir, path_len) >= path_len)
            {
                path_out[0] = 0;
                res = RESULT_INVAL;
            }
            env->ReleaseStringUTFChars(path_obj, filesDir);
        }
        else
        {
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
        JNIEnv*                   env = thread.GetEnv();
        if (!env)
        {
            return RESULT_UNKNOWN;
        }

        jclass    uri_class = env->FindClass("android/net/Uri");
        jstring   str_url = env->NewStringUTF(url);
        jmethodID parse_method = env->GetStaticMethodID(uri_class, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
        jobject   uri = env->CallStaticObjectMethod(uri_class, parse_method, str_url);
        env->DeleteLocalRef(str_url);
        if (uri == NULL)
        {
            return RESULT_UNKNOWN;
        }

        jclass    intent_class = env->FindClass("android/content/Intent");
        jfieldID  action_view_field = env->GetStaticFieldID(intent_class, "ACTION_VIEW", "Ljava/lang/String;");
        jobject   str_action_view = env->GetStaticObjectField(intent_class, action_view_field);
        jmethodID intent_constructor = env->GetMethodID(intent_class, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
        jobject   intent = env->NewObject(intent_class, intent_constructor, str_action_view, uri);
        if (intent == NULL)
        {
            return RESULT_UNKNOWN;
        }

        jclass    activity_class = env->FindClass("android/app/NativeActivity");
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

    Result GetResourcesPath(int argc, char* argv[], char* path, uint32_t path_len)
    {
        return dmSysPosix::GetResourcesPath(argc, argv, path, path_len);
    }

    Result GetLogPath(char* path, uint32_t path_len)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv*                   env = thread.GetEnv();
        if (!env)
        {
            return RESULT_UNKNOWN;
        }

        Result    res = RESULT_OK;

        jclass    activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_files_dir_method = env->GetMethodID(activity_class, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
        jobject   files_dir_obj = env->CallObjectMethod(thread.GetActivity()->clazz, get_files_dir_method, 0);
        if (!files_dir_obj)
        {
            dmLogError("Failed to get log directory. Is android.permission.WRITE_EXTERNAL_STORAGE set in AndroidManifest.xml?");
            res = RESULT_UNKNOWN;
        }
        else
        {
            jclass    file_class = env->FindClass("java/io/File");
            jmethodID getPathMethod = env->GetMethodID(file_class, "getPath", "()Ljava/lang/String;");
            jstring   path_obj = (jstring)env->CallObjectMethod(files_dir_obj, getPathMethod);
            if (path_obj)
            {
                const char* filesDir = env->GetStringUTFChars(path_obj, NULL);
                if (dmStrlCpy(path, filesDir, path_len) >= path_len)
                {
                    res = RESULT_INVAL;
                }
                env->ReleaseStringUTFChars(path_obj, filesDir);
            }
            else
            {
                res = RESULT_UNKNOWN;
            }
        }
        return res;
    }

    void FillTimeZone(SystemInfo* info)
    {
        dmSysPosix::FillTimeZone(info);
    }

    void GetSystemInfo(SystemInfo* info)
    {
        memset(info, 0, sizeof(*info));
        dmStrlCpy(info->m_SystemName, "Android", sizeof(info->m_SystemName));

        dmAndroid::ThreadAttacher thread;
        JNIEnv*                   env = thread.GetEnv();
        if (!env)
        {
            return;
        }

        jclass    locale_class = env->FindClass("java/util/Locale");
        jmethodID get_default_method = env->GetStaticMethodID(locale_class, "getDefault", "()Ljava/util/Locale;");
        jmethodID get_country_method = env->GetMethodID(locale_class, "getCountry", "()Ljava/lang/String;");
        jmethodID get_language_method = env->GetMethodID(locale_class, "getLanguage", "()Ljava/lang/String;");
        jobject   locale = (jobject)env->CallStaticObjectMethod(locale_class, get_default_method);
        jstring   countryObj = (jstring)env->CallObjectMethod(locale, get_country_method);
        jstring   languageObj = (jstring)env->CallObjectMethod(locale, get_language_method);

        char      lang[32] = { 0 };
        if (languageObj)
        {
            const char* language = env->GetStringUTFChars(languageObj, NULL);
            dmStrlCpy(lang, language, sizeof(lang));
            env->ReleaseStringUTFChars(languageObj, language);
        }
        if (countryObj)
        {
            dmStrlCat(lang, "_", sizeof(lang));
            const char* country = env->GetStringUTFChars(countryObj, NULL);
            dmStrlCat(lang, country, sizeof(lang));
            env->ReleaseStringUTFChars(countryObj, country);
        }
        FillLanguageTerritory(lang, info);
        FillTimeZone(info);

        jclass  build_class = env->FindClass("android/os/Build");
        jstring manufacturerObj = (jstring)env->GetStaticObjectField(build_class, env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;"));
        jstring modelObj = (jstring)env->GetStaticObjectField(build_class, env->GetStaticFieldID(build_class, "MODEL", "Ljava/lang/String;"));

        jclass  build_version_class = env->FindClass("android/os/Build$VERSION");
        jstring releaseObj = (jstring)env->GetStaticObjectField(build_version_class, env->GetStaticFieldID(build_version_class, "RELEASE", "Ljava/lang/String;"));
        jint    sdkint = (jint)env->GetStaticIntField(build_version_class, env->GetStaticFieldID(build_version_class, "SDK_INT", "I")); // supported from api level 4

        dmSnPrintf(info->m_ApiVersion, sizeof(info->m_ApiVersion), "%d", sdkint);

        if (manufacturerObj)
        {
            const char* manufacturer = env->GetStringUTFChars(manufacturerObj, NULL);
            dmStrlCpy(info->m_Manufacturer, manufacturer, sizeof(info->m_Manufacturer));
            env->ReleaseStringUTFChars(manufacturerObj, manufacturer);
        }
        if (modelObj)
        {
            const char* model = env->GetStringUTFChars(modelObj, NULL);
            dmStrlCpy(info->m_DeviceModel, model, sizeof(info->m_DeviceModel));
            env->ReleaseStringUTFChars(modelObj, model);
        }
        if (releaseObj)
        {
            const char* release = env->GetStringUTFChars(releaseObj, NULL);
            dmStrlCpy(info->m_SystemVersion, release, sizeof(info->m_SystemVersion));
            env->ReleaseStringUTFChars(releaseObj, release);
        }
    }

    void GetSecureInfo(SystemInfo* info)
    {
        dmAndroid::ThreadAttacher thread;
        JNIEnv*                   env = thread.GetEnv();
        if (!env)
        {
            return;
        }
        jclass    activity_class = env->FindClass("android/app/NativeActivity");
        jmethodID get_content_resolver_method = env->GetMethodID(activity_class, "getContentResolver", "()Landroid/content/ContentResolver;");
        jobject   content_resolver = env->CallObjectMethod(thread.GetActivity()->clazz, get_content_resolver_method);

        jclass    secure_class = env->FindClass("android/provider/Settings$Secure");
        if (secure_class)
        {
            jstring   android_id_string = env->NewStringUTF("android_id");
            jmethodID get_string_method = env->GetStaticMethodID(secure_class, "getString", "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");
            jstring   android_id_obj = (jstring)env->CallStaticObjectMethod(secure_class, get_string_method, content_resolver, android_id_string);
            env->DeleteLocalRef(android_id_string);
            if (android_id_obj)
            {
                const char* android_id = env->GetStringUTFChars(android_id_obj, NULL);
                dmStrlCpy(info->m_DeviceIdentifier, android_id, sizeof(info->m_DeviceIdentifier));
                env->ReleaseStringUTFChars(android_id_obj, android_id);
            }
        }
        else
        {
            dmLogWarning("Unable to get 'android.id'. Is permission android.permission.READ_PHONE_STATE set?");
        }
    }

    bool GetApplicationInfo(const char* id, ApplicationInfo* info)
    {
        memset(info, 0, sizeof(*info));

        dmAndroid::ThreadAttacher thread;
        JNIEnv*                   env = thread.GetEnv();
        if (!env)
        {
            return false;
        }

        jclass    def_activity_class = env->GetObjectClass(thread.GetActivity()->clazz);
        jmethodID isAppInstalled = env->GetMethodID(def_activity_class, "isAppInstalled", "(Ljava/lang/String;)Z");
        jstring   str_url = env->NewStringUTF(id);
        jboolean  installed = env->CallBooleanMethod(thread.GetActivity()->clazz, isAppInstalled, str_url);
        env->DeleteLocalRef(str_url);

        info->m_Installed = installed;
        return installed;
    }

    static const char* FixAndroidResourcePath(const char* path)
    {
        // Assets on Android are loaded with a relative path from assets.
        if (strncmp(path, "./", 2) == 0)
        {
            path += 2;
        }
        while (*path == '/')
        {
            ++path;
        }
        return path;
    }

    static AAssetManager* GetAndroidAssetManager()
    {
        struct android_app* app = dmAndroid::GetAndroidApp();
        if (!app)
            return 0;
        if (!app->activity)
            return 0;
        return app->activity->assetManager;
    }

    bool ResourceExists(const char* path)
    {
        AAssetManager* am = GetAndroidAssetManager();
        if (am)
        {
            AAsset* asset = AAssetManager_open(am, path, AASSET_MODE_RANDOM);
            if (asset)
            {
                AAsset_close(asset);
                return true;
            }
        }
        return dmSysPosix::ResourceExists(path);
    }

    Result ResourceSize(const char* path, uint32_t* resource_size)
    {
        AAssetManager* am = GetAndroidAssetManager();
        if (am)
        {
            path = FixAndroidResourcePath(path);
            AAsset* asset = AAssetManager_open(am, path, AASSET_MODE_RANDOM);
            if (asset)
            {
                *resource_size = (uint32_t)AAsset_getLength(asset);

                AAsset_close(asset);
                return RESULT_OK;
            }
        }

        return dmSysPosix::ResourceSize(path, resource_size);
    }

    Result LoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size)
    {
        *resource_size = 0;

        AAssetManager* am = GetAndroidAssetManager();
        if (am)
        {
            const char* asset_path = FixAndroidResourcePath(path);

            // NOTE: AASSET_MODE_BUFFER is much faster than AASSET_MODE_RANDOM.
            AAsset* asset = AAssetManager_open(am, asset_path, AASSET_MODE_BUFFER);
            if (asset)
            {
                uint32_t asset_size = (uint32_t)AAsset_getLength(asset);
                if (asset_size > buffer_size)
                {
                    AAsset_close(asset);
                    return RESULT_INVAL;
                }
                uint32_t nread = (uint32_t)AAsset_read(asset, buffer, asset_size);
                AAsset_close(asset);
                if (nread != asset_size)
                {
                    return RESULT_IO;
                }
                *resource_size = asset_size;
                return RESULT_OK;
            }
        }

        return dmSysPosix::LoadResource(path, buffer, buffer_size, resource_size);
    }

    Result LoadResourcePartial(const char* path, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
    {
        if (buffer == 0 || size == 0)
            return RESULT_INVAL;

        AAssetManager* am = GetAndroidAssetManager();
        if (am)
        {
            const char* asset_path = FixAndroidResourcePath(path);

            // NOTE: AASSET_MODE_BUFFER is much faster than AASSET_MODE_RANDOM.
            AAsset* asset = AAssetManager_open(am, asset_path, AASSET_MODE_BUFFER);
            if (asset)
            {
                int result = AAsset_seek(asset, offset, SEEK_SET);
                if (result < 0)
                {
                    return RESULT_INVAL;
                }
                uint32_t nmemb = (uint32_t)AAsset_read(asset, buffer, size);
                AAsset_close(asset);
                *nread = nmemb;
                return RESULT_OK;
            }
        }

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
