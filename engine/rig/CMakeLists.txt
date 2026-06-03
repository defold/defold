cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_rig LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="RIG")

set(RIG_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RIG_SRC_DIR "${RIG_ROOT}/src")
set(RIG_PROTO_ROOT "${RIG_ROOT}/proto")
set(RIG_PROTO "${RIG_PROTO_ROOT}/rig/rig_ddf.proto")
set(RIG_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/rig")
set(RIG_PROTO_CC "${RIG_GEN_DIR}/rig_ddf.cpp")
set(RIG_PROTO_H "${RIG_GEN_DIR}/rig_ddf.h")
set(RIG_PROTO_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/python/rig")
set(RIG_PROTO_PY "${RIG_PROTO_PY_DIR}/rig_ddf_pb2.py")
set(RIG_PROTO_PY_INIT "${RIG_PROTO_PY_DIR}/__init__.py")
set(RIG_BPROTO "${RIG_GEN_DIR}/rig_ddf.bproto")
file(GLOB_RECURSE RIG_DMSDK_HEADERS CONFIGURE_DEPENDS
  "${RIG_SRC_DIR}/dmsdk/*.h"
  "${RIG_SRC_DIR}/dmsdk/*.hpp")

defold_protoc_gen_cpp("${RIG_PROTO_CC}" "${RIG_PROTO}" INCLUDES "${RIG_PROTO_ROOT}")
defold_protoc_gen_py("${RIG_PROTO_PY}" "${RIG_PROTO}" INCLUDES "${RIG_PROTO_ROOT}")
defold_protoc_gen_bproto("${RIG_BPROTO}" "${RIG_PROTO}" INCLUDES "${RIG_PROTO_ROOT}")
add_custom_target(rig_proto_headers DEPENDS "${RIG_PROTO_CC}" "${RIG_PROTO_H}")
add_custom_target(rig_proto_py ALL DEPENDS "${RIG_PROTO_PY}" "${RIG_PROTO_PY_INIT}")
add_custom_target(rig_bproto ALL DEPENDS "${RIG_BPROTO}")

function(rig_configure_target target)
  target_include_directories(${target} PUBLIC
    "${RIG_SRC_DIR}"
    "${RIG_ROOT}"
    "${RIG_PROTO_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(${target} PRIVATE "DLIB_LOG_DOMAIN=\"RIG\"")
  add_dependencies(${target} rig_proto_headers rig_proto_py rig_bproto)
  if(TARGET graphics_proto)
    add_dependencies(${target} graphics_proto_header)
  endif()
  add_dependencies(${target} graphics_sdk_headers)
endfunction()

defold_add_library(rig STATIC
  "${RIG_SRC_DIR}/rig.cpp"
  "${RIG_PROTO_CC}")
rig_configure_target(rig)
defold_target_link_libraries(rig "${TARGET_PLATFORM}" SCOPE PUBLIC
  ddf
  dlib
  $<$<TARGET_EXISTS:graphics_proto>:graphics_proto>)
defold_target_link_socket(rig "${TARGET_PLATFORM}" PUBLIC)

defold_add_library(rig_null STATIC
  "${RIG_SRC_DIR}/rig_null.cpp"
  "${RIG_PROTO_CC}")
rig_configure_target(rig_null)
defold_target_link_libraries(rig_null "${TARGET_PLATFORM}" SCOPE PUBLIC
  ddf
  dlib
  $<$<TARGET_EXISTS:graphics_proto>:graphics_proto>)
defold_target_link_socket(rig_null "${TARGET_PLATFORM}" PUBLIC)

if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86-win32|x86_64-macos|arm64-macos)$")
  defold_add_library(rig_shared SHARED
    "${RIG_SRC_DIR}/rig.cpp"
    "${RIG_PROTO_CC}")
  defold_target_skip_sanitizer(rig_shared)
  rig_configure_target(rig_shared)
  defold_target_link_libraries(rig_shared "${TARGET_PLATFORM}"
    ddf_noasan
    dlib_noasan
    profile_null_noasan
    platform_null
    graphics_null_noasan
    image_noasan
    graphics_proto_noasan)
  defold_target_link_socket(rig_shared "${TARGET_PLATFORM}" PRIVATE)
endif()

defold_install_targets(TARGETS rig rig_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})
if(TARGET rig_shared)
  defold_install_targets(TARGETS rig_shared
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

defold_add_sdk_headers_target(rig_public_sdk_headers
  DESTINATION include/rig
  SOURCE_ROOT "${RIG_SRC_DIR}"
  FILES "${RIG_SRC_DIR}/rig.h"
  DEPENDS rig)
defold_add_sdk_headers_target(rig_generated_sdk_headers
  DESTINATION include/rig
  SOURCE_ROOT "${RIG_GEN_DIR}"
  FILES "${RIG_PROTO_H}"
  DEPENDS rig)
defold_add_sdk_headers_target(rig_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk
  SOURCE_ROOT "${RIG_SRC_DIR}/dmsdk"
  FILES ${RIG_DMSDK_HEADERS}
  DEPENDS rig)
set(RIG_SDK_HEADER_TARGETS
  rig_public_sdk_headers
  rig_generated_sdk_headers
  rig_dmsdk_sdk_headers
  graphics_sdk_headers)
add_custom_target(rig_sdk_headers
  DEPENDS ${RIG_SDK_HEADER_TARGETS})

install(FILES
  "${RIG_SRC_DIR}/rig.h"
  "${RIG_PROTO_H}"
  DESTINATION include/rig)
install(DIRECTORY "${RIG_SRC_DIR}/dmsdk/" DESTINATION sdk/include/dmsdk FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(FILES "${RIG_PROTO}" DESTINATION share/proto)
install(FILES "${RIG_BPROTO}" DESTINATION share/proto/rig)
install(FILES "${RIG_PROTO_PY_INIT}" "${RIG_PROTO_PY}" DESTINATION lib/python/rig)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
