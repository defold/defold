cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(SCRIPT_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(SCRIPT_ROOT "${SCRIPT_TEST_ROOT}/../.." ABSOLUTE)
set(SCRIPT_SRC_DIR "${SCRIPT_ROOT}/src")
set(SCRIPT_TEST_RUNTIME_BUILD_DIR "${SCRIPT_ROOT}/build/src/test")
set(SCRIPT_TEST_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/test")
set(SCRIPT_TEST_PROTO "${SCRIPT_TEST_ROOT}/test_ddf.proto")
set(SCRIPT_TEST_PROTO_CC "${SCRIPT_TEST_GEN_DIR}/test_ddf.cpp")
set(SCRIPT_TEST_PROTO_H "${SCRIPT_TEST_GEN_DIR}/test_ddf.h")
set(SCRIPT_TEST_PROTO_PY "${CMAKE_CURRENT_BINARY_DIR}/python/test_ddf_pb2.py")
set(SCRIPT_TEST_PUBLIC_INCLUDE_DIR "${DEFOLD_BUILD_INCLUDE_DIR}")

file(MAKE_DIRECTORY "${SCRIPT_TEST_PUBLIC_INCLUDE_DIR}/script")
configure_file("${SCRIPT_TEST_ROOT}/test_script.h" "${SCRIPT_TEST_PUBLIC_INCLUDE_DIR}/script/test_script.h" COPYONLY)

defold_protoc_gen_cpp("${SCRIPT_TEST_PROTO_CC}" "${SCRIPT_TEST_PROTO}" INCLUDES "${SCRIPT_TEST_ROOT}" "${SCRIPT_SRC_DIR}" "${DEFOLD_SDK_ROOT}/share/proto")
defold_protoc_gen_py("${SCRIPT_TEST_PROTO_PY}" "${SCRIPT_TEST_PROTO}" INCLUDES "${SCRIPT_TEST_ROOT}" "${SCRIPT_SRC_DIR}" "${DEFOLD_SDK_ROOT}/share/proto")
add_custom_target(script_test_proto_py DEPENDS "${SCRIPT_TEST_PROTO_PY}")

file(GLOB SCRIPT_TEST_LUA_SOURCES CONFIGURE_DEPENDS "${SCRIPT_TEST_ROOT}/scripts/*.lua")
set(SCRIPT_TEST_LUAC_OUTPUTS)
foreach(_script_lua IN LISTS SCRIPT_TEST_LUA_SOURCES)
  get_filename_component(_script_lua_name "${_script_lua}" NAME_WE)
  set(_script_luac "${SCRIPT_TEST_RUNTIME_BUILD_DIR}/scripts/${_script_lua_name}.luac")
  add_custom_command(
    OUTPUT "${_script_luac}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SCRIPT_TEST_RUNTIME_BUILD_DIR}/scripts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_script_lua}" "${_script_luac}"
    DEPENDS "${_script_lua}"
    VERBATIM
    COMMENT "Copying script test data ${_script_lua_name}.luac")
  list(APPEND SCRIPT_TEST_LUAC_OUTPUTS "${_script_luac}")
endforeach()

add_custom_target(script_test_lua_data DEPENDS ${SCRIPT_TEST_LUAC_OUTPUTS})

defold_add_library(test_ddf_proto STATIC "${SCRIPT_TEST_PROTO_CC}")
target_include_directories(test_ddf_proto PUBLIC
  "${SCRIPT_TEST_GEN_DIR}"
  "${SCRIPT_TEST_ROOT}")
add_dependencies(test_ddf_proto script_test_proto_py)
defold_target_link_libraries(test_ddf_proto "${TARGET_PLATFORM}" ddf dlib)

defold_add_library(test_script STATIC
  "${SCRIPT_TEST_ROOT}/test_script.cpp"
  "${SCRIPT_TEST_ROOT}/test_script_private.cpp")
target_include_directories(test_script PUBLIC
  "${SCRIPT_TEST_ROOT}"
  "${SCRIPT_SRC_DIR}"
  "${SCRIPT_TEST_PUBLIC_INCLUDE_DIR}")
defold_target_link_libraries(test_script "${TARGET_PLATFORM}" dlib lua script)

defold_install_targets(TARGETS test_script
                       ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
                       LIBRARY DESTINATION lib/${TARGET_PLATFORM}
                       RUNTIME DESTINATION bin/${TARGET_PLATFORM})

set(SCRIPT_TEST_EXPORTS ResourceProviderFile GraphicsAdapterNull)

function(script_configure_test target)
  target_include_directories(${target} PRIVATE
    "${SCRIPT_TEST_ROOT}"
    "${SCRIPT_SRC_DIR}"
    "${SCRIPT_TEST_GEN_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    testmain
    ddf
    dlib
    zip
    mbedtls
    profile_null
    lua
    platform_null
    graphics_null
    image
    graphics_proto
    extension
    resource
    script
    test_script)
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_libraries_web(${target} "${TARGET_PLATFORM}"
    library_sys.js
    library_script.js)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()
  add_dependencies(${target} script_test_lua_data)
  defold_register_test_target(${target} OFF "${SCRIPT_ROOT}"
    STAGE_FILES
      "build/src/test/scripts" "build/src/test/scripts")
  defold_register_test_with_server(${target} "${TARGET_PLATFORM}"
    PORT 9001
    WORKDIR "${SCRIPT_ROOT}"
    CONFIG_NAME unittest.cfg)
  if(TARGET run_${target}_server)
    add_dependencies(run_${target}_server script_test_lua_data)
  endif()
endfunction()

function(script_add_test target)
  set(_script_test_exports ${SCRIPT_TEST_EXPORTS})
  if(target STREQUAL "test_script_types")
    list(APPEND _script_test_exports ScriptTypesExt)
  endif()
  set(_script_export_file "${CMAKE_CURRENT_BINARY_DIR}/${target}_exported_symbols.cpp")
  defold_create_exported_symbols_file("${_script_export_file}" "${_script_test_exports}")
  defold_add_executable(${target} ${ARGN} "${_script_export_file}")
  script_configure_test(${target})
endfunction()

script_add_test(test_script_ddf "${SCRIPT_TEST_ROOT}/test_script_ddf.cpp")
target_link_libraries(test_script_ddf PRIVATE test_ddf_proto)

script_add_test(test_script_msg "${SCRIPT_TEST_ROOT}/test_script_msg.cpp")
target_link_libraries(test_script_msg PRIVATE test_ddf_proto)

script_add_test(test_script_hash "${SCRIPT_TEST_ROOT}/test_script_hash.cpp")
script_add_test(test_script_vmath "${SCRIPT_TEST_ROOT}/test_script_vmath.cpp")
script_add_test(test_script_table_log "${SCRIPT_TEST_ROOT}/test_script_table_log.cpp")
script_add_test(test_script_timer "${SCRIPT_TEST_ROOT}/test_script_timer.cpp")
script_add_test(test_script_sys "${SCRIPT_TEST_ROOT}/test_script_sys.cpp")
script_add_test(test_script_graphics "${SCRIPT_TEST_ROOT}/test_script_graphics.cpp")
script_add_test(test_script_lua "${SCRIPT_TEST_ROOT}/test_script_lua.cpp")
script_add_test(test_script_module "${SCRIPT_TEST_ROOT}/test_script_module.cpp")
script_add_test(test_script_json "${SCRIPT_TEST_ROOT}/test_script_json.cpp")
script_add_test(test_script_zlib "${SCRIPT_TEST_ROOT}/test_script_zlib.cpp")
script_add_test(test_script_user_type "${SCRIPT_TEST_ROOT}/test_script_user_type.cpp")
script_add_test(test_script_luasocket "${SCRIPT_TEST_ROOT}/test_script_luasocket.cpp")
script_add_test(test_script_bitop "${SCRIPT_TEST_ROOT}/test_script_bitop.cpp")
script_add_test(test_script_types "${SCRIPT_TEST_ROOT}/test_script_types.cpp")
script_add_test(test_script_types_null "${SCRIPT_TEST_ROOT}/test_script_types_null.cpp")

set(_table_embeds)
foreach(_table_data IN ITEMS
  table_cos_v0.dat table_sin_v0.dat table_cos_v1.dat table_sin_v1.dat
  table_v818192.dat table_tstring_v1.dat table_tstring_v2.dat table_tstring_v3.dat)
  set(_input "${SCRIPT_TEST_ROOT}/data/${_table_data}")
  set(_out_h "${CMAKE_CURRENT_BINARY_DIR}/data/${_table_data}.embed.h")
  set(_out_cpp "${CMAKE_CURRENT_BINARY_DIR}/data/${_table_data}.embed.cpp")
  defold_embed_to_header(INPUT "${_input}" OUTPUT_HEADER "${_out_h}" OUTPUT_CPP "${_out_cpp}")
  list(APPEND _table_embeds "${_out_cpp}")
endforeach()
script_add_test(test_script_table "${SCRIPT_TEST_ROOT}/test_script_table.cpp" ${_table_embeds})

install(FILES "${SCRIPT_TEST_ROOT}/test_script.h" DESTINATION include/script)
