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

#ifndef VKQUALITY_UTIL_H_
#define VKQUALITY_UTIL_H_

#include "vkquality.h"
#include "vkquality_prediction_file.h"
#include <jni.h>
#include <memory>
#include <mutex>
#include "ThreadUtil.h"

namespace vkquality {

static constexpr uint32_t kCacheSchemaVersion = 2;

class VkQualityManager {
 private:
  // Allows construction with std::unique_ptr from a static method, but
  // disallows construction outside of the class since no one else can
  // construct a ConstructorTag
  struct ConstructorTag {};

  struct CacheFile {
    int32_t schema_version;
    int32_t list_version;
    int32_t recommendation;
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t driver_version;
    int32_t reserved;
    int32_t reserved2;
  };

 public:
  VkQualityManager(JNIEnv *env, AAssetManager *asset_manager,
                   const char *storage_path, const char *asset_filename, 
                   const vkqGraphicsAPIInfo *api_info, int32_t flags,
                   ConstructorTag);

  ~VkQualityManager() = default;

  static vkQualityInitResult Init(JNIEnv *env, AAssetManager *asset_manager,
                                  const char *storage_path,
                                  const char *asset_filename,
                                  const vkqGraphicsAPIInfo *api_info,
                                  int32_t flags);

  static void DestroyInstance(JNIEnv *env);

  static vkQualityRecommendation GetQualityRecommendation();

 private:

  static VkQualityManager* GetInstance();

  static std::string GetStaticStringField(JNIEnv *env, jclass clz,
                                          const char *name);

  static int32_t GetVulkanDEQPLevel(JNIEnv *env);

  static vkQualityInitResult InitDeviceInfo(JNIEnv *env, DeviceInfo &device_info,
                                            const vkqGraphicsAPIInfo *api_info,
                                            const int32_t flags);

  bool LoadCache(const DeviceInfo &device_info);

  void SaveCache(const DeviceInfo &device_info);

  static vkQualityInitResult LoadFile(AAssetManager *asset_manager,
                                      const std::string &storage_path,
                                      const std::string &file_name,
                                      size_t &file_size, void **file_bytes);

  static bool SaveFile(const std::string &storage_path,
                       const std::string &file_name,
                       const size_t file_size, const void *file_bytes);

  vkQualityInitResult StartRecommendation();

  AAssetManager *asset_manager_ = nullptr;
  JNIEnv *env_ = nullptr;
  std::string asset_filename_;
  std::string storage_path_;
  const vkqGraphicsAPIInfo *api_info_ = nullptr;

  int32_t cache_list_version_ = -1;
  int32_t flags_ = 0;

  VkQualityPredictionFile prediction_file_;

  vkQualityRecommendation cache_recommendation_ = kRecommendationErrorNotInitialized;
  vkQualityRecommendation quality_recommendation_ = kRecommendationErrorNotInitialized;

  static std::mutex instance_mutex_;
  static std::unique_ptr<VkQualityManager> instance_ GUARDED_BY(instance_mutex_);
};

} // namespace vkquality

#endif // VKQUALITY_UTIL_H_
