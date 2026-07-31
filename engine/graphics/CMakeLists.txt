cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_graphics LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="GRAPHICS")

set(GRAPHICS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GRAPHICS_SRC_DIR "${GRAPHICS_ROOT}/src")
set(GRAPHICS_PROTO_ROOT "${GRAPHICS_ROOT}/proto")
set(GRAPHICS_PROTO "${GRAPHICS_PROTO_ROOT}/graphics/graphics_ddf.proto")
set(GRAPHICS_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/graphics")
set(GRAPHICS_PROTO_CC "${GRAPHICS_GEN_DIR}/graphics_ddf.cpp")
set(GRAPHICS_PROTO_H "${GRAPHICS_GEN_DIR}/graphics_ddf.h")
set(GRAPHICS_PROTO_PY_DIR "${CMAKE_CURRENT_BINARY_DIR}/python/graphics")
set(GRAPHICS_PROTO_PY "${GRAPHICS_PROTO_PY_DIR}/graphics_ddf_pb2.py")
set(GRAPHICS_PROTO_PY_INIT "${GRAPHICS_PROTO_PY_DIR}/__init__.py")
set(GRAPHICS_BPROTO "${GRAPHICS_GEN_DIR}/graphics_ddf.bproto")
set(GRAPHICS_CORE_DEPENDENCIES ddf dlib image)
set(GRAPHICS_CORE_NOASAN_DEPENDENCIES ddf dlib image_noasan)
set(GRAPHICS_NULL_DEPENDENCIES ${GRAPHICS_CORE_DEPENDENCIES} platform_null)
set(GRAPHICS_NULL_NOASAN_DEPENDENCIES ${GRAPHICS_CORE_NOASAN_DEPENDENCIES} platform_null)
set(GRAPHICS_DEPENDENCIES ${GRAPHICS_CORE_DEPENDENCIES} platform)
set(GRAPHICS_TRANSCODER_NULL_DEPENDENCIES dlib)
set(GRAPHICS_TRANSCODER_BASISU_DEPENDENCIES dlib basis_transcoder)
set(GRAPHICS_PUBLIC_HEADERS
  "${GRAPHICS_SRC_DIR}/graphics.h"
  "${GRAPHICS_SRC_DIR}/graphics_util.h"
  "${GRAPHICS_SRC_DIR}/graphics_native.h"
  "${GRAPHICS_SRC_DIR}/opengl/win32/glext.h"
  "${GRAPHICS_SRC_DIR}/opengl/graphics_opengl_defines.h")
file(GLOB_RECURSE GRAPHICS_DMSDK_HEADERS CONFIGURE_DEPENDS
  "${GRAPHICS_SRC_DIR}/dmsdk/*.h"
  "${GRAPHICS_SRC_DIR}/dmsdk/*.hpp")

defold_protoc_gen_cpp("${GRAPHICS_PROTO_CC}" "${GRAPHICS_PROTO}" INCLUDES "${GRAPHICS_PROTO_ROOT}")
defold_protoc_gen_py("${GRAPHICS_PROTO_PY}" "${GRAPHICS_PROTO}" PYTHON_ROOT "${CMAKE_CURRENT_BINARY_DIR}/python" INCLUDES "${GRAPHICS_PROTO_ROOT}")
defold_protoc_gen_bproto("${GRAPHICS_BPROTO}" "${GRAPHICS_PROTO}" INCLUDES "${GRAPHICS_PROTO_ROOT}")
add_custom_target(graphics_proto_py ALL DEPENDS "${GRAPHICS_PROTO_PY}" "${GRAPHICS_PROTO_PY_INIT}")
add_custom_target(graphics_bproto ALL DEPENDS "${GRAPHICS_BPROTO}")
add_custom_target(graphics_proto_sources DEPENDS "${GRAPHICS_PROTO_CC}" "${GRAPHICS_PROTO_H}")

defold_add_library(graphics_proto_generated OBJECT "${GRAPHICS_PROTO_CC}")
target_include_directories(graphics_proto_generated PUBLIC
  "${GRAPHICS_SRC_DIR}"
  "${GRAPHICS_PROTO_ROOT}"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(graphics_proto_generated PRIVATE "DLIB_LOG_DOMAIN=\"GRAPHICS\"")
add_dependencies(graphics_proto_generated graphics_proto_sources)
defold_target_link_libraries(graphics_proto_generated "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_CORE_DEPENDENCIES})

defold_add_library(graphics_proto_generated_noasan OBJECT "${GRAPHICS_PROTO_CC}")
target_include_directories(graphics_proto_generated_noasan PUBLIC
  "${GRAPHICS_SRC_DIR}"
  "${GRAPHICS_PROTO_ROOT}"
  "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_definitions(graphics_proto_generated_noasan PRIVATE "DLIB_LOG_DOMAIN=\"GRAPHICS\"")
add_dependencies(graphics_proto_generated_noasan graphics_proto_sources)
defold_target_link_libraries(graphics_proto_generated_noasan "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_CORE_NOASAN_DEPENDENCIES})

add_custom_target(graphics_proto_header)
add_dependencies(graphics_proto_header graphics_proto_sources)

function(graphics_target_os out_var)
  if(TARGET_PLATFORM MATCHES "win32$")
    set(_target_os "win32")
  elseif(TARGET_PLATFORM MATCHES "macos$")
    set(_target_os "osx")
  elseif(TARGET_PLATFORM MATCHES "ios$")
    set(_target_os "ios")
  elseif(TARGET_PLATFORM MATCHES "android$")
    set(_target_os "android")
  elseif(TARGET_PLATFORM MATCHES "linux$")
    set(_target_os "linux")
  elseif(TARGET_PLATFORM MATCHES "web$")
    set(_target_os "web")
  elseif(TARGET_PLATFORM MATCHES "nx64$")
    set(_target_os "nx64")
  else()
    set(_target_os "${TARGET_PLATFORM_OS}")
  endif()
  set(${out_var} "${_target_os}" PARENT_SCOPE)
endfunction()

function(graphics_native_source out_var feature)
  graphics_target_os(_target_os)
  if(EXISTS "${GRAPHICS_SRC_DIR}/${_target_os}/graphics_native.cpp")
    set(_native "${GRAPHICS_SRC_DIR}/${_target_os}/graphics_native.cpp")
  else()
    set(_native "${GRAPHICS_SRC_DIR}/${feature}/graphics_native.cpp")
  endif()
  set(${out_var} "${_native}" PARENT_SCOPE)
endfunction()

function(graphics_common_setup target)
  target_include_directories(${target} PUBLIC
    "${GRAPHICS_SRC_DIR}"
    "${GRAPHICS_PROTO_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_definitions(${target} PRIVATE "DLIB_LOG_DOMAIN=\"GRAPHICS\"")
  add_dependencies(${target} graphics_proto_generated graphics_proto_py graphics_bproto)
endfunction()

set(GRAPHICS_PROTO_OBJECTS $<TARGET_OBJECTS:graphics_proto_generated>)
set(GRAPHICS_PROTO_NOASAN_OBJECTS $<TARGET_OBJECTS:graphics_proto_generated_noasan>)
set(GRAPHICS_PROTO_SOURCES "${GRAPHICS_SRC_DIR}/graphics_proto.cpp" ${GRAPHICS_PROTO_OBJECTS})
set(GRAPHICS_PROTO_NOASAN_SOURCES "${GRAPHICS_SRC_DIR}/graphics_proto.cpp" ${GRAPHICS_PROTO_NOASAN_OBJECTS})

defold_add_library(graphics_proto STATIC ${GRAPHICS_PROTO_SOURCES})
graphics_common_setup(graphics_proto)
defold_target_link_libraries(graphics_proto "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_CORE_DEPENDENCIES})

defold_add_library(graphics_proto_noasan STATIC ${GRAPHICS_PROTO_NOASAN_SOURCES})
graphics_common_setup(graphics_proto_noasan)
add_dependencies(graphics_proto_noasan graphics_proto_generated_noasan)
defold_target_link_libraries(graphics_proto_noasan "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_CORE_NOASAN_DEPENDENCIES})
set_target_properties(graphics_proto_noasan PROPERTIES DEFOLD_SDK_HEADERS_TARGET graphics_sdk_headers)

file(GLOB GRAPHICS_CORE_SOURCES CONFIGURE_DEPENDS "${GRAPHICS_SRC_DIR}/*.cpp")
file(GLOB GRAPHICS_NULL_SOURCES CONFIGURE_DEPENDS "${GRAPHICS_SRC_DIR}/null/*.cpp")
set(GRAPHICS_NULL_ALL_SOURCES ${GRAPHICS_CORE_SOURCES} ${GRAPHICS_NULL_SOURCES} ${GRAPHICS_PROTO_OBJECTS})
set(GRAPHICS_NULL_NOASAN_ALL_SOURCES ${GRAPHICS_CORE_SOURCES} ${GRAPHICS_NULL_SOURCES} ${GRAPHICS_PROTO_NOASAN_OBJECTS})
list(FILTER GRAPHICS_NULL_ALL_SOURCES EXCLUDE REGEX "/graphics_glfw_wrappers\\.cpp$")
list(FILTER GRAPHICS_NULL_NOASAN_ALL_SOURCES EXCLUDE REGEX "/graphics_glfw_wrappers\\.cpp$")

defold_add_library(graphics_null STATIC ${GRAPHICS_NULL_ALL_SOURCES})
graphics_common_setup(graphics_null)
target_include_directories(graphics_null PUBLIC "${GRAPHICS_SRC_DIR}/null")
target_compile_definitions(graphics_null PUBLIC DM_GRAPHICS_NULL)
defold_target_link_libraries(graphics_null "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_NULL_DEPENDENCIES})
set_target_properties(graphics_null PROPERTIES DEFOLD_SDK_HEADERS_TARGET graphics_sdk_headers)

defold_add_library(graphics_null_noasan STATIC ${GRAPHICS_NULL_NOASAN_ALL_SOURCES})
graphics_common_setup(graphics_null_noasan)
add_dependencies(graphics_null_noasan graphics_proto_generated_noasan)
target_include_directories(graphics_null_noasan PUBLIC "${GRAPHICS_SRC_DIR}/null")
target_compile_definitions(graphics_null_noasan PUBLIC DM_GRAPHICS_NULL)
defold_target_link_libraries(graphics_null_noasan "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_NULL_NOASAN_DEPENDENCIES})
set_target_properties(graphics_null_noasan PROPERTIES DEFOLD_SDK_HEADERS_TARGET graphics_sdk_headers)

set(GRAPHICS_INSTALL_TARGETS graphics_proto graphics_proto_noasan graphics_null graphics_null_noasan)

set(GRAPHICS_PLATFORM_SUPPORTS_OPENGL ON)
set(GRAPHICS_PLATFORM_SUPPORTS_OPENGLES OFF)
set(GRAPHICS_PLATFORM_SUPPORTS_VULKAN ON)
set(GRAPHICS_PLATFORM_SUPPORTS_DX12 OFF)
set(GRAPHICS_PLATFORM_SUPPORTS_METAL OFF)
set(GRAPHICS_PLATFORM_SUPPORTS_WEBGPU OFF)
set(GRAPHICS_PLATFORM_REQUIRES_DX12 OFF)
if(TARGET_PLATFORM STREQUAL "x86_64-xbone" AND COMMAND defold_xbox_get_graphics_libraries)
  defold_xbox_get_graphics_libraries(DEFOLD_PLATFORM_GRAPHICS_LIBS "${TARGET_PLATFORM}")
endif()
if(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
  set(GRAPHICS_PLATFORM_SUPPORTS_VULKAN OFF)
  set(GRAPHICS_PLATFORM_SUPPORTS_WEBGPU ON)
endif()
if(TARGET_PLATFORM STREQUAL "x86_64-ios")
  set(GRAPHICS_PLATFORM_SUPPORTS_VULKAN OFF)
endif()
if(TARGET_PLATFORM MATCHES "^(arm64-linux|armv7-android|arm64-android|x86_64-android|x86_64-android|x86_64-ios|arm64-ios)$")
  set(GRAPHICS_PLATFORM_SUPPORTS_OPENGLES ON)
endif()
if(TARGET_PLATFORM STREQUAL "x86_64-win32")
  set(GRAPHICS_PLATFORM_SUPPORTS_DX12 ON)
endif()
if(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|x86_64-ios|arm64-ios)$")
  set(GRAPHICS_PLATFORM_SUPPORTS_METAL ON)
endif()
if(DEFOLD_PLATFORM_GRAPHICS_LIBS)
  set(GRAPHICS_PLATFORM_SUPPORTS_OPENGL OFF)
  set(GRAPHICS_PLATFORM_SUPPORTS_OPENGLES OFF)
  set(GRAPHICS_PLATFORM_SUPPORTS_VULKAN OFF)
  set(GRAPHICS_PLATFORM_SUPPORTS_DX12 OFF)
  set(GRAPHICS_PLATFORM_SUPPORTS_METAL OFF)
  set(GRAPHICS_PLATFORM_SUPPORTS_WEBGPU OFF)
  if("graphics" IN_LIST DEFOLD_PLATFORM_GRAPHICS_LIBS)
    set(GRAPHICS_PLATFORM_SUPPORTS_OPENGL ON)
  endif()
  if("graphics_opengles" IN_LIST DEFOLD_PLATFORM_GRAPHICS_LIBS)
    set(GRAPHICS_PLATFORM_SUPPORTS_OPENGLES ON)
  endif()
  if("graphics_vulkan" IN_LIST DEFOLD_PLATFORM_GRAPHICS_LIBS)
    set(GRAPHICS_PLATFORM_SUPPORTS_VULKAN ON)
  endif()
  if("graphics_webgpu" IN_LIST DEFOLD_PLATFORM_GRAPHICS_LIBS)
    set(GRAPHICS_PLATFORM_SUPPORTS_WEBGPU ON)
  endif()
  if("graphics_dx12" IN_LIST DEFOLD_PLATFORM_GRAPHICS_LIBS)
    set(GRAPHICS_PLATFORM_SUPPORTS_DX12 ON)
    set(GRAPHICS_PLATFORM_REQUIRES_DX12 ON)
  endif()
  if("graphics_metal" IN_LIST DEFOLD_PLATFORM_GRAPHICS_LIBS)
    set(GRAPHICS_PLATFORM_SUPPORTS_METAL ON)
  endif()
endif()

set(GRAPHICS_BUILD_OPENGLES ${GRAPHICS_PLATFORM_SUPPORTS_OPENGLES})
set(GRAPHICS_BUILD_WEBGPU ${GRAPHICS_PLATFORM_SUPPORTS_WEBGPU})
set(GRAPHICS_BUILD_VULKAN ${GRAPHICS_PLATFORM_SUPPORTS_VULKAN})
set(GRAPHICS_BUILD_METAL ${GRAPHICS_PLATFORM_SUPPORTS_METAL})
set(GRAPHICS_BUILD_DX12 OFF)

if(GRAPHICS_PLATFORM_SUPPORTS_DX12 AND (WITH_DX12 OR GRAPHICS_PLATFORM_REQUIRES_DX12))
  set(GRAPHICS_BUILD_DX12 ON)
endif()
if(GRAPHICS_PLATFORM_SUPPORTS_METAL AND WITH_METAL)
  set(GRAPHICS_BUILD_METAL ON)
endif()

file(GLOB GRAPHICS_OPENGL_SOURCES CONFIGURE_DEPENDS "${GRAPHICS_SRC_DIR}/*.cpp" "${GRAPHICS_SRC_DIR}/opengl/*.cpp")
if(GRAPHICS_PLATFORM_SUPPORTS_OPENGL)
  defold_add_library(graphics STATIC ${GRAPHICS_OPENGL_SOURCES} ${GRAPHICS_PROTO_OBJECTS})
  graphics_common_setup(graphics)
  target_include_directories(graphics PUBLIC "${GRAPHICS_SRC_DIR}/opengl")
  if(TARGET_PLATFORM MATCHES "win32$")
    target_include_directories(graphics PUBLIC "${GRAPHICS_SRC_DIR}/opengl/win32")
  endif()
  if(TARGET_PLATFORM MATCHES "^(x86_64-linux|arm64-linux)$")
    target_compile_definitions(graphics PRIVATE GL_GLEXT_PROTOTYPES _GLFW_HAS_GLXGETPROCADDRESS)
  elseif(TARGET_PLATFORM MATCHES "^(wasm-web|wasm_pthread-web)$")
    target_compile_definitions(graphics PRIVATE GL_GLEXT_PROTOTYPES)
  endif()
  target_compile_definitions(graphics PUBLIC DM_GRAPHICS_USE_OPENGL)
  defold_target_link_libraries(graphics "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_DEPENDENCIES})
  list(APPEND GRAPHICS_INSTALL_TARGETS graphics)
endif()

if(GRAPHICS_BUILD_OPENGLES)
  defold_add_library(graphics_opengles STATIC ${GRAPHICS_OPENGL_SOURCES} ${GRAPHICS_PROTO_OBJECTS})
  graphics_common_setup(graphics_opengles)
  target_include_directories(graphics_opengles PUBLIC "${GRAPHICS_SRC_DIR}/opengl")
  target_compile_definitions(graphics_opengles PUBLIC DM_GRAPHICS_USE_OPENGLES)
  if(NOT TARGET_PLATFORM MATCHES "^(x86_64-ios|arm64-ios)$")
    target_compile_definitions(graphics_opengles PUBLIC DM_HAVE_OPENGL_COMPUTE_SUPPORT)
  endif()
  defold_target_link_libraries(graphics_opengles "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_DEPENDENCIES})
  set_target_properties(graphics_opengles PROPERTIES DEFOLD_SDK_HEADERS_TARGET graphics_sdk_headers)
  list(APPEND GRAPHICS_INSTALL_TARGETS graphics_opengles)
endif()

if(GRAPHICS_BUILD_WEBGPU)
  file(GLOB GRAPHICS_WEBGPU_SOURCES CONFIGURE_DEPENDS "${GRAPHICS_SRC_DIR}/*.cpp" "${GRAPHICS_SRC_DIR}/webgpu/*.cpp")
  foreach(_webgpu_target IN ITEMS graphics_webgpu graphics_webgpu_wagyu)
    if(_webgpu_target STREQUAL "graphics_webgpu_wagyu")
      set(_webgpu_define DM_GRAPHICS_WEBGPU_WAGYU)
    else()
      set(_webgpu_define DM_GRAPHICS_WEBGPU)
    endif()
    defold_add_library(${_webgpu_target} STATIC ${GRAPHICS_WEBGPU_SOURCES} ${GRAPHICS_PROTO_OBJECTS})
    graphics_common_setup(${_webgpu_target})
    target_include_directories(${_webgpu_target} PUBLIC "${GRAPHICS_SRC_DIR}/webgpu")
    target_compile_definitions(${_webgpu_target} PUBLIC ${_webgpu_define})
    defold_target_link_libraries(${_webgpu_target} "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_DEPENDENCIES})
    set_target_properties(${_webgpu_target} PROPERTIES DEFOLD_SDK_HEADERS_TARGET graphics_sdk_headers)
    list(APPEND GRAPHICS_INSTALL_TARGETS ${_webgpu_target})
  endforeach()
  target_compile_options(graphics_webgpu_wagyu PRIVATE "--use-port=${DEFOLD_SDK_ROOT}/ext/wagyu-port/webgpu-port.py:wagyu=true")
endif()

if(GRAPHICS_BUILD_VULKAN)
  file(GLOB GRAPHICS_VULKAN_SOURCES CONFIGURE_DEPENDS "${GRAPHICS_SRC_DIR}/*.cpp" "${GRAPHICS_SRC_DIR}/vulkan/*.cpp")
  list(FILTER GRAPHICS_VULKAN_SOURCES EXCLUDE REGEX "/vulkan/graphics_native\\.cpp$")
  graphics_native_source(_graphics_vulkan_native "vulkan")
  list(APPEND GRAPHICS_VULKAN_SOURCES "${_graphics_vulkan_native}")
  if(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos|x86_64-win32|x86-win32|x86_64-linux|arm64-linux)$")
    list(APPEND GRAPHICS_VULKAN_SOURCES "${GRAPHICS_SRC_DIR}/vulkan/desktop/graphics_vulkan_surface.cpp")
    if(TARGET_PLATFORM MATCHES "^(x86_64-macos|arm64-macos)$")
      list(APPEND GRAPHICS_VULKAN_SOURCES "${GRAPHICS_SRC_DIR}/vulkan/osx/graphics_vulkan_osx.mm")
    endif()
  elseif(TARGET_PLATFORM STREQUAL "arm64-ios")
    list(APPEND GRAPHICS_VULKAN_SOURCES
      "${GRAPHICS_SRC_DIR}/vulkan/ios/graphics_vulkan_ios_surface.mm"
      "${GRAPHICS_SRC_DIR}/vulkan/osx/graphics_vulkan_osx.mm")
  elseif(TARGET_PLATFORM MATCHES "^(armv7-android|arm64-android|x86_64-android)$")
    list(APPEND GRAPHICS_VULKAN_SOURCES "${GRAPHICS_SRC_DIR}/vulkan/android/graphics_vulkan_android.cpp")
  endif()
  if(TARGET_PLATFORM STREQUAL "arm64-nx64")
    list(FILTER GRAPHICS_VULKAN_SOURCES EXCLUDE REGEX "/graphics_glfw_wrappers\\.cpp$")
  endif()

  defold_add_library(graphics_vulkan STATIC ${GRAPHICS_VULKAN_SOURCES} ${GRAPHICS_PROTO_OBJECTS})
  graphics_common_setup(graphics_vulkan)
  target_include_directories(graphics_vulkan PUBLIC "${GRAPHICS_SRC_DIR}/vulkan")
  if(WITH_VULKAN_VALIDATION)
    target_compile_definitions(graphics_vulkan PUBLIC DM_VULKAN_VALIDATION=1)
  endif()
  if(TARGET platform_vulkan)
    defold_target_link_libraries(graphics_vulkan "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_CORE_DEPENDENCIES} platform_vulkan)
  else()
    defold_target_link_libraries(graphics_vulkan "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_DEPENDENCIES})
  endif()
  set_target_properties(graphics_vulkan PROPERTIES DEFOLD_SDK_HEADERS_TARGET graphics_sdk_headers)
  list(APPEND GRAPHICS_INSTALL_TARGETS graphics_vulkan)
endif()

if(GRAPHICS_BUILD_DX12)
  file(GLOB GRAPHICS_DX12_SOURCES CONFIGURE_DEPENDS "${GRAPHICS_SRC_DIR}/*.cpp" "${GRAPHICS_SRC_DIR}/dx12/*.cpp")
  list(FILTER GRAPHICS_DX12_SOURCES EXCLUDE REGEX "/dx12/graphics_dx12_(vendor|win32|xbox)\\.cpp$")
  set(GRAPHICS_DX12_PLATFORM_DIR "${GRAPHICS_SRC_DIR}/dx12")
  if(TARGET_PLATFORM_OS STREQUAL "xbone")
    set(_DEFOLD_XBONE_GRAPHICS_DX12_DIR "${DEFOLD_XBONE_PRIVATE_REPO_ROOT}/engine/graphics/src/dx12")
    if(NOT EXISTS "${_DEFOLD_XBONE_GRAPHICS_DX12_DIR}/graphics_dx12_xbox.cpp")
      message(FATAL_ERROR "Xbox DX12 source not found: ${_DEFOLD_XBONE_GRAPHICS_DX12_DIR}/graphics_dx12_xbox.cpp")
    endif()
    list(FILTER GRAPHICS_DX12_SOURCES EXCLUDE REGEX "/dx12/graphics_dx12(_private)?\\.cpp$")
    set(GRAPHICS_DX12_PLATFORM_DIR "${_DEFOLD_XBONE_GRAPHICS_DX12_DIR}")
    list(APPEND GRAPHICS_DX12_SOURCES
      "${GRAPHICS_DX12_PLATFORM_DIR}/graphics_dx12.cpp"
      "${GRAPHICS_DX12_PLATFORM_DIR}/graphics_dx12_private.cpp"
      "${GRAPHICS_DX12_PLATFORM_DIR}/graphics_dx12_xbox.cpp")
  elseif(TARGET_PLATFORM MATCHES "win32$")
    list(APPEND GRAPHICS_DX12_SOURCES "${GRAPHICS_DX12_PLATFORM_DIR}/graphics_dx12_win32.cpp")
  endif()
  defold_add_library(graphics_dx12 STATIC ${GRAPHICS_DX12_SOURCES} ${GRAPHICS_PROTO_OBJECTS})
  graphics_common_setup(graphics_dx12)
  target_include_directories(graphics_dx12 PUBLIC
    "${GRAPHICS_DX12_PLATFORM_DIR}"
    "${GRAPHICS_SRC_DIR}/dx12"
    "${DEFOLD_EXT_INCLUDE_DIR}/directx")
  defold_target_link_libraries(graphics_dx12 "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_DEPENDENCIES})
  set_target_properties(graphics_dx12 PROPERTIES DEFOLD_SDK_HEADERS_TARGET graphics_sdk_headers)
  list(APPEND GRAPHICS_INSTALL_TARGETS graphics_dx12)
endif()

if(GRAPHICS_BUILD_METAL)
  file(GLOB GRAPHICS_METAL_SOURCES CONFIGURE_DEPENDS
    "${GRAPHICS_SRC_DIR}/*.cpp"
    "${GRAPHICS_SRC_DIR}/metal/*.cpp"
    "${GRAPHICS_SRC_DIR}/metal/*.mm")
  defold_add_library(graphics_metal STATIC ${GRAPHICS_METAL_SOURCES} ${GRAPHICS_PROTO_OBJECTS})
  graphics_common_setup(graphics_metal)
  target_include_directories(graphics_metal PUBLIC "${GRAPHICS_SRC_DIR}/metal")
  target_compile_features(graphics_metal PUBLIC cxx_std_17)
  defold_target_link_libraries(graphics_metal "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_DEPENDENCIES})
  set_target_properties(graphics_metal PROPERTIES DEFOLD_SDK_HEADERS_TARGET graphics_sdk_headers)
  list(APPEND GRAPHICS_INSTALL_TARGETS graphics_metal)
endif()

defold_add_library(graphics_transcoder_null STATIC "${GRAPHICS_SRC_DIR}/transcoder/graphics_transcoder_null.cpp")
target_include_directories(graphics_transcoder_null PUBLIC "${GRAPHICS_SRC_DIR}" "${GRAPHICS_PROTO_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(graphics_transcoder_null graphics_proto_header)
defold_target_link_libraries(graphics_transcoder_null "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_TRANSCODER_NULL_DEPENDENCIES})

defold_add_library(graphics_transcoder_basisu STATIC "${GRAPHICS_SRC_DIR}/transcoder/graphics_transcoder_basisu.cpp")
target_include_directories(graphics_transcoder_basisu PUBLIC "${GRAPHICS_SRC_DIR}" "${GRAPHICS_PROTO_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(graphics_transcoder_basisu graphics_proto_header)
defold_target_link_libraries(graphics_transcoder_basisu "${TARGET_PLATFORM}" SCOPE PUBLIC ${GRAPHICS_TRANSCODER_BASISU_DEPENDENCIES})

list(APPEND GRAPHICS_INSTALL_TARGETS graphics_transcoder_null graphics_transcoder_basisu)

defold_install_targets(TARGETS ${GRAPHICS_INSTALL_TARGETS}
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

defold_add_sdk_headers_target(graphics_public_sdk_headers
  DESTINATION include/graphics
  SOURCE_ROOT "${GRAPHICS_SRC_DIR}"
  FILES ${GRAPHICS_PUBLIC_HEADERS}
  DEPENDS graphics_proto)
defold_add_sdk_headers_target(graphics_generated_sdk_headers
  DESTINATION include/graphics
  SOURCE_ROOT "${GRAPHICS_GEN_DIR}"
  FILES "${GRAPHICS_PROTO_H}"
  DEPENDS graphics_proto_header)
defold_add_sdk_headers_target(graphics_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk
  SOURCE_ROOT "${GRAPHICS_SRC_DIR}/dmsdk"
  FILES ${GRAPHICS_DMSDK_HEADERS}
  DEPENDS graphics_proto)
add_custom_target(graphics_sdk_headers
  DEPENDS
    graphics_public_sdk_headers
    graphics_generated_sdk_headers
    graphics_dmsdk_sdk_headers
    ddf_sdk_headers
    dlib_sdk_headers)

install(FILES
  "${GRAPHICS_SRC_DIR}/graphics.h"
  "${GRAPHICS_SRC_DIR}/graphics_util.h"
  "${GRAPHICS_SRC_DIR}/graphics_native.h"
  ${GRAPHICS_PROTO_H}
  DESTINATION include/graphics)
install(FILES "${GRAPHICS_SRC_DIR}/opengl/win32/glext.h" DESTINATION include/graphics/opengl/win32)
install(FILES "${GRAPHICS_SRC_DIR}/opengl/graphics_opengl_defines.h" DESTINATION include/graphics/opengl)
install(FILES "${GRAPHICS_PROTO}" DESTINATION share/proto/graphics)
install(FILES "${GRAPHICS_BPROTO}" DESTINATION share/proto/graphics)
install(FILES "${GRAPHICS_SRC_DIR}/texc.py" DESTINATION bin)
install(FILES "${GRAPHICS_SRC_DIR}/waf_graphics.py" DESTINATION lib/python)
install(FILES "${GRAPHICS_PROTO_PY_INIT}" "${GRAPHICS_PROTO_PY}" DESTINATION lib/python/graphics)
install(DIRECTORY "${GRAPHICS_SRC_DIR}/dmsdk/" DESTINATION sdk/include/dmsdk FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
