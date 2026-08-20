set(SHADERC_STANDALONE_DIR "${CMAKE_CURRENT_LIST_DIR}")

defold_add_executable(shaderc_standalone "${SHADERC_STANDALONE_DIR}/shaderc_standalone.cpp")
target_include_directories(shaderc_standalone PRIVATE "${SHADERC_SRC_DIR}")
defold_target_link_libraries(shaderc_standalone "${TARGET_PLATFORM}"
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
