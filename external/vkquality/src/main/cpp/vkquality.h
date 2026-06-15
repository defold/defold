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

#ifndef VKQUALITY_H_
#define VKQUALITY_H_

#include <android/asset_manager.h>
#include <cstdint>
#include <jni.h>

/**
 * @brief Flag bitfields that cam be passed to ::vkQuality_initializeFlags
 * in the flags parameter.
 */
 enum vkQualityInitFlags : int32_t {
     /**
      * @brief Disable the quality check against the SoC/driver fingerprint additional
      * allow/deny list introduced in version 1.2
      */
     kInitFlagSkipFingerprintRecommendationCheck = (1 << 2)
 };

/**
 * @brief Result codes returned by ::vkQuality_initialize.
 */
enum vkQualityInitResult : int32_t {
  /**
   * @brief VKQuality initialization was successful.
   */
  kSuccess = 0,
  /**
   * @brief VkQuality failed to initialize, unspecified reason
   */
  kErrorInitializationFailure = -1,
  /**
   * @brief VkQuality failed to initialize, Vulkan was either
   * not available on the device or couldn't be initialized
   */
  kErrorNoVulkan = -2,
  /**
   * @brief VkQuality failed to initialize, specified quality
   * data file was an incompatible version
   */
  kErrorInvalidDataVersion = -3,
  /**
   * @brief VkQuality failed to initialize, specified quality
   * data file was invalid
   */
  kErrorInvalidDataFile = -4,
  /**
   * @brief VkQuality failed to initialize, specified quality
   * data file could not be found in the app bundle or
   * in the storage directory
   */
  kErrorMissingDataFile = -5
};

/**
 * @brief API recommendation returned by ::vkQuality_getRecommendation.
 */
enum vkQualityRecommendation : int32_t {
  /**
   * @brief A recommendation is not yet ready, call ::vkQuality_getRecommendation
   * again after a brief interval.
   */
  kRecommendationNotReady = -2,
  /**
   * @brief VkQuality is not initialized. Either ::vkQuality_initialize was
   * not called or it returned a failure code.
   */
  kRecommendationErrorNotInitialized = -1,
  /**
   * @brief Recommend using the Vulkan API. Reason is a device match
   * was found in the device allow list
   */
  kRecommendationVulkanBecauseDeviceMatch = 0,
  /**
   * @brief Recommend using the Vulkan API. Reason is a GPU/driver match
   * was found in the predicted quality allow list
   */
  kRecommendationVulkanBecausePredictionMatch,
  /**
   * @brief Recommend using the Vulkan API. Reason is device is running
   * on a higher version of Android beyond what is covered by this
   * release of VkQuality
   */
  kRecommendationVulkanBecauseFutureAndroid,
  /**
   * @brief Recommend using the OpenGL ES API. Reason is the device
   * is running on a version of Android lower than 10, or only
   * supports Vulkan 1.0.x
   */
  kRecommendationGLESBecauseOldDevice,
  /**
   * @brief Recommend using the GLES API. Reason is a device match
   * was found in the device allow list, but its Vulkan driver version
   * was below a specified minimum version number
   */
  kRecommendationGLESBecauseOldDriver,
  /**
   * @brief Recommend using the OpenGL ES API. Reason is no matches
   * were found in the Vulkan device allow list or Vulkan predicted
   * quality list
   */
  kRecommendationGLESBecauseNoDeviceMatch,
  /**
   * @brief Recommend using the OpenGL ES API. Reason is a GPU/driver match
   * was found in the predicted quality deny list
   */
  kRecommendationGLESBecausePredictionMatch
};

/**
 * @brief Struct used to optionally pass graphics API information to
 * ::vkQuality_initializeFlagsInfo so the library can use it for
 * making a recommendation without having to stand up a graphics API
 * instance to query. Pointers in the struct may be null and are
 * only expected to be valid until return from ::vkQuality_initializeFlagsInfo
 */
typedef struct vkqGraphicsAPIInfo {
  /**
   * @brief If non-null, expected to be a pointer containing the contents
   * of calling glGetString with the GL_VERSION enum. If a string is provided,
   * the library will not create a GL instance and instead use the string.
   */
   const char *gles_version_string;

  /**
   * @brief If non-null, expected to be a pointer to a valid 
   * `vkGetPhysicalDeviceProperties` structure. If provided, the library
   * will use the data in this structure instead of creating a Vulkan
   * instance to retrieve it.
   */
   void *vk_physical_device_properties;
} vkqGraphicsAPIInfo;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize VkQuality, constructing internal resources.
 * @param env The JNIEnv attached to the thread calling the function.
 * @param asset_manager A pointer to an active NDK Asset Manager instance.
 * Passing nullptr will disable lookup of the quality data file from the
 * app bundle.
 * @param storage_path An absolute path to a storage directory. This
 * directory is assumed to exist and be writable using standard file i/o.
 * A recommendation cache file will be written in this directory. VkQuality
 * will look for the quality data file in this directory before looking
 * in the application bundle. Passing nullptr will disable recommendation caching
 * and quality data file lookup outside the application bundle.
 * @param asset_filename The name of the quality data file. This can be a partial
 * path, but must exist in either the app bundle assets, or in the directory
 * referenced by `storage_path`
 * @return `kSuccess` if successful, otherwise an error code relating
 * to initialization failure.
 * @see vkQuality_destroy
 */
vkQualityInitResult vkQuality_initialize(JNIEnv *env, AAssetManager *asset_manager,
                                         const char *storage_path,
                                         const char *asset_filename);

/**
 * @brief Initialize VkQuality, constructing internal resources.
 * @param env The JNIEnv attached to the thread calling the function.
 * @param asset_manager A pointer to an active NDK Asset Manager instance.
 * Passing nullptr will disable lookup of the quality data file from the
 * app bundle.
 * @param storage_path An absolute path to a storage directory. This
 * directory is assumed to exist and be writable using standard file i/o.
 * A recommendation cache file will be written in this directory. VkQuality
 * will look for the quality data file in this directory before looking
 * in the application bundle. Passing nullptr will disable recommendation caching
 * and quality data file lookup outside the application bundle.
 * @param asset_filename The name of the quality data file. This can be a partial
 * path, but must exist in either the app bundle assets, or in the directory
 * referenced by `storage_path`
 * @param flags A bit field of ::vkQualityInitFlags enum values specifying
 * initialization flags to alter default behavior
 * @return `kSuccess` if successful, otherwise an error code relating
 * to initialization failure.
 * @see vkQuality_destroy
 */
vkQualityInitResult vkQuality_initializeFlags(JNIEnv *env, AAssetManager *asset_manager,
                                         const char *storage_path,
                                         const char *asset_filename,
                                         int32_t flags);

/**
 * @brief Initialize VkQuality, constructing internal resources.
 * @param env The JNIEnv attached to the thread calling the function.
 * @param asset_manager A pointer to an active NDK Asset Manager instance.
 * Passing nullptr will disable lookup of the quality data file from the
 * app bundle.
 * @param storage_path An absolute path to a storage directory. This
 * directory is assumed to exist and be writable using standard file i/o.
 * A recommendation cache file will be written in this directory. VkQuality
 * will look for the quality data file in this directory before looking
 * in the application bundle. Passing nullptr will disable recommendation caching
 * and quality data file lookup outside the application bundle.
 * @param asset_filename The name of the quality data file. This can be a partial
 * path, but must exist in either the app bundle assets, or in the directory
 * referenced by `storage_path`
 * @param api_info An optional pointer to a ::GraphicsAPIInfo structure that
 * can be used to pass API driver/version information to the library. If this
 * data is provided for a given API, VkQuality will skip initializing the API to
 * retrieve API information itself and use the provided values for recommendations.
 * Pointer lifetime is only expected to be valid until function return.
 * @param flags A bit field of ::vkQualityInitFlags enum values specifying
 * initialization flags to alter default behavior
 * @return `kSuccess` if successful, otherwise an error code relating
 * to initialization failure.
 * @see vkQuality_destroy
 */
 vkQualityInitResult vkQuality_initializeFlagsInfo(JNIEnv *env, AAssetManager *asset_manager,
                                         const char *storage_path,
                                         const char *asset_filename,
                                         const vkqGraphicsAPIInfo *api_info,
                                         int32_t flags);                                        

/**
 * @brief Destroy resources that VkQuality has created.
 * @param env The JNIEnv attached to the thread calling the function.
 * @see vkQuality_initialize
 */
void vkQuality_destroy(JNIEnv *env);

/**
 * @brief Retrieve a graphics API recommendation for the running device
 * @return An recommendation defined by the ::vkQualityRecommendation enum
 */
vkQualityRecommendation vkQuality_getRecommendation();

#ifdef __cplusplus
}
#endif

#endif // VKQUALITY_H_
