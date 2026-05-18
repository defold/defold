cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_resource LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="RESOURCE")

set(RESOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RESOURCE_SRC_DIR "${RESOURCE_ROOT}/src")
set(RESOURCE_PROTO_ROOT "${RESOURCE_ROOT}/proto")
set(RESOURCE_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/resource")
set(RESOURCE_PROTO_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/python/resource")

set(RESOURCE_PROTO_FILES
  "${RESOURCE_PROTO_ROOT}/resource/resource_ddf.proto"
  "${RESOURCE_PROTO_ROOT}/resource/liveupdate_ddf.proto")

set(RESOURCE_PROTO_CC)
set(RESOURCE_PROTO_HEADERS)
set(RESOURCE_PROTO_PY)
set(RESOURCE_BPROTO)
foreach(_proto IN LISTS RESOURCE_PROTO_FILES)
  get_filename_component(_base "${_proto}" NAME_WE)
  set(_cc "${RESOURCE_GEN_DIR}/${_base}.cpp")
  set(_h "${RESOURCE_GEN_DIR}/${_base}.h")
  set(_py "${RESOURCE_PROTO_PY_DIR}/${_base}_pb2.py")
  set(_bproto "${RESOURCE_GEN_DIR}/${_base}.bproto")
  defold_protoc_gen_cpp("${_cc}" "${_proto}" INCLUDES "${RESOURCE_PROTO_ROOT}")
  defold_protoc_gen_py("${_py}" "${_proto}" INCLUDES "${RESOURCE_PROTO_ROOT}")
  defold_protoc_gen_bproto("${_bproto}" "${_proto}" INCLUDES "${RESOURCE_PROTO_ROOT}")
  list(APPEND RESOURCE_PROTO_CC "${_cc}")
  list(APPEND RESOURCE_PROTO_HEADERS "${_h}")
  list(APPEND RESOURCE_PROTO_PY "${_py}")
  list(APPEND RESOURCE_BPROTO "${_bproto}")
endforeach()
set(RESOURCE_PROTO_PY_INIT "${RESOURCE_PROTO_PY_DIR}/__init__.py")
add_custom_target(resource_proto_py ALL DEPENDS ${RESOURCE_PROTO_PY} "${RESOURCE_PROTO_PY_INIT}")
add_custom_target(resource_bproto ALL DEPENDS ${RESOURCE_BPROTO})

file(GLOB RESOURCE_TOP_SOURCES CONFIGURE_DEPENDS "${RESOURCE_SRC_DIR}/*.cpp")
file(GLOB RESOURCE_PROVIDER_SOURCES CONFIGURE_DEPENDS "${RESOURCE_SRC_DIR}/providers/*.cpp")
set(RESOURCE_SOURCES ${RESOURCE_TOP_SOURCES} ${RESOURCE_PROVIDER_SOURCES})

if(TARGET_PLATFORM MATCHES "web$")
  list(FILTER RESOURCE_SOURCES EXCLUDE REGEX "/provider_http\\.cpp$")
  list(APPEND RESOURCE_SOURCES "${RESOURCE_SRC_DIR}/async/load_queue_sync.cpp")
else()
  list(FILTER RESOURCE_SOURCES EXCLUDE REGEX "/provider_http_null\\.cpp$")
  list(APPEND RESOURCE_SOURCES "${RESOURCE_SRC_DIR}/async/load_queue_threaded.cpp")
endif()
list(APPEND RESOURCE_SOURCES "${RESOURCE_SRC_DIR}/async/load_queue.cpp")

if(TARGET_PLATFORM MATCHES "^(armv7-android|arm64-android)$")
  list(APPEND RESOURCE_SOURCES "${RESOURCE_SRC_DIR}/mount/mount_android.cpp")
elseif(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux|x86_64-macos|arm64-macos|arm64-ios|x86_64-ios)$")
  list(APPEND RESOURCE_SOURCES "${RESOURCE_SRC_DIR}/mount/mount_mmap.cpp")
else()
  list(APPEND RESOURCE_SOURCES "${RESOURCE_SRC_DIR}/mount/mount_generic.cpp")
endif()

defold_add_library(resource STATIC ${RESOURCE_SOURCES} ${RESOURCE_PROTO_CC})
target_include_directories(resource PUBLIC
  "${RESOURCE_SRC_DIR}"
  "${RESOURCE_PROTO_ROOT}"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(resource PRIVATE "DLIB_LOG_DOMAIN=\"RESOURCE\"")
add_dependencies(resource resource_proto_py resource_bproto)
defold_target_link_libraries(resource "${TARGET_PLATFORM}" SCOPE PUBLIC
  ddf
  dlib
  image
  zip
  mbedtls)
defold_target_link_socket(resource "${TARGET_PLATFORM}" PUBLIC)

defold_install_targets(TARGETS resource
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES
  "${RESOURCE_SRC_DIR}/resource.h"
  "${RESOURCE_SRC_DIR}/resource_archive.h"
  "${RESOURCE_SRC_DIR}/resource_chunk_cache.h"
  "${RESOURCE_SRC_DIR}/resource_manifest.h"
  "${RESOURCE_SRC_DIR}/resource_mounts.h"
  "${RESOURCE_SRC_DIR}/resource_util.h"
  "${RESOURCE_SRC_DIR}/resource_verify.h"
  ${RESOURCE_PROTO_HEADERS}
  DESTINATION include/resource)
install(FILES "${RESOURCE_SRC_DIR}/providers/provider.h" DESTINATION include/resource/providers)
install(FILES "${RESOURCE_SRC_DIR}/waf_resource.py" DESTINATION lib/python)
install(FILES "${RESOURCE_SRC_DIR}/arcc.py" DESTINATION bin)
install(FILES ${RESOURCE_PROTO_FILES} DESTINATION share/proto/resource)
install(FILES ${RESOURCE_BPROTO} DESTINATION share/proto/resource)
install(FILES "${RESOURCE_PROTO_PY_INIT}" ${RESOURCE_PROTO_PY} DESTINATION lib/python/resource)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
