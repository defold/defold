set(MODELC_TEST_DIR "${CMAKE_CURRENT_LIST_DIR}")

defold_add_executable(test_model "${MODELC_TEST_DIR}/test_model.cpp")
target_include_directories(test_model PRIVATE "${MODELC_SRC_DIR}" "${MODELC_TEST_DIR}/..")
defold_target_link_libraries(test_model "${TARGET_PLATFORM}"
  model
  dlib
  zip
  mbedtls
  profile)
defold_register_test_target(test_model ON "${MODELC_ROOT}")
