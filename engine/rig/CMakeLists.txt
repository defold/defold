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

defold_protoc_gen_cpp("${RIG_PROTO_CC}" "${RIG_PROTO}" INCLUDES "${RIG_PROTO_ROOT}")
defold_protoc_gen_py("${RIG_PROTO_PY}" "${RIG_PROTO}" INCLUDES "${RIG_PROTO_ROOT}")
defold_protoc_gen_bproto("${RIG_BPROTO}" "${RIG_PROTO}" INCLUDES "${RIG_PROTO_ROOT}")
add_custom_target(rig_proto_py ALL DEPENDS "${RIG_PROTO_PY}" "${RIG_PROTO_PY_INIT}")
add_custom_target(rig_bproto ALL DEPENDS "${RIG_BPROTO}")

function(rig_configure_target target)
  target_include_directories(${target} PUBLIC
    "${RIG_SRC_DIR}"
    "${RIG_ROOT}"
    "${RIG_PROTO_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(${target} PRIVATE "DLIB_LOG_DOMAIN=\"RIG\"")
  add_dependencies(${target} rig_proto_py rig_bproto)
endfunction()

defold_add_library(rig STATIC
  "${RIG_SRC_DIR}/rig.cpp"
  "${RIG_PROTO_CC}")
rig_configure_target(rig)
defold_target_link_libraries(rig "${TARGET_PLATFORM}" SCOPE PUBLIC
  ddf
  dlib)
defold_target_link_socket(rig "${TARGET_PLATFORM}" PUBLIC)

defold_add_library(rig_null STATIC
  "${RIG_SRC_DIR}/rig_null.cpp"
  "${RIG_PROTO_CC}")
rig_configure_target(rig_null)
defold_target_link_libraries(rig_null "${TARGET_PLATFORM}" SCOPE PUBLIC
  ddf
  dlib)
defold_target_link_socket(rig_null "${TARGET_PLATFORM}" PUBLIC)

if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86-win32|x86_64-macos|arm64-macos)$")
  defold_add_library(rig_shared SHARED
    "${RIG_SRC_DIR}/rig.cpp"
    "${RIG_PROTO_CC}")
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

install(TARGETS rig rig_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})
if(TARGET rig_shared)
  install(TARGETS rig_shared
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

install(FILES
  "${RIG_SRC_DIR}/rig.h"
  "${RIG_PROTO_H}"
  DESTINATION include/rig)
install(FILES "${RIG_PROTO}" DESTINATION share/proto)
install(FILES "${RIG_BPROTO}" DESTINATION share/proto/rig)
install(FILES "${RIG_PROTO_PY_INIT}" "${RIG_PROTO_PY}" DESTINATION lib/python/rig)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
