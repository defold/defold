cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(bullet_external VERSION 2.77 LANGUAGES CXX)

set(BULLET_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/package/bullet-2.77/src")
file(GLOB_RECURSE BULLET_COLLISION_SOURCES CONFIGURE_DEPENDS
  "${BULLET_SRC_DIR}/BulletCollision/*.cpp")
file(GLOB_RECURSE BULLET_DYNAMICS_SOURCES CONFIGURE_DEPENDS
  "${BULLET_SRC_DIR}/BulletDynamics/*.cpp")
file(GLOB_RECURSE BULLET_LINEAR_MATH_SOURCES CONFIGURE_DEPENDS
  "${BULLET_SRC_DIR}/LinearMath/*.cpp")

add_library(BulletCollision STATIC ${BULLET_COLLISION_SOURCES})
add_library(BulletDynamics STATIC ${BULLET_DYNAMICS_SOURCES})
add_library(LinearMath STATIC ${BULLET_LINEAR_MATH_SOURCES})

foreach(_bullet_target IN ITEMS BulletCollision BulletDynamics LinearMath)
  target_include_directories(${_bullet_target} PRIVATE "${BULLET_SRC_DIR}")
  target_compile_definitions(${_bullet_target} PRIVATE NDEBUG _USE_MATH_DEFINES)
  target_link_libraries(${_bullet_target} PRIVATE defold_sdk)
  if(NOT TARGET_PLATFORM MATCHES "win32$")
    target_compile_options(${_bullet_target} PRIVATE -Wno-c++11-narrowing)
  endif()
endforeach()

if(DEFOLD_EXTERNAL_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "External package install prefix" FORCE)
endif()

install(TARGETS BulletCollision BulletDynamics LinearMath
        ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")
install(DIRECTORY "${BULLET_SRC_DIR}/"
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
