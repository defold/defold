cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../../scripts/cmake")
include(defold)

set(GAMEOBJECT_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(GAMEOBJECT_ROOT "${GAMEOBJECT_TEST_ROOT}/../../.." ABSOLUTE)
set(GAMEOBJECT_SRC_DIR "${GAMEOBJECT_ROOT}/src")
set(GAMEOBJECT_PROTO_ROOT "${GAMEOBJECT_ROOT}/proto")
set(GAMEOBJECT_TEST_RUNTIME_DIR "${GAMEOBJECT_ROOT}/build/src/gameobject/test")

find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Java COMPONENTS Runtime Development REQUIRED)

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

function(gameobject_read_manifest out_var manifest)
  if(NOT EXISTS "${manifest}")
    message(FATAL_ERROR "Missing gameobject test manifest: ${manifest}")
  endif()

  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
  file(STRINGS "${manifest}" _manifest_lines)

  set(_entries)
  foreach(_line IN LISTS _manifest_lines)
    string(REGEX REPLACE "#.*$" "" _entry "${_line}")
    string(STRIP "${_entry}" _entry)
    if(NOT _entry STREQUAL "")
      list(APPEND _entries "${_entry}")
    endif()
  endforeach()

  set(${out_var} ${_entries} PARENT_SCOPE)
endfunction()

set(_GAMEOBJECT_BOB_LIGHT "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar")
if(NOT EXISTS "${_GAMEOBJECT_BOB_LIGHT}")
  message(FATAL_ERROR "bob-light.jar not found: ${_GAMEOBJECT_BOB_LIGHT}")
endif()

set(_GAMEOBJECT_TEST_DATA_FOLDERS_FILE "${GAMEOBJECT_TEST_ROOT}/test_data_folders.txt")
gameobject_read_manifest(_GAMEOBJECT_TEST_DATA_FOLDERS "${_GAMEOBJECT_TEST_DATA_FOLDERS_FILE}")
if(NOT _GAMEOBJECT_TEST_DATA_FOLDERS)
  message(FATAL_ERROR "No gameobject test data folders listed in ${_GAMEOBJECT_TEST_DATA_FOLDERS_FILE}")
endif()

function(gameobject_build_test_data out_target dir)
  list(FIND _GAMEOBJECT_TEST_DATA_FOLDERS "${dir}" _folder_idx)
  if(_folder_idx EQUAL -1)
    set(${out_target} "" PARENT_SCOPE)
    return()
  endif()

  set(_inputs_file "${GAMEOBJECT_TEST_ROOT}/${dir}/build.inputs")
  if(NOT EXISTS "${_inputs_file}")
    message(FATAL_ERROR "Missing gameobject test data input list: ${_inputs_file}")
  endif()

  file(GLOB _folder_sources CONFIGURE_DEPENDS LIST_DIRECTORIES false "${GAMEOBJECT_TEST_ROOT}/${dir}/*")
  list(FILTER _folder_sources EXCLUDE REGEX "/build/")
  list(FILTER _folder_sources EXCLUDE REGEX "\\.prebuilt_")
  list(FILTER _folder_sources EXCLUDE REGEX "\\.DS_Store$")

  set(_stamp "${CMAKE_CURRENT_BINARY_DIR}/.bob/${dir}.stamp")
  add_custom_command(
    OUTPUT "${_stamp}"
    COMMAND "${Python3_EXECUTABLE}" "${GAMEOBJECT_TEST_ROOT}/build_test_data.py"
      --bob-light "${_GAMEOBJECT_BOB_LIGHT}"
      --root "${GAMEOBJECT_TEST_ROOT}"
      --folder "${dir}"
      --inputs "${_inputs_file}"
      --output-root "${GAMEOBJECT_TEST_RUNTIME_DIR}"
      --stamp "${_stamp}"
      --platform "${TARGET_PLATFORM}"
      --javac "${Java_JAVAC_EXECUTABLE}"
      --protoc "${DEFOLD_PROTOC_EXECUTABLE}"
      --gameobject-proto "${GAMEOBJECT_PROTO_ROOT}/gameobject/gameobject_ddf.proto"
      --proto-include "${GAMEOBJECT_PROTO_ROOT}"
      --proto-include "${GAMEOBJECT_ROOT}/../ddf/src"
      --proto-include "${DEFOLD_SDK_ROOT}/share/proto"
      --proto-include "${DEFOLD_SDK_ROOT}/ext/include"
    DEPENDS "${GAMEOBJECT_TEST_ROOT}/build_test_data.py" "${_inputs_file}" ${_folder_sources}
    WORKING_DIRECTORY "${GAMEOBJECT_TEST_ROOT}"
    COMMENT "Building gameobject test data folder ${dir}"
    VERBATIM)

  set(_target "gameobject_test_content_${dir}")
  add_custom_target(${_target} DEPENDS "${_stamp}")
  set(${out_target} ${_target} PARENT_SCOPE)
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
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gameobject/test/${dir}")
  file(GLOB _dir_files CONFIGURE_DEPENDS "${_test_dir}/*")

  set(_cpp)
  set(_proto)
  set(_a_pb_files)
  set(_b_pb_files)
  set(_c_pb_files)
  set(_mt_pb_files)
  set(_it_pb_files)
  set(_rt_pb_files)
  set(_no_user_data_files)
  foreach(_src IN LISTS _dir_files)
    get_filename_component(_ext "${_src}" EXT)
    if(_ext STREQUAL ".cpp")
      list(APPEND _cpp "${_src}")
    elseif(_ext STREQUAL ".proto")
      list(APPEND _proto "${_src}")
    elseif(_ext STREQUAL ".a_pb")
      list(APPEND _a_pb_files "${_src}")
    elseif(_ext STREQUAL ".b_pb")
      list(APPEND _b_pb_files "${_src}")
    elseif(_ext STREQUAL ".c_pb")
      list(APPEND _c_pb_files "${_src}")
    elseif(_ext STREQUAL ".mt_pb")
      list(APPEND _mt_pb_files "${_src}")
    elseif(_ext STREQUAL ".it_pb")
      list(APPEND _it_pb_files "${_src}")
    elseif(_ext STREQUAL ".rt_pb")
      list(APPEND _rt_pb_files "${_src}")
    elseif(_src MATCHES "\\.no_user_data$")
      list(APPEND _no_user_data_files "${_src}")
    endif()
  endforeach()

  set(_generated_proto_cpp)
  foreach(_proto_file IN LISTS _proto)
    get_filename_component(_base "${_proto_file}" NAME_WE)
    set(_proto_cpp "${CMAKE_CURRENT_BINARY_DIR}/gameobject/test/${dir}/${_base}.cpp")
    defold_protoc_gen_cpp("${_proto_cpp}" "${_proto_file}"
      INCLUDES "${GAMEOBJECT_TEST_ROOT}" "${GAMEOBJECT_SRC_DIR}" "${GAMEOBJECT_PROTO_ROOT}" "${DEFOLD_SDK_ROOT}/share/proto")
    list(APPEND _generated_proto_cpp "${_proto_cpp}")
  endforeach()

  set(_resources)
  gameobject_build_test_data(_content_target "${dir}")

  set(_encoded_specs
    ".a_pb|.a|${GAMEOBJECT_TEST_ROOT}/component/test_gameobject_component_ddf.proto|TestGameObjectDDF.AResource|_a_pb_files"
    ".b_pb|.b|${GAMEOBJECT_TEST_ROOT}/component/test_gameobject_component_ddf.proto|TestGameObjectDDF.BResource|_b_pb_files"
    ".c_pb|.c|${GAMEOBJECT_TEST_ROOT}/component/test_gameobject_component_ddf.proto|TestGameObjectDDF.CResource|_c_pb_files"
    ".mt_pb|.mt|${GAMEOBJECT_TEST_ROOT}/message/test_gameobject_message_ddf.proto|TestGameObjectDDF.MessageTarget|_mt_pb_files"
    ".it_pb|.it|${GAMEOBJECT_TEST_ROOT}/input/test_gameobject_input_ddf.proto|TestGameObjectDDF.InputTarget|_it_pb_files"
    ".rt_pb|.rt|${GAMEOBJECT_TEST_ROOT}/reload/test_gameobject_reload_ddf.proto|TestGameObjectDDF.ReloadTarget|_rt_pb_files"
    ".no_user_data|.no_user_datac|${GAMEOBJECT_TEST_ROOT}/props/test_gameobject_props_ddf.proto|TestGameObjectDDF.NoUserDataResource|_no_user_data_files")
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
    list(GET _spec_list 4 _encoded_sources_var)
    set(_encoded_sources ${${_encoded_sources_var}})
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
    "${CMAKE_CURRENT_BINARY_DIR}/${dir}"
    "${CMAKE_CURRENT_BINARY_DIR}/gameobject/test/${dir}"
    "${DEFOLD_GAMEOBJECT_BINARY_DIR}/src/gameobject"
    "${DEFOLD_GAMEOBJECT_BINARY_DIR}/src/proto"
    "${DEFOLD_GAMEOBJECT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  if(TARGET gameobject_proto_headers)
    add_dependencies(${_target} gameobject_proto_headers)
  endif()
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
    set_source_files_properties(${_resources} PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    target_sources(${_target} PRIVATE ${_resources})
    add_custom_target(${_target}_resources DEPENDS ${_resources})
    add_dependencies(${_target} ${_target}_resources)
    if(TARGET run_${_target})
      add_dependencies(run_${_target} ${_target}_resources)
    endif()
  endif()
  if(_content_target)
    add_dependencies(${_target} ${_content_target})
    if(TARGET run_${_target})
      add_dependencies(run_${_target} ${_content_target})
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
gameobject_add_test(props EXTS ".cpp" ".proto" ".go_pb" ".script" ".collection" ".no_user_data")
gameobject_add_test(reload EXTS ".go_pb" ".script" ".cpp" ".proto" ".rt_pb")
gameobject_add_test(script)
gameobject_add_test(lua)
