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
#include "vkquality_prediction_file.h"
#include "vkquality_matching.h"
#include <ctype.h>
#include <malloc.h>
#include <vector>

namespace vkquality {

static constexpr char kNullString = '\0';

static const std::string str_fmt(const char *const fmt_string, ...) {
  va_list va_args;
  va_start(va_args, fmt_string);
  va_list va_args_copy;
  va_copy(va_args_copy, va_args);
  const int formatted_length = std::vsnprintf(NULL, 0, fmt_string, va_args_copy);
  va_end(va_args_copy);

  std::vector<char> format_buffer(formatted_length + 1);
  std::vsnprintf(format_buffer.data(), format_buffer.size(), fmt_string, va_args);
  va_end(va_args);
  return std::string(format_buffer.data(), formatted_length);
}

static bool CheckOffsetListValidity(const uint32_t *offset_list, const uint32_t offset_count,
                                    const size_t file_size) {
  for (uint32_t i = 0; i < offset_count; ++i) {
    size_t offset = offset_list[i];
    if (offset >= file_size) {
      return false;
    }
  }
  return true;
}

VkQualityPredictionFile::VkQualityPredictionFile() {
  file_parse_error_ = "No error";
}

VkQualityPredictionFile::~VkQualityPredictionFile() {
  if (file_header_ != nullptr) {
    free(const_cast<VkQualityFileHeader *>(file_header_));
  }
}

VkQualityPredictionFile::FileParseResult VkQualityPredictionFile::ValidateFile(
    void *file_data, const size_t file_size, const uint32_t library_version) {
  const VkQualityFileHeader *header = reinterpret_cast<const VkQualityFileHeader *>(file_data);

  // Guard against integer overflow by assuming a valid file will be under a megabyte in size
  static constexpr size_t kMaxValidFileSize = 1024 * 1024; // 1 megabyte
  if (file_size > kMaxValidFileSize) {
    file_parse_error_ = str_fmt("File size (%d) exceeds maximum allowed size of %d bytes",
                                (int)file_size, (int)kMaxValidFileSize);
    return kFileParseResult_Error_TooSmall;
  }

  // File must be at least the size of the header
  if (file_size < sizeof(VkQualityFileHeader)) {
    file_parse_error_ = str_fmt("File size (%d) smaller than header size: %d",
                                (int)file_size, (int)sizeof(VkQualityFileHeader));
    return kFileParseResult_Error_TooSmall;
  }
  if (header->file_identifier != kVkQuality_File_Identifier) {
    file_parse_error_ = "File identifier invalid";
    return kFileParseResult_Error_InvalidIdentifier;
  }
  if (header->library_minimum_version > library_version) {
    file_parse_error_ = str_fmt("File minimum library version is %x, but library is %x",
                                header->library_minimum_version, library_version);
    return kFileParseResult_Error_LibraryTooOldForFile;
  }

  // 1. Device list validation
  if (header->device_list_offset > file_size) {
    file_parse_error_ = "Invalid file: Device list offset exceeds file size";
    return kFileParseResult_Error_DeviceListOverflow;
  }
  if (header->device_list_count > file_size / sizeof(VkQualityDeviceAllowListEntry)) {
    file_parse_error_ = "Invalid file: Device list count exceeds maximum possible count";
    return kFileParseResult_Error_DeviceListOverflow;
  }
  const size_t device_list_size = header->device_list_count * sizeof(VkQualityDeviceAllowListEntry);
  if (device_list_size > file_size - header->device_list_offset) {
    file_parse_error_ = "Invalid file: Device list overflows end of file";
    return kFileParseResult_Error_DeviceListOverflow;
  }

  // 2. Driver allow list validation
  if (header->driver_allow_offset > file_size) {
    file_parse_error_ = "Invalid file: driver allow list offset exceeds file size";
    return kFileParseResult_Error_DriverAllowOverflow;
  }
  if (header->driver_allow_count > file_size / sizeof(VkQualityDriverFingerprintEntry)) {
    file_parse_error_ = "Invalid file: driver allow list count exceeds maximum possible count";
    return kFileParseResult_Error_DriverAllowOverflow;
  }
  const size_t driver_allow_list_size = header->driver_allow_count * sizeof(VkQualityDriverFingerprintEntry);
  if (driver_allow_list_size > file_size - header->driver_allow_offset) {
    file_parse_error_ = "Invalid file: driver allow list overflows end of file";
    return kFileParseResult_Error_DriverAllowOverflow;
  }

  // 3. Driver deny list validation
  if (header->driver_deny_offset > file_size) {
    file_parse_error_ = "Invalid file: driver deny list offset exceeds file size";
    return kFileParseResult_Error_DriverDenyOverflow;
  }
  if (header->driver_deny_count > file_size / sizeof(VkQualityDriverFingerprintEntry)) {
    file_parse_error_ = "Invalid file: driver deny list count exceeds maximum possible count";
    return kFileParseResult_Error_DriverDenyOverflow;
  }
  const size_t driver_deny_list_size = header->driver_deny_count * sizeof(VkQualityDriverFingerprintEntry);
  if (driver_deny_list_size > file_size - header->driver_deny_offset) {
    file_parse_error_ = "Invalid file: driver deny list overflows end of file";
    return kFileParseResult_Error_DriverDenyOverflow;
  }

  // 4. GPU allow list validation
  if (header->gpu_allow_predict_offset > file_size) {
    file_parse_error_ = "Invalid file: GPU allow list offset exceeds file size";
    return kFileParseResult_Error_GpuAllowOverflow;
  }
  if (header->gpu_allow_predict_count > file_size / sizeof(VkQualityGpuPredictEntry)) {
    file_parse_error_ = "Invalid file: GPU allow list count exceeds maximum possible count";
    return kFileParseResult_Error_GpuAllowOverflow;
  }
  const size_t gpu_allow_list_size = header->gpu_allow_predict_count * sizeof(VkQualityGpuPredictEntry);
  if (gpu_allow_list_size > file_size - header->gpu_allow_predict_offset) {
    file_parse_error_ = "Invalid file: GPU allow list overflows end of file";
    return kFileParseResult_Error_GpuAllowOverflow;
  }

  // 5. GPU deny list validation
  if (header->gpu_deny_predict_offset > file_size) {
    file_parse_error_ = "Invalid file: GPU deny list offset exceeds file size";
    return kFileParseResult_Error_GpuDenyOverflow;
  }
  if (header->gpu_deny_predict_count > file_size / sizeof(VkQualityGpuPredictEntry)) {
    file_parse_error_ = "Invalid file: GPU deny list count exceeds maximum possible count";
    return kFileParseResult_Error_GpuDenyOverflow;
  }
  const size_t gpu_deny_list_size = header->gpu_deny_predict_count * sizeof(VkQualityGpuPredictEntry);
  if (gpu_deny_list_size > file_size - header->gpu_deny_predict_offset) {
    file_parse_error_ = "Invalid file: GPU deny list overflows end of file";
    return kFileParseResult_Error_GpuDenyOverflow;
  }

  // 6. SoC allow list validation
  if (header->soc_allow_offset > file_size) {
    file_parse_error_ = "Invalid file: SoC allow list offset exceeds file size";
    return kFileParseResult_Error_SoCAllowOverflow;
  }
  if (header->soc_allow_count > file_size / sizeof(VkQualityDriverSoCEntry)) {
    file_parse_error_ = "Invalid file: SoC allow list count exceeds maximum possible count";
    return kFileParseResult_Error_SoCAllowOverflow;
  }
  const size_t soc_allow_list_size = header->soc_allow_count * sizeof(VkQualityDriverSoCEntry);
  if (soc_allow_list_size > file_size - header->soc_allow_offset) {
    file_parse_error_ = "Invalid file: SoC allow list overflows end of file";
    return kFileParseResult_Error_SoCAllowOverflow;
  }

  // 7. SoC deny list validation
  if (header->soc_deny_offset > file_size) {
    file_parse_error_ = "Invalid file: SoC deny list offset exceeds file size";
    return kFileParseResult_Error_SoCDenyOverflow;
  }
  if (header->soc_deny_count > file_size / sizeof(VkQualityDriverSoCEntry)) {
    file_parse_error_ = "Invalid file: SoC deny list count exceeds maximum possible count";
    return kFileParseResult_Error_SoCDenyOverflow;
  }
  const size_t soc_deny_list_size = header->soc_deny_count * sizeof(VkQualityDriverSoCEntry);
  if (soc_deny_list_size > file_size - header->soc_deny_offset) {
    file_parse_error_ = "Invalid file: soc deny list overflows end of file";
    return kFileParseResult_Error_SoCDenyOverflow;
  }

  // 8. String offset list validation
  if (header->string_table_offset > file_size) {
    file_parse_error_ = "Invalid file: string table offset exceeds file size";
    return kFileParseResult_Error_StringOffsetOverflow;
  }
  if (header->string_table_count > file_size / sizeof(uint32_t)) {
    file_parse_error_ = "Invalid file: string table count exceeds maximum possible count";
    return kFileParseResult_Error_StringOffsetOverflow;
  }
  const size_t string_offset_list_size = header->string_table_count * sizeof(uint32_t);
  if (string_offset_list_size > file_size - header->string_table_offset) {
    file_parse_error_ = "Invalid file: string table offset list overflows end of file";
    return kFileParseResult_Error_StringOffsetOverflow;
  }
  const uint8_t *file_start = reinterpret_cast<const uint8_t *>(file_data);
  const uint32_t *string_offsets = reinterpret_cast<const uint32_t *>(
      (file_start + header->string_table_offset));
  if (!CheckOffsetListValidity(string_offsets, header->string_table_count, file_size)) {
    file_parse_error_ = "Invalid file: String offset table entry overflows end of file";
    return kFileParseResult_Error_StringOffsetOverflow;
  }

  // 9. Shortcut offset list validation
  if (header->device_list_shortcuts_offset > file_size) {
    file_parse_error_ = "Invalid file: shortcut offset list offset exceeds file size";
    return kFileParseResult_Error_ShortcutOverflow;
  }
  if (VkQualityPredictionFile::kShortcut_Offset_Count > file_size / sizeof(uint32_t)) {
    file_parse_error_ = "Invalid file: shortcut offset list count exceeds maximum possible count";
    return kFileParseResult_Error_ShortcutOverflow;
  }
  const size_t shortcut_offset_list_size = VkQualityPredictionFile::kShortcut_Offset_Count * sizeof(uint32_t);
  if (shortcut_offset_list_size > file_size - header->device_list_shortcuts_offset) {
    file_parse_error_ = "Invalid file: shortcut offset list overflows end of file";
    return kFileParseResult_Error_ShortcutOverflow;
  }

  return kFileParseResult_Success;
}

VkQualityPredictionFile::FileParseResult VkQualityPredictionFile::ParseFileData(
    void *file_data, const size_t file_size, const uint32_t library_version) {
  VkQualityPredictionFile::FileParseResult result =
      ValidateFile(file_data, file_size, library_version);

  if (result != kFileParseResult_Success) {
    return result;
  }

  total_file_size_ = file_size;
  file_header_ = reinterpret_cast<const VkQualityFileHeader *>(file_data);
  const uint8_t *file_start = reinterpret_cast<const uint8_t *>(file_data);

  string_offset_table_ = reinterpret_cast<const uint32_t *>(
      (file_start + file_header_->string_table_offset));
  device_shortcut_table_ = reinterpret_cast<const uint32_t *>(
      (file_start + file_header_->device_list_shortcuts_offset));
  device_table_ = reinterpret_cast<const VkQualityDeviceAllowListEntry *>(
      (file_start + file_header_->device_list_offset));
  driver_allow_table_ = reinterpret_cast<const VkQualityDriverFingerprintEntry *>(
      (file_start + file_header_->driver_allow_offset));
  driver_deny_table_ = reinterpret_cast<const VkQualityDriverFingerprintEntry *>((
      file_start + file_header_->driver_deny_offset));
  gpu_allow_table_ = reinterpret_cast<const VkQualityGpuPredictEntry *>(
      (file_start + file_header_->gpu_allow_predict_offset));
  gpu_deny_table_ = reinterpret_cast<const VkQualityGpuPredictEntry *>((
      file_start + file_header_->gpu_deny_predict_offset));
  soc_allow_table_ = reinterpret_cast<const VkQualityDriverSoCEntry *>(
      (file_start + file_header_->soc_allow_offset));
  soc_deny_table_ = reinterpret_cast<const VkQualityDriverSoCEntry *>((
      file_start + file_header_->soc_deny_offset));

  return result;
}

VkQualityPredictionFile::FileMatchResult VkQualityPredictionFile::FindDeviceMatch(
    const DeviceInfo &device_info, const int32_t flags) {

  // Search for a prediction from the SoC/fingerprint list
  FileMatchResult result = kFileMatch_None;
  if ((flags & kInitFlagSkipFingerprintRecommendationCheck) == 0) {
      result = SearchDriverLists(device_info);
      if (result != kFileMatch_None) {
          return result;
      }
  }
  // Next search for an explicit device match in the device list
  result = SearchDeviceList(device_info);
  if (result == kFileMatch_None) {
    // If there was no device match, look for a GPU allow or deny prediction match
    result = SearchGpuLists(device_info);
  }

  return result;
}

VkQualityPredictionFile::FileMatchResult VkQualityPredictionFile::SearchDeviceList(
    const DeviceInfo &device_info) {

  // Shortcut offset table is sorted Device.BRAND from A-Z and then everything else, default to
  // the 'everything else' entry after the alphabet
  uint32_t letter_index = 26;
  const char brand_first_letter = toupper(device_info.brand.c_str()[0]);
  if (brand_first_letter >= 'A' && brand_first_letter <= 'Z') {
    letter_index = brand_first_letter - 'A';
  }
  const uint32_t start_device_table_index = device_shortcut_table_[letter_index];

  for (uint32_t i = start_device_table_index; i < file_header_->device_list_count; ++i) {
    const char *brand_string = GetString(device_table_[i].brand_string_index);
    const char *device_string = GetString(device_table_[i].device_string_index);
    std::string_view brand_view(brand_string);
    std::string_view device_view(device_string);
    FileMatchResult result = VkQualityMatching::CheckDeviceMatch(device_info,
                                                                 brand_view,
                                                                 device_view,
                                                                 device_table_[i].min_api_version,
                                                                 device_table_[i].min_driver_version);
    if (result != kFileMatch_None) {
      return result;
    }
  }
  return kFileMatch_None;
}

VkQualityPredictionFile::FileMatchResult VkQualityPredictionFile::SearchDriverLists(
    const DeviceInfo &device_info) {
  FileMatchResult result = SearchDriverList(device_info, kFileMatch_DriverAllow);
  if (result == kFileMatch_None) {
    result = SearchDriverList(device_info, kFileMatch_DriverDeny);
  }
  return result;
}

VkQualityPredictionFile::FileMatchResult VkQualityPredictionFile::SearchDriverList(
    const DeviceInfo &device_info, const FileMatchResult match_result) {
  if (device_info.soc.empty()) {
    // SoC check requires Android API >= 31, string will be empty on
    // earlier versions of Android
    return kFileMatch_None;
  }

  // Linear search at the moment for simplicity and small list sizes, the
  // data for SoC strings and driver fingerprint strings should be alphabetically
  // sorted in a manner compatible with performing a binary search for a future
  // optimization
  uint32_t driver_count;
  uint32_t soc_count;
  const VkQualityDriverFingerprintEntry *driver_table;
  const VkQualityDriverSoCEntry *soc_table;
  if (match_result == kFileMatch_DriverAllow) {
    driver_count = file_header_->driver_allow_count;
    driver_table = driver_allow_table_;
    soc_count = file_header_->soc_allow_count;
    soc_table = soc_allow_table_;
  } else if (match_result == kFileMatch_DriverDeny) {
    driver_count = file_header_->driver_deny_count;
    driver_table = driver_deny_table_;
    soc_count = file_header_->soc_deny_count;
    soc_table = soc_deny_table_;
  } else {
    return kFileMatch_None;
  }

  for (uint32_t soc_index = 0; soc_index < soc_count; ++soc_index) {
    const char *soc_string = GetString(soc_table[soc_index].soc_string_index);
    if (strcasecmp(soc_string, device_info.soc.c_str()) == 0) {
      const uint32_t fingerprint_offset = soc_table[soc_index].soc_fingerprint_offset;
      const uint32_t fingerprint_count = soc_table[soc_index].soc_fingerprint_count;
      for (uint32_t driver_index = fingerprint_offset;
          driver_index < (fingerprint_offset + fingerprint_count); ++driver_index) {
        const char *fingerprint_string =
            GetString(driver_table[driver_index].driver_version_string_index);
        if (strcmp(fingerprint_string, device_info.gles_version.c_str()) == 0) {
          return match_result;
        }
      }
      return kFileMatch_None;
    }
  }

  return kFileMatch_None;
}

VkQualityPredictionFile::FileMatchResult VkQualityPredictionFile::SearchGpuLists(
    const DeviceInfo &device_info) {

  FileMatchResult result = SearchGpuList(device_info, kFileMatch_GpuAllow);
  if (result == kFileMatch_None) {
    result = SearchGpuList(device_info, kFileMatch_GpuDeny);
  }
  return result;
}

VkQualityPredictionFile::FileMatchResult VkQualityPredictionFile::SearchGpuList(
    const DeviceInfo &device_info, const FileMatchResult match_result) {

  const VkQualityGpuPredictEntry *gpu_table;
  uint32_t table_count;
  if (match_result == kFileMatch_GpuAllow) {
    gpu_table = gpu_allow_table_;
    table_count = file_header_->gpu_allow_predict_count;
  } else if (match_result == kFileMatch_GpuDeny) {
    gpu_table = gpu_deny_table_;
    table_count = file_header_->gpu_deny_predict_count;
  } else {
    return kFileMatch_None;
  }

  for (uint32_t i = 0; i < table_count; ++i) {
    const char *device_string = GetString(gpu_table[i].device_name_string_index);
    std::string_view device_view(device_string);
    FileMatchResult result = VkQualityMatching::CheckGpuMatch(device_info,
                                                              device_string,
                                                              gpu_table[i].device_id,
                                                              gpu_table[i].vendor_id,
                                                              gpu_table[i].min_api_version,
                                                              gpu_table[i].min_driver_version,
                                                              match_result);
    if (result == match_result) {
      return result;
    }
  }

  return kFileMatch_None;
}

const char *VkQualityPredictionFile::GetString(const uint32_t string_index) {
  // Bounds check both the string index and the actual string data, return
  // a placeholder null string if either end up out of bounds
  if (string_index >= file_header_->string_table_count) {
    return &kNullString;
  }
  uint32_t string_offset_start = string_offset_table_[string_index];
  if (string_offset_start > total_file_size_) {
    return &kNullString;
  }
  const size_t max_possible_length = total_file_size_ - string_offset_start;
  const char *string_base = reinterpret_cast<const char *>(file_header_) + string_offset_start;
  const size_t string_length = strnlen(string_base, max_possible_length);
  if (string_length == max_possible_length) {
    return &kNullString;
  }
  return string_base;
}

}