cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(box2d_defold_external VERSION 2.2.1 LANGUAGES CXX)

file(GLOB_RECURSE BOX2D_DEFOLD_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/Box2D/*.cpp")

add_library(box2d_defold STATIC ${BOX2D_DEFOLD_SOURCES})
target_include_directories(box2d_defold PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_compile_definitions(box2d_defold PRIVATE NDEBUG)
target_link_libraries(box2d_defold PRIVATE defold_sdk)

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS box2d_defold ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Box2D/"
        DESTINATION include/box2d_defold/Box2D
        FILES_MATCHING PATTERN "*.h")
