cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_glfw_ios LANGUAGES ${DEFOLD_LANGUAGE_LIST})

if(NOT TARGET_PLATFORM MATCHES "^(arm64-ios|arm64_sim-ios)$")
  message(FATAL_ERROR "This GLFW CMake target supports iOS only")
endif()

file(GLOB GLFW_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/lib/*.c"
  "${CMAKE_CURRENT_LIST_DIR}/lib/ios/*.m"
  "${CMAKE_CURRENT_LIST_DIR}/lib/ios/app/*.m")

add_library(dmglfw STATIC ${GLFW_SOURCES})
target_include_directories(dmglfw PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/lib"
  "${CMAKE_CURRENT_LIST_DIR}/lib/ios")
target_link_libraries(dmglfw PRIVATE defold_sdk)

install(TARGETS dmglfw ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(FILES include/GL/glfw.h include/GL/glfw_native.h DESTINATION include/glfw)
