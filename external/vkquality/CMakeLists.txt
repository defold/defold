cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(functions)
include(functions_sdk)
include(sdk)

project(vkquality LANGUAGES CXX)

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

if(NOT TARGET_PLATFORM MATCHES "^(armv7|arm64|x86_64)-android$")
  message(FATAL_ERROR "VkQuality is only supported on Android platforms, got: ${TARGET_PLATFORM}")
endif()

set(VKQUALITY_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src/main")
set(VKQUALITY_CPP_DIR "${VKQUALITY_SRC_DIR}/cpp")
set(VKQUALITY_ASSETS_DIR "${VKQUALITY_SRC_DIR}/assets")

add_library(vkquality SHARED
  "${VKQUALITY_CPP_DIR}/gles_util.cpp"
  "${VKQUALITY_CPP_DIR}/vkquality_c.cpp"
  "${VKQUALITY_CPP_DIR}/vkquality_manager.cpp"
  "${VKQUALITY_CPP_DIR}/vkquality_matching.cpp"
  "${VKQUALITY_CPP_DIR}/vkquality_prediction_file.cpp"
  "${VKQUALITY_CPP_DIR}/vulkan_util.cpp")

target_include_directories(vkquality PRIVATE "${VKQUALITY_CPP_DIR}")
target_compile_features(vkquality PRIVATE cxx_std_17)
target_compile_options(vkquality PRIVATE -Os -Wall)
target_link_options(vkquality PRIVATE -Wl,-z,max-page-size=16384)
target_link_libraries(vkquality PRIVATE android EGL GLESv3 log dl)

install(TARGETS vkquality
        LIBRARY DESTINATION "lib/${TARGET_PLATFORM}")

install(FILES
  "${VKQUALITY_CPP_DIR}/vkquality.h"
  "${VKQUALITY_CPP_DIR}/vkquality_file_format.h"
  DESTINATION include/vkquality)
install(FILES "${VKQUALITY_ASSETS_DIR}/vkqualitydata.vkq" DESTINATION share/vkquality/assets)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/LICENSE" DESTINATION share/licenses/vkquality)
