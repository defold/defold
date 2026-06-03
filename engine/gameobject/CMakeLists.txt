cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_gameobject LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="GAMEOBJECT")

set(GAMEOBJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GAMEOBJECT_SRC_DIR "${GAMEOBJECT_ROOT}/src")
set(GAMEOBJECT_PROTO_ROOT "${GAMEOBJECT_ROOT}/proto")
set(GAMEOBJECT_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/src/proto/gameobject")
set(GAMEOBJECT_PROTO_PY_ROOT "${CMAKE_CURRENT_BINARY_DIR}/proto")
set(GAMEOBJECT_PROTO_PY_DIR "${GAMEOBJECT_PROTO_PY_ROOT}/gameobject")

file(GLOB GAMEOBJECT_PROTO_FILES CONFIGURE_DEPENDS "${GAMEOBJECT_PROTO_ROOT}/gameobject/*.proto")
set(GAMEOBJECT_PUBLIC_HEADERS
  "${GAMEOBJECT_SRC_DIR}/gameobject/gameobject.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/gameobject_props.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/gameobject_props_lua.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/component.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/script.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/gameobject_script_util.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/res_lua.h")
file(GLOB_RECURSE GAMEOBJECT_DMSDK_HEADERS CONFIGURE_DEPENDS
  "${GAMEOBJECT_SRC_DIR}/dmsdk/*.h"
  "${GAMEOBJECT_SRC_DIR}/dmsdk/*.hpp")
set(GAMEOBJECT_PROTO_CC)
set(GAMEOBJECT_PROTO_HEADERS)
set(GAMEOBJECT_PROTO_PY)
foreach(_proto IN LISTS GAMEOBJECT_PROTO_FILES)
  get_filename_component(_base "${_proto}" NAME_WE)
  set(_cc "${GAMEOBJECT_GEN_DIR}/${_base}.cpp")
  set(_h "${GAMEOBJECT_GEN_DIR}/${_base}.h")
  set(_py "${GAMEOBJECT_PROTO_PY_DIR}/${_base}_pb2.py")
  defold_protoc_gen_cpp("${_cc}" "${_proto}" INCLUDES "${GAMEOBJECT_PROTO_ROOT}" "${DEFOLD_SDK_ROOT}/share/proto")
  defold_protoc_gen_py("${_py}" "${_proto}" PYTHON_ROOT "${GAMEOBJECT_PROTO_PY_ROOT}" INCLUDES "${GAMEOBJECT_PROTO_ROOT}" "${DEFOLD_SDK_ROOT}/share/proto")
  list(APPEND GAMEOBJECT_PROTO_CC "${_cc}")
  list(APPEND GAMEOBJECT_PROTO_HEADERS "${_h}")
  list(APPEND GAMEOBJECT_PROTO_PY "${_py}")
endforeach()
set(GAMEOBJECT_PROTO_PY_INIT "${GAMEOBJECT_PROTO_PY_DIR}/__init__.py")
add_custom_target(gameobject_proto_py ALL DEPENDS ${GAMEOBJECT_PROTO_PY} "${GAMEOBJECT_PROTO_PY_INIT}")
add_custom_target(gameobject_proto_headers DEPENDS ${GAMEOBJECT_PROTO_CC} ${GAMEOBJECT_PROTO_HEADERS})

file(GLOB GAMEOBJECT_SOURCES CONFIGURE_DEPENDS "${GAMEOBJECT_SRC_DIR}/gameobject/*.cpp")

defold_add_library(gameobject STATIC ${GAMEOBJECT_SOURCES} ${GAMEOBJECT_PROTO_CC})
target_include_directories(gameobject PUBLIC
  "${GAMEOBJECT_SRC_DIR}"
  "${GAMEOBJECT_SRC_DIR}/gameobject"
  "${GAMEOBJECT_PROTO_ROOT}"
  "${CMAKE_CURRENT_BINARY_DIR}/src/gameobject"
  "${CMAKE_CURRENT_BINARY_DIR}/src/proto"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(gameobject PRIVATE "DLIB_LOG_DOMAIN=\"GAMEOBJECT\"")
add_dependencies(gameobject gameobject_proto_headers gameobject_proto_py)
defold_target_link_libraries(gameobject "${TARGET_PLATFORM}" SCOPE PUBLIC
  lua
  ddf
  dlib
  resource
  script
  extension
  rig)

defold_install_targets(TARGETS gameobject
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

defold_add_sdk_headers_target(gameobject_public_sdk_headers
  DESTINATION include/gameobject
  SOURCE_ROOT "${GAMEOBJECT_SRC_DIR}/gameobject"
  FILES ${GAMEOBJECT_PUBLIC_HEADERS}
  DEPENDS gameobject)
defold_add_sdk_headers_target(gameobject_generated_sdk_headers
  DESTINATION include/gameobject
  SOURCE_ROOT "${GAMEOBJECT_GEN_DIR}"
  FILES ${GAMEOBJECT_PROTO_HEADERS}
  DEPENDS gameobject)
defold_add_sdk_headers_target(gameobject_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk
  SOURCE_ROOT "${GAMEOBJECT_SRC_DIR}/dmsdk"
  FILES ${GAMEOBJECT_DMSDK_HEADERS}
  DEPENDS gameobject)
set(GAMEOBJECT_SDK_HEADER_TARGETS
  gameobject_public_sdk_headers
  gameobject_generated_sdk_headers
  gameobject_dmsdk_sdk_headers)
list(APPEND GAMEOBJECT_SDK_HEADER_TARGETS
  hid_sdk_headers
  lua_sdk_headers
  resource_sdk_headers
  script_sdk_headers)
add_custom_target(gameobject_sdk_headers
  DEPENDS ${GAMEOBJECT_SDK_HEADER_TARGETS})

install(FILES
  "${GAMEOBJECT_SRC_DIR}/gameobject/gameobject.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/gameobject_props.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/gameobject_props_lua.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/component.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/script.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/gameobject_script_util.h"
  "${GAMEOBJECT_SRC_DIR}/gameobject/res_lua.h"
  ${GAMEOBJECT_PROTO_HEADERS}
  DESTINATION include/gameobject)
install(DIRECTORY "${GAMEOBJECT_SRC_DIR}/dmsdk/" DESTINATION sdk/include/dmsdk FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(FILES ${GAMEOBJECT_PROTO_FILES} DESTINATION share/proto/gameobject)
install(FILES "${GAMEOBJECT_PROTO_PY_INIT}" ${GAMEOBJECT_PROTO_PY} DESTINATION lib/python/gameobject)
install(FILES ${GAMEOBJECT_PROTO_PY} DESTINATION lib/python)

if(BUILD_TESTS)
  add_subdirectory(src/gameobject/test)
endif()
