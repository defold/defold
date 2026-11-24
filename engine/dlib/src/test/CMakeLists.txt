cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(DLIB_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(DLIB_ROOT "${DLIB_TEST_ROOT}/../..")
set(DLIB_SRC_DIR "${DLIB_ROOT}/src")

set(DLIB_TEST_PROFILE_LIB profile)
set(DLIB_TEST_SKIP_PROFILE_RUN OFF)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web|arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  set(DLIB_TEST_PROFILE_LIB profile_null)
  set(DLIB_TEST_SKIP_PROFILE_RUN ON)
endif()

set(DLIB_TEST_SKIP_THREADS OFF)
if(TARGET_PLATFORM STREQUAL "wasm-web")
  set(DLIB_TEST_SKIP_THREADS ON)
endif()

set(DLIB_TEST_SKIP_HTTP OFF)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web|armv7-android|arm64-android|x86_64-android|x86_64-android)$")
  set(DLIB_TEST_SKIP_HTTP ON)
endif()

set(DLIB_TEST_SKIP_RUN OFF)
if(TARGET_PLATFORM MATCHES "web$")
  set(DLIB_TEST_SKIP_RUN ON)
endif()

set(DLIB_TEST_SKIP_MDNS_RUN ${DLIB_TEST_SKIP_RUN})
if(TARGET_PLATFORM MATCHES "ios$")
  # iOS Local Network privacy for Bonjour/mDNS is an interactive user grant,
  # even for development-signed apps, so keep this out of unattended runs.
  set(DLIB_TEST_SKIP_MDNS_RUN ON)
endif()

set(DLIB_TEST_SKIP_HTTP_RUN OFF)
if(TARGET_PLATFORM MATCHES "win32$")
  set(DLIB_TEST_SKIP_HTTP_RUN ON)
endif()
if(DEFINED ENV{GITHUB_ACTIONS} AND TARGET_PLATFORM MATCHES "macos$")
  set(DLIB_TEST_SKIP_HTTP ON)
endif()

set(DLIB_TEST_DEFINES "DLIB_LOG_DOMAIN=\"DLIB\"")
if(DLIB_TEST_SKIP_THREADS)
  # Keep non-threaded message coverage while omitting cases that create threads.
  list(APPEND DLIB_TEST_DEFINES DM_NO_THREAD_SUPPORT)
endif()
if(TARGET_PLATFORM MATCHES "ios$")
  list(APPEND DLIB_TEST_DEFINES DM_TEST_SKIP_NETWORK_DNS)
endif()
if(DEFINED ENV{DM_IPV6_UNSUPPORTED})
  list(APPEND DLIB_TEST_DEFINES DM_IPV6_UNSUPPORTED)
endif()

function(dlib_add_test name)
  set(options SKIP_RUN C)
  set(oneValueArgs TARGET_NAME)
  set(multiValueArgs SOURCES LIBS DEFINES INCLUDES)
  cmake_parse_arguments(DTEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(DTEST_TARGET_NAME)
    set(_target "${DTEST_TARGET_NAME}")
  else()
    set(_target "${name}")
  endif()

  set(_sources ${DTEST_SOURCES})
  if(DTEST_C)
    list(APPEND _sources "${DLIB_TEST_ROOT}/${name}.c")
  else()
    list(APPEND _sources "${DLIB_TEST_ROOT}/${name}.cpp")
  endif()

  defold_add_executable(${_target} ${_sources})
  if(DTEST_C)
    target_compile_features(${_target} PRIVATE c_std_11)
  endif()
  target_include_directories(${_target} PRIVATE
    "${DLIB_TEST_ROOT}"
    "${DLIB_SRC_DIR}"
    ${DTEST_INCLUDES})
  target_compile_definitions(${_target} PRIVATE ${DLIB_TEST_DEFINES} ${DTEST_DEFINES})
  defold_target_link_libraries(${_target} "${TARGET_PLATFORM}"
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
dlib_add_test(test_dlib)
dlib_add_test(test_hash)
dlib_add_test(test_endian C)
dlib_add_test(test_hash C TARGET_NAME test_hash_c)
dlib_add_test(test_context_registry C)
dlib_add_test(test_time)

set(_dlib_test_socket_defines)
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  # WebSocket emulation does not provide native TCP, socket options or DNS.
  # Keep the portable address tests enabled on both web variants.
  list(APPEND _dlib_test_socket_defines DM_TEST_NO_NATIVE_SOCKETS)
endif()
dlib_add_test(test_socket DEFINES ${_dlib_test_socket_defines})

if(NOT DLIB_TEST_SKIP_THREADS)
  dlib_add_test(test_thread)
  dlib_add_test(test_mutex)
endif()

if(DLIB_TEST_SKIP_PROFILE_RUN)
  dlib_add_test(test_profile SOURCES "${DLIB_TEST_ROOT}/test_profiler_dummy.cpp" SKIP_RUN)
else()
  dlib_add_test(test_profile SOURCES "${DLIB_TEST_ROOT}/test_profiler_dummy.cpp")
endif()
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
  dlib_add_test(test_http C SKIP_RUN)
  dlib_add_test(test_httpclient SKIP_RUN)
  if(NOT DLIB_TEST_SKIP_HTTP_RUN AND TARGET_PLATFORM STREQUAL HOST_PLATFORM)
    # The C HTTP tests use endpoints provided by dlib's Java server.
    set(_http_server_classes "${CMAKE_CURRENT_BINARY_DIR}/http_server")
    set(_http_server_classpath
      "${DLIB_ROOT}/ext/*:${DLIB_ROOT}/ext/jetty/*:${DLIB_ROOT}/ext/jetty/logging/*:${_http_server_classes}")
    file(GLOB _http_server_jars CONFIGURE_DEPENDS
      "${DLIB_ROOT}/ext/*.jar"
      "${DLIB_ROOT}/ext/jetty/*.jar"
      "${DLIB_ROOT}/ext/jetty/logging/*.jar")
    add_custom_command(
      OUTPUT "${_http_server_classes}/TestHttpServer.class"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_http_server_classes}"
      COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options
        -cp "${_http_server_classpath}" -d "${_http_server_classes}"
        "${DLIB_TEST_ROOT}/http_server/TestHttpServer.java"
      DEPENDS "${DLIB_TEST_ROOT}/http_server/TestHttpServer.java" ${_http_server_jars}
      VERBATIM)
    add_custom_target(test_http_server
      DEPENDS "${_http_server_classes}/TestHttpServer.class")
    _defold_find_python(_http_test_python)
    foreach(_http_test IN ITEMS test_http test_httpclient)
      add_dependencies(${_http_test} test_http_server)
      set(_http_test_command
        "${_http_test_python}" "${DLIB_ROOT}/scripts/run_test_http.py"
        --java "${DEFOLD_JAVA_EXECUTABLE}"
        --classpath "${_http_server_classpath}"
        --keystore "${DLIB_TEST_ROOT}/data/keystore"
        "$<TARGET_FILE:${_http_test}>")
      if(_http_test STREQUAL "test_httpclient")
        # Exercise response buffer growth and TLS negotiation using the local server.
        list(APPEND _http_test_command "--test-filter=HttpResponseBuffer.*" "--test-filter=dmHttpClient.Tls*")
      endif()
      defold_test_run_settings(_http_test_runner _http_test_options shared)
      add_custom_target(run_${_http_test}
        COMMAND ${_http_test_runner} ${_http_test_command}
        DEPENDS ${_http_test}
        ${_http_test_options}
        VERBATIM
        COMMENT "Running ${_http_test} with dlib's HTTP test server")
      set(_http_test_dependency ${_http_test})
      if(CMAKE_GENERATOR STREQUAL "Xcode")
        add_custom_target(prepare_run_${_http_test} DEPENDS ${_http_test})
        set(_http_test_dependency prepare_run_${_http_test})
      endif()
      defold_register_sequential_test_command(run_${_http_test}
        COMMAND ${_http_test_command}
        DEPENDS ${_http_test_dependency})
      if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
        defold_add_to_run_tests(run_${_http_test}
          COMMAND ${_http_test_command}
          DEPENDS ${_http_test})
      endif()
    endforeach()
  endif()
  dlib_add_test(test_httpcache SKIP_RUN)
  dlib_add_test(test_httpserver SKIP_RUN)
  dlib_add_test(test_webserver SKIP_RUN)
  dlib_add_test(test_webserver_openapi)
  dlib_add_test(test_connection_pool SKIP_RUN)
endif()

dlib_add_test(test_uri)
dlib_add_test(test_stringpool)

set(_dlib_generated_embed "generated data\n")
set(_dlib_generated_embed_path "${CMAKE_CURRENT_BINARY_DIR}/generated.embed")
if(EXISTS "${_dlib_generated_embed_path}")
  file(READ "${_dlib_generated_embed_path}" _dlib_generated_embed_existing)
else()
  set(_dlib_generated_embed_existing "")
endif()
if(NOT _dlib_generated_embed_existing STREQUAL _dlib_generated_embed)
  file(WRITE "${_dlib_generated_embed_path}" "${_dlib_generated_embed}")
endif()
unset(_dlib_generated_embed_existing)
unset(_dlib_generated_embed_path)
unset(_dlib_generated_embed)
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
else()
  dlib_add_test(test_sys)
endif()
if(DLIB_TEST_SKIP_MDNS_RUN)
  dlib_add_test(test_mdns SKIP_RUN)
else()
  dlib_add_test(test_mdns)
endif()
dlib_add_test(test_template)
if(NOT DLIB_TEST_SKIP_THREADS)
  set(_dlib_test_log_defines)
  if(TARGET_PLATFORM MATCHES "^(armv7-android|arm64-android|x86_64-android)$")
    list(APPEND _dlib_test_log_defines DM_NO_SYSTEM_FUNCTION)
  endif()
  if(DLIB_TEST_SKIP_HTTP_RUN)
    dlib_add_test(test_log DEFINES ${_dlib_test_log_defines} SKIP_RUN)
  else()
    dlib_add_test(test_log DEFINES ${_dlib_test_log_defines})
  endif()
endif()
dlib_add_test(test_path)
dlib_add_test(test_trig_lookup)
dlib_add_test(test_vmath)

# Each ISA suite has a baseline runner and an in-repo, portable SIMDe reference.
include(simd)
function(dlib_add_simd_test target isa version)
  set(_suite ${target}_suite)
  add_library(${_suite} OBJECT EXCLUDE_FROM_ALL
    "${DLIB_TEST_ROOT}/test_simd.c"
    "${DLIB_TEST_ROOT}/test_simd_simde.c")
  target_link_libraries(${_suite} PRIVATE defold_sdk)
  target_include_directories(${_suite} PRIVATE "${DLIB_SRC_DIR}")
  target_compile_definitions(${_suite} PRIVATE ${ARGN})
  defold_target_simd_x86_isa(${_suite} "${isa}")
  defold_add_executable(${target} "${DLIB_TEST_ROOT}/test_simd_main.c" $<TARGET_OBJECTS:${_suite}>)
  target_include_directories(${target} PRIVATE "${DLIB_SRC_DIR}")
  target_compile_definitions(${target} PRIVATE DM_SIMD_TEST_VERSION=${version} DM_SIMD_TEST_NAME="${target}")
  set_target_properties(${target} ${_suite} PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED TRUE C_EXTENSIONS FALSE
    INTERPROCEDURAL_OPTIMIZATION FALSE
    INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
    INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE)
  defold_target_simd_x86_isa(${target} SSE2)
  if(TARGET_PLATFORM STREQUAL "armv7-android")
    target_compile_options(${target} PRIVATE -mfpu=vfpv3-d16)
    if(version STREQUAL "DSIMD_VERSION_SCALAR")
      target_compile_options(${_suite} PRIVATE -mfpu=vfpv3-d16)
    else()
      target_compile_options(${_suite} PRIVATE -mfpu=neon)
    endif()
  endif()
  if(TARGET_PLATFORM MATCHES "web$")
    target_compile_options(${target} PRIVATE -mno-simd128)
    if(isa STREQUAL "WASM")
      target_compile_options(${_suite} PRIVATE -msimd128)
      target_link_options(${target} PRIVATE -msimd128
        "SHELL:--pre-js \"${DLIB_TEST_ROOT}/test_simd_wasm.js\"")
      set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${DLIB_TEST_ROOT}/test_simd_wasm.js")
    else()
      target_compile_options(${_suite} PRIVATE -mno-simd128)
      target_link_options(${target} PRIVATE -mno-simd128)
    endif()
  endif()
  if(TARGET_PLATFORM MATCHES "linux|android")
    target_link_libraries(${target} PRIVATE m)
  endif()
  defold_register_test_target(${target} ON "${DLIB_ROOT}")
endfunction()

dlib_add_simd_test(test_simd_scalar SSE2 DSIMD_VERSION_SCALAR DM_SIMD_FORCE_SCALAR=1)
if(TARGET_PLATFORM MATCHES "^x86_64-")
  dlib_add_simd_test(test_simd SSE3 DSIMD_VERSION_SSE3)
  dlib_add_simd_test(test_simd_sse42 SSE4.2 DSIMD_VERSION_SSE42)
  dlib_add_simd_test(test_simd_avx2 AVX2 DSIMD_VERSION_AVX2)
  dlib_add_simd_test(test_simd_avx512 AVX512 DSIMD_VERSION_AVX512)
elseif(TARGET_PLATFORM MATCHES "^(arm64|arm64_sim|armv7)-")
  dlib_add_simd_test(test_simd NEON DSIMD_VERSION_NEON)
elseif(TARGET_PLATFORM MATCHES "web$")
  dlib_add_simd_test(test_simd WASM DSIMD_VERSION_WASM_SIMD128)
else()
  dlib_add_simd_test(test_simd SSE2 DSIMD_VERSION_SCALAR DM_SIMD_FORCE_SCALAR=1)
endif()

# Portable wide paths remain covered on hosts without native AVX support.
dlib_add_simd_test(test_simd_wide64 SSE2 DSIMD_VERSION_SCALAR DM_SIMD_FORCE_SCALAR=1 DM_SIMD_WIDE_BYTES=64)

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

if(NOT TARGET_PLATFORM MATCHES "web$")
  defold_embed_to_header(
    INPUT "${DLIB_ROOT}/../engine/content/builtins/ca-certificates/cacert.pem"
    OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/cacert.pem.embed.h"
    OUTPUT_CPP "${CMAKE_CURRENT_BINARY_DIR}/cacert.pem.embed.cpp")
  dlib_add_test(test_ssl_certificates
    SOURCES "${CMAKE_CURRENT_BINARY_DIR}/cacert.pem.embed.cpp"
    INCLUDES "${CMAKE_CURRENT_BINARY_DIR}")
endif()
