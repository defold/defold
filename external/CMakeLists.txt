cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../scripts/cmake")
include(defold)

project(defold_ext LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)

add_subdirectory(bullet3d)
add_subdirectory(basisu)
add_subdirectory(lz4)

# Model importer codecs are only used by desktop build tools and the editor.
if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86_64-macos|arm64-macos)$")
  add_subdirectory(meshoptimizer)
  add_subdirectory(draco)
endif()

# Always build GLFW from source for iOS; these targets have no prebuilt package.
if(TARGET_PLATFORM MATCHES "^(arm64-ios|arm64_sim-ios)$")
  add_subdirectory(glfw)
endif()
