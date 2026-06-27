cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(functions)
include(functions_sdk)
include(sdk)

project(vkquality LANGUAGES CXX)

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

if(NOT TARGET_PLATFORM MATCHES "^(armv7|arm64)-android$")
  message(FATAL_ERROR "VkQuality is only supported on Android platforms, got: ${TARGET_PLATFORM}")
endif()

set(VKQUALITY_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src/main")
set(VKQUALITY_CPP_DIR "${VKQUALITY_SRC_DIR}/cpp")
set(VKQUALITY_JAVA_DIR "${VKQUALITY_SRC_DIR}/java")
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

find_package(Java COMPONENTS Development REQUIRED)

if(NOT DEFOLD_ANDROID_JAR)
  message(FATAL_ERROR "DEFOLD_ANDROID_JAR must point to android.jar")
endif()

set(VKQUALITY_JAVA_SOURCES
  "${VKQUALITY_JAVA_DIR}/com/google/android/games/vkquality/DeviceMitigationRecord.java"
  "${VKQUALITY_JAVA_DIR}/com/google/android/games/vkquality/SecurityPatchDate.java"
  "${VKQUALITY_JAVA_DIR}/com/google/android/games/vkquality/StartupMitigation.java"
  "${VKQUALITY_JAVA_DIR}/com/google/android/games/vkquality/StartupMitigationRecord.java"
  "${VKQUALITY_JAVA_DIR}/com/google/android/games/vkquality/VKQuality.java")

set(VKQUALITY_CLASSES_DIR "${CMAKE_CURRENT_BINARY_DIR}/java_classes")
set(VKQUALITY_JAR "${CMAKE_CURRENT_BINARY_DIR}/vkquality.jar")

add_custom_command(
  OUTPUT "${VKQUALITY_JAR}"
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${VKQUALITY_CLASSES_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${VKQUALITY_CLASSES_DIR}"
  COMMAND "${Java_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
          -classpath "${DEFOLD_ANDROID_JAR}"
          -d "${VKQUALITY_CLASSES_DIR}"
          ${VKQUALITY_JAVA_SOURCES}
  COMMAND "${Java_JAR_EXECUTABLE}" cf "${VKQUALITY_JAR}" -C "${VKQUALITY_CLASSES_DIR}" .
  DEPENDS ${VKQUALITY_JAVA_SOURCES}
  COMMENT "Packaging vkquality.jar"
  VERBATIM)

add_custom_target(vkquality_jar ALL DEPENDS "${VKQUALITY_JAR}")

install(FILES "${VKQUALITY_JAR}" DESTINATION share/java)
install(FILES "${VKQUALITY_ASSETS_DIR}/vkqualitydata.vkq" DESTINATION share/vkquality/assets)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/LICENSE" DESTINATION share/licenses/vkquality)
