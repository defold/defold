cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../scripts/cmake")
# These dependencies build protoc itself and do not generate code with it.
set(DEFOLD_REQUIRE_PROTOC OFF)
include(defold)

project(defold_ext LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)

add_subdirectory(bullet3d)
add_subdirectory(basisu)
add_subdirectory(lz4)
if(NOT TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  add_subdirectory(luajit)
endif()
add_subdirectory(box2d)
add_subdirectory(box2d_v2)
add_subdirectory(harfbuzz)
add_subdirectory(libunibreak)
add_subdirectory(opus)
add_subdirectory(sheenbidi)
add_subdirectory(skribidi)

# Protobuf is used by desktop tools, including the host tools in cross-builds.
if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-linux|x86_64-linux|x86_64-win32)$")
  add_subdirectory(protobuf)
endif()
