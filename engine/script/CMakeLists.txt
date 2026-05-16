cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_script LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="SCRIPT")

set(SCRIPT_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(SCRIPT_SRC_DIR "${SCRIPT_ROOT}/src")
set(SCRIPT_PROTO_ROOT "${SCRIPT_SRC_DIR}")
set(SCRIPT_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/script")
set(SCRIPT_PROTO_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/python/script")

set(SCRIPT_PROTO_FILES
  "${SCRIPT_SRC_DIR}/script/ddf_script.proto"
  "${SCRIPT_SRC_DIR}/script/http_ddf.proto"
  "${SCRIPT_SRC_DIR}/script/lua_source_ddf.proto"
  "${SCRIPT_SRC_DIR}/script/script_doc_ddf.proto"
  "${SCRIPT_SRC_DIR}/script/sys_ddf.proto")

set(SCRIPT_PROTO_CC)
set(SCRIPT_PROTO_HEADERS)
set(SCRIPT_PROTO_PY)
set(SCRIPT_BPROTO)
foreach(_proto IN LISTS SCRIPT_PROTO_FILES)
  get_filename_component(_base "${_proto}" NAME_WE)
  set(_cc "${SCRIPT_GEN_DIR}/${_base}.cpp")
  set(_h "${SCRIPT_GEN_DIR}/${_base}.h")
  set(_py "${SCRIPT_PROTO_PY_DIR}/${_base}_pb2.py")
  set(_bproto "${SCRIPT_GEN_DIR}/${_base}.bproto")
  defold_protoc_gen_cpp("${_cc}" "${_proto}" INCLUDES "${SCRIPT_PROTO_ROOT}")
  defold_protoc_gen_py("${_py}" "${_proto}" PYTHON_ROOT "${CMAKE_CURRENT_BINARY_DIR}/python" INCLUDES "${SCRIPT_PROTO_ROOT}")
  defold_protoc_gen_bproto("${_bproto}" "${_proto}" INCLUDES "${SCRIPT_PROTO_ROOT}")
  list(APPEND SCRIPT_PROTO_CC "${_cc}")
  list(APPEND SCRIPT_PROTO_HEADERS "${_h}")
  list(APPEND SCRIPT_PROTO_PY "${_py}")
  list(APPEND SCRIPT_BPROTO "${_bproto}")
endforeach()
set(SCRIPT_PROTO_PY_INIT "${SCRIPT_PROTO_PY_DIR}/__init__.py")
add_custom_target(script_proto_py ALL DEPENDS ${SCRIPT_PROTO_PY} "${SCRIPT_PROTO_PY_INIT}")
add_custom_target(script_bproto ALL DEPENDS ${SCRIPT_BPROTO})

file(GLOB SCRIPT_TOP_SOURCES CONFIGURE_DEPENDS "${SCRIPT_SRC_DIR}/*.cpp")
set(SCRIPT_SOURCES ${SCRIPT_TOP_SOURCES} ${SCRIPT_PROTO_CC})

if(TARGET_PLATFORM MATCHES "web$")
  list(FILTER SCRIPT_SOURCES EXCLUDE REGEX "/script_html5\\.cpp$")
  list(FILTER SCRIPT_SOURCES EXCLUDE REGEX "/http_service\\.cpp$")
else()
  list(FILTER SCRIPT_SOURCES EXCLUDE REGEX "/script_html5_js\\.cpp$")
  list(FILTER SCRIPT_SOURCES EXCLUDE REGEX "/http_service_null\\.cpp$")
endif()

file(GLOB SCRIPT_BITOP_SOURCES CONFIGURE_DEPENDS "${SCRIPT_SRC_DIR}/bitop/*.c")
file(GLOB SCRIPT_CJSON_SOURCES CONFIGURE_DEPENDS "${SCRIPT_SRC_DIR}/luacjson/*.c")
file(GLOB SCRIPT_LUASOCKET_SOURCES CONFIGURE_DEPENDS "${SCRIPT_SRC_DIR}/luasocket/*.c")
list(FILTER SCRIPT_LUASOCKET_SOURCES EXCLUDE REGEX "/mime\\.c$")
list(FILTER SCRIPT_LUASOCKET_SOURCES EXCLUDE REGEX "/serial\\.c$")
list(FILTER SCRIPT_LUASOCKET_SOURCES EXCLUDE REGEX "/unix\\.c$")

if(TARGET_PLATFORM MATCHES "web$")
  foreach(_luasocket_source IN ITEMS luasocket.c tcp.c udp.c select.c inet.c buffer.c options.c io.c usocket.c auxiliar.c)
    list(FILTER SCRIPT_LUASOCKET_SOURCES EXCLUDE REGEX "/${_luasocket_source}$")
  endforeach()
else()
  list(FILTER SCRIPT_LUASOCKET_SOURCES EXCLUDE REGEX "/luasocket_html5\\.c$")
endif()

if(TARGET_PLATFORM MATCHES "^(win32|x86_64-win32|x86_64-xbone)$")
  list(FILTER SCRIPT_LUASOCKET_SOURCES EXCLUDE REGEX "/usocket\\.c$")
else()
  list(FILTER SCRIPT_LUASOCKET_SOURCES EXCLUDE REGEX "/wsocket\\.c$")
endif()

list(APPEND SCRIPT_SOURCES
  ${SCRIPT_BITOP_SOURCES}
  ${SCRIPT_LUASOCKET_SOURCES}
  ${SCRIPT_CJSON_SOURCES})

defold_add_library(script STATIC ${SCRIPT_SOURCES})
target_include_directories(script PUBLIC
  "${SCRIPT_SRC_DIR}"
  "${SCRIPT_SRC_DIR}/script"
  "${SCRIPT_GEN_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(script PRIVATE
  "DLIB_LOG_DOMAIN=\"SCRIPT\""
  "LUASOCKET_API=")
add_dependencies(script script_proto_py script_bproto)
defold_target_link_libraries(script "${TARGET_PLATFORM}" SCOPE PUBLIC
  lua
  ddf
  dlib
  zip
  mbedtls
  resource
  extension)
defold_target_link_socket(script "${TARGET_PLATFORM}" PUBLIC)

install(TARGETS script
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES
  "${SCRIPT_SRC_DIR}/script.h"
  "${SCRIPT_SRC_DIR}/http_service.h"
  ${SCRIPT_PROTO_HEADERS}
  DESTINATION include/script)
install(FILES
  "${SCRIPT_SRC_DIR}/script/ddf_script.proto"
  "${SCRIPT_SRC_DIR}/script/sys_ddf.proto"
  "${SCRIPT_SRC_DIR}/script/lua_source_ddf.proto"
  DESTINATION share/proto/script)
install(FILES ${SCRIPT_BPROTO} DESTINATION share/proto/script)
install(FILES "${SCRIPT_PROTO_PY_INIT}" ${SCRIPT_PROTO_PY} DESTINATION lib/python/script)
install(FILES "${SCRIPT_PROTO_PY_DIR}/script_doc_ddf_pb2.py" DESTINATION lib/python)
install(FILES "${SCRIPT_ROOT}/test_script_server.py" DESTINATION lib/python)
if(TARGET_PLATFORM MATCHES "web$")
  install(FILES "${SCRIPT_ROOT}/lib/js/library_script.js" DESTINATION "lib/${TARGET_PLATFORM}/js")
endif()

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
