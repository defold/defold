cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_gui LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="GUI")

set(GUI_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(GUI_SRC_DIR "${GUI_ROOT}/src")

defold_add_library(gui STATIC
  "${GUI_SRC_DIR}/gui.cpp"
  "${GUI_SRC_DIR}/gui_script.cpp")
target_include_directories(gui PUBLIC "${GUI_SRC_DIR}")
target_compile_definitions(gui PRIVATE "DLIB_LOG_DOMAIN=\"GUI\"")
defold_target_link_libraries(gui "${TARGET_PLATFORM}" SCOPE PUBLIC lua)

defold_add_library(gui_null STATIC "${GUI_SRC_DIR}/gui_null.cpp")
target_include_directories(gui_null PUBLIC "${GUI_SRC_DIR}")
target_compile_definitions(gui_null PRIVATE "DLIB_LOG_DOMAIN=\"GUI\"")
defold_target_link_libraries(gui_null "${TARGET_PLATFORM}" SCOPE PUBLIC lua)

install(TARGETS gui gui_null
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
