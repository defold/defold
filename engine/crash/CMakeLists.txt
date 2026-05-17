cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_crash LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="CRASH")

set(CRASH_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(CRASH_SRC_DIR "${CRASH_ROOT}/src")

set(CRASH_LOAD_ADDRS_SOURCE "${CRASH_SRC_DIR}/load_addrs_null.cpp")
set(CRASH_BACKTRACE_SOURCE "${CRASH_SRC_DIR}/backtrace_null.cpp")
set(CRASH_FILE_SOURCE "${CRASH_SRC_DIR}/file_null.cpp")
set(CRASH_LINK_LIBS)

if(TARGET_PLATFORM MATCHES "android")
  set(CRASH_LOAD_ADDRS_SOURCE "${CRASH_SRC_DIR}/load_addrs_proc_smap.cpp")
  set(CRASH_BACKTRACE_SOURCE "${CRASH_SRC_DIR}/backtrace_libunwind_ndk.cpp")
  set(CRASH_FILE_SOURCE "${CRASH_SRC_DIR}/file_posix.cpp")
  list(APPEND CRASH_LINK_LIBS unwind)
elseif(TARGET_PLATFORM MATCHES "linux")
  set(CRASH_LOAD_ADDRS_SOURCE "${CRASH_SRC_DIR}/load_addrs_proc_smap.cpp")
  set(CRASH_BACKTRACE_SOURCE "${CRASH_SRC_DIR}/backtrace_execinfo.cpp")
  set(CRASH_FILE_SOURCE "${CRASH_SRC_DIR}/file_posix.cpp")
elseif(TARGET_PLATFORM MATCHES "web$")
  set(CRASH_BACKTRACE_SOURCE "${CRASH_SRC_DIR}/backtrace_web.cpp")
  set(CRASH_FILE_SOURCE "${CRASH_SRC_DIR}/file_posix.cpp")
elseif(TARGET_PLATFORM MATCHES "win32$")
  set(CRASH_LOAD_ADDRS_SOURCE "${CRASH_SRC_DIR}/load_addrs_win32.cpp")
  set(CRASH_BACKTRACE_SOURCE "${CRASH_SRC_DIR}/backtrace_win32.cpp")
  set(CRASH_FILE_SOURCE "${CRASH_SRC_DIR}/file_win32.cpp")
  list(APPEND CRASH_LINK_LIBS dbghelp.lib psapi.lib)
elseif(NOT TARGET_PLATFORM MATCHES "^(arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  set(CRASH_BACKTRACE_SOURCE "${CRASH_SRC_DIR}/backtrace_execinfo.cpp")
  set(CRASH_FILE_SOURCE "${CRASH_SRC_DIR}/file_posix.cpp")
endif()

if(TARGET_PLATFORM MATCHES "macos|ios")
  set(CRASH_LOAD_ADDRS_SOURCE "${CRASH_SRC_DIR}/load_addrs_mach.cpp")
endif()
if(TARGET_PLATFORM STREQUAL "x86_64-macos")
  set(CRASH_BACKTRACE_SOURCE "${CRASH_SRC_DIR}/backtrace_libunwind.cpp")
endif()
if(TARGET_PLATFORM MATCHES "^(arm64-nx64|x86_64-ps4|x86_64-ps5|x86_64-xbone)$")
  set(CRASH_LOAD_ADDRS_SOURCE "${CRASH_SRC_DIR}/load_addrs_null.cpp")
  set(CRASH_BACKTRACE_SOURCE "${CRASH_SRC_DIR}/backtrace_null.cpp")
  set(CRASH_FILE_SOURCE "${CRASH_SRC_DIR}/file_null.cpp")
endif()

function(crash_configure_target target)
  target_include_directories(${target} PUBLIC "${CRASH_SRC_DIR}")
  target_compile_definitions(${target} PRIVATE "DLIB_LOG_DOMAIN=\"CRASH\"")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" SCOPE PUBLIC
    ddf
    dlib
    image
    zip
    mbedtls
    resource
    extension
    script
    lua
    ${CRASH_LINK_LIBS})
endfunction()

defold_add_library(crashext_null STATIC
  "${CRASH_SRC_DIR}/crash_null.cpp"
  "${CRASH_SRC_DIR}/script_crash_null.cpp"
  "${CRASH_SRC_DIR}/load_addrs_null.cpp"
  "${CRASH_SRC_DIR}/backtrace_null.cpp"
  "${CRASH_SRC_DIR}/file_null.cpp")
crash_configure_target(crashext_null)

defold_add_library(crashext STATIC
  "${CRASH_SRC_DIR}/crash.cpp"
  "${CRASH_SRC_DIR}/script_crash.cpp"
  "${CRASH_LOAD_ADDRS_SOURCE}"
  "${CRASH_BACKTRACE_SOURCE}"
  "${CRASH_FILE_SOURCE}")
crash_configure_target(crashext)

defold_install_targets(TARGETS crashext crashext_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})
install(FILES "${CRASH_SRC_DIR}/crash.h" DESTINATION include/crash)

if(TARGET_PLATFORM MATCHES "macos|linux|win32$")
  set(_crashtool_suffix "_32")
  if(TARGET_PLATFORM MATCHES "64")
    set(_crashtool_suffix "_64")
  endif()
  defold_add_executable(crashtool${_crashtool_suffix} "${CRASH_SRC_DIR}/tool/crashtool.cpp")
  target_include_directories(crashtool${_crashtool_suffix} PRIVATE "${CRASH_SRC_DIR}")
  defold_target_link_libraries(crashtool${_crashtool_suffix} "${TARGET_PLATFORM}"
    script
    extension
    resource
    ddf
    dlib
    image
    zip
    mbedtls
    profile_null
    lua
    crashext
    ${CRASH_LINK_LIBS})
  if(TARGET_PLATFORM MATCHES "linux")
    target_link_libraries(crashtool${_crashtool_suffix} PRIVATE dl)
  endif()
  defold_install_targets(TARGETS crashtool${_crashtool_suffix}
          RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
