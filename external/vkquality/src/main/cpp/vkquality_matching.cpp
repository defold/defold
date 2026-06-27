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

#include "vkquality_matching.h"
#include <string.h>
#include <vector>

namespace vkquality {

static constexpr size_t kMaxWildcards = 4;

size_t VkQualityMatching::CountWildcards(const std::string_view &str, size_t *string_length,
                                         size_t *offset_array) {
  size_t offset = 0;
  size_t wildcard_count = 0;
  const size_t str_len = str.length();
  while (offset < str_len) {
    if (str[offset] == '*' && wildcard_count < kMaxWildcards) {
      offset_array[wildcard_count++] = offset;
    }
    ++offset;
  }
  *string_length = offset;
  return wildcard_count;
}

VkQualityMatching::StringMatchResult VkQualityMatching::WildcardsMatch(
    const std::string_view &a, const std::string_view &b,
    const size_t wildcard_count, const size_t wildcard_length,
    const size_t *wildcard_offsets) {
  size_t pos = 0;

  // If the compare string doesn't start with a wildcard, the input string must start with
  // the prefix chars before the first '*' in the compare string
  if (b[0] != '*') {
    const size_t first_star_offset = wildcard_offsets[0];
    std::string_view prefix = b.substr(0, first_star_offset);
    if (a.length() < prefix.length() || a.substr(0, prefix.length()) != prefix) {
      return VkQualityMatching::kStringMatch_None;
    }
    pos = prefix.length();
  }

  // Process each substring in turn
  for (size_t i = 0; i < wildcard_count; ++i) {
    const size_t current_star = wildcard_offsets[i];
    size_t segment_len = 0;
    if (i + 1 < wildcard_count) {
      segment_len = wildcard_offsets[i + 1] - (current_star + 1);
    } else {
      segment_len = wildcard_length - (current_star + 1);
    }

    std::string_view segment = b.substr(current_star + 1, segment_len);
    if (!segment.empty()) {
      size_t found = a.find(segment, pos);
      if (found == std::string_view::npos) {
        return VkQualityMatching::kStringMatch_None;
      }
      pos = found + segment.length();
    }
  }

  return VkQualityMatching::kStringMatch_Substring;
}

VkQualityMatching::StringMatchResult VkQualityMatching::StringMatches(
    const std::string_view &a, const std::string_view &b) {
  const size_t a_length = a.length();
  const size_t b_length = b.length();
  if (a_length == 0 || b_length == 0) {
    return kStringMatch_None;
  }

  // Wildcard support is basic:
  // ^foo = match if A starts with 'foo'
  // *foo = match if A has 'foo'
  // More advanced * wildcard:
  // Foo*bar = match if A starts with 'Foo' contains 'bar'
  // Foo*bar*baz = match if A starts with 'Foo' contains 'bar' and 'baz'
  // *bar*baz = match if A contains 'bar' and 'baz'
  size_t wildcard_length = 0;
  size_t wildcard_offsets[kMaxWildcards];
  size_t wildcard_count = CountWildcards(b, &wildcard_length, wildcard_offsets);
  if (wildcard_count > 1 || (wildcard_count == 1 && b[0] != '*')) {
    return WildcardsMatch(a, b, wildcard_count, wildcard_length, wildcard_offsets);
  }

  if (b[0] == '^' && b_length > 1) {
    // Substring match at start of string
    std::string_view pattern = b.substr(1);
    if (a.length() >= pattern.length() && a.substr(0, pattern.length()) == pattern) {
      return kStringMatch_Substring_Start;
    }
  } else if (b[0] == '*' && b_length > 1) {
    // Substring match anywhere in string
    std::string_view pattern = b.substr(1);
    if (a.find(pattern) != std::string_view::npos) {
      return kStringMatch_Substring;
    }
  } else {
    // Exact match
    if (a == b) {
      return kStringMatch_Exact;
    }
  }

  return kStringMatch_None;
}

VkQualityPredictionFile::FileMatchResult VkQualityMatching::CheckDeviceMatch(
    const DeviceInfo &device_info,
    const std::string_view &brand,
    const std::string_view &device,
    const uint32_t min_api,
    const uint32_t min_driver) {
  VkQualityPredictionFile::FileMatchResult result = VkQualityPredictionFile::kFileMatch_None;
  bool version_too_old = false;

  // Must at least have a brand string
  if (brand.empty()) {
    return VkQualityPredictionFile::kFileMatch_None;
  }

  if (min_api > 0 && device_info.api_level < min_api) {
    version_too_old = true;
  }

  if (min_driver > 0 && device_info.vk_driver_version < min_driver) {
    version_too_old = true;
  }

  if (device.empty()) {
    std::string_view brand_view(device_info.brand);
    if (brand_view == brand && !version_too_old) {
      return VkQualityPredictionFile::kFileMatch_BrandWildcard;
    }
  } else {
    std::string_view brand_view(device_info.brand);
    std::string_view device_view(device_info.device);
    if (brand_view == brand && device_view == device) {
      if (version_too_old) {
       return VkQualityPredictionFile::kFileMatch_DeviceOldVersion;
      } else {
        return VkQualityPredictionFile::kFileMatch_ExactDevice;
      }
    }
  }

  return result;
}

VkQualityPredictionFile::FileMatchResult VkQualityMatching::CheckGpuMatch(
    const DeviceInfo &device_info,
    const std::string_view &device,
    const uint32_t device_id,
    const uint32_t vendor_id,
    const uint32_t min_api,
    const uint32_t min_driver,
    const VkQualityPredictionFile::FileMatchResult match_result) {

  // Require a device name string, or an explicit device/vendor id combo
  if ((device_id == 0 || vendor_id == 0) && device.empty()) {
    return VkQualityPredictionFile::kFileMatch_None;
  }

  if (match_result == VkQualityPredictionFile::kFileMatch_GpuAllow) {
    if (min_driver > 0 && device_info.vk_driver_version < min_driver) {
      return VkQualityPredictionFile::kFileMatch_None;
    }
    if (min_api > 0 && device_info.api_level < min_api) {
      return VkQualityPredictionFile::kFileMatch_None;
    }
  } else {
    if (min_driver > 0 && device_info.vk_driver_version > min_driver) {
      return VkQualityPredictionFile::kFileMatch_None;
    }
    if (min_api > 0 && device_info.api_level > min_api) {
      return VkQualityPredictionFile::kFileMatch_None;
    }
  }

  if (device_id == device_info.vk_device_id &&
      vendor_id == device_info.vk_vendor_id) {
    return match_result;
  }

  if (VkQualityMatching::StringMatches(device_info.vk_device_name, device) !=
      VkQualityMatching::kStringMatch_None) {
    return match_result;
  }

  return VkQualityPredictionFile::kFileMatch_None;
}

}