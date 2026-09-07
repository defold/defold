cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(functions)
include(functions_sdk)
include(sdk)

if(TARGET_PLATFORM MATCHES "^(arm64|x86_64)-(macos|ios)$")
  string(REGEX REPLACE "-.*$" "" SKRIBIDI_APPLE_ARCH "${TARGET_PLATFORM}")
  set(CMAKE_OSX_ARCHITECTURES "${SKRIBIDI_APPLE_ARCH}" CACHE STRING "Skribidi Apple target architecture" FORCE)
endif()

project(skribidi LANGUAGES C)

option(SKRIBIDI_BASELINE_TESTS "Build the standalone Skribidi correctness and performance baseline" OFF)

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

if(NOT DEFOLD_EXTERNAL_PLATFORM)
  set(DEFOLD_EXTERNAL_PLATFORM "${TARGET_PLATFORM}")
endif()

set(SKRIBIDI_ROOT "${CMAKE_CURRENT_LIST_DIR}/package/Skribidi")
set(SKRIBIDI_SRC_DIR "${SKRIBIDI_ROOT}/src")
set(SKRIBIDI_INCLUDE_DIR "${SKRIBIDI_ROOT}/include")
set(BUDOUX_ROOT "${CMAKE_CURRENT_LIST_DIR}/package/budouxc-main")
file(GLOB SKRIBIDI_HEADERS CONFIGURE_DEPENDS "${SKRIBIDI_INCLUDE_DIR}/*.h")

add_library(skribidi STATIC
  "${SKRIBIDI_SRC_DIR}/skb_attribute_collection.c"
  "${SKRIBIDI_SRC_DIR}/skb_attributes.c"
  "${SKRIBIDI_SRC_DIR}/skb_common.c"
  "${SKRIBIDI_SRC_DIR}/skb_editor_rules.c"
  "${SKRIBIDI_SRC_DIR}/skb_font_collection.c"
  "${SKRIBIDI_SRC_DIR}/skb_icon_collection.c"
  "${SKRIBIDI_SRC_DIR}/skb_layout.c"
  "${SKRIBIDI_SRC_DIR}/skb_layout_cache.c"
  "${SKRIBIDI_SRC_DIR}/skb_rich_layout.c"
  "${SKRIBIDI_SRC_DIR}/skb_rich_text.c"
  "${SKRIBIDI_SRC_DIR}/skb_text.c"
  "${BUDOUX_ROOT}/src/budoux.c")

target_compile_features(skribidi PRIVATE c_std_17)
target_compile_definitions(skribidi PRIVATE NDEBUG SKB_NO_OPEN)
target_include_directories(skribidi PRIVATE
  "${SKRIBIDI_SRC_DIR}"
  "${SKRIBIDI_INCLUDE_DIR}"
  "${BUDOUX_ROOT}/include"
  "${DEFOLD_SDK_ROOT}/ext/include"
  "${DEFOLD_SDK_ROOT}/ext/include/harfbuzz"
  "${DEFOLD_SDK_ROOT}/ext/include/libunibreak")

if(MSVC)
  set_property(TARGET skribidi PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(skribidi PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX "lib")

install(TARGETS skribidi
  ARCHIVE DESTINATION "lib/${DEFOLD_EXTERNAL_PLATFORM}")

install(FILES ${SKRIBIDI_HEADERS}
  DESTINATION include/skribidi)

if(SKRIBIDI_BASELINE_TESTS)
  enable_testing()

  add_executable(skribidi_baseline
    "${CMAKE_CURRENT_LIST_DIR}/test/skribidi_baseline.c")
  target_compile_features(skribidi_baseline PRIVATE c_std_17)
  target_compile_definitions(skribidi_baseline PRIVATE
    "SKRIBIDI_BASELINE_DATA_DIR=\"${SKRIBIDI_ROOT}/example/data\""
    "SKRIBIDI_BASELINE_CORPUS_PATH=\"${CMAKE_CURRENT_LIST_DIR}/test/data/a_tale_of_two_cities_32k.txt\""
    "SKRIBIDI_BASELINE_PLATFORM=\"${DEFOLD_EXTERNAL_PLATFORM}\"")
  target_include_directories(skribidi_baseline PRIVATE "${SKRIBIDI_INCLUDE_DIR}")
  target_link_directories(skribidi_baseline PRIVATE
    "${DEFOLD_SDK_ROOT}/ext/lib/${DEFOLD_EXTERNAL_PLATFORM}")
  if(MSVC)
    target_compile_options(skribidi_baseline PRIVATE /W4 /WX)
  else()
    target_compile_options(skribidi_baseline PRIVATE -Wall -Wextra -Werror)
  endif()
  target_link_libraries(skribidi_baseline PRIVATE skribidi)
  defold_target_link_libraries(skribidi_baseline "${TARGET_PLATFORM}" SCOPE PRIVATE
    harfbuzz
    sheenbidi
    unibreak)
  if(NOT WIN32)
    target_link_libraries(skribidi_baseline PRIVATE m)
  endif()

  add_test(NAME skribidi_baseline_verify
    COMMAND skribidi_baseline --verify)
endif()
