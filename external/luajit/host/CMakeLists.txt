cmake_minimum_required(VERSION 4.0)

# This project builds native generators; it must not inherit the target SDK.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows" AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
  # Discover the native Windows compiler, headers and libraries outside a VS shell.
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../scripts/cmake")
  include(functions)
  include(sdk_windows)
endif()
project(luajit_host LANGUAGES C)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
file(READ "${LUAJIT_ARCH_FILE}" arch_config)
string(REGEX MATCH "LJ_BITS ([0-9]+)" unused "${arch_config}")
set(target_pointer_size "${CMAKE_MATCH_1}")
if(target_pointer_size STREQUAL "32" AND MSVC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "LuaJIT's VM generator requires a 32-bit host compiler for this target")
elseif(target_pointer_size STREQUAL "32" AND NOT MSVC)
  add_compile_options(-m32)
  add_link_options(-m32)
elseif(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 AND target_pointer_size STREQUAL "64")
  message(FATAL_ERROR "LuaJIT's VM generator requires a 64-bit host compiler for this target")
endif()
string(REGEX MATCH "LJ_ARCH ([a-z0-9]+)" unused "${arch_config}")
set(dasm_arch "${CMAKE_MATCH_1}")
string(REGEX MATCHALL "LJ_DASM [A-Z_]+" dasm_settings "${arch_config}")
set(dasm_flags)
foreach(setting IN LISTS dasm_settings)
  string(REPLACE "LJ_DASM " "" setting "${setting}")
  list(APPEND dasm_flags -D "${setting}")
endforeach()
string(REGEX MATCH "LJ_VERSION ([0-9]+)" unused "${arch_config}")
if(CMAKE_MATCH_1)
  list(APPEND dasm_flags -D "VER=${CMAKE_MATCH_1}")
endif()
string(REGEX MATCHALL "LJ_CONFIG \"[A-Z0-9_]+\" [0-9]+" host_settings "${arch_config}")
foreach(setting IN LISTS host_settings)
  string(REGEX MATCH "LJ_CONFIG \"([A-Z0-9_]+)\"" unused "${setting}")
  list(FILTER LUAJIT_DEFINITIONS EXCLUDE REGEX "^${CMAKE_MATCH_1}(=|$)")
  string(REGEX REPLACE "LJ_CONFIG \"([A-Z0-9_]+)\" ([0-9]+)" "\\1=\\2" setting "${setting}")
  list(APPEND LUAJIT_DEFINITIONS "${setting}")
endforeach()

set(src "${LUAJIT_SOURCE_DIR}/src")
add_executable(minilua "${src}/host/minilua.c")
if(NOT MSVC)
  target_link_libraries(minilua PRIVATE m)
endif()
add_custom_command(OUTPUT "${LUAJIT_GENERATED_DIR}/luajit.h"
  COMMAND minilua "${src}/host/genversion.lua" "${src}/luajit_rolling.h"
    "${LUAJIT_SOURCE_DIR}/.relver" "${LUAJIT_GENERATED_DIR}/luajit.h"
  DEPENDS minilua "${src}/host/genversion.lua" "${src}/luajit_rolling.h" "${LUAJIT_SOURCE_DIR}/.relver"
  VERBATIM)
add_custom_command(OUTPUT "${LUAJIT_GENERATED_DIR}/host/buildvm_arch.h"
  COMMAND minilua "${LUAJIT_SOURCE_DIR}/dynasm/dynasm.lua" -LN ${dasm_flags}
    -o "${LUAJIT_GENERATED_DIR}/host/buildvm_arch.h" "${src}/vm_${dasm_arch}.dasc"
  DEPENDS minilua "${src}/vm_${dasm_arch}.dasc" "${LUAJIT_ARCH_FILE}"
  VERBATIM)
add_executable(buildvm "${src}/host/buildvm.c" "${src}/host/buildvm_asm.c"
  "${src}/host/buildvm_peobj.c" "${src}/host/buildvm_lib.c" "${src}/host/buildvm_fold.c"
  "${LUAJIT_GENERATED_DIR}/host/buildvm_arch.h" "${LUAJIT_GENERATED_DIR}/luajit.h")
target_include_directories(buildvm PRIVATE "${LUAJIT_GENERATED_DIR}/host"
  "${LUAJIT_GENERATED_DIR}" "${src}")
target_compile_definitions(buildvm PRIVATE ${LUAJIT_DEFINITIONS})
if(MSVC)
  target_compile_definitions(minilua PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(buildvm PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set(lib_sources)
foreach(lib IN ITEMS base math bit string table io os package debug jit ffi buffer)
  list(APPEND lib_sources "${src}/lib_${lib}.c")
endforeach()
set(outputs "${LUAJIT_VM_FILE}")
add_custom_command(OUTPUT "${LUAJIT_VM_FILE}"
  COMMAND buildvm -m "${LUAJIT_VM_MODE}" -o "${LUAJIT_VM_FILE}"
  DEPENDS buildvm VERBATIM)
foreach(mode IN ITEMS bcdef ffdef libdef recdef vmdef folddef)
  set(inputs ${lib_sources})
  set(output "${LUAJIT_GENERATED_DIR}/lj_${mode}.h")
  if(mode STREQUAL "folddef")
    set(inputs "${src}/lj_opt_fold.c")
  elseif(mode STREQUAL "vmdef")
    set(output "${LUAJIT_GENERATED_DIR}/jit/vmdef.lua")
  endif()
  add_custom_command(OUTPUT "${output}"
    COMMAND buildvm -m "${mode}" -o "${output}" ${inputs}
    DEPENDS buildvm ${inputs} VERBATIM)
  list(APPEND outputs "${output}")
endforeach()
add_custom_target(generate ALL DEPENDS ${outputs})
