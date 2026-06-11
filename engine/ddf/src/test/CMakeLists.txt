set(DDF_TEST_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(DDF_TEST_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/test")

set(DDF_TEST_PROTO_FILES
  "${DDF_TEST_SRC_DIR}/test_ddf_proto.proto"
  "${DDF_TEST_SRC_DIR}/test_ddf_import.proto")

set(DDF_TEST_PROTO_CC)
set(DDF_TEST_PROTO_PBCC)
foreach(_proto IN LISTS DDF_TEST_PROTO_FILES)
  get_filename_component(_base "${_proto}" NAME_WE)
  set(_cc "${DDF_TEST_GEN_DIR}/${_base}.cpp")
  set(_pbcc "${DDF_TEST_GEN_DIR}/${_base}.pb.cc")
  ddf_protoc_gen_cpp("${_cc}" "${_proto}" INCLUDES "${DDF_TEST_SRC_DIR}")
  defold_protoc_gen_protobuf_cpp("${_pbcc}" "${_proto}" INCLUDES "${DDF_SRC_DIR}" "${DDF_TEST_SRC_DIR}")
  list(APPEND DDF_TEST_PROTO_CC "${_cc}")
  list(APPEND DDF_TEST_PROTO_PBCC "${_pbcc}")
endforeach()

defold_add_executable(test_ddf
  "${DDF_TEST_SRC_DIR}/test_ddf.cpp"
  "${DDF_TEST_SRC_DIR}/test_ddf_struct.cpp"
  ${DDF_TEST_PROTO_CC}
  ${DDF_TEST_PROTO_PBCC})

target_include_directories(test_ddf PRIVATE
  "${DDF_TEST_SRC_DIR}"
  "${DDF_SRC_DIR}"
  "${DDF_PROTO_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/../..")

defold_target_link_libraries(test_ddf "${TARGET_PLATFORM}"
  testmain
  protobuf
  dlib
  profile_null
  ddf)
defold_target_link_socket(test_ddf "${TARGET_PLATFORM}" PRIVATE)

if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_ddf PRIVATE Threads::Threads dl)
endif()

defold_target_link_libraries_web(test_ddf "${TARGET_PLATFORM}" library_sys.js)
defold_register_test_target(test_ddf)
