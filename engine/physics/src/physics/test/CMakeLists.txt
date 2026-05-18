cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../../scripts/cmake")
include(defold)

set(PHYSICS_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PHYSICS_ROOT "${PHYSICS_TEST_ROOT}/../../../..")
set(PHYSICS_SRC_ROOT "${PHYSICS_ROOT}/src")

function(physics_configure_test target physics_target run_test)
  target_include_directories(${target} PRIVATE
    "${PHYSICS_TEST_ROOT}"
    "${PHYSICS_SRC_ROOT}")
  defold_target_link_libraries(${target} "${TARGET_PLATFORM}"
    testmain
    dlib
    mbedtls
    profile_null
    ${physics_target})
  defold_target_link_socket(${target} "${TARGET_PLATFORM}" PRIVATE)
  if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${target} PRIVATE Threads::Threads dl)
  endif()
  defold_register_test_target(${target} ${run_test} "${PHYSICS_ROOT}")
endfunction()

defold_add_executable(test_physics_bullet3d "${PHYSICS_TEST_ROOT}/test_physics.cpp")
target_compile_definitions(test_physics_bullet3d PRIVATE PHYSICS_TEST_BULLET_3D)
physics_configure_test(test_physics_bullet3d physics ON)

defold_add_executable(test_physics_box2d "${PHYSICS_TEST_ROOT}/test_physics.cpp")
target_compile_definitions(test_physics_box2d PRIVATE PHYSICS_TEST_BOX2D)
physics_configure_test(test_physics_box2d physics_2d ON)

defold_add_executable(test_physics_box2d_defold "${PHYSICS_TEST_ROOT}/test_physics.cpp")
target_compile_definitions(test_physics_box2d_defold PRIVATE PHYSICS_TEST_BOX2D_DEFOLD)
target_include_directories(test_physics_box2d_defold PRIVATE "${DEFOLD_EXT_INCLUDE_DIR}/box2d_defold")
physics_configure_test(test_physics_box2d_defold physics_2d_defold ON)

defold_add_executable(test_physics_2d_box2d "${PHYSICS_TEST_ROOT}/test_physics_2d.cpp")
target_compile_definitions(test_physics_2d_box2d PRIVATE PHYSICS_TEST_BOX2D)
physics_configure_test(test_physics_2d_box2d physics_2d ON)
defold_target_link_bullet3d(test_physics_2d_box2d "${TARGET_PLATFORM}")

defold_add_executable(test_physics_2d_box2d_defold "${PHYSICS_TEST_ROOT}/test_physics_2d.cpp")
target_compile_definitions(test_physics_2d_box2d_defold PRIVATE PHYSICS_TEST_BOX2D_DEFOLD)
target_include_directories(test_physics_2d_box2d_defold PRIVATE "${DEFOLD_EXT_INCLUDE_DIR}/box2d_defold")
physics_configure_test(test_physics_2d_box2d_defold physics_2d_defold ON)
defold_target_link_bullet3d(test_physics_2d_box2d_defold "${TARGET_PLATFORM}")

defold_add_executable(test_physics_null "${PHYSICS_TEST_ROOT}/test_physics.cpp")
target_compile_definitions(test_physics_null PRIVATE PHYSICS_TEST_BOX2D_DEFOLD)
target_include_directories(test_physics_null PRIVATE "${DEFOLD_EXT_INCLUDE_DIR}/box2d_defold")
physics_configure_test(test_physics_null physics_null OFF)
defold_target_link_bullet3d(test_physics_null "${TARGET_PLATFORM}")

defold_add_executable(test_physics_2d_null "${PHYSICS_TEST_ROOT}/test_physics_2d.cpp")
target_compile_definitions(test_physics_2d_null PRIVATE PHYSICS_TEST_BOX2D_DEFOLD)
target_include_directories(test_physics_2d_null PRIVATE "${DEFOLD_EXT_INCLUDE_DIR}/box2d_defold")
physics_configure_test(test_physics_2d_null physics_null OFF)
defold_target_link_bullet3d(test_physics_2d_null "${TARGET_PLATFORM}")
