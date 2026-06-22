/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "vkquality.h"
#include "vkquality_manager.h"
#include <android/asset_manager_jni.h>
#include <jni.h>

extern "C" {

#define VKQUALITY_MAJOR_VERSION 1
#define VKQUALITY_MINOR_VERSION 2
#define VKQUALITY_BUGFIX_VERSION 4

#define VKQUALITY_GENERATE_PACKED_VERSION(MAJOR, MINOR, BUGFIX) \
    ((MAJOR << 16) | (MINOR << 8) | (BUGFIX))

#define VKQUALITY_PACKED_VERSION                               \
    VKQUALITY_GENERATE_PACKED_VERSION(VKQUALITY_MAJOR_VERSION, \
                                      VKQUALITY_MINOR_VERSION, \
                                      VKQUALITY_BUGFIX_VERSION)

#define VKQUALITY_VERSION_CONCAT_NX(PREFIX, MAJOR, MINOR, BUGFIX) \
    PREFIX##_##MAJOR##_##MINOR##_##BUGFIX
#define VKQUALITY_VERSION_CONCAT(PREFIX, MAJOR, MINOR, BUGFIX) \
    VKQUALITY_VERSION_CONCAT_NX(PREFIX, MAJOR, MINOR, BUGFIX)
#define VKQUALITY_VERSION_SYMBOL                                           \
    VKQUALITY_VERSION_CONCAT(VKQUALITY_version, VKQUALITY_MAJOR_VERSION, \
                              VKQUALITY_MINOR_VERSION,                     \
                              VKQUALITY_BUGFIX_VERSION)

void VKQUALITY_VERSION_SYMBOL();

// Private, used internally by file manager
uint32_t VkQuality_getVersion() {
  return VKQUALITY_PACKED_VERSION;
}

vkQualityInitResult vkQuality_initialize(JNIEnv *env, AAssetManager *asset_manager,
                                         const char *storage_path,
                                         const char *asset_filename) {
  return vkquality::VkQualityManager::Init(env, asset_manager, storage_path, asset_filename,
                                           nullptr,
                                           kInitFlagSkipFingerprintRecommendationCheck);
}

vkQualityInitResult vkQuality_initializeFlags(JNIEnv *env, AAssetManager *asset_manager,
                                         const char *storage_path,
                                         const char *asset_filename,
                                         int32_t flags) {
    return vkquality::VkQualityManager::Init(env, asset_manager, storage_path, asset_filename,
                                             nullptr,
                                             flags | kInitFlagSkipFingerprintRecommendationCheck);
}

vkQualityInitResult vkQuality_initializeFlagsInfo(JNIEnv *env, AAssetManager *asset_manager,
  const char *storage_path,
  const char *asset_filename,
  const vkqGraphicsAPIInfo *api_info,
  int32_t flags) {
    return vkquality::VkQualityManager::Init(env, asset_manager, storage_path, asset_filename,
                                             api_info, flags);
}

void vkQuality_destroy(JNIEnv *env) {
  vkquality::VkQualityManager::DestroyInstance(env);
}

vkQualityRecommendation vkQuality_getRecommendation() {
  return vkquality::VkQualityManager::GetQualityRecommendation();
}

JNIEXPORT jint JNICALL
Java_com_google_android_games_vkquality_VKQuality_startVkQualityFlags(
    JNIEnv *env, jobject activity, jobject jasset_manager,
    jstring jstorage_path, jstring jdata_filename, jint flags) {

  auto path_cstr = env->GetStringUTFChars(jstorage_path, nullptr);
  auto path_length = env->GetStringUTFLength(jstorage_path);
  std::string storage_path(path_cstr, path_length);
  env->ReleaseStringUTFChars(jstorage_path, path_cstr);

  auto data_cstr = env->GetStringUTFChars(jdata_filename, nullptr);
  auto data_length = env->GetStringUTFLength(jdata_filename);
  std::string data_filename(data_cstr, data_length);
  env->ReleaseStringUTFChars(jdata_filename, data_cstr);

  AAssetManager *asset_manager = AAssetManager_fromJava(env, jasset_manager);

  return vkQuality_initializeFlags(env, asset_manager, storage_path.c_str(), data_filename.c_str(),
                              flags);
}

JNIEXPORT jint JNICALL
Java_com_google_android_games_vkquality_VKQuality_startVkQuality(
        JNIEnv *env, jobject activity, jobject jasset_manager,
        jstring jstorage_path, jstring jdata_filename) {
    return Java_com_google_android_games_vkquality_VKQuality_startVkQualityFlags(env, activity,
                                                                                 jasset_manager,
                                                                                 jstorage_path,
                                                                                 jdata_filename, 0);
}

JNIEXPORT jint JNICALL
Java_com_google_android_games_vkquality_VKQuality_getVkQuality(
    JNIEnv *env, jobject activity) {
  return vkQuality_getRecommendation();
}

JNIEXPORT void JNICALL
Java_com_google_android_games_vkquality_VKQuality_stopVkQuality(
    JNIEnv *env, jobject activity) {
  vkQuality_destroy(env);
}

void VKQUALITY_VERSION_SYMBOL() {
  // Intentionally empty: this function is used to ensure that the proper
  // version of the library is linked against the proper headers.
  // In case of mismatch, a linker error will be triggered because of an
  // undefined symbol, as the name of the function depends on the version.
}


} // extern "C"