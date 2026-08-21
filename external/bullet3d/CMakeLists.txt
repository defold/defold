cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(bullet3d LANGUAGES ${DEFOLD_LANGUAGE_LIST})

if(NOT DEFOLD_EXTERNAL_INSTALL_PREFIX)
  message(FATAL_ERROR "DEFOLD_EXTERNAL_INSTALL_PREFIX must be set")
endif()

set(CMAKE_INSTALL_PREFIX "${DEFOLD_EXTERNAL_INSTALL_PREFIX}" CACHE PATH "Bullet install prefix" FORCE)

set(BULLET_VERSION 3.25)
set(BULLET_PHYSICS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/package/bullet-${BULLET_VERSION}")
set(BUILD_SHARED_LIBS OFF)
set(INSTALL_LIBS OFF)

add_subdirectory("${BULLET_PHYSICS_SOURCE_DIR}/src/LinearMath" LinearMath)
add_subdirectory("${BULLET_PHYSICS_SOURCE_DIR}/src/BulletCollision" BulletCollision)
add_subdirectory("${BULLET_PHYSICS_SOURCE_DIR}/src/BulletDynamics" BulletDynamics)

set(_BULLET_TARGETS LinearMath BulletCollision BulletDynamics)
foreach(_target IN LISTS _BULLET_TARGETS)
  target_compile_definitions(${_target} PRIVATE NDEBUG _USE_MATH_DEFINES)
  target_link_libraries(${_target} PRIVATE defold_sdk)
  set_target_properties(${_target} PROPERTIES PREFIX "lib")
endforeach()

install(TARGETS ${_BULLET_TARGETS}
        ARCHIVE DESTINATION "lib/${TARGET_PLATFORM}")

install(DIRECTORY "${BULLET_PHYSICS_SOURCE_DIR}/src/BulletCollision"
                  "${BULLET_PHYSICS_SOURCE_DIR}/src/BulletDynamics"
                  "${BULLET_PHYSICS_SOURCE_DIR}/src/LinearMath"
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
install(FILES
        "${BULLET_PHYSICS_SOURCE_DIR}/src/btBulletCollisionCommon.h"
        "${BULLET_PHYSICS_SOURCE_DIR}/src/btBulletDynamicsCommon.h"
        DESTINATION include)
install(FILES "${BULLET_PHYSICS_SOURCE_DIR}/LICENSE.txt"
        DESTINATION share/licenses/bullet3d)
