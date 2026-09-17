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

#ifndef VKQUALITY_VULKAN_UTIL_H_
#define VKQUALITY_VULKAN_UTIL_H_

#include "vkquality_manager.h"

namespace vkquality {

class VulkanUtil {
 public:
  static vkQualityInitResult CopyDeviceVulkanInfo(DeviceInfo &device_info,
      void *vk_physical_device_properties);
  static vkQualityInitResult GetDeviceVulkanInfo(DeviceInfo &device_info);
  static uint32_t GetMinimumRecommendedVulkanVersion();
  static uint32_t GetVulkanApiVersionForApiLevel(const int device_api_level);
  static int GetFutureApiLevelRecommendation();

 private:
  // Minimum Android API levels for Vulkan 1.3/1.1 version support
  static constexpr int kMinimum_vk13_api_level = 33;
  static constexpr int kMinimum_vk11_api_level = 29;
  // Android API level to always recommend Vulkan even if no allowlist
  // or predict allowlist match
  static constexpr int kMinimum_vk_always_api_level = 35;
};

} // namespace vkquality

#endif // VKQUALITY_VULKAN_UTIL_H_
