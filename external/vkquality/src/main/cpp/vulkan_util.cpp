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

#include "vulkan_util.h"
#include <dlfcn.h>
#include <vector>
#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace vkquality {

uint32_t VulkanUtil::GetVulkanApiVersionForApiLevel(const int device_api_level) {
  if (device_api_level >= kMinimum_vk13_api_level) {
    return VK_API_VERSION_1_3;
  } else if (device_api_level >= kMinimum_vk11_api_level) {
    return VK_API_VERSION_1_1;
  }
  return VK_API_VERSION_1_0;
}

vkQualityInitResult VulkanUtil::CopyDeviceVulkanInfo(DeviceInfo &device_info,
    void *vk_physical_device_properties) {
    if (vk_physical_device_properties == nullptr) {
      return kErrorNoVulkan;
    }
    const VkPhysicalDeviceProperties &device_properties =
      *(reinterpret_cast<const VkPhysicalDeviceProperties *>(
        vk_physical_device_properties));
    device_info.vk_api_version = device_properties.apiVersion;
    device_info.vk_driver_version = device_properties.driverVersion;
    device_info.vk_device_id = device_properties.deviceID;
    device_info.vk_vendor_id = device_properties.vendorID;
    device_info.vk_device_name = device_properties.deviceName;
    return kSuccess;
}

vkQualityInitResult VulkanUtil::GetDeviceVulkanInfo(DeviceInfo &device_info) {
  void *lib_vulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  if (lib_vulkan == nullptr) {
    return kErrorNoVulkan;
  }

  auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
      dlsym(lib_vulkan, "vkCreateInstance"));
  if (vkCreateInstance == nullptr) {
    dlclose(lib_vulkan);
    return kErrorNoVulkan;
  }
  auto vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
      dlsym(lib_vulkan, "vkDestroyInstance"));
  if (vkDestroyInstance == nullptr) {
    dlclose(lib_vulkan);
    return kErrorNoVulkan;
  }
  auto vkEnumeratePhysicalDevices =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          dlsym(lib_vulkan, "vkEnumeratePhysicalDevices"));
  if (vkEnumeratePhysicalDevices == nullptr) {
    dlclose(lib_vulkan);
    return kErrorNoVulkan;
  }
  auto vkGetPhysicalDeviceQueueFamilyProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          dlsym(lib_vulkan, "vkGetPhysicalDeviceQueueFamilyProperties"));
  if (vkGetPhysicalDeviceQueueFamilyProperties == nullptr) {
    dlclose(lib_vulkan);
    return kErrorNoVulkan;
  }
  auto vkGetPhysicalDeviceProperties =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
          dlsym(lib_vulkan, "vkGetPhysicalDeviceProperties"));
  if (vkGetPhysicalDeviceProperties == nullptr) {
    dlclose(lib_vulkan);
    return kErrorNoVulkan;
  }

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "vkQuality";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "AGDK";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VulkanUtil::GetVulkanApiVersionForApiLevel(device_info.api_level);

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = 0;
  create_info.ppEnabledExtensionNames = nullptr;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledLayerCount = 0;
  create_info.ppEnabledLayerNames = nullptr;
  create_info.pNext = nullptr;

  VkInstance vk_instance;
  VkResult result = vkCreateInstance(&create_info, nullptr, &vk_instance);
  if (result != VK_SUCCESS) {
    dlclose(lib_vulkan);
    return kErrorNoVulkan;
  }

  uint32_t device_count = 0;
  result = vkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr);
  if (result != VK_SUCCESS || device_count == 0) {
    vkDestroyInstance(vk_instance, nullptr);
    dlclose(lib_vulkan);
    return kErrorNoVulkan;
  }
  std::vector<VkPhysicalDevice> physical_devices(device_count);
  vkEnumeratePhysicalDevices(vk_instance, &device_count, physical_devices.data());

  bool found_graphics_device = false;
  for (VkPhysicalDevice physical_device : physical_devices) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                             queue_families.data());

    for (const auto &queue_family : queue_families) {
      if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        found_graphics_device = true;
        break;
      }
    }
    if (found_graphics_device) {
      VkPhysicalDeviceProperties device_properties{};
      vkGetPhysicalDeviceProperties(physical_device, &device_properties);
      device_info.vk_api_version = device_properties.apiVersion;
      device_info.vk_driver_version = device_properties.driverVersion;
      device_info.vk_device_id = device_properties.deviceID;
      device_info.vk_vendor_id = device_properties.vendorID;
      device_info.vk_device_name = device_properties.deviceName;
      break;
    }
  }
  vkDestroyInstance(vk_instance, nullptr);

  dlclose(lib_vulkan);
  if (!found_graphics_device) {
    return kErrorNoVulkan;
  }
  return kSuccess;
}

int VulkanUtil::GetFutureApiLevelRecommendation() {
  return kMinimum_vk_always_api_level;
}

uint32_t VulkanUtil::GetMinimumRecommendedVulkanVersion() {
  return VK_API_VERSION_1_1;
}

} // namespace vkquality