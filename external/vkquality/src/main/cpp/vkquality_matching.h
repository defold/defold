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

#ifndef VKQUALITY_MATCHING_H_
#define VKQUALITY_MATCHING_H_

#include "vkquality_prediction_file.h"
#include <string_view>

namespace vkquality {

class VkQualityMatching {
public:
  enum StringMatchResult {
    kStringMatch_None = 0,
    kStringMatch_Exact,
    kStringMatch_Substring_Start,
    kStringMatch_Substring
  };

  static size_t CountWildcards(const std::string_view &str, size_t *string_length, size_t *offset_array);

  static StringMatchResult WildcardsMatch(
      const std::string_view &a, const std::string_view &b,
      const size_t wildcard_count, const size_t wildcard_length,
      const size_t *wildcard_offsets);

  static StringMatchResult StringMatches(const std::string_view &a, const std::string_view &b);

  static VkQualityPredictionFile::FileMatchResult CheckDeviceMatch(
      const DeviceInfo &device_info,
      const std::string_view &brand,
      const std::string_view &device,
      const uint32_t min_api,
      const uint32_t min_driver);

  static VkQualityPredictionFile::FileMatchResult CheckGpuMatch(
      const DeviceInfo &device_info,
      const std::string_view &device,
      const uint32_t device_id,
      const uint32_t vendor_id,
      const uint32_t min_api,
      const uint32_t min_driver,
      const VkQualityPredictionFile::FileMatchResult match_result);
};

}
#endif //VKQUALITY_MATCHING_H_
