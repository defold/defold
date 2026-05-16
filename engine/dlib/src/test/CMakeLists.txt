cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(DLIB_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(DLIB_ROOT "${DLIB_TEST_ROOT}/../..")
set(DLIB_SRC_DIR "${DLIB_ROOT}/src")

set(DLIB_TEST_PROFILE_LIB profile)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web|arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  set(DLIB_TEST_PROFILE_LIB profile_null)
endif()

set(DLIB_TEST_SKIP_THREADS OFF)
if(TARGET_PLATFORM STREQUAL "wasm-web")
  set(DLIB_TEST_SKIP_THREADS ON)
endif()

set(DLIB_TEST_SKIP_HTTP OFF)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web|armv7-android|arm64-android)$")
  set(DLIB_TEST_SKIP_HTTP ON)
endif()

set(DLIB_TEST_SKIP_RUN OFF)
if(TARGET_PLATFORM MATCHES "web$")
  set(DLIB_TEST_SKIP_RUN ON)
endif()

set(DLIB_TEST_SKIP_HTTP_RUN OFF)
if(TARGET_PLATFORM MATCHES "win32$")
  set(DLIB_TEST_SKIP_HTTP_RUN ON)
endif()
if(DEFINED ENV{GITHUB_ACTIONS} AND TARGET_PLATFORM MATCHES "macos$")
  set(DLIB_TEST_SKIP_HTTP ON)
endif()

set(DLIB_TEST_DEFINES)
if(DEFINED ENV{DM_IPV6_UNSUPPORTED})
  list(APPEND DLIB_TEST_DEFINES DM_IPV6_UNSUPPORTED)
endif()
if(TARGET_PLATFORM MATCHES "^(arm64-ios|x86_64-ios)$")
  list(APPEND DLIB_TEST_DEFINES DM_NO_SYSTEM_FUNCTION)
endif()

function(dlib_add_test name)
  set(options SKIP_RUN)
  set(oneValueArgs TARGET_NAME)
  set(multiValueArgs SOURCES LIBS DEFINES INCLUDES)
  cmake_parse_arguments(DTEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(DTEST_TARGET_NAME)
    set(_target "${DTEST_TARGET_NAME}")
  else()
    set(_target "${name}")
  endif()

  set(_sources ${DTEST_SOURCES})
  list(APPEND _sources "${DLIB_TEST_ROOT}/${name}.cpp")

  defold_add_executable(${_target} ${_sources})
  target_include_directories(${_target} PRIVATE
    "${DLIB_TEST_ROOT}"
    "${DLIB_SRC_DIR}"
    ${DTEST_INCLUDES})
  target_compile_definitions(${_target} PRIVATE ${DLIB_TEST_DEFINES} ${DTEST_DEFINES})
  target_link_libraries(${_target} PRIVATE
    testmain
    dlib
    zip
    mbedtls
    ${DLIB_TEST_PROFILE_LIB}
    ${DTEST_LIBS})
  defold_target_link_socket(${_target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_app(${_target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_libraries_web(${_target} "${TARGET_PLATFORM}" SCOPE PRIVATE library_sys.js)

  if(TARGET_PLATFORM MATCHES "linux$")
    target_link_libraries(${_target} PRIVATE dl pthread)
  endif()

  if(DTEST_SKIP_RUN)
    defold_register_test_target(${_target} OFF "${DLIB_ROOT}")
  else()
    defold_register_test_target(${_target} ON "${DLIB_ROOT}"
      STAGE_FILES
        "${DLIB_TEST_ROOT}/data" "src/test/data")
  endif()
endfunction()

dlib_add_test(test_memory)
dlib_add_test(test_align)
dlib_add_test(test_buffer)
dlib_add_test(test_math)
dlib_add_test(test_transform)
dlib_add_test(test_hashtable)
dlib_add_test(test_array)
dlib_add_test(test_double_linked_list)
dlib_add_test(test_set)
dlib_add_test(test_indexpool)
dlib_add_test(test_dlib SOURCES "${DLIB_TEST_ROOT}/test_endian.c")
dlib_add_test(test_hash SOURCES "${DLIB_TEST_ROOT}/test_hash.c")
dlib_add_test(test_time)

if(NOT DLIB_TEST_SKIP_THREADS)
  dlib_add_test(test_socket)
  dlib_add_test(test_thread)
  dlib_add_test(test_mutex)
endif()

dlib_add_test(test_profile SOURCES "${DLIB_TEST_ROOT}/test_profiler_dummy.cpp")
dlib_add_test(test_profile
  TARGET_NAME test_profile_null
  SOURCES "${DLIB_TEST_ROOT}/test_profiler_dummy.cpp"
  LIBS profile_null
  SKIP_RUN)

dlib_add_test(test_poolallocator)
dlib_add_test(test_memprofile)
dlib_add_test(test_message)
dlib_add_test(test_configfile SKIP_RUN)
dlib_add_test(test_dstrings)

if(NOT DLIB_TEST_SKIP_HTTP)
  dlib_add_test(test_httpclient SKIP_RUN)
  dlib_add_test(test_httpcache SKIP_RUN)
  dlib_add_test(test_httpserver SKIP_RUN)
  dlib_add_test(test_webserver SKIP_RUN)
  dlib_add_test(test_connection_pool SKIP_RUN)
endif()

dlib_add_test(test_uri)
dlib_add_test(test_stringpool)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated.embed" "generated data\n")
defold_embed_to_header(
  INPUT "${DLIB_TEST_ROOT}/data/test.embed"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/data/test.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/data/test.embed.cpp")
defold_embed_to_header(
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/generated.embed"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated.embed.cpp")
dlib_add_test(test_embed
  SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/data/test.embed.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated.embed.cpp"
  INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")

if(NOT DLIB_TEST_SKIP_THREADS)
  dlib_add_test(test_atomic)
  dlib_add_test(test_spinlock)
  dlib_add_test(test_condition_variable)
  dlib_add_test(test_job_system)
endif()

if(DLIB_TEST_SKIP_RUN)
  dlib_add_test(test_sys SKIP_RUN)
  dlib_add_test(test_mdns SKIP_RUN)
else()
  dlib_add_test(test_sys)
  dlib_add_test(test_mdns)
endif()
dlib_add_test(test_template)
if(NOT DLIB_TEST_SKIP_THREADS)
  if(DLIB_TEST_SKIP_HTTP_RUN)
    dlib_add_test(test_log SKIP_RUN)
  else()
    dlib_add_test(test_log)
  endif()
endif()
dlib_add_test(test_path)
dlib_add_test(test_trig_lookup)
dlib_add_test(test_vmath)
dlib_add_test(test_intersection)
dlib_add_test(test_easing)
dlib_add_test(test_utf8)

if(TARGET_PLATFORM MATCHES "^(arm64-linux|x86_64-linux)$")
  dlib_add_test(test_xdg)
endif()

dlib_add_test(test_zip)
dlib_add_test(test_zlib)

defold_embed_to_header(
  INPUT "${DLIB_TEST_ROOT}/data/foo.lz4"
  OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/data/foo.lz4.embed.h"
  OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/data/foo.lz4.embed.cpp")
dlib_add_test(test_lz4
  SOURCES "${CMAKE_CURRENT_BINARY_DIR}/data/foo.lz4.embed.cpp"
  INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")

set(DLIB_IMAGE_EMBED_FILES
  color_check_2x2.png
  color_check_2x2_premult.png
  color_check_2x2_indexed.png
  case2319.jpg
  color16_check_2x2.png
  gray_check_2x2.png
  gray_alpha_check_2x2.png
  defold_64.jpg
  defold_64_progressive.jpg
  valid.astc
  invalid.astc)
set(DLIB_IMAGE_EMBED_SOURCES)
foreach(_embed_file IN LISTS DLIB_IMAGE_EMBED_FILES)
  defold_embed_to_header(
    INPUT "${DLIB_TEST_ROOT}/data/${_embed_file}"
    OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/data/${_embed_file}.embed.h"
    OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/data/${_embed_file}.embed.cpp"
    OUT_CPP_VAR _embed_cpp)
  list(APPEND DLIB_IMAGE_EMBED_SOURCES "${_embed_cpp}")
endforeach()
defold_add_library(test_image_embeds STATIC
  "${DLIB_TEST_ROOT}/test_image_embeds_dummy.cpp"
  ${DLIB_IMAGE_EMBED_SOURCES})
target_include_directories(test_image_embeds PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

dlib_add_test(test_image
  LIBS image test_image_embeds
  INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")
dlib_add_test(test_image
  TARGET_NAME test_image_null
  LIBS image_null test_image_embeds
  INCLUDES "${CMAKE_CURRENT_BINARY_DIR}"
  SKIP_RUN)

dlib_add_test(test_pprint)
dlib_add_test(test_objectpool)
dlib_add_test(test_opaque_handle_container)
dlib_add_test(test_crypt)
