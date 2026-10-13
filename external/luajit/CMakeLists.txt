cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
set(DEFOLD_REQUIRE_PROTOC OFF)
include(defold)

project(luajit_external LANGUAGES C CXX)

set(CMAKE_INSTALL_PREFIX "${DEFOLD_SDK_ROOT}/ext" CACHE PATH "External library install prefix" FORCE)
set(LUAJIT_REVISION 3e223cb8a41cff3b92931acb846af7b67a5d2537)
set(LUAJIT_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/${LUAJIT_REVISION}.zip")
set(LUAJIT_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/defold.patch")
set(LUAJIT_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/sources/LuaJIT-${LUAJIT_REVISION}")
set(LUAJIT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(CMAKE_CONFIGURATION_TYPES)
  string(APPEND LUAJIT_GENERATED_DIR "/$<CONFIG>")
endif()

find_package(Git REQUIRED)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LUAJIT_ARCHIVE}" "${LUAJIT_PATCH}")
file(SHA256 "${LUAJIT_ARCHIVE}" archive_hash)
file(SHA256 "${LUAJIT_PATCH}" patch_hash)
set(signature "${archive_hash}\n${patch_hash}\n")
set(previous "")
if(EXISTS "${LUAJIT_SOURCE_DIR}/.defold-source")
  file(READ "${LUAJIT_SOURCE_DIR}/.defold-source" previous)
endif()
if(NOT signature STREQUAL previous)
  file(REMOVE_RECURSE "${LUAJIT_SOURCE_DIR}")
  file(ARCHIVE_EXTRACT INPUT "${LUAJIT_ARCHIVE}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/sources" TOUCH)
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply --no-index --unsafe-paths "${LUAJIT_PATCH}"
    WORKING_DIRECTORY "${LUAJIT_SOURCE_DIR}" COMMAND_ERROR_IS_FATAL ANY)
  file(WRITE "${LUAJIT_SOURCE_DIR}/.defold-source" "${signature}")
endif()
set(LUAJIT_SRC "${LUAJIT_SOURCE_DIR}/src")

# These options must agree in the runtime and the native VM generator.
set(LUAJIT_DEFINITIONS LUA_USERDATA_ALIGNMENT=16)
if(TARGET_PLATFORM MATCHES "-(ios|android)$")
  list(APPEND LUAJIT_DEFINITIONS LUAJIT_DISABLE_JIT LUAJIT_NUMMODE=2)
  if(TARGET_PLATFORM STREQUAL "armv7-android")
    list(APPEND LUAJIT_DEFINITIONS LUAJIT_DISABLE_GC64)
  endif()
endif()
if(TARGET_PLATFORM MATCHES "-ios$")
  list(APPEND LUAJIT_DEFINITIONS TARGET_OS_IPHONE=1)
elseif(TARGET_PLATFORM MATCHES "^(arm64-macos|armv7-android)$")
  list(APPEND LUAJIT_DEFINITIONS LUAJIT_UNWIND_INTERNAL)
elseif(TARGET_PLATFORM MATCHES "^(x86_64-macos|.*-(linux|android))$")
  list(APPEND LUAJIT_DEFINITIONS LUAJIT_UNWIND_EXTERNAL)
elseif(TARGET_PLATFORM MATCHES "-xbone$")
  list(APPEND LUAJIT_DEFINITIONS LUAJIT_OS=LUAJIT_OS_WINDOWS _DURANGO
    LUAJIT_DISABLE_JIT LUAJIT_USE_SYSMALLOC)
elseif(DEFOLD_IS_PRIVATE_VENDOR OR TARGET_PLATFORM MATCHES "-(ps4|ps5|nx64|xbone)$")
  list(APPEND LUAJIT_DEFINITIONS LUAJIT_OS=LUAJIT_OS_OTHER
    LUAJIT_DISABLE_JIT LUAJIT_DISABLE_FFI LUAJIT_USE_SYSMALLOC)
endif()

set(LUAJIT_LIB_SOURCES
  lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c
  lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c lib_buffer.c)
list(TRANSFORM LUAJIT_LIB_SOURCES PREPEND "${LUAJIT_SRC}/")
file(GLOB LUAJIT_CORE_SOURCES CONFIGURE_DEPENDS "${LUAJIT_SRC}/lj_*.c")
add_library(luajit STATIC ${LUAJIT_CORE_SOURCES} ${LUAJIT_LIB_SOURCES}
  "${LUAJIT_SRC}/lib_aux.c" "${LUAJIT_SRC}/lib_init.c")
target_link_libraries(luajit PRIVATE defold_sdk)
target_include_directories(luajit PUBLIC "${LUAJIT_GENERATED_DIR}" "${LUAJIT_SRC}")
target_compile_definitions(luajit PRIVATE ${LUAJIT_DEFINITIONS})
set_target_properties(luajit PROPERTIES PREFIX "lib" OUTPUT_NAME "luajit-5.1")
if(TARGET_PLATFORM MATCHES "-xbone$")
  set_target_properties(luajit PROPERTIES PREFIX "")
endif()
if(MSVC)
  target_compile_definitions(luajit PRIVATE "_CRT_STDIO_INLINE=__declspec(dllexport)__inline")
else()
  target_compile_definitions(luajit PRIVATE _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)
  target_compile_options(luajit PRIVATE -U_FORTIFY_SOURCE
    -fno-strict-aliasing -fno-strict-float-cast-overflow)
  # Override platform stack protection after the inherited SDK options, as in upstream.
  set_source_files_properties(${LUAJIT_CORE_SOURCES} ${LUAJIT_LIB_SOURCES}
    "${LUAJIT_SRC}/lib_aux.c" "${LUAJIT_SRC}/lib_init.c"
    PROPERTIES COMPILE_OPTIONS -fno-stack-protector)
endif()

if(TARGET_PLATFORM MATCHES "-(win32|xbone)$")
  set(LUAJIT_VM_MODE peobj)
  set(LUAJIT_VM_FILE "${LUAJIT_GENERATED_DIR}/lj_vm.obj")
  set_source_files_properties("${LUAJIT_VM_FILE}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
else()
  enable_language(ASM)
  set(LUAJIT_VM_FILE "${LUAJIT_GENERATED_DIR}/lj_vm.S")
  if(TARGET_PLATFORM MATCHES "-(macos|ios)$")
    set(LUAJIT_VM_MODE machasm)
  else()
    set(LUAJIT_VM_MODE elfasm)
  endif()
endif()
target_sources(luajit PRIVATE "${LUAJIT_VM_FILE}")

# Query lj_arch.h with the target compiler and inherited SDK flags. The host
# generator must use the target ABI, even when its own CPU or OS differs.
set(LUAJIT_TARGET_COMPILER_FLAGS "${CMAKE_C_FLAGS}")
foreach(config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${config}" config_upper)
  string(APPEND LUAJIT_TARGET_COMPILER_FLAGS " $<$<CONFIG:${config}>:${CMAKE_C_FLAGS_${config_upper}}>")
endforeach()
if(CMAKE_C_COMPILER_TARGET)
  string(APPEND LUAJIT_TARGET_COMPILER_FLAGS " --target=${CMAKE_C_COMPILER_TARGET}")
endif()
if(CMAKE_SYSROOT)
  string(APPEND LUAJIT_TARGET_COMPILER_FLAGS " --sysroot=\"${CMAKE_SYSROOT}\"")
endif()
if(CMAKE_OSX_SYSROOT)
  string(APPEND LUAJIT_TARGET_COMPILER_FLAGS " -isysroot \"${DEFOLD_TARGET_SYSROOT}\"")
  foreach(arch IN LISTS CMAKE_OSX_ARCHITECTURES)
    string(APPEND LUAJIT_TARGET_COMPILER_FLAGS " -arch ${arch}")
  endforeach()
endif()
set(LUAJIT_HOST_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/host-build")
if(CMAKE_CONFIGURATION_TYPES)
  string(APPEND LUAJIT_HOST_BUILD_DIR "/$<CONFIG>")
endif()
configure_file(run_host.cmake.in run_host.cmake.gen @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/run_host-$<CONFIG>.cmake"
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run_host.cmake.gen"
  CONDITION "$<COMPILE_LANGUAGE:C>" TARGET luajit)

set(LUAJIT_GENERATED_FILES "${LUAJIT_VM_FILE}" "${LUAJIT_GENERATED_DIR}/luajit.h"
  "${LUAJIT_GENERATED_DIR}/jit/vmdef.lua")
foreach(mode IN ITEMS bcdef ffdef libdef recdef folddef)
  list(APPEND LUAJIT_GENERATED_FILES "${LUAJIT_GENERATED_DIR}/lj_${mode}.h")
endforeach()
file(GLOB LUAJIT_GENERATOR_INPUTS CONFIGURE_DEPENDS
  "${LUAJIT_SRC}/*.h" "${LUAJIT_SRC}/host/*" "${LUAJIT_SRC}/vm_*.dasc"
  "${LUAJIT_SOURCE_DIR}/dynasm/*")
add_custom_command(OUTPUT ${LUAJIT_GENERATED_FILES}
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/run_host-$<CONFIG>.cmake"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/run_host-$<CONFIG>.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/target_arch.h" "${CMAKE_CURRENT_SOURCE_DIR}/host/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/host/emscripten.cmake"
    ${LUAJIT_GENERATOR_INPUTS} ${LUAJIT_LIB_SOURCES} "${LUAJIT_SRC}/lj_opt_fold.c"
  COMMENT "Generating LuaJIT VM and headers for ${TARGET_PLATFORM}"
  VERBATIM)
add_custom_target(luajit_generate DEPENDS ${LUAJIT_GENERATED_FILES})
add_dependencies(luajit luajit_generate)

if(TARGET_PLATFORM MATCHES "^(arm64|x86_64)-(macos|linux|win32)$")
  add_executable(luajit_bin "${LUAJIT_SRC}/luajit.c")
  add_dependencies(luajit_bin luajit_generate)
  target_link_libraries(luajit_bin PRIVATE luajit defold_sdk)
  target_compile_definitions(luajit_bin PRIVATE ${LUAJIT_DEFINITIONS})
  set_target_properties(luajit_bin PROPERTIES OUTPUT_NAME luajit-64)
  if(NOT MSVC)
    target_link_libraries(luajit_bin PRIVATE m ${CMAKE_DL_LIBS})
    if(TARGET_PLATFORM MATCHES "-linux$")
      target_link_options(luajit_bin PRIVATE -Wl,-E)
    endif()
  endif()
  install(TARGETS luajit_bin RUNTIME DESTINATION "bin/${TARGET_PLATFORM}")
endif()

install(TARGETS luajit ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(FILES "${LUAJIT_GENERATED_DIR}/luajit.h" DESTINATION include/luajit-2.1)
foreach(header IN ITEMS lua.h lualib.h lauxlib.h luaconf.h lua.hpp lua_rename.h)
  install(FILES "${LUAJIT_SRC}/${header}" DESTINATION include/luajit-2.1)
endforeach()
if(TARGET_PLATFORM STREQUAL HOST_PLATFORM)
  # These modules describe the installed host compiler, not a cross-built runtime.
  install(DIRECTORY "${LUAJIT_SRC}/jit/" DESTINATION share/luajit/jit FILES_MATCHING PATTERN "*.lua")
  install(FILES "${LUAJIT_GENERATED_DIR}/jit/vmdef.lua" DESTINATION share/luajit/jit)
endif()
install(FILES "${LUAJIT_SOURCE_DIR}/COPYRIGHT" DESTINATION share/licenses/luajit)
