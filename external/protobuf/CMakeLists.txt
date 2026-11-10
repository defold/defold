cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
set(DEFOLD_REQUIRE_PROTOC OFF)
include(defold)

project(protobuf_external VERSION 35.1 LANGUAGES C CXX)

if(NOT TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-linux|x86_64-linux|x86_64-win32)$")
  message(FATAL_ERROR "Unsupported Protobuf platform: ${TARGET_PLATFORM}")
endif()

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

find_program(PATCH_EXECUTABLE patch REQUIRED)

# Re-extract only when the bundled archive or its patches change. Register these
# inputs with CMake so Ninja also notices changes without an explicit configure.
function(protobuf_extract name)
  set(archive "${CMAKE_CURRENT_SOURCE_DIR}/${name}.tar.gz")
  set(inputs "${archive}" ${ARGN})
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${inputs})
  set(signature "")
  foreach(input IN LISTS inputs)
    file(SHA256 "${input}" hash)
    string(APPEND signature "${hash}\n")
  endforeach()
  set(source "${CMAKE_CURRENT_BINARY_DIR}/sources/${name}")
  set(stamp "${source}/.defold-source")
  set(previous "")
  if(EXISTS "${stamp}")
    file(READ "${stamp}" previous)
  endif()
  if(NOT signature STREQUAL previous)
    file(REMOVE_RECURSE "${source}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/sources" TOUCH)
    foreach(patch IN LISTS ARGN)
      execute_process(COMMAND "${PATCH_EXECUTABLE}" -p1 -s -f -i "${patch}"
        WORKING_DIRECTORY "${source}" COMMAND_ERROR_IS_FATAL ANY)
    endforeach()
    file(WRITE "${stamp}" "${signature}")
  endif()
endfunction()

protobuf_extract(abseil-cpp-20250512.1)
protobuf_extract(protobuf-35.1
  "${CMAKE_CURRENT_SOURCE_DIR}/java-parser-compat.patch"
  "${CMAKE_CURRENT_SOURCE_DIR}/defold-build.patch")

# The upstream install exports must not depend on Defold's build-only SDK
# target, but every compiled target still needs its platform settings.
get_property(protobuf_link_libraries DIRECTORY PROPERTY LINK_LIBRARIES)
list(REMOVE_ITEM protobuf_link_libraries defold_sdk)
list(APPEND protobuf_link_libraries "$<BUILD_INTERFACE:defold_sdk>")
set_property(DIRECTORY PROPERTY LINK_LIBRARIES "${protobuf_link_libraries}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INSTALL_BINDIR "bin/${TARGET_PLATFORM}")
set(CMAKE_INSTALL_LIBDIR "lib/${TARGET_PLATFORM}")
set(BUILD_SHARED_LIBS OFF)
add_compile_definitions(NDEBUG)

set(ABSL_ENABLE_INSTALL ON)
set(ABSL_BUILD_TESTING OFF)
set(ABSL_MSVC_STATIC_RUNTIME ON)
set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/sources/abseil-cpp-20250512.1" abseil)

set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_CONFORMANCE OFF)
set(protobuf_BUILD_EXAMPLES OFF)
set(protobuf_BUILD_SHARED_LIBS OFF)
set(protobuf_BUILD_PROTOC_BINARIES ON)
set(protobuf_BUILD_LIBUPB OFF)
set(protobuf_DISABLE_RTTI ON)
set(protobuf_LOCAL_DEPENDENCIES_ONLY ON)
set(protobuf_WITH_ZLIB OFF)
set(protobuf_DEBUG_POSTFIX "")
add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/sources/protobuf-35.1" protobuf)

# Protobuf and Abseil select /MTd for Debug, but their compiled targets must
# use the same runtime as the engine (/MT in every configuration).
function(protobuf_restore_msvc_runtime directory)
  get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS targets)
    get_target_property(type ${target} TYPE)
    if(NOT type STREQUAL "INTERFACE_LIBRARY" AND NOT type STREQUAL "UTILITY")
      set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    endif()
  endforeach()
  get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(subdirectory IN LISTS subdirectories)
    protobuf_restore_msvc_runtime("${subdirectory}")
  endforeach()
endfunction()

if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  protobuf_restore_msvc_runtime("${CMAKE_CURRENT_SOURCE_DIR}")
endif()
