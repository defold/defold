cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../../scripts/cmake")
include(defold)

set(PARTICLE_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PARTICLE_ROOT "${PARTICLE_TEST_ROOT}/../..")
set(PARTICLE_PROTO "${PARTICLE_ROOT}/proto/particle/particle_ddf.proto")
set(PARTICLE_TEST_BUILD_DIR "${PARTICLE_ROOT}/build/src/test")

file(GLOB PARTICLEFX_SOURCES CONFIGURE_DEPENDS "${PARTICLE_TEST_ROOT}/*.particlefx")
set(PARTICLEFX_OUTPUTS)
foreach(_particlefx IN LISTS PARTICLEFX_SOURCES)
  get_filename_component(_particlefx_name "${_particlefx}" NAME_WE)
  set(_particlefx_out "${PARTICLE_TEST_BUILD_DIR}/${_particlefx_name}.particlefxc")
  defold_protoc_encode(
    "${_particlefx_out}"
    "${_particlefx}"
    "${PARTICLE_PROTO}"
    "dmParticleDDF.ParticleFX"
    INCLUDES "${PARTICLE_ROOT}/proto/particle" "${PARTICLE_ROOT}/proto")
  list(APPEND PARTICLEFX_OUTPUTS "${_particlefx_out}")
endforeach()

add_custom_target(particle_test_data DEPENDS ${PARTICLEFX_OUTPUTS})

defold_add_executable(test_particle "${PARTICLE_TEST_ROOT}/test_particle.cpp")
add_dependencies(test_particle particle_test_data)
target_include_directories(test_particle PRIVATE
  "${PARTICLE_TEST_ROOT}"
  "${PARTICLE_ROOT}/src"
  "${PARTICLE_ROOT}/proto"
  "${CMAKE_CURRENT_BINARY_DIR}/../..")
defold_target_link_libraries(test_particle "${TARGET_PLATFORM}"
  testmain
  ddf
  dlib
  mbedtls
  platform_null
  graphics_null
  image
  graphics_proto
  profile_null
  particle)
defold_target_link_socket(test_particle "${TARGET_PLATFORM}" PRIVATE)
if(TARGET_PLATFORM MATCHES "arm64-linux|x86_64-linux")
  find_package(Threads REQUIRED)
  target_link_libraries(test_particle PRIVATE Threads::Threads dl)
endif()
defold_register_test_target(test_particle ON "${PARTICLE_ROOT}"
  STAGE_FILES
    "build/src/test" "build/src/test")
