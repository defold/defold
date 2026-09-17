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

#ifndef VKQUALITY_DEVICE_INFO_H_
#define VKQUALITY_DEVICE_INFO_H_

#include <cstdint>
#include <string>

namespace vkquality {

static constexpr uint32_t kWildcardValue = 0;

struct DeviceInfo {
  std::string brand;
  std::string device;
  std::string soc;
  std::string vk_device_name;
  std::string gles_renderer;
  std::string gles_vendor;
  std::string gles_version;
  int32_t api_level = kWildcardValue;
  int32_t vk_deqp_level = kWildcardValue;
  uint32_t vk_api_version = kWildcardValue;
  uint32_t vk_device_id = kWildcardValue;
  uint32_t vk_driver_version = kWildcardValue;
  uint32_t vk_vendor_id = kWildcardValue;
};

}

#endif //VKQUALITYAAR_VKQUALITY_DEVICE_INFO_H
