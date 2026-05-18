cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_ddf LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="DDF")

set(DDF_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(DDF_SRC_DIR "${DDF_ROOT}/src")
set(DDF_PROTO_DIR "${DDF_SRC_DIR}/ddf")
set(DDF_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/ddf")
set(DDF_PROTO_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/python/ddf")
set(DDF_PLUGIN_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/plugin")
set(DDF_PLUGIN_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/python")
set(DDF_MAJOR_VERSION 1)
set(DDF_MINOR_VERSION 0)

string(CONCAT _ddf_config_h
  "#define DDF_MAJOR_VERSION ${DDF_MAJOR_VERSION}\n"
  "#define DDF_MINOR_VERSION ${DDF_MINOR_VERSION}\n")
set(_ddf_config_h_path "${CMAKE_CURRENT_BINARY_DIR}/config.h")
if(EXISTS "${_ddf_config_h_path}")
  file(READ "${_ddf_config_h_path}" _ddf_config_h_existing)
else()
  set(_ddf_config_h_existing "")
endif()
if(NOT _ddf_config_h_existing STREQUAL _ddf_config_h)
  file(WRITE "${_ddf_config_h_path}" "${_ddf_config_h}")
endif()
unset(_ddf_config_h_existing)
unset(_ddf_config_h_path)
unset(_ddf_config_h)

set(DDF_PROTO_FILES
  "${DDF_PROTO_DIR}/ddf_extensions.proto"
  "${DDF_PROTO_DIR}/ddf_math.proto"
  "${DDF_PROTO_DIR}/ddf_struct.proto")

function(ddf_protoc_gen_cpp OUT_CPP SRC_PROTO)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs INCLUDES)
  cmake_parse_arguments(DDFCPP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  get_filename_component(_out_dir "${OUT_CPP}" DIRECTORY)
  get_filename_component(_src_abs "${SRC_PROTO}" ABSOLUTE)
  get_filename_component(_src_name_we "${SRC_PROTO}" NAME_WE)
  set(_out_h "${_out_dir}/${_src_name_we}.h")

  if(HOST_PLATFORM_IS_WINDOWS)
    set(_ddf_plugin_path "${DDF_SRC_DIR}/ddfc_cxx.bat")
    file(TO_NATIVE_PATH "${_ddf_plugin_path}" _ddf_plugin_native)
    set(_ddf_plugin_arg "--plugin=protoc-gen-ddf=${_ddf_plugin_native}")
  else()
    set(_ddf_plugin_path "${DDF_SRC_DIR}/ddfc_cxx")
    set(_ddf_plugin_arg "--plugin=protoc-gen-ddf=${_ddf_plugin_path}")
  endif()

  set(_PROTOC_BIN protoc)
  if(DEFINED DEFOLD_PROTOC_EXECUTABLE AND EXISTS "${DEFOLD_PROTOC_EXECUTABLE}")
    set(_PROTOC_BIN "${DEFOLD_PROTOC_EXECUTABLE}")
  endif()

  file(MAKE_DIRECTORY "${_out_dir}")
  set(_inc_dirs "${DDF_SRC_DIR}")
  if(DDFCPP_INCLUDES)
    list(APPEND _inc_dirs ${DDFCPP_INCLUDES})
  endif()
  list(APPEND _inc_dirs "${DEFOLD_EXT_INCLUDE_DIR}")
  list(REMOVE_DUPLICATES _inc_dirs)
  set(_inc_flags)
  foreach(_inc IN LISTS _inc_dirs)
    list(APPEND _inc_flags -I "${_inc}")
  endforeach()

  add_custom_command(
    OUTPUT "${OUT_CPP}" "${_out_h}"
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${DDF_PROTOC_PLUGIN_PYTHONPATH}" ${_PROTOC_BIN} ${_ddf_plugin_arg} --ddf_out=${_out_dir} ${_inc_flags} "${_src_abs}"
    DEPENDS "${_src_abs}" "${_ddf_plugin_path}" "${DDF_SRC_DIR}/ddfc.py" ${DDF_PROTOC_PLUGIN_PY_DEPS}
    VERBATIM
    COMMENT "Generating DDF C++ from ${SRC_PROTO}")
  set_source_files_properties("${OUT_CPP}" "${_out_h}" PROPERTIES GENERATED TRUE)
endfunction()

function(ddf_protoc_gen_core_protobuf_cpp OUT_CC SRC_PROTO)
  get_filename_component(_src_abs "${SRC_PROTO}" ABSOLUTE)
  get_filename_component(_src_name_we "${SRC_PROTO}" NAME_WE)
  set(_out_h "${DDF_GEN_DIR}/${_src_name_we}.pb.h")

  set(_PROTOC_BIN protoc)
  if(DEFINED DEFOLD_PROTOC_EXECUTABLE AND EXISTS "${DEFOLD_PROTOC_EXECUTABLE}")
    set(_PROTOC_BIN "${DEFOLD_PROTOC_EXECUTABLE}")
  endif()

  file(MAKE_DIRECTORY "${DDF_GEN_DIR}")
  add_custom_command(
    OUTPUT "${OUT_CC}" "${_out_h}"
    COMMAND ${_PROTOC_BIN} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} -I "${DDF_SRC_DIR}" -I "${DEFOLD_EXT_INCLUDE_DIR}" "${_src_abs}"
    DEPENDS "${_src_abs}"
    VERBATIM
    COMMENT "Generating protobuf C++ from ${SRC_PROTO}")
  set_source_files_properties("${OUT_CC}" "${_out_h}" PROPERTIES GENERATED TRUE)
endfunction()

set(DDF_PROTO_CC)
set(DDF_PROTO_HEADERS)
set(DDF_PROTO_PBCC)
set(DDF_PROTO_PY)
set(DDF_PROTO_PY_INIT "${DDF_PROTO_PY_DIR}/__init__.py")

set(_ddf_python_path_entries "${DDF_PLUGIN_PY_DIR}" "${DDF_SRC_DIR}")
if(DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
  file(TO_CMAKE_PATH "$ENV{PYTHONPATH}" _ddf_existing_pythonpath)
  list(APPEND _ddf_python_path_entries ${_ddf_existing_pythonpath})
endif()
cmake_path(CONVERT "${_ddf_python_path_entries}" TO_NATIVE_PATH_LIST DDF_PROTOC_PLUGIN_PYTHONPATH)

if(HOST_PLATFORM_IS_WINDOWS)
  set(DEFOLD_DDFC_CXX_EXECUTABLE "${DDF_SRC_DIR}/ddfc_cxx.bat" CACHE FILEPATH "Path to Defold DDF C++ protoc plugin" FORCE)
else()
  set(DEFOLD_DDFC_CXX_EXECUTABLE "${DDF_SRC_DIR}/ddfc_cxx" CACHE FILEPATH "Path to Defold DDF C++ protoc plugin" FORCE)
endif()
set(DEFOLD_DDFC_CXX_PYTHONPATH "${DDF_PROTOC_PLUGIN_PYTHONPATH}" CACHE STRING "Python path for Defold DDF C++ protoc plugin" FORCE)

set(DDF_PLUGIN_PROTO_SRC "${DEFOLD_EXT_INCLUDE_DIR}/google/protobuf/compiler/plugin.proto")
set(DDF_PLUGIN_PROTO "${DDF_PLUGIN_GEN_DIR}/plugin.proto")
set(DDF_PLUGIN_PY "${DDF_PLUGIN_PY_DIR}/plugin_pb2.py")
add_custom_command(
  OUTPUT "${DDF_PLUGIN_PROTO}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${DDF_PLUGIN_GEN_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DDF_PLUGIN_PROTO_SRC}" "${DDF_PLUGIN_PROTO}"
  DEPENDS "${DDF_PLUGIN_PROTO_SRC}"
  VERBATIM
  COMMENT "Copying protobuf compiler plugin.proto")
defold_protoc_gen_py("${DDF_PLUGIN_PY}" "${DDF_PLUGIN_PROTO}" INCLUDES "${DDF_PLUGIN_GEN_DIR}")
add_custom_target(ddf_plugin_py ALL DEPENDS "${DDF_PLUGIN_PY}")

foreach(_proto IN LISTS DDF_PROTO_FILES)
  get_filename_component(_base "${_proto}" NAME_WE)
  set(_py "${DDF_PROTO_PY_DIR}/${_base}_pb2.py")
  defold_protoc_gen_py("${_py}" "${_proto}" PYTHON_ROOT "${DDF_PLUGIN_PY_DIR}" INCLUDES "${DDF_SRC_DIR}")
  list(APPEND DDF_PROTO_PY "${_py}")
endforeach()

add_custom_target(ddf_proto_py ALL DEPENDS ${DDF_PROTO_PY} "${DDF_PROTO_PY_INIT}")

set(DDF_PROTOC_PLUGIN_PY_DEPS
  "${DDF_PLUGIN_PY}"
  "${DDF_PROTO_PY_DIR}/ddf_extensions_pb2.py"
  "${DDF_PROTO_PY_INIT}")

foreach(_proto IN LISTS DDF_PROTO_FILES)
  get_filename_component(_base "${_proto}" NAME_WE)
  set(_cc "${DDF_GEN_DIR}/${_base}.cpp")
  set(_h "${DDF_GEN_DIR}/${_base}.h")
  set(_pbcc "${DDF_GEN_DIR}/${_base}.pb.cc")
  ddf_protoc_gen_cpp("${_cc}" "${_proto}")
  ddf_protoc_gen_core_protobuf_cpp("${_pbcc}" "${_proto}")
  list(APPEND DDF_PROTO_CC "${_cc}")
  list(APPEND DDF_PROTO_HEADERS "${_h}")
  list(APPEND DDF_PROTO_PBCC "${_pbcc}")
endforeach()

set(DDF_SOURCES
  "${DDF_PROTO_DIR}/ddf.cpp"
  "${DDF_PROTO_DIR}/ddf_load.cpp"
  "${DDF_PROTO_DIR}/ddf_save.cpp"
  "${DDF_PROTO_DIR}/ddf_inputbuffer.cpp"
  "${DDF_PROTO_DIR}/ddf_util.cpp"
  "${DDF_PROTO_DIR}/ddf_message.cpp"
  "${DDF_PROTO_DIR}/ddf_loadcontext.cpp"
  "${DDF_PROTO_DIR}/ddf_outputstream.cpp")

defold_add_library(ddf STATIC ${DDF_SOURCES} ${DDF_PROTO_CC} ${DDF_PROTO_PBCC})
target_include_directories(ddf PUBLIC
  "${DDF_SRC_DIR}"
  "${DDF_PROTO_DIR}/.."
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(ddf PRIVATE "DLIB_LOG_DOMAIN=\"DDF\"")
add_dependencies(ddf ddf_proto_py ddf_plugin_py)

defold_add_library(ddf_noasan STATIC ${DDF_SOURCES} ${DDF_PROTO_CC})
target_include_directories(ddf_noasan PUBLIC
  "${DDF_SRC_DIR}"
  "${DDF_PROTO_DIR}/.."
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(ddf_noasan PRIVATE "DLIB_LOG_DOMAIN=\"DDF\"")
add_dependencies(ddf_noasan ddf_proto_py ddf_plugin_py)

defold_install_targets(TARGETS ddf ddf_noasan
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES "${DDF_PROTO_DIR}/ddf.h" ${DDF_PROTO_HEADERS} DESTINATION include/ddf)
install(FILES "${DDF_SRC_DIR}/waf_ddf.py" DESTINATION lib/python)
install(FILES
  "${DDF_SRC_DIR}/ddfc.py"
  "${DDF_SRC_DIR}/ddfc_java"
  "${DDF_SRC_DIR}/ddfc_cxx"
  DESTINATION bin
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES "${DDF_SRC_DIR}/ddfc_cxx.bat" DESTINATION bin)
install(FILES ${DDF_PROTO_FILES} DESTINATION share/proto/ddf)
install(FILES "${DDF_PLUGIN_PY}" DESTINATION lib/python)
install(FILES "${DDF_PROTO_PY_INIT}" ${DDF_PROTO_PY} DESTINATION lib/python/ddf)
install(DIRECTORY "${DDF_SRC_DIR}/dmsdk/" DESTINATION sdk/include/dmsdk FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
