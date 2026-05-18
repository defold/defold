cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_lua LANGUAGES ${DEFOLD_LANGUAGE_LIST})

set(LUA_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(LUA_SRC_DIR "${LUA_ROOT}/src")
set(LUA_CORE_DIR "${LUA_SRC_DIR}/lua")

set(LUA_DEFINES LUA_ANSI DM_LUA_USE_LUA51)
if(TARGET_PLATFORM MATCHES "arm64-ios|x86_64-ios")
  list(APPEND LUA_DEFINES LUA_NO_SYSTEM)
endif()

set(LUA_LIB_SOURCES
  "${LUA_CORE_DIR}/lapi.c"
  "${LUA_CORE_DIR}/lcode.c"
  "${LUA_CORE_DIR}/ldebug.c"
  "${LUA_CORE_DIR}/ldo.c"
  "${LUA_CORE_DIR}/ldump.c"
  "${LUA_CORE_DIR}/lfunc.c"
  "${LUA_CORE_DIR}/lgc.c"
  "${LUA_CORE_DIR}/llex.c"
  "${LUA_CORE_DIR}/lmem.c"
  "${LUA_CORE_DIR}/lobject.c"
  "${LUA_CORE_DIR}/lopcodes.c"
  "${LUA_CORE_DIR}/lparser.c"
  "${LUA_CORE_DIR}/lstate.c"
  "${LUA_CORE_DIR}/lstring.c"
  "${LUA_CORE_DIR}/ltable.c"
  "${LUA_CORE_DIR}/ltm.c"
  "${LUA_CORE_DIR}/lundump.c"
  "${LUA_CORE_DIR}/lvm.c"
  "${LUA_CORE_DIR}/lzio.c"
  "${LUA_CORE_DIR}/lauxlib.c"
  "${LUA_CORE_DIR}/lbaselib.c"
  "${LUA_CORE_DIR}/ldblib.c"
  "${LUA_CORE_DIR}/liolib.c"
  "${LUA_CORE_DIR}/lmathlib.c"
  "${LUA_CORE_DIR}/loslib.c"
  "${LUA_CORE_DIR}/ltablib.c"
  "${LUA_CORE_DIR}/lstrlib.c"
  "${LUA_CORE_DIR}/loadlib.c"
  "${LUA_CORE_DIR}/linit.c")

defold_add_library(lua STATIC ${LUA_LIB_SOURCES})
target_include_directories(lua PUBLIC "${LUA_CORE_DIR}" "${LUA_SRC_DIR}")
target_compile_definitions(lua PUBLIC ${LUA_DEFINES})

if(TARGET_PLATFORM MATCHES "linux|android|web")
  target_link_libraries(lua PUBLIC m)
endif()

if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86_64-macos|arm64-macos)$")
  set(LUA_ASAN_SOURCE)
  if(WITH_ASAN)
    set(LUA_ASAN_SOURCE "${LUA_SRC_DIR}/lua_asan.cpp")
  endif()

  defold_add_executable(lua_bin "${LUA_CORE_DIR}/lua.c" ${LUA_ASAN_SOURCE})
  set_target_properties(lua_bin PROPERTIES OUTPUT_NAME lua)
  target_include_directories(lua_bin PRIVATE "${LUA_CORE_DIR}" "${LUA_SRC_DIR}")
  target_compile_definitions(lua_bin PRIVATE ${LUA_DEFINES})
  target_link_libraries(lua_bin PRIVATE lua)

  defold_add_executable(luac "${LUA_CORE_DIR}/luac.c" "${LUA_CORE_DIR}/print.c" ${LUA_ASAN_SOURCE})
  target_include_directories(luac PRIVATE "${LUA_CORE_DIR}" "${LUA_SRC_DIR}")
  target_compile_definitions(luac PRIVATE ${LUA_DEFINES})
  target_link_libraries(luac PRIVATE lua)

  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    target_link_libraries(lua_bin PRIVATE dl)
    target_link_libraries(luac PRIVATE dl)
  endif()
endif()

defold_install_targets(TARGETS lua
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

if(TARGET lua_bin)
  defold_install_targets(TARGETS lua_bin
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

if(TARGET luac)
  defold_install_targets(TARGETS luac
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

install(FILES
  "${LUA_CORE_DIR}/lua.h"
  "${LUA_CORE_DIR}/lauxlib.h"
  "${LUA_CORE_DIR}/lualib.h"
  "${LUA_CORE_DIR}/luaconf.h"
  DESTINATION include/lua)

file(GLOB_RECURSE LUA_CSHARP_SDK_FILES CONFIGURE_DEPENDS
     RELATIVE "${LUA_SRC_DIR}/cs/dmsdk"
     "${LUA_SRC_DIR}/cs/dmsdk/*.cs")
foreach(_lua_cs_file IN LISTS LUA_CSHARP_SDK_FILES)
  get_filename_component(_lua_cs_dir "${_lua_cs_file}" DIRECTORY)
  if(_lua_cs_dir)
    install(FILES "${LUA_SRC_DIR}/cs/dmsdk/${_lua_cs_file}" DESTINATION "sdk/cs/dmsdk/${_lua_cs_dir}")
  else()
    install(FILES "${LUA_SRC_DIR}/cs/dmsdk/${_lua_cs_file}" DESTINATION sdk/cs/dmsdk)
  endif()
endforeach()

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
