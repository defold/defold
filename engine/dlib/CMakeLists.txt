cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_dlib LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(DLIB_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(DLIB_SRC_DIR "${DLIB_ROOT}/src")
set(DLIB_CORE_DIR "${DLIB_SRC_DIR}/dlib")
set(DLIB_MBEDTLS_DIR "${DLIB_SRC_DIR}/mbedtls")

function(dlib_append_unique out_var)
  set(_values ${${out_var}})
  foreach(_value IN LISTS ARGN)
    if(_value AND NOT _value IN_LIST _values)
      list(APPEND _values "${_value}")
    endif()
  endforeach()
  set(${out_var} "${_values}" PARENT_SCOPE)
endfunction()

function(dlib_remove_matching in_out_var)
  set(_sources ${${in_out_var}})
  foreach(_pattern IN LISTS ARGN)
    list(FILTER _sources EXCLUDE REGEX "${_pattern}")
  endforeach()
  set(${in_out_var} "${_sources}" PARENT_SCOPE)
endfunction()

function(dlib_target_os out_var)
  if(TARGET_PLATFORM MATCHES "win32$")
    set(_target_os "win32")
  elseif(TARGET_PLATFORM MATCHES "macos$")
    set(_target_os "macos")
  elseif(TARGET_PLATFORM MATCHES "ios$")
    set(_target_os "ios")
  elseif(TARGET_PLATFORM MATCHES "android$")
    set(_target_os "android")
  elseif(TARGET_PLATFORM MATCHES "linux$")
    set(_target_os "linux")
  elseif(TARGET_PLATFORM MATCHES "web$")
    set(_target_os "web")
  elseif(TARGET_PLATFORM MATCHES "nx64$")
    set(_target_os "nx64")
  elseif(TARGET_PLATFORM MATCHES "ps5$")
    set(_target_os "ps5")
  elseif(TARGET_PLATFORM MATCHES "ps4$")
    set(_target_os "ps4")
  elseif(TARGET_PLATFORM MATCHES "xbone$")
    set(_target_os "xbone")
  else()
    set(_target_os "${TARGET_PLATFORM_OS}")
  endif()
  set(${out_var} "${_target_os}" PARENT_SCOPE)
endfunction()

function(dlib_platform_tags out_var)
  dlib_target_os(_target_os)
  set(_tags "${_target_os}")
  if(_target_os STREQUAL "nx64")
    list(APPEND _tags switch nintendo)
  elseif(_target_os STREQUAL "ps4" OR _target_os STREQUAL "ps5")
    list(APPEND _tags playstation sony)
  elseif(_target_os STREQUAL "xbone")
    list(APPEND _tags xbox microsoft win32)
  endif()

  if(_target_os STREQUAL "macos" OR _target_os STREQUAL "ios")
    list(APPEND _tags darwin apple)
  endif()

  set(${out_var} "${_tags}" PARENT_SCOPE)
endfunction()

function(dlib_fallback_tags out_var)
  dlib_target_os(_target_os)
  set(_tags)
  if(_target_os MATCHES "^(android|ios|linux|macos|web)$")
    list(APPEND _tags posix)
  endif()
  list(APPEND _tags default)
  set(${out_var} "${_tags}" PARENT_SCOPE)
endfunction()

function(dlib_select_feature out_selected out_all feature_base)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs EXTRA_TAGS PREFERRED_TAGS)
  cmake_parse_arguments(DFEAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  file(GLOB _all CONFIGURE_DEPENDS
       "${DLIB_SRC_DIR}/${feature_base}.cpp"
       "${DLIB_SRC_DIR}/${feature_base}.c"
       "${DLIB_SRC_DIR}/${feature_base}.cc"
       "${DLIB_SRC_DIR}/${feature_base}.cxx"
       "${DLIB_SRC_DIR}/${feature_base}.mm"
       "${DLIB_SRC_DIR}/${feature_base}.m"
       "${DLIB_SRC_DIR}/${feature_base}_*.cpp"
       "${DLIB_SRC_DIR}/${feature_base}_*.c"
       "${DLIB_SRC_DIR}/${feature_base}_*.cc"
       "${DLIB_SRC_DIR}/${feature_base}_*.cxx"
       "${DLIB_SRC_DIR}/${feature_base}_*.mm"
       "${DLIB_SRC_DIR}/${feature_base}_*.m")

  set(_selected)
  foreach(_ext IN ITEMS cpp c cc cxx mm m)
    if(EXISTS "${DLIB_SRC_DIR}/${feature_base}.${_ext}")
      dlib_append_unique(_selected "${DLIB_SRC_DIR}/${feature_base}.${_ext}")
    endif()
  endforeach()

  set(_tag_files)
  foreach(_tag IN LISTS DFEAT_PREFERRED_TAGS)
    foreach(_ext IN ITEMS cpp c cc cxx mm m)
      if(EXISTS "${DLIB_SRC_DIR}/${feature_base}_${_tag}.${_ext}")
        dlib_append_unique(_tag_files "${DLIB_SRC_DIR}/${feature_base}_${_tag}.${_ext}")
      endif()
    endforeach()
  endforeach()

  if(NOT _tag_files)
    dlib_platform_tags(_platform_tags)
    set(_candidate_tags ${_platform_tags} ${DFEAT_EXTRA_TAGS})
    foreach(_tag IN LISTS _candidate_tags)
      foreach(_ext IN ITEMS cpp c cc cxx mm m)
        if(EXISTS "${DLIB_SRC_DIR}/${feature_base}_${_tag}.${_ext}")
          dlib_append_unique(_tag_files "${DLIB_SRC_DIR}/${feature_base}_${_tag}.${_ext}")
        endif()
      endforeach()
    endforeach()
  endif()

  if(NOT _tag_files)
    dlib_fallback_tags(_fallback_tags)
    foreach(_tag IN LISTS _fallback_tags)
      foreach(_ext IN ITEMS cpp c cc cxx mm m)
        if(EXISTS "${DLIB_SRC_DIR}/${feature_base}_${_tag}.${_ext}")
          dlib_append_unique(_tag_files "${DLIB_SRC_DIR}/${feature_base}_${_tag}.${_ext}")
        endif()
      endforeach()
    endforeach()
  endif()

  dlib_append_unique(_selected ${_tag_files})
  if(NOT _all)
    message(FATAL_ERROR "Could not find any source files for feature ${feature_base}")
  endif()
  if(NOT _selected)
    message(FATAL_ERROR "Could not find selected source files for feature ${feature_base} on platform ${TARGET_PLATFORM}")
  endif()

  set(${out_selected} "${_selected}" PARENT_SCOPE)
  set(${out_all} "${_all}" PARENT_SCOPE)
endfunction()

dlib_target_os(DLIB_TARGET_OS)
defold_feature_enabled(mbedtls DLIB_USE_MBEDTLS)

set(DLIB_ZLIB_DEFINES ZLIB_CONST)
if(NOT TARGET_PLATFORM MATCHES "win32$")
  list(APPEND DLIB_ZLIB_DEFINES Z_HAVE_UNISTD_H HAVE_HIDDEN)
endif()

set(DLIB_EXTRA_DEFINES
  ${DLIB_ZLIB_DEFINES}
  MINIZ_NO_TIME
  ZIP_ENABLE_INFLATE=1
  ZIP_ENABLE_DEFLATE=0
  ZIP_HAVE_SYMLINK=0)
if(NOT TARGET_PLATFORM MATCHES "win32$")
  list(APPEND DLIB_EXTRA_DEFINES CONFIG_USE_DEV_URANDOM)
endif()

set(DLIB_MBEDTLS_DEFINES
  MBEDTLS_THREADING_ALT
  MBEDTLS_THREADING_C
  DM_MBEDTLS_DISABLE_ASM
  "MBEDTLS_USER_CONFIG_FILE=\"mbedtls_user_config.h\""
  "TF_PSA_CRYPTO_USER_CONFIG_FILE=\"tf_psa_crypto_user_config.h\"")
if(TARGET_PLATFORM STREQUAL "wasm-web")
  list(APPEND DLIB_MBEDTLS_DEFINES DM_MBEDTLS_CRYPTO_ONLY)
elseif(NOT DLIB_TARGET_OS MATCHES "^(macos|ios|web)$")
  list(APPEND DLIB_MBEDTLS_DEFINES DM_MBEDTLS_TLS_CLIENT_ONLY)
endif()
if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
  list(APPEND DLIB_MBEDTLS_DEFINES DM_MBEDTLS_ERROR_STRERROR_DUMMY)
else()
  list(APPEND DLIB_MBEDTLS_DEFINES DM_MBEDTLS_ERROR_C)
endif()

set(DLIB_PUBLIC_INCLUDE_DIRS
  "${DLIB_SRC_DIR}"
  "${DLIB_MBEDTLS_DIR}/defold"
  "${DLIB_MBEDTLS_DIR}"
  "${DLIB_MBEDTLS_DIR}/include"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/include"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/include")

set(DLIB_MBEDTLS_INCLUDE_DIRS
  ${DLIB_PUBLIC_INCLUDE_DIRS}
  "${DLIB_MBEDTLS_DIR}/library"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/core"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/dispatch"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/src"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/everest/include"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/p256-m"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/pqcp/include"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/extras"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/platform"
  "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/utilities")

file(GLOB DLIB_BASE_SOURCES CONFIGURE_DEPENDS
     "${DLIB_CORE_DIR}/*.cpp"
     "${DLIB_SRC_DIR}/zlib/*.c"
     "${DLIB_SRC_DIR}/lz4/*.c")

set(DLIB_FEATURE_SOURCES)
set(DLIB_FEATURE_FILES)
foreach(_feature IN ITEMS
    dlib/condition_variable
    dlib/crypt
    dlib/file_descriptor
    dlib/mutex
    dlib/socket
    dlib/sslsocket
    dlib/sys
    dlib/time
    dlib/thread)
  set(_extra_tags)
  set(_preferred_tags)
  if(_feature STREQUAL "dlib/crypt")
    if(DLIB_USE_MBEDTLS AND DLIB_TARGET_OS MATCHES "^(macos|ios)$")
      set(_preferred_tags mbedtls)
    elseif(DLIB_TARGET_OS MATCHES "^(macos|ios)$")
      set(_extra_tags)
    else()
      set(_extra_tags mbedtls)
    endif()
  elseif(_feature STREQUAL "dlib/sslsocket")
    if(DLIB_USE_MBEDTLS AND DLIB_TARGET_OS MATCHES "^(macos|ios)$")
      set(_preferred_tags mbedtls)
    elseif(DLIB_TARGET_OS MATCHES "^(android|linux|win32)$")
      set(_extra_tags mbedtls)
    endif()
  elseif(_feature STREQUAL "dlib/sys")
    if(DLIB_TARGET_OS MATCHES "^(android|linux|macos|ios|web|nx64|ps4|ps5)$")
      set(_extra_tags posix)
    endif()
  endif()
  dlib_select_feature(_selected _all "${_feature}" EXTRA_TAGS ${_extra_tags} PREFERRED_TAGS ${_preferred_tags})
  dlib_append_unique(DLIB_FEATURE_SOURCES ${_selected})
  dlib_append_unique(DLIB_FEATURE_FILES ${_all})
endforeach()

set(DLIB_SOURCES ${DLIB_BASE_SOURCES})
foreach(_feature_file IN LISTS DLIB_FEATURE_FILES)
  list(REMOVE_ITEM DLIB_SOURCES "${_feature_file}")
endforeach()
dlib_append_unique(DLIB_SOURCES ${DLIB_FEATURE_SOURCES})
dlib_remove_matching(DLIB_SOURCES "/image\\.cpp$" "/image_null\\.cpp$")
dlib_remove_matching(DLIB_SOURCES "_null\\.cpp$")
if(TARGET_PLATFORM MATCHES "web$")
  dlib_remove_matching(DLIB_SOURCES "/http_cache\\.cpp$" "/http_cache_verify\\.cpp$" "/configfile_http\\.cpp$")
  list(APPEND DLIB_SOURCES
    "${DLIB_CORE_DIR}/http_cache_null.cpp"
    "${DLIB_CORE_DIR}/configfile_http_null.cpp")
endif()

file(GLOB DLIB_ZIP_SOURCES CONFIGURE_DEPENDS "${DLIB_SRC_DIR}/zip/*.c")
defold_add_library(zip STATIC ${DLIB_ZIP_SOURCES})
target_include_directories(zip PUBLIC "${DLIB_SRC_DIR}" "${DLIB_SRC_DIR}/zip")
target_compile_definitions(zip PUBLIC VALGRIND ${DLIB_EXTRA_DEFINES})

defold_add_library(zip_noasan STATIC ${DLIB_ZIP_SOURCES})
target_include_directories(zip_noasan PUBLIC "${DLIB_SRC_DIR}" "${DLIB_SRC_DIR}/zip")
target_compile_definitions(zip_noasan PUBLIC VALGRIND ${DLIB_EXTRA_DEFINES})

if(TARGET_PLATFORM STREQUAL "wasm-web")
  set(DLIB_MBEDTLS_SOURCES
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/src/md5.c"
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/src/sha1.c"
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/src/sha256.c"
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/src/sha512.c"
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/extras/md.c"
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/platform/platform.c"
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/platform/platform_util.c"
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/utilities/base64.c"
    "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/utilities/constant_time.c")
else()
  file(GLOB_RECURSE DLIB_MBEDTLS_SOURCES CONFIGURE_DEPENDS
       "${DLIB_MBEDTLS_DIR}/library/*.c"
       "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/core/*.c"
       "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/drivers/builtin/src/*.c"
       "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/extras/*.c"
       "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/platform/*.c"
       "${DLIB_MBEDTLS_DIR}/tf-psa-crypto/utilities/*.c")
endif()

defold_add_library(mbedtls STATIC ${DLIB_MBEDTLS_SOURCES})
target_include_directories(mbedtls PUBLIC ${DLIB_MBEDTLS_INCLUDE_DIRS})
target_compile_definitions(mbedtls PUBLIC ${DLIB_MBEDTLS_DEFINES})

defold_add_library(mbedtls_noasan STATIC ${DLIB_MBEDTLS_SOURCES})
target_include_directories(mbedtls_noasan PUBLIC ${DLIB_MBEDTLS_INCLUDE_DIRS})
target_compile_definitions(mbedtls_noasan PUBLIC ${DLIB_MBEDTLS_DEFINES})

defold_add_library(dlib STATIC ${DLIB_SOURCES})
target_include_directories(dlib PUBLIC ${DLIB_PUBLIC_INCLUDE_DIRS})
target_compile_definitions(dlib PUBLIC VALGRIND "DLIB_LOG_DOMAIN=\"DLIB\"" ${DLIB_EXTRA_DEFINES} ${DLIB_MBEDTLS_DEFINES})
target_link_libraries(dlib PUBLIC zip mbedtls)
if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-ios|x86_64-ios)$")
  foreach(_fw IN ITEMS CFNetwork Security)
    target_link_options(dlib PUBLIC "-Wl,-framework,${_fw}")
  endforeach()
endif()

defold_add_library(dlib_noasan STATIC ${DLIB_SOURCES})
target_include_directories(dlib_noasan PUBLIC ${DLIB_PUBLIC_INCLUDE_DIRS})
target_compile_definitions(dlib_noasan PUBLIC VALGRIND "DLIB_LOG_DOMAIN=\"DLIB\"" ${DLIB_EXTRA_DEFINES} ${DLIB_MBEDTLS_DEFINES})
target_link_libraries(dlib_noasan PUBLIC zip_noasan mbedtls_noasan)
if(TARGET_PLATFORM MATCHES "^(arm64-macos|x86_64-macos|arm64-ios|x86_64-ios)$")
  foreach(_fw IN ITEMS CFNetwork Security)
    target_link_options(dlib_noasan PUBLIC "-Wl,-framework,${_fw}")
  endforeach()
endif()

defold_add_library(image STATIC "${DLIB_CORE_DIR}/image.cpp")
target_include_directories(image PUBLIC "${DLIB_SRC_DIR}")
target_compile_definitions(image PUBLIC VALGRIND ${DLIB_EXTRA_DEFINES})

defold_add_library(image_noasan STATIC "${DLIB_CORE_DIR}/image.cpp")
target_include_directories(image_noasan PUBLIC "${DLIB_SRC_DIR}")
target_compile_definitions(image_noasan PUBLIC VALGRIND ${DLIB_EXTRA_DEFINES})

defold_add_library(image_null STATIC "${DLIB_CORE_DIR}/image_null.cpp")
target_include_directories(image_null PUBLIC "${DLIB_SRC_DIR}")
target_compile_definitions(image_null PUBLIC VALGRIND ${DLIB_EXTRA_DEFINES})

defold_add_library(image_null_noasan STATIC "${DLIB_CORE_DIR}/image_null.cpp")
target_include_directories(image_null_noasan PUBLIC "${DLIB_SRC_DIR}")
target_compile_definitions(image_null_noasan PUBLIC VALGRIND ${DLIB_EXTRA_DEFINES})

defold_add_library(profile_null STATIC "${DLIB_CORE_DIR}/profile/profile_null.cpp")
target_include_directories(profile_null PUBLIC "${DLIB_SRC_DIR}" "${DLIB_CORE_DIR}/profile")
target_compile_definitions(profile_null PUBLIC ${DLIB_EXTRA_DEFINES})

defold_add_library(profile_null_noasan STATIC "${DLIB_CORE_DIR}/profile/profile_null.cpp")
target_include_directories(profile_null_noasan PUBLIC "${DLIB_SRC_DIR}" "${DLIB_CORE_DIR}/profile")
target_compile_definitions(profile_null_noasan PUBLIC ${DLIB_EXTRA_DEFINES})

defold_add_library(profile STATIC "${DLIB_CORE_DIR}/profile/profile.cpp")
target_include_directories(profile PUBLIC "${DLIB_SRC_DIR}" "${DLIB_CORE_DIR}/profile")
target_compile_definitions(profile PUBLIC ${DLIB_EXTRA_DEFINES})

defold_add_library(profile_noasan STATIC "${DLIB_CORE_DIR}/profile/profile.cpp")
target_include_directories(profile_noasan PUBLIC "${DLIB_SRC_DIR}" "${DLIB_CORE_DIR}/profile")
target_compile_definitions(profile_noasan PUBLIC ${DLIB_EXTRA_DEFINES})

if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86-win32|x86_64-macos|arm64-macos)$")
  defold_add_library(dlib_shared SHARED ${DLIB_SOURCES})
  target_include_directories(dlib_shared PUBLIC ${DLIB_PUBLIC_INCLUDE_DIRS})
  target_compile_definitions(dlib_shared PUBLIC VALGRIND "DLIB_LOG_DOMAIN=\"DLIB\"" ${DLIB_EXTRA_DEFINES} ${DLIB_MBEDTLS_DEFINES})
  target_link_libraries(dlib_shared PRIVATE profile_null_noasan mbedtls_noasan zip_noasan)
  defold_target_link_socket(dlib_shared "${TARGET_PLATFORM}" PRIVATE)
  if(TARGET_PLATFORM MATCHES "macos$")
    foreach(_fw IN ITEMS CFNetwork Security)
      target_link_options(dlib_shared PRIVATE "-Wl,-framework,${_fw}")
    endforeach()
  endif()

  if(NOT TARGET_PLATFORM MATCHES "win32$")
    defold_add_library(dlib_memprofile SHARED "${DLIB_CORE_DIR}/memprofile.cpp")
    target_include_directories(dlib_memprofile PUBLIC "${DLIB_SRC_DIR}")
    target_compile_definitions(dlib_memprofile PUBLIC DM_LIBMEMPROFILE)
    if(TARGET_PLATFORM MATCHES "linux$")
      target_link_libraries(dlib_memprofile PRIVATE dl)
    endif()
  endif()
endif()

set(DLIB_BASIS_FLAGS BASISD_SUPPORT_KTX2_ZSTD=0)
if(TARGET_PLATFORM MATCHES "^(x86_64-win32|x86-win32|x86_64-macos)$")
  list(APPEND DLIB_BASIS_FLAGS BASISU_SUPPORT_SSE=1)
endif()

file(GLOB_RECURSE DLIB_BASIS_TRANSCODER_SOURCES CONFIGURE_DEPENDS "${DLIB_SRC_DIR}/basis/transcoder/*.cpp")
defold_add_library(basis_transcoder STATIC ${DLIB_BASIS_TRANSCODER_SOURCES})
target_include_directories(basis_transcoder PUBLIC "${DLIB_SRC_DIR}/basis")
target_compile_definitions(basis_transcoder PUBLIC
  DM_BASIS_TRANSCODER_UASTC
  BASISD_SUPPORT_KTX2=0
  MINIZ_NO_STDIO
  ${DLIB_BASIS_FLAGS})
target_link_libraries(basis_transcoder PUBLIC dlib)

if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86-win32|x86_64-macos|arm64-macos)$")
  file(GLOB_RECURSE DLIB_BASIS_ENCODER_SOURCES CONFIGURE_DEPENDS
       "${DLIB_SRC_DIR}/basis/encoder/*.c"
       "${DLIB_SRC_DIR}/basis/encoder/*.cpp"
       "${DLIB_SRC_DIR}/basis/transcoder/*.cpp")
  defold_add_library(basis_encoder STATIC ${DLIB_BASIS_ENCODER_SOURCES})
  target_include_directories(basis_encoder PUBLIC "${DLIB_SRC_DIR}/basis")
  target_compile_definitions(basis_encoder PUBLIC DM_BASIS_ENCODER MINIZ_NO_STDIO ${DLIB_BASIS_FLAGS})

  defold_add_library(basis_encoder_noasan STATIC ${DLIB_BASIS_ENCODER_SOURCES})
  target_include_directories(basis_encoder_noasan PUBLIC "${DLIB_SRC_DIR}/basis")
  target_compile_definitions(basis_encoder_noasan PUBLIC DM_BASIS_ENCODER MINIZ_NO_STDIO ${DLIB_BASIS_FLAGS})
endif()

set(DLIB_INSTALL_TARGETS
  dlib dlib_noasan zip zip_noasan mbedtls mbedtls_noasan
  image image_noasan image_null image_null_noasan
  profile profile_noasan profile_null profile_null_noasan
  basis_transcoder)
if(TARGET dlib_shared)
  list(APPEND DLIB_INSTALL_TARGETS dlib_shared)
endif()
if(TARGET dlib_memprofile)
  list(APPEND DLIB_INSTALL_TARGETS dlib_memprofile)
endif()
if(TARGET basis_encoder)
  list(APPEND DLIB_INSTALL_TARGETS basis_encoder basis_encoder_noasan)
endif()

install(TARGETS ${DLIB_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES
  "${DLIB_CORE_DIR}/align.h"
  "${DLIB_CORE_DIR}/array.h"
  "${DLIB_CORE_DIR}/atomic.h"
  "${DLIB_CORE_DIR}/buffer.h"
  "${DLIB_CORE_DIR}/condition_variable.h"
  "${DLIB_CORE_DIR}/configfile.h"
  "${DLIB_CORE_DIR}/crypt.h"
  "${DLIB_CORE_DIR}/dalloca.h"
  "${DLIB_CORE_DIR}/dlib.h"
  "${DLIB_CORE_DIR}/double_linked_list.h"
  "${DLIB_CORE_DIR}/dstrings.h"
  "${DLIB_CORE_DIR}/easing.h"
  "${DLIB_CORE_DIR}/endian.h"
  "${DLIB_CORE_DIR}/endian.hpp"
  "${DLIB_CORE_DIR}/hash.h"
  "${DLIB_CORE_DIR}/hashtable.h"
  "${DLIB_CORE_DIR}/http_cache.h"
  "${DLIB_CORE_DIR}/http_cache_verify.h"
  "${DLIB_CORE_DIR}/http_client.h"
  "${DLIB_CORE_DIR}/http_server.h"
  "${DLIB_CORE_DIR}/image.h"
  "${DLIB_CORE_DIR}/index_pool.h"
  "${DLIB_CORE_DIR}/jobsystem.h"
  "${DLIB_CORE_DIR}/log.h"
  "${DLIB_CORE_DIR}/lz4.h"
  "${DLIB_CORE_DIR}/math.h"
  "${DLIB_CORE_DIR}/memory.h"
  "${DLIB_CORE_DIR}/memprofile.h"
  "${DLIB_CORE_DIR}/message.h"
  "${DLIB_CORE_DIR}/mdns.h"
  "${DLIB_CORE_DIR}/mutex.h"
  "${DLIB_CORE_DIR}/object_pool.h"
  "${DLIB_CORE_DIR}/opaque_handle_container.h"
  "${DLIB_CORE_DIR}/path.h"
  "${DLIB_CORE_DIR}/platform.h"
  "${DLIB_CORE_DIR}/poolallocator.h"
  "${DLIB_CORE_DIR}/pprint.h"
  "${DLIB_CORE_DIR}/profile/profile.h"
  "${DLIB_CORE_DIR}/safe_windows.h"
  "${DLIB_CORE_DIR}/set.h"
  "${DLIB_CORE_DIR}/shared_library.h"
  "${DLIB_CORE_DIR}/socket.h"
  "${DLIB_CORE_DIR}/sslsocket.h"
  "${DLIB_CORE_DIR}/spinlock.h"
  "${DLIB_CORE_DIR}/static_assert.h"
  "${DLIB_CORE_DIR}/stringpool.h"
  "${DLIB_CORE_DIR}/sys.h"
  "${DLIB_CORE_DIR}/template.h"
  "${DLIB_CORE_DIR}/thread.h"
  "${DLIB_CORE_DIR}/time.h"
  "${DLIB_CORE_DIR}/transform.h"
  "${DLIB_CORE_DIR}/trig_lookup.h"
  "${DLIB_CORE_DIR}/uri.h"
  "${DLIB_CORE_DIR}/utf8.h"
  "${DLIB_CORE_DIR}/vmath.h"
  "${DLIB_CORE_DIR}/webserver.h"
  "${DLIB_CORE_DIR}/zlib.h"
  "${DLIB_CORE_DIR}/zip.h"
  "${DLIB_CORE_DIR}/testutil.h"
  DESTINATION include/dlib)

install(FILES "${DLIB_CORE_DIR}/profile/profile.h" DESTINATION include/dlib/profile)
install(DIRECTORY "${DLIB_SRC_DIR}/stb/" DESTINATION ext/include/stb FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${DLIB_SRC_DIR}/jc/" DESTINATION ext/include/jc FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${DLIB_SRC_DIR}/basis/encoder/" DESTINATION ext/include/basis/encoder FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${DLIB_SRC_DIR}/basis/transcoder/" DESTINATION ext/include/basis/transcoder FILES_MATCHING PATTERN "*.h")
install(FILES "${DLIB_SRC_DIR}/python/dlib/__init__.py" DESTINATION lib/python/dlib)
install(FILES "${DLIB_CORE_DIR}/memprofile.py" DESTINATION lib/python)
install(PROGRAMS "${DLIB_ROOT}/bin/memprofile.sh" DESTINATION bin)
if(TARGET_PLATFORM MATCHES "web$")
  install(FILES "${DLIB_CORE_DIR}/js/library_sys.js" DESTINATION lib/${TARGET_PLATFORM}/js)
endif()

set(DLIB_JAVA_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/java_classes")
set(DLIB_JAR "${CMAKE_CURRENT_BINARY_DIR}/dlib.jar")
add_custom_command(
  OUTPUT "${DLIB_JAR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLIB_JAVA_BUILD_DIR}"
  COMMAND "${DEFOLD_JAVAC_EXECUTABLE}" -g --release 8 -Xlint:-options -d "${DLIB_JAVA_BUILD_DIR}" "${DLIB_SRC_DIR}/java/com/dynamo/crypt/Crypt.java"
  COMMAND "${Java_JAR_EXECUTABLE}" cf "${DLIB_JAR}" -C "${DLIB_JAVA_BUILD_DIR}" .
  DEPENDS "${DLIB_SRC_DIR}/java/com/dynamo/crypt/Crypt.java"
  VERBATIM)
add_custom_target(dlib_java ALL DEPENDS "${DLIB_JAR}")
install(FILES "${DLIB_JAR}" DESTINATION share/java)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
