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

#ifndef VKQUALITY_PREDICTION_FILE_H_
#define VKQUALITY_PREDICTION_FILE_H_

#include "vkquality_device_info.h"
#include "vkquality_file_format.h"

namespace vkquality {

class VkQualityPredictionFile {
public:
  static constexpr uint32_t kVkQuality_File_Identifier = 0x564b5141; // VKQA
  // A-Z and 'everything else'
  static constexpr uint32_t kShortcut_Offset_Count = 27;

  enum FileParseResult : int32_t {
    kFileParseResult_Success = 0,
    kFileParseResult_Error_TooSmall,
    kFileParseResult_Error_InvalidIdentifier,
    kFileParseResult_Error_LibraryTooOldForFile,
    kFileParseResult_Error_DeviceListOverflow,
    kFileParseResult_Error_DriverAllowOverflow,
    kFileParseResult_Error_DriverDenyOverflow,
    kFileParseResult_Error_GpuAllowOverflow,
    kFileParseResult_Error_GpuDenyOverflow,
    kFileParseResult_Error_SoCAllowOverflow,
    kFileParseResult_Error_SoCDenyOverflow,
    kFileParseResult_Error_StringOffsetOverflow,
    kFileParseResult_Error_ShortcutOverflow
  };

  enum FileMatchResult : int32_t {
    kFileMatch_ExactDevice = 0,
    kFileMatch_DeviceOldVersion,
    kFileMatch_BrandWildcard,
    kFileMatch_DriverAllow,
    kFileMatch_DriverDeny,
    kFileMatch_GpuAllow,
    kFileMatch_GpuDeny,
    kFileMatch_None
  };

  VkQualityPredictionFile();
  ~VkQualityPredictionFile();

  FileParseResult ParseFileData(void *file_data, const size_t file_size,
                                const uint32_t library_version);

  FileMatchResult FindDeviceMatch(const DeviceInfo &device_info, const int32_t flags);

  uint32_t GetListVersion() const { return file_header_->list_version; }

  int32_t GetFutureAndroidAPILevel() const {
    return file_header_->min_future_vulkan_recommendation_api;
  }

  const std::string &GetParseErrorString() const { return file_parse_error_; }

private:
  const char *GetString(const uint32_t string_index);

  FileParseResult ValidateFile(void *file_data, const size_t file_size,
                               const uint32_t library_version);

  FileMatchResult SearchDeviceList(const DeviceInfo &device_info);
  FileMatchResult SearchDriverLists(const DeviceInfo &device_info);
  FileMatchResult SearchDriverList(const DeviceInfo &device_info,
                                   const FileMatchResult match_result);
  FileMatchResult SearchGpuLists(const DeviceInfo &device_info);
  FileMatchResult SearchGpuList(const DeviceInfo &device_info, const FileMatchResult match_result);

  size_t total_file_size_ = 0;
  const VkQualityFileHeader *file_header_ = nullptr;
  const uint32_t *string_offset_table_ = nullptr;
  const uint32_t *device_shortcut_table_ = nullptr;
  const VkQualityDeviceAllowListEntry *device_table_ = nullptr;
  const VkQualityDriverFingerprintEntry *driver_allow_table_ = nullptr;
  const VkQualityDriverFingerprintEntry *driver_deny_table_ = nullptr;
  const VkQualityGpuPredictEntry *gpu_allow_table_ = nullptr;
  const VkQualityGpuPredictEntry *gpu_deny_table_ = nullptr;
  const VkQualityDriverSoCEntry *soc_allow_table_ = nullptr;
  const VkQualityDriverSoCEntry *soc_deny_table_ = nullptr;
  std::string file_parse_error_;
};

} // namespace vkquality

#endif // VKQUALITY_FILE_MANAGER_H_
