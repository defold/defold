cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_gui LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="GUI")

set(GUI_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GUI_SRC_DIR "${GUI_ROOT}/src")
set(GUI_PUBLIC_HEADERS
  "${GUI_SRC_DIR}/gui.h"
  "${GUI_SRC_DIR}/gui_script.h")
set(GUI_DEPENDENCIES
  ddf
  dlib
  font
  gameobject
  hid
  lua
  particle
  render
  rig
  script
  $<$<TARGET_EXISTS:graphics_proto>:graphics_proto>)
file(GLOB_RECURSE GUI_DMSDK_HEADERS CONFIGURE_DEPENDS
  "${GUI_SRC_DIR}/dmsdk/*.h"
  "${GUI_SRC_DIR}/dmsdk/*.hpp")

function(gui_configure_target target)
  target_include_directories(${target} PUBLIC "${GUI_SRC_DIR}")
  target_compile_definitions(${target} PRIVATE "DLIB_LOG_DOMAIN=\"GUI\"")
  foreach(_gui_dependency IN ITEMS
      graphics_proto_header
      particle_proto_headers
      rig_proto_headers
      render_proto_headers)
    if(TARGET ${_gui_dependency})
      add_dependencies(${target} ${_gui_dependency})
    endif()
  endforeach()
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" SCOPE PUBLIC
    ${GUI_DEPENDENCIES})
endfunction()

defold_add_library(gui STATIC
  "${GUI_SRC_DIR}/gui.cpp"
  "${GUI_SRC_DIR}/gui_script.cpp")
gui_configure_target(gui)

defold_add_library(gui_null STATIC "${GUI_SRC_DIR}/gui_null.cpp")
gui_configure_target(gui_null)
set_target_properties(gui_null PROPERTIES DEFOLD_SDK_HEADERS_TARGET gui_sdk_headers)

defold_install_targets(TARGETS gui gui_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

defold_add_sdk_headers_target(gui_public_sdk_headers
  DESTINATION include/gui
  SOURCE_ROOT "${GUI_SRC_DIR}"
  FILES ${GUI_PUBLIC_HEADERS}
  DEPENDS gui)
defold_add_sdk_headers_target(gui_dmsdk_sdk_headers
  DESTINATION sdk/include/dmsdk
  SOURCE_ROOT "${GUI_SRC_DIR}/dmsdk"
  FILES ${GUI_DMSDK_HEADERS}
  DEPENDS gui)
set(GUI_SDK_HEADER_TARGETS gui_public_sdk_headers gui_dmsdk_sdk_headers)
list(APPEND GUI_SDK_HEADER_TARGETS
  ddf_sdk_headers
  dlib_sdk_headers
  font_sdk_headers
  gameobject_sdk_headers
  hid_sdk_headers
  lua_sdk_headers
  particle_sdk_headers
  render_sdk_headers
  rig_sdk_headers
  script_sdk_headers)
add_custom_target(gui_sdk_headers
  DEPENDS ${GUI_SDK_HEADER_TARGETS})

install(FILES
  "${GUI_SRC_DIR}/gui.h"
  "${GUI_SRC_DIR}/gui_script.h"
  DESTINATION include/gui)
install(DIRECTORY "${GUI_SRC_DIR}/dmsdk/" DESTINATION sdk/include/dmsdk FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
