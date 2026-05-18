set(SHADERC_TEST_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(SHADERC_TEST_DATA_DIR "${SHADERC_ROOT}/build/src/test/data")

set(SHADERC_GLSLANG "${DEFOLD_SDK_ROOT}/ext/bin/${HOST_PLATFORM}/glslang${CMAKE_EXECUTABLE_SUFFIX}")
if(NOT EXISTS "${SHADERC_GLSLANG}")
  set(SHADERC_GLSLANG "${DEFOLD_SDK_ROOT}/ext/bin/${HOST_PLATFORM}/glslang")
endif()
if(NOT EXISTS "${SHADERC_GLSLANG}")
  message(FATAL_ERROR "glslang not found for host platform ${HOST_PLATFORM}")
endif()

file(GLOB SHADERC_TEST_SHADERS CONFIGURE_DEPENDS
  "${SHADERC_TEST_DIR}/data/*.vert"
  "${SHADERC_TEST_DIR}/data/*.frag"
  "${SHADERC_TEST_DIR}/data/*.comp")

set(SHADERC_TEST_SPV)
foreach(_shader IN LISTS SHADERC_TEST_SHADERS)
  get_filename_component(_shader_name "${_shader}" NAME_WE)
  set(_spv "${SHADERC_TEST_DATA_DIR}/${_shader_name}.spv")
  add_custom_command(
    OUTPUT "${_spv}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADERC_TEST_DATA_DIR}"
    COMMAND "${SHADERC_GLSLANG}" -w --auto-map-bindings --auto-map-locations -g -V -o "${_spv}" "${_shader}"
    DEPENDS "${_shader}"
    COMMENT "Compiling ${_shader_name}.spv")
  list(APPEND SHADERC_TEST_SPV "${_spv}")
endforeach()

add_custom_target(shaderc_test_spv DEPENDS ${SHADERC_TEST_SPV})
add_dependencies(build_tests shaderc_test_spv)

defold_add_executable(test_shaderc "${SHADERC_TEST_DIR}/test_shaderc.cpp")
target_include_directories(test_shaderc PRIVATE "${SHADERC_SRC_DIR}" "${SHADERC_TEST_DIR}/..")
target_compile_definitions(test_shaderc PRIVATE "DM_SHADERC_TEST_DATA_DIR=\"${SHADERC_TEST_DATA_DIR}\"")
target_compile_definitions(test_shaderc PRIVATE ${SHADERC_DEFINES})
defold_target_link_libraries(test_shaderc "${TARGET_PLATFORM}"
  shaderc
  dlib
  zip
  mbedtls
  profile
  spirv-cross-c
  spirv-cross-core
  spirv-cross-glsl
  spirv-cross-hlsl
  spirv-cross-msl
  spirv-cross-reflect
  ${SHADERC_EXTRA_LIBS})
add_dependencies(test_shaderc shaderc_test_spv)
defold_register_test_target(test_shaderc ON "${SHADERC_ROOT}"
  STAGE_FILES
    "build/src/test/data" "build/src/test/data")
