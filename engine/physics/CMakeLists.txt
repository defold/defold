cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../scripts/cmake")
include(defold)

project(defold_physics LANGUAGES ${DEFOLD_LANGUAGE_LIST})

add_compile_definitions(DLIB_LOG_DOMAIN="PHYSICS")

set(PHYSICS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PHYSICS_SRC_ROOT "${PHYSICS_ROOT}/src")
set(PHYSICS_SRC_DIR "${PHYSICS_SRC_ROOT}/physics")

set(PHYSICS_COMMON_SOURCES
  "${PHYSICS_SRC_DIR}/physics.cpp"
  "${PHYSICS_SRC_DIR}/physics_common.cpp")
set(PHYSICS_BULLET_SOURCES
  "${PHYSICS_SRC_DIR}/physics_3d.cpp"
  "${PHYSICS_SRC_DIR}/debug_draw_3d.cpp")
set(PHYSICS_NULL_2D_SOURCE "${PHYSICS_SRC_DIR}/physics_2d_null.cpp")
set(PHYSICS_NULL_3D_SOURCE "${PHYSICS_SRC_DIR}/physics_3d_null.cpp")
file(GLOB PHYSICS_BOX2D_SOURCES CONFIGURE_DEPENDS "${PHYSICS_SRC_DIR}/box2d/*.cpp")
file(GLOB PHYSICS_BOX2D_DEFOLD_SOURCES CONFIGURE_DEPENDS "${PHYSICS_SRC_DIR}/box2d_defold/*.cpp")
defold_get_box2d_library(PHYSICS_BOX2D_LIB)

function(physics_configure_library target)
  target_include_directories(${target} PUBLIC
    "${PHYSICS_SRC_DIR}"
    "${PHYSICS_SRC_ROOT}")
  target_compile_definitions(${target} PRIVATE "DLIB_LOG_DOMAIN=\"PHYSICS\"")
endfunction()

defold_add_library(physics STATIC
  ${PHYSICS_COMMON_SOURCES}
  ${PHYSICS_BULLET_SOURCES}
  ${PHYSICS_BOX2D_DEFOLD_SOURCES})
physics_configure_library(physics)
target_include_directories(physics PUBLIC "${DEFOLD_EXT_INCLUDE_DIR}/box2d_defold")
defold_target_link_libraries(physics "${TARGET_PLATFORM}" dlib box2d_defold)
defold_target_link_bullet3d(physics "${TARGET_PLATFORM}" SCOPE PUBLIC)

defold_add_library(physics_null STATIC
  "${PHYSICS_SRC_DIR}/physics_common.cpp"
  "${PHYSICS_NULL_2D_SOURCE}"
  "${PHYSICS_NULL_3D_SOURCE}")
physics_configure_library(physics_null)
target_include_directories(physics_null PUBLIC "${DEFOLD_EXT_INCLUDE_DIR}/box2d_defold")
defold_target_link_libraries(physics_null "${TARGET_PLATFORM}" dlib)

defold_add_library(physics_2d STATIC
  ${PHYSICS_COMMON_SOURCES}
  "${PHYSICS_NULL_3D_SOURCE}"
  ${PHYSICS_BOX2D_SOURCES})
physics_configure_library(physics_2d)
target_include_directories(physics_2d PUBLIC "${DEFOLD_EXT_INCLUDE_DIR}/box2d")
target_include_directories(physics_2d BEFORE PUBLIC "${DEFOLD_EXT_INCLUDE_DIR}/box2d/..")
defold_target_link_libraries(physics_2d "${TARGET_PLATFORM}" dlib ${PHYSICS_BOX2D_LIB})

defold_add_library(physics_2d_defold STATIC
  ${PHYSICS_COMMON_SOURCES}
  "${PHYSICS_NULL_3D_SOURCE}"
  ${PHYSICS_BOX2D_DEFOLD_SOURCES})
physics_configure_library(physics_2d_defold)
target_include_directories(physics_2d_defold PUBLIC "${DEFOLD_EXT_INCLUDE_DIR}/box2d_defold")
defold_target_link_libraries(physics_2d_defold "${TARGET_PLATFORM}" dlib box2d_defold)

defold_add_library(physics_3d STATIC
  ${PHYSICS_COMMON_SOURCES}
  ${PHYSICS_BULLET_SOURCES}
  "${PHYSICS_NULL_2D_SOURCE}")
physics_configure_library(physics_3d)
defold_target_link_libraries(physics_3d "${TARGET_PLATFORM}" dlib)
defold_target_link_bullet3d(physics_3d "${TARGET_PLATFORM}" SCOPE PUBLIC)

defold_install_targets(TARGETS physics physics_null physics_2d physics_2d_defold physics_3d
        ARCHIVE DESTINATION lib/${TARGET_PLATFORM}
        LIBRARY DESTINATION lib/${TARGET_PLATFORM}
        RUNTIME DESTINATION bin/${TARGET_PLATFORM})

install(FILES "${PHYSICS_SRC_DIR}/physics.h" DESTINATION include/physics)
install(FILES "${PHYSICS_SRC_DIR}/waf_physics.py" DESTINATION lib/python)
if(BUILD_TESTS)
  add_subdirectory(src/physics/test)
endif()
