set(TEXC_TEST_DIR "${CMAKE_CURRENT_LIST_DIR}")

defold_add_executable(test_texc "${TEXC_TEST_DIR}/test_texc.cpp")
target_include_directories(test_texc PRIVATE "${TEXC_SRC_DIR}" "${TEXC_TEST_DIR}/..")
defold_target_link_libraries(test_texc "${TARGET_PLATFORM}"
  texc
  dlib
  zip
  mbedtls
  image
  profile_null
  basis_encoder
  basis_transcoder
  astcenc)
defold_register_test_target(test_texc ON "${TEXC_ROOT}")
