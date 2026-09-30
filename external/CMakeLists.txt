cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../scripts/cmake")
include(defold)

project(defold_ext LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)

add_subdirectory(bullet3d)
add_subdirectory(basisu)
add_subdirectory(lz4)

# Always build GLFW from source for iOS and Android; these targets have no prebuilt package.
if(TARGET_PLATFORM MATCHES "^(arm64-ios|arm64_sim-ios|armv7-android|arm64-android|x86_64-android)$")
  add_subdirectory(glfw)
endif()
