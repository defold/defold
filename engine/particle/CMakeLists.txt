cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_particle LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="PARTICLE")

set(PARTICLE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PARTICLE_SRC_DIR "${PARTICLE_ROOT}/src")
set(PARTICLE_PROTO_ROOT "${PARTICLE_ROOT}/proto")
set(PARTICLE_PROTO "${PARTICLE_PROTO_ROOT}/particle/particle_ddf.proto")
set(PARTICLE_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/particle")
set(PARTICLE_PROTO_CC "${PARTICLE_GEN_DIR}/particle_ddf.cpp")
set(PARTICLE_PROTO_H "${PARTICLE_GEN_DIR}/particle_ddf.h")
set(PARTICLE_PROTO_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/python/particle")
set(PARTICLE_PROTO_PY "${PARTICLE_PROTO_PY_DIR}/particle_ddf_pb2.py")
set(PARTICLE_PROTO_PY_INIT "${PARTICLE_PROTO_PY_DIR}/__init__.py")
set(PARTICLE_BPROTO "${PARTICLE_GEN_DIR}/particle_ddf.bproto")

defold_protoc_gen_cpp("${PARTICLE_PROTO_CC}" "${PARTICLE_PROTO}" INCLUDES "${PARTICLE_PROTO_ROOT}")
defold_protoc_gen_py("${PARTICLE_PROTO_PY}" "${PARTICLE_PROTO}" INCLUDES "${PARTICLE_PROTO_ROOT}")
defold_protoc_gen_bproto("${PARTICLE_BPROTO}" "${PARTICLE_PROTO}" INCLUDES "${PARTICLE_PROTO_ROOT}")
add_custom_target(particle_proto_py DEPENDS "${PARTICLE_PROTO_PY}" "${PARTICLE_PROTO_PY_INIT}")
add_custom_target(particle_bproto DEPENDS "${PARTICLE_BPROTO}")

defold_add_library(particle STATIC
  "${PARTICLE_SRC_DIR}/particle.cpp"
  "${PARTICLE_PROTO_CC}")
target_include_directories(particle PUBLIC
  "${PARTICLE_SRC_DIR}"
  "${PARTICLE_PROTO_ROOT}"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(particle PRIVATE "DLIB_LOG_DOMAIN=\"PARTICLE\"")
add_dependencies(particle particle_proto_py particle_bproto)
defold_target_link_libraries(particle "${TARGET_PLATFORM}" ddf dlib)

if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-win32|x86_64-macos|arm64-macos)$")
  defold_add_library(particle_shared SHARED
    "${PARTICLE_SRC_DIR}/particle.cpp"
    "${PARTICLE_PROTO_CC}")
  target_include_directories(particle_shared PUBLIC
    "${PARTICLE_SRC_DIR}"
    "${PARTICLE_PROTO_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(particle_shared PRIVATE "DLIB_LOG_DOMAIN=\"PARTICLE\"")
  add_dependencies(particle_shared particle_proto_py particle_bproto)
  defold_target_link_libraries(particle_shared "${TARGET_PLATFORM}"
    ddf_noasan
    dlib_noasan
    profile_null_noasan
    platform_null
    graphics_null_noasan
    image_noasan
    graphics_proto_noasan)
endif()

install(TARGETS particle
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

if(TARGET particle_shared)
  install(TARGETS particle_shared
          ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
          LIBRARY DESTINATION lib/${TARGET_PLATFORM}
          RUNTIME DESTINATION bin/${TARGET_PLATFORM})
endif()

install(FILES
  "${PARTICLE_SRC_DIR}/particle.h"
  "${PARTICLE_PROTO_H}"
  DESTINATION include/particle)
install(FILES "${PARTICLE_PROTO}" DESTINATION share/proto)
install(FILES "${PARTICLE_BPROTO}" DESTINATION share/proto/particle)
install(FILES "${PARTICLE_PROTO_PY_INIT}" "${PARTICLE_PROTO_PY}" DESTINATION lib/python/particle)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
