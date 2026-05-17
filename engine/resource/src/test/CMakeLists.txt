cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(RESOURCE_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RESOURCE_ROOT "${RESOURCE_TEST_ROOT}/../..")
set(RESOURCE_SRC_DIR "${RESOURCE_ROOT}/src")
set(RESOURCE_TEST_BUILD_DIR "${RESOURCE_ROOT}/build/src/test")
set(RESOURCE_TEST_DATA_SCRIPT "${RESOURCE_TEST_ROOT}/build_cmake_test_data.py")

find_program(RESOURCE_PYTHON NAMES python3 python)
if(NOT RESOURCE_PYTHON)
  message(FATAL_ERROR "python3/python not found")
endif()

set(RESOURCE_BOB_LIGHT "${DEFOLD_SDK_ROOT}/share/java/bob-light.jar")
if(NOT EXISTS "${RESOURCE_BOB_LIGHT}")
  message(FATAL_ERROR "bob-light.jar not found: ${RESOURCE_BOB_LIGHT}")
endif()

set(RESOURCE_ARCHIVE_BASE_SOURCES
  "archive_data/file4.ad"
  "archive_data/file1.ad"
  "archive_data/file3.ad"
  "archive_data/file2.ad"
  "archive_data/file5.script")

function(resource_compile_copy out_var rel_path)
  get_filename_component(_resource_src_ext "${rel_path}" LAST_EXT)
  if(_resource_src_ext STREQUAL ".ad")
    string(REGEX REPLACE "\\.ad$" ".adc" _resource_out_rel "${rel_path}")
  elseif(_resource_src_ext STREQUAL ".script")
    string(REGEX REPLACE "\\.script$" ".scriptc" _resource_out_rel "${rel_path}")
  else()
    message(FATAL_ERROR "resource_compile_copy: unsupported resource extension: ${rel_path}")
  endif()

  set(_resource_src "${RESOURCE_TEST_ROOT}/${rel_path}")
  set(_resource_out "${RESOURCE_TEST_BUILD_DIR}/${_resource_out_rel}")
  get_filename_component(_resource_out_dir "${_resource_out}" DIRECTORY)
  add_custom_command(
    OUTPUT "${_resource_out}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_resource_out_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_resource_src}" "${_resource_out}"
    DEPENDS "${_resource_src}"
    VERBATIM
    COMMENT "Copying resource test data ${rel_path}")
  set(${out_var} "${_resource_out}" PARENT_SCOPE)
endfunction()

function(resource_archive out_var name use_compression)
  set(_resource_archive_inputs ${ARGN})
  set(_resource_archive_depends "${RESOURCE_ARCHIVE_ORDER_DEPENDS}")
  list(APPEND _resource_archive_depends "${RESOURCE_BOB_LIGHT}" ${_resource_archive_inputs})
  set(_resource_archive_outputs
    "${RESOURCE_TEST_BUILD_DIR}/${name}.dmanifest"
    "${RESOURCE_TEST_BUILD_DIR}/${name}.arci"
    "${RESOURCE_TEST_BUILD_DIR}/${name}.arcd"
    "${RESOURCE_TEST_BUILD_DIR}/${name}.manifest_hash")

  set(_resource_archive_flags -m)
  if(use_compression)
    list(APPEND _resource_archive_flags -c)
  endif()

  set(_resource_has_liveupdate FALSE)
  foreach(_resource_archive_input IN LISTS _resource_archive_inputs)
    get_filename_component(_resource_archive_input_name "${_resource_archive_input}" NAME)
    if(_resource_archive_input_name MATCHES "liveupdate")
      set(_resource_has_liveupdate TRUE)
    endif()
  endforeach()
  if(_resource_has_liveupdate)
    list(APPEND _resource_archive_outputs "${RESOURCE_TEST_BUILD_DIR}/${name}.zip")
  endif()

  add_custom_command(
    OUTPUT ${_resource_archive_outputs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${RESOURCE_TEST_BUILD_DIR}"
    COMMAND "${Java_JAVA_EXECUTABLE}" -cp "${RESOURCE_BOB_LIGHT}" com.dynamo.bob.archive.ArchiveBuilder
            "${RESOURCE_TEST_BUILD_DIR}"
            "${RESOURCE_TEST_BUILD_DIR}/${name}"
            ${_resource_archive_flags}
            ${_resource_archive_inputs}
    DEPENDS ${_resource_archive_depends}
    VERBATIM
    COMMAND_EXPAND_LISTS
    COMMENT "Generating resource test archive ${name}")
  set(${out_var} ${_resource_archive_outputs} PARENT_SCOPE)
  set(RESOURCE_ARCHIVE_ORDER_DEPENDS ${_resource_archive_outputs} PARENT_SCOPE)
endfunction()

set(RESOURCE_ARCHIVE_OUTPUTS)
set(RESOURCE_BASE_ARCHIVE_OUTPUTS)
foreach(_resource_rel IN LISTS RESOURCE_ARCHIVE_BASE_SOURCES)
  resource_compile_copy(_resource_out "${_resource_rel}")
  list(APPEND RESOURCE_ARCHIVE_OUTPUTS "${_resource_out}")
  list(APPEND RESOURCE_BASE_ARCHIVE_OUTPUTS "${_resource_out}")
endforeach()
foreach(_resource_rel IN ITEMS "archive_data/liveupdate.file7.ad" "archive_data/liveupdate.file6.script")
  resource_compile_copy(_resource_out "${_resource_rel}")
  list(APPEND RESOURCE_ARCHIVE_OUTPUTS "${_resource_out}")
endforeach()

file(GLOB RESOURCE_PROTO_MESSAGE_SOURCES CONFIGURE_DEPENDS "${RESOURCE_TEST_ROOT}/*.*_pb")
set(RESOURCE_PROTO_MESSAGE_OUTPUTS)
foreach(_resource_proto_message IN LISTS RESOURCE_PROTO_MESSAGE_SOURCES)
  get_filename_component(_resource_proto_message_name "${_resource_proto_message}" NAME)
  if(_resource_proto_message_name MATCHES "\\.cont_pb$")
    set(_resource_message "TestResource.ResourceContainerDesc")
    set(_resource_out_ext ".cont")
    string(REGEX REPLACE "\\.cont_pb$" "" _resource_proto_message_name "${_resource_proto_message_name}")
  elseif(_resource_proto_message_name MATCHES "\\.foo_pb$")
    set(_resource_message "TestResource.ResourceFoo")
    set(_resource_out_ext ".foo")
    string(REGEX REPLACE "\\.foo_pb$" "" _resource_proto_message_name "${_resource_proto_message_name}")
  else()
    message(FATAL_ERROR "Unsupported resource proto test input: ${_resource_proto_message}")
  endif()
  set(_resource_proto_out "${RESOURCE_TEST_BUILD_DIR}/${_resource_proto_message_name}${_resource_out_ext}")
  defold_protoc_encode(
    "${_resource_proto_out}"
    "${_resource_proto_message}"
    "${RESOURCE_TEST_ROOT}/test_resource_ddf.proto"
    "${_resource_message}"
    INCLUDES "${RESOURCE_TEST_ROOT}")
  list(APPEND RESOURCE_PROTO_MESSAGE_OUTPUTS "${_resource_proto_out}")
endforeach()

set(RESOURCE_ZIP_INPUTS)
file(GLOB_RECURSE RESOURCE_ZIP_SOURCE_INPUTS CONFIGURE_DEPENDS "${RESOURCE_TEST_ROOT}/zipfiles/*")
foreach(_resource_zip_input IN LISTS RESOURCE_ZIP_SOURCE_INPUTS)
  if(IS_DIRECTORY "${_resource_zip_input}")
    continue()
  endif()
  file(RELATIVE_PATH _resource_zip_rel "${RESOURCE_TEST_ROOT}" "${_resource_zip_input}")
  get_filename_component(_resource_zip_ext "${_resource_zip_rel}" LAST_EXT)
  if(_resource_zip_ext STREQUAL ".ad" OR _resource_zip_ext STREQUAL ".script")
    resource_compile_copy(_resource_zip_out "${_resource_zip_rel}")
    list(APPEND RESOURCE_ZIP_INPUTS "${_resource_zip_out}")
  else()
    set(_resource_zip_out "${RESOURCE_TEST_BUILD_DIR}/${_resource_zip_rel}")
    get_filename_component(_resource_zip_out_dir "${_resource_zip_out}" DIRECTORY)
    add_custom_command(
      OUTPUT "${_resource_zip_out}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_resource_zip_out_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_resource_zip_input}" "${_resource_zip_out}"
      DEPENDS "${_resource_zip_input}"
      VERBATIM
      COMMENT "Copying resource zip test data ${_resource_zip_rel}")
    list(APPEND RESOURCE_ZIP_INPUTS "${_resource_zip_out}")
  endif()
endforeach()

set(RESOURCE_OVERRIDE_OUTPUTS)
file(GLOB_RECURSE RESOURCE_OVERRIDE_SOURCE_INPUTS CONFIGURE_DEPENDS "${RESOURCE_TEST_ROOT}/overrides/*")
foreach(_resource_override_input IN LISTS RESOURCE_OVERRIDE_SOURCE_INPUTS)
  if(IS_DIRECTORY "${_resource_override_input}")
    continue()
  endif()
  file(RELATIVE_PATH _resource_override_rel "${RESOURCE_TEST_ROOT}" "${_resource_override_input}")
  get_filename_component(_resource_override_ext "${_resource_override_rel}" LAST_EXT)
  if(_resource_override_ext STREQUAL ".ad" OR _resource_override_ext STREQUAL ".script")
    resource_compile_copy(_resource_override_out "${_resource_override_rel}")
    list(APPEND RESOURCE_OVERRIDE_OUTPUTS "${_resource_override_out}")
  else()
    set(_resource_override_out "${RESOURCE_TEST_BUILD_DIR}/${_resource_override_rel}")
    get_filename_component(_resource_override_out_dir "${_resource_override_out}" DIRECTORY)
    add_custom_command(
      OUTPUT "${_resource_override_out}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${_resource_override_out_dir}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_resource_override_input}" "${_resource_override_out}"
      DEPENDS "${_resource_override_input}"
      VERBATIM
      COMMENT "Copying resource override test data ${_resource_override_rel}")
    list(APPEND RESOURCE_OVERRIDE_OUTPUTS "${_resource_override_out}")
  endif()
endforeach()

resource_compile_copy(RESOURCE_EMPTY_SCRIPT_OUTPUT "empty.script")

resource_archive(RESOURCE_ARCHIVE_NO_LU_OUTPUTS resources_no_lu FALSE ${RESOURCE_BASE_ARCHIVE_OUTPUTS})
resource_archive(RESOURCE_ARCHIVE_UNCOMPRESSED_OUTPUTS resources FALSE ${RESOURCE_ARCHIVE_OUTPUTS})
resource_archive(RESOURCE_ARCHIVE_COMPRESSED_OUTPUTS resources_compressed TRUE ${RESOURCE_ARCHIVE_OUTPUTS})
resource_archive(RESOURCE_LU_BASE_OUTPUTS luresources_base FALSE ${RESOURCE_ARCHIVE_OUTPUTS})
resource_archive(RESOURCE_LU_COMPRESSED_OUTPUTS luresources_compressed TRUE ${RESOURCE_ARCHIVE_OUTPUTS})
resource_archive(RESOURCE_PB_OUTPUTS resources_pb FALSE ${RESOURCE_PROTO_MESSAGE_OUTPUTS})

set(RESOURCE_LU_OUTPUTS)
foreach(_resource_ext IN ITEMS arci arcd dmanifest manifest_hash)
  set(_resource_lu_output "${RESOURCE_TEST_BUILD_DIR}/luresources.${_resource_ext}")
  add_custom_command(
    OUTPUT "${_resource_lu_output}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${RESOURCE_TEST_BUILD_DIR}/luresources_base.${_resource_ext}"
            "${_resource_lu_output}"
    DEPENDS "${RESOURCE_TEST_BUILD_DIR}/luresources_base.${_resource_ext}"
    VERBATIM
    COMMENT "Copying resource liveupdate archive ${_resource_ext}")
  list(APPEND RESOURCE_LU_OUTPUTS "${_resource_lu_output}")
endforeach()

add_custom_command(
  OUTPUT "${RESOURCE_TEST_BUILD_DIR}/luresources.zip"
  COMMAND "${RESOURCE_PYTHON}" "${RESOURCE_TEST_DATA_SCRIPT}" add-to-zip
          --base-zip "${RESOURCE_TEST_BUILD_DIR}/luresources_base.zip"
          --output "${RESOURCE_TEST_BUILD_DIR}/luresources.zip"
          --cwd "${RESOURCE_TEST_BUILD_DIR}/zipfiles"
          ${RESOURCE_ZIP_INPUTS}
  DEPENDS "${RESOURCE_TEST_DATA_SCRIPT}" "${RESOURCE_TEST_BUILD_DIR}/luresources_base.zip" ${RESOURCE_ZIP_INPUTS}
  VERBATIM
  COMMAND_EXPAND_LISTS
  COMMENT "Generating resource liveupdate zip")
list(APPEND RESOURCE_LU_OUTPUTS "${RESOURCE_TEST_BUILD_DIR}/luresources.zip")

add_custom_command(
  OUTPUT "${RESOURCE_TEST_BUILD_DIR}/somedata"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${RESOURCE_TEST_BUILD_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${RESOURCE_TEST_ROOT}/files/somedata" "${RESOURCE_TEST_BUILD_DIR}/somedata"
  DEPENDS "${RESOURCE_TEST_ROOT}/files/somedata"
  VERBATIM
  COMMENT "Copying resource test file somedata")

add_custom_target(resource_test_data DEPENDS
  ${RESOURCE_ARCHIVE_NO_LU_OUTPUTS}
  ${RESOURCE_ARCHIVE_UNCOMPRESSED_OUTPUTS}
  ${RESOURCE_ARCHIVE_COMPRESSED_OUTPUTS}
  ${RESOURCE_LU_OUTPUTS}
  ${RESOURCE_LU_COMPRESSED_OUTPUTS}
  ${RESOURCE_PB_OUTPUTS}
  ${RESOURCE_OVERRIDE_OUTPUTS}
  "${RESOURCE_EMPTY_SCRIPT_OUTPUT}"
  "${RESOURCE_TEST_BUILD_DIR}/somedata")

function(resource_configure_test target run_test)
  set(_resource_exports ${ARGN})
  if(_resource_exports)
    set(_resource_export_file "${CMAKE_CURRENT_BINARY_DIR}/${target}_exported_symbols.cpp")
    defold_create_exported_symbols_file("${_resource_export_file}" "${_resource_exports}")
    target_sources(${target} PRIVATE "${_resource_export_file}")
  endif()
  target_include_directories(${target} PRIVATE
    "${RESOURCE_TEST_ROOT}"
    "${RESOURCE_SRC_DIR}"
    "${RESOURCE_ROOT}/proto"
    "${CMAKE_CURRENT_BINARY_DIR}/../..")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    testmain
    ddf
    dlib
    mbedtls
    profile_null
    lua
    resource)
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()
  add_dependencies(${target} resource_test_data)
  defold_register_test_target(${target} ${run_test} "${RESOURCE_ROOT}"
    STAGE_FILES
      "src/test/archive_data" "src/test/archive_data"
      "src/test/files" "src/test/files"
      "src/test/mountfiles" "src/test/mountfiles"
      "src/test/overrides" "src/test/overrides"
      "src/test/zipfiles" "src/test/zipfiles"
      "build/src/test" "build/src/test")
endfunction()

defold_add_executable(test_block_allocator "${RESOURCE_TEST_ROOT}/test_block_allocator.cpp")
resource_configure_test(test_block_allocator ON)

defold_add_executable(test_provider_file "${RESOURCE_TEST_ROOT}/test_provider_file.cpp")
resource_configure_test(test_provider_file ON ResourceProviderFile)

defold_add_executable(test_provider_zip "${RESOURCE_TEST_ROOT}/test_provider_zip.cpp")
resource_configure_test(test_provider_zip ON ResourceProviderZip)

defold_add_executable(test_provider_multi "${RESOURCE_TEST_ROOT}/test_provider_multi.cpp")
resource_configure_test(test_provider_multi ON ResourceProviderArchive ResourceProviderZip ResourceProviderFile)

defold_add_executable(test_resource_chunk_cache "${RESOURCE_TEST_ROOT}/test_resource_chunk_cache.cpp")
resource_configure_test(test_resource_chunk_cache ON ResourceProviderArchive)
