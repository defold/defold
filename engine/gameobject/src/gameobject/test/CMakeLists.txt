cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../../scripts/cmake")
include(defold)

set(GAMEOBJECT_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(GAMEOBJECT_ROOT "${GAMEOBJECT_TEST_ROOT}/../../.." ABSOLUTE)
set(GAMEOBJECT_SRC_DIR "${GAMEOBJECT_ROOT}/src")
set(GAMEOBJECT_PROTO_ROOT "${GAMEOBJECT_ROOT}/proto")
set(GAMEOBJECT_TEST_RUNTIME_DIR "${GAMEOBJECT_ROOT}/build/src/gameobject/test")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(WIN32)
  set(GAMEOBJECT_TEST_PYTHONPATH_SEP ";")
else()
  set(GAMEOBJECT_TEST_PYTHONPATH_SEP ":")
endif()
set(GAMEOBJECT_TEST_PYTHONPATH
  "${CMAKE_BINARY_DIR}/proto${GAMEOBJECT_TEST_PYTHONPATH_SEP}${CMAKE_BINARY_DIR}/proto/gameobject${GAMEOBJECT_TEST_PYTHONPATH_SEP}${DEFOLD_SDK_ROOT}/lib/python${GAMEOBJECT_TEST_PYTHONPATH_SEP}${DEFOLD_SDK_ROOT}/ext/lib/python")

set(GAMEOBJECT_TEST_EXPORTS
  ResourceTypeGameObject
  ResourceTypeCollection
  ResourceTypeScript
  ResourceTypeLua
  ResourceTypeAnim
  ResourceProviderFile
  ComponentTypeScript
  ComponentTypeAnim)
defold_create_exported_symbols_file("${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp" "${GAMEOBJECT_TEST_EXPORTS}")

function(gameobject_compile_python_resource out_file src_file kind)
  add_custom_command(
    OUTPUT "${out_file}"
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${GAMEOBJECT_TEST_PYTHONPATH}" "${Python3_EXECUTABLE}"
      "${GAMEOBJECT_TEST_ROOT}/compile_gameobject_test_resource.py"
      --type "${kind}"
      --input "${src_file}"
      --output "${out_file}"
    DEPENDS "${src_file}" "${GAMEOBJECT_TEST_ROOT}/compile_gameobject_test_resource.py" gameobject_proto_py
    VERBATIM
    COMMENT "Compiling ${src_file}")
endfunction()

function(gameobject_compile_ddf_resource out_file src_file schema_proto message_name)
  defold_protoc_encode(
    "${out_file}"
    "${src_file}"
    "${schema_proto}"
    "${message_name}"
    INCLUDES
      "${GAMEOBJECT_TEST_ROOT}"
      "${GAMEOBJECT_TEST_ROOT}/input"
      "${GAMEOBJECT_PROTO_ROOT}"
      "${DEFOLD_SDK_ROOT}/share/proto")
endfunction()

function(gameobject_add_test dir)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs EXTS)
  cmake_parse_arguments(GOT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  if(NOT GOT_EXTS)
    set(GOT_EXTS ".cpp" ".proto" ".go_pb" ".script")
  endif()

  set(_test_dir "${GAMEOBJECT_TEST_ROOT}/${dir}")
  set(_runtime_dir "${GAMEOBJECT_TEST_RUNTIME_DIR}/${dir}")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/src/gameobject/test/${dir}")
  file(GLOB _cpp CONFIGURE_DEPENDS "${_test_dir}/*.cpp")
  file(GLOB _proto CONFIGURE_DEPENDS "${_test_dir}/*.proto")

  set(_generated_proto_cpp)
  foreach(_proto_file IN LISTS _proto)
    get_filename_component(_base "${_proto_file}" NAME_WE)
    set(_proto_cpp "${CMAKE_CURRENT_BINARY_DIR}/gameobject/test/${dir}/${_base}.cpp")
    defold_protoc_gen_cpp("${_proto_cpp}" "${_proto_file}"
      INCLUDES "${GAMEOBJECT_TEST_ROOT}" "${GAMEOBJECT_SRC_DIR}" "${GAMEOBJECT_PROTO_ROOT}" "${DEFOLD_SDK_ROOT}/share/proto")
    list(APPEND _generated_proto_cpp "${_proto_cpp}")
  endforeach()

  set(_resources)
  list(FIND GOT_EXTS ".go_pb" _go_idx)
  if(NOT _go_idx EQUAL -1)
    file(GLOB _go_files CONFIGURE_DEPENDS "${_test_dir}/*.go_pb")
    foreach(_src IN LISTS _go_files)
      get_filename_component(_base "${_src}" NAME_WE)
      set(_out "${_runtime_dir}/${_base}.goc")
      gameobject_compile_python_resource("${_out}" "${_src}" gameobject)
      list(APPEND _resources "${_out}")
    endforeach()
  endif()

  list(FIND GOT_EXTS ".script" _script_idx)
  if(NOT _script_idx EQUAL -1)
    file(GLOB _script_files CONFIGURE_DEPENDS "${_test_dir}/*.script" "${_test_dir}/*.lua")
    foreach(_src IN LISTS _script_files)
      get_filename_component(_base "${_src}" NAME_WE)
      get_filename_component(_ext "${_src}" EXT)
      if(_ext STREQUAL ".lua")
        set(_out "${_runtime_dir}/${_base}.luac")
      else()
        set(_out "${_runtime_dir}/${_base}.scriptc")
      endif()
      gameobject_compile_python_resource("${_out}" "${_src}" lua)
      list(APPEND _resources "${_out}")
    endforeach()
  endif()

  list(FIND GOT_EXTS ".collection" _collection_idx)
  if(NOT _collection_idx EQUAL -1)
    file(GLOB _collection_files CONFIGURE_DEPENDS "${_test_dir}/*.collection")
    foreach(_src IN LISTS _collection_files)
      get_filename_component(_base "${_src}" NAME_WE)
      set(_out "${_runtime_dir}/${_base}.collectionc")
      gameobject_compile_python_resource("${_out}" "${_src}" collection)
      list(APPEND _resources "${_out}")
    endforeach()
  endif()

  set(_encoded_specs
    ".a_pb|.a|${GAMEOBJECT_TEST_ROOT}/component/test_gameobject_component_ddf.proto|TestGameObjectDDF.AResource"
    ".b_pb|.b|${GAMEOBJECT_TEST_ROOT}/component/test_gameobject_component_ddf.proto|TestGameObjectDDF.BResource"
    ".c_pb|.c|${GAMEOBJECT_TEST_ROOT}/component/test_gameobject_component_ddf.proto|TestGameObjectDDF.CResource"
    ".mt_pb|.mt|${GAMEOBJECT_TEST_ROOT}/message/test_gameobject_message_ddf.proto|TestGameObjectDDF.MessageTarget"
    ".it_pb|.it|${GAMEOBJECT_TEST_ROOT}/input/test_gameobject_input_ddf.proto|TestGameObjectDDF.InputTarget"
    ".rt_pb|.rt|${GAMEOBJECT_TEST_ROOT}/reload/test_gameobject_reload_ddf.proto|TestGameObjectDDF.ReloadTarget"
    ".no_user_data|.no_user_datac|${GAMEOBJECT_TEST_ROOT}/props/test_gameobject_props_ddf.proto|TestGameObjectDDF.NoUserDataResource")
  foreach(_spec IN LISTS _encoded_specs)
    string(REPLACE "|" ";" _spec_list "${_spec}")
    list(GET _spec_list 0 _src_ext)
    list(FIND GOT_EXTS "${_src_ext}" _ext_idx)
    if(_ext_idx EQUAL -1)
      continue()
    endif()
    list(GET _spec_list 1 _out_ext)
    list(GET _spec_list 2 _schema)
    list(GET _spec_list 3 _message)
    file(GLOB _encoded_sources CONFIGURE_DEPENDS "${_test_dir}/*${_src_ext}")
    foreach(_src IN LISTS _encoded_sources)
      get_filename_component(_base "${_src}" NAME_WE)
      set(_out "${_runtime_dir}/${_base}${_out_ext}")
      gameobject_compile_ddf_resource("${_out}" "${_src}" "${_schema}" "${_message}")
      list(APPEND _resources "${_out}")
    endforeach()
  endforeach()

  set(_target "test_gameobject_${dir}")
  defold_add_executable(${_target}
    "${GAMEOBJECT_TEST_ROOT}/test_main.cpp"
    ${_cpp}
    ${_generated_proto_cpp}
    "${CMAKE_CURRENT_BINARY_DIR}/__exported_symbols.cpp")
  target_include_directories(${_target} PRIVATE
    "${GAMEOBJECT_SRC_DIR}"
    "${GAMEOBJECT_SRC_DIR}/gameobject"
    "${GAMEOBJECT_TEST_ROOT}"
    "${GAMEOBJECT_TEST_ROOT}/.."
    "${GAMEOBJECT_PROTO_ROOT}"
    "${CMAKE_BINARY_DIR}/src/gameobject/test/${dir}"
    "${CMAKE_BINARY_DIR}/src/gameobject"
    "${CMAKE_BINARY_DIR}/src/proto"
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  defold_target_link_libraries(${_target} "${TARGET_PLATFORM}"
    testmain
    ddf
    dlib
    zip
    mbedtls
    resource
    script
    graphics_null
    platform_null
    lua
    extension
    profile_null
    rig
    hid
    gameobject)
  defold_target_link_socket(${_target} "${TARGET_PLATFORM}" PRIVATE)
  defold_target_link_app(${_target} "${TARGET_PLATFORM}" SCOPE PRIVATE)
  defold_target_link_libraries_web(${_target} "${TARGET_PLATFORM}"
    library_sys.js
    library_script.js)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${_target} PRIVATE Threads::Threads dl)
  endif()
  defold_register_test_target(${_target} ON "${GAMEOBJECT_ROOT}"
    STAGE_FILES
      "build/src/gameobject/test/${dir}" "build/src/gameobject/test/${dir}")
  if(_resources)
    add_custom_target(${_target}_resources DEPENDS ${_resources})
    add_dependencies(${_target} ${_target}_resources)
    if(TARGET run_${_target})
      add_dependencies(run_${_target} ${_target}_resources)
    endif()
  endif()
endfunction()

gameobject_add_test(anim)
gameobject_add_test(bones EXTS ".cpp" ".a_pb" ".go_pb" ".script")
gameobject_add_test(collection EXTS ".cpp" ".go_pb" ".script" ".collection" ".a_pb")
gameobject_add_test(spawn_delete EXTS ".cpp" ".proto" ".go_pb" ".script" ".a_pb")
gameobject_add_test(component EXTS ".cpp" ".proto" ".go_pb" ".script" ".a_pb" ".b_pb" ".c_pb")
gameobject_add_test(delete)
gameobject_add_test(factory EXTS ".cpp" ".a_pb" ".go_pb" ".script")
gameobject_add_test(hierarchy)
gameobject_add_test(id)
gameobject_add_test(input EXTS ".go_pb" ".script" ".cpp" ".proto" ".it_pb")
gameobject_add_test(message EXTS ".go_pb" ".script" ".cpp" ".proto" ".mt_pb")
gameobject_add_test(props)
gameobject_add_test(reload EXTS ".go_pb" ".script" ".cpp" ".proto" ".rt_pb")
gameobject_add_test(script)
gameobject_add_test(lua)
