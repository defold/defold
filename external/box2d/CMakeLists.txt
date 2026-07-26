cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(box2d_external VERSION 3.1.0 LANGUAGES C)

file(GLOB BOX2D_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/Box2D/src/*.c")

foreach(_box2d_target IN ITEMS box2d box2d_nosimd)
  add_library(${_box2d_target} STATIC ${BOX2D_SOURCES})
  target_include_directories(${_box2d_target} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/Box2D/include"
    "${CMAKE_CURRENT_LIST_DIR}/Box2D/src")
  target_compile_features(${_box2d_target} PRIVATE c_std_17)
  target_link_libraries(${_box2d_target} PRIVATE defold_sdk)
endforeach()

target_compile_definitions(box2d_nosimd PRIVATE BOX2D_DISABLE_SIMD)

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS box2d box2d_nosimd ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Box2D/include/box2d"
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Box2D/src/"
        DESTINATION include/box2d/src
        FILES_MATCHING PATTERN "*.h")
