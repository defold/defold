cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_render LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="RENDER")

set(RENDER_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(RENDER_SRC_DIR "${RENDER_ROOT}/src")
set(RENDER_PROTO_ROOT "${RENDER_ROOT}/proto")
set(RENDER_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/render")
set(RENDER_PROTO_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/python/render")

file(GLOB RENDER_PROTO_FILES CONFIGURE_DEPENDS "${RENDER_PROTO_ROOT}/render/*.proto")
set(RENDER_PROTO_CC)
set(RENDER_PROTO_HEADERS)
set(RENDER_PROTO_PY)
set(RENDER_BPROTO)
foreach(_proto IN LISTS RENDER_PROTO_FILES)
  get_filename_component(_base "${_proto}" NAME_WE)
  set(_cc "${RENDER_GEN_DIR}/${_base}.cpp")
  set(_h "${RENDER_GEN_DIR}/${_base}.h")
  set(_py "${RENDER_PROTO_PY_DIR}/${_base}_pb2.py")
  set(_bproto "${RENDER_GEN_DIR}/${_base}.bproto")
  defold_protoc_gen_cpp("${_cc}" "${_proto}" INCLUDES "${RENDER_PROTO_ROOT}")
  defold_protoc_gen_py("${_py}" "${_proto}" INCLUDES "${RENDER_PROTO_ROOT}")
  defold_protoc_gen_bproto("${_bproto}" "${_proto}" INCLUDES "${RENDER_PROTO_ROOT}")
  list(APPEND RENDER_PROTO_CC "${_cc}")
  list(APPEND RENDER_PROTO_HEADERS "${_h}")
  list(APPEND RENDER_PROTO_PY "${_py}")
  list(APPEND RENDER_BPROTO "${_bproto}")
endforeach()
set(RENDER_PROTO_PY_INIT "${RENDER_PROTO_PY_DIR}/__init__.py")
add_custom_target(render_proto_py ALL DEPENDS ${RENDER_PROTO_PY} "${RENDER_PROTO_PY_INIT}")
add_custom_target(render_bproto ALL DEPENDS ${RENDER_BPROTO})
add_custom_target(render_proto_headers DEPENDS ${RENDER_PROTO_CC} ${RENDER_PROTO_HEADERS})

file(GLOB RENDER_CORE_SOURCES CONFIGURE_DEPENDS "${RENDER_SRC_DIR}/render/*.cpp")
file(GLOB RENDER_FONT_SOURCES CONFIGURE_DEPENDS "${RENDER_SRC_DIR}/render/font/*.cpp")
set(RENDER_SOURCES ${RENDER_CORE_SOURCES} ${RENDER_FONT_SOURCES} ${RENDER_PROTO_CC})
list(FILTER RENDER_SOURCES EXCLUDE REGEX "/font_renderer_default\\.cpp$")

if(TARGET_PLATFORM MATCHES "web$")
  list(APPEND RENDER_SOURCES "${RENDER_SRC_DIR}/platform/renderer_web.cpp")
else()
  list(APPEND RENDER_SOURCES "${RENDER_SRC_DIR}/platform/renderer_common.cpp")
endif()

defold_add_library(render STATIC ${RENDER_SOURCES})
target_include_directories(render PUBLIC
  "${RENDER_SRC_DIR}"
  "${RENDER_PROTO_ROOT}"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(render PRIVATE "DLIB_LOG_DOMAIN=\"RENDER\"")
add_dependencies(render render_proto_headers render_proto_py render_bproto)
defold_target_link_libraries(render "${TARGET_PLATFORM}" SCOPE PUBLIC
  ddf
  dlib
  resource
  graphics
  image
  graphics_proto
  script
  extension
  lua)
defold_target_link_socket(render "${TARGET_PLATFORM}" PUBLIC)

defold_add_library(render_font_default STATIC
  "${RENDER_SRC_DIR}/render/font/default/font_default_vertex.cpp")
target_include_directories(render_font_default PUBLIC
  "${RENDER_SRC_DIR}"
  "${RENDER_PROTO_ROOT}"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(render_font_default PRIVATE "DLIB_LOG_DOMAIN=\"RENDER\"")
add_dependencies(render_font_default render_proto_headers render_proto_py render_bproto)
if(TARGET graphics_proto)
  add_dependencies(render_font_default graphics_proto_header)
endif()
if(TARGET script_proto_headers)
  add_dependencies(render_font_default script_proto_headers)
endif()
defold_target_link_libraries(render_font_default "${TARGET_PLATFORM}" SCOPE PUBLIC
  ddf
  dlib
  script
  $<$<TARGET_EXISTS:graphics_proto>:graphics_proto>)

defold_install_targets(TARGETS render render_font_default
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES
  "${RENDER_SRC_DIR}/render/display_profiles.h"
  "${RENDER_SRC_DIR}/render/render.h"
  ${RENDER_PROTO_HEADERS}
  DESTINATION include/render)
install(FILES
  "${RENDER_SRC_DIR}/render/font/fontmap.h"
  "${RENDER_SRC_DIR}/render/font/font_glyphbank.h"
  "${RENDER_SRC_DIR}/render/font/font_renderer.h"
  DESTINATION include/render/font)
install(FILES "${RENDER_SRC_DIR}/waf_render.py" DESTINATION lib/python)
install(FILES
  "${RENDER_PROTO_ROOT}/render/material_ddf.proto"
  "${RENDER_PROTO_ROOT}/render/render_ddf.proto"
  "${RENDER_PROTO_ROOT}/render/font_ddf.proto"
  DESTINATION share/proto/render)
install(FILES ${RENDER_BPROTO} DESTINATION share/proto/render)
install(FILES "${RENDER_PROTO_PY_INIT}" ${RENDER_PROTO_PY} DESTINATION lib/python/render)
if(TARGET_PLATFORM MATCHES "web$")
  install(FILES "${RENDER_SRC_DIR}/js/library_render.js" DESTINATION "lib/${TARGET_PLATFORM}/js")
endif()

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
