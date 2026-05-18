cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_gui LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="GUI")

set(GUI_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GUI_SRC_DIR "${GUI_ROOT}/src")

function(gui_configure_target target)
  target_include_directories(${target} PUBLIC "${GUI_SRC_DIR}")
  target_compile_definitions(${target} PRIVATE "DLIB_LOG_DOMAIN=\"GUI\"")
  foreach(_gui_dependency IN ITEMS
      graphics_proto_header
      particle_proto_headers
      rig_proto_headers
      script_proto_headers
      render_proto_headers)
    if(TARGET ${_gui_dependency})
      add_dependencies(${target} ${_gui_dependency})
    endif()
  endforeach()
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}" SCOPE PUBLIC
    ddf
    dlib
    font
    hid
    lua
    particle
    render
    rig
    script
    $<$<TARGET_EXISTS:graphics_proto>:graphics_proto>)
endfunction()

defold_add_library(gui STATIC
  "${GUI_SRC_DIR}/gui.cpp"
  "${GUI_SRC_DIR}/gui_script.cpp")
gui_configure_target(gui)

defold_add_library(gui_null STATIC "${GUI_SRC_DIR}/gui_null.cpp")
gui_configure_target(gui_null)

defold_install_targets(TARGETS gui gui_null
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})
install(FILES
  "${GUI_SRC_DIR}/gui.h"
  "${GUI_SRC_DIR}/gui_script.h"
  DESTINATION include/gui)

if(BUILD_TESTS)
  add_subdirectory(src/test)
endif()
